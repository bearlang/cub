import pycub.tokens as tokens
import pycub.statement as statement
import pycub.disambiguate as disambiguate
from pycub.reader import Reader

# not sure how pythonic this is
token_map = {
  tokens.L_BREAK: 'parse_break',
  tokens.L_CLASS: 'parse_class',
  tokens.L_CONTINUE: 'parse_continue',
  tokens.L_IF: 'parse_if',
  tokens.L_LET: 'parse_let',
  tokens.L_OPEN_BRACE: 'parse_block',
  tokens.L_RETURN: 'parse_return',
  tokens.L_TYPEDEF: 'parse_typedef',
  tokens.L_DO: 'parse_do_while',
  tokens.L_WHILE: 'parse_while',
  tokens.L_FOR: 'parse_for',
  tokens.L_IDENTIFIER: 'parse_ambiguous_statement'
}

expected_fail_map = {
  # 'parse_break': 'break statement',
  # 'parse_class': 'class declaration,
  # 'parse_continue': 'continue statement',
  # 'parse_if': 'if statement',
  # 'parse_let': 'let statement',
  'parse_block': 'block',
  # 'parse_return': 'return statement',
  'parse_typedef': 'type definition',
  # 'parse_do_while': 'do-while loop',
  # 'parse_while': 'while loop',
  # 'parse_for': 'for loop'
}

class ParseError(RuntimeError):
  def __init__(self, line, offset, message):
    self.line = line
    self.offset = offset
    self.message = message

  def __str__(self):
    return "%s at %d:%d" % (self.message, self.line, self.offset)

def parse_error(token, message):
  raise ParseError(token.line, token.offset, message)

def expected_error(reader, token, expected):
  if token is None:
    raise ParseError(reader.line, reader.offset,
      "expected %s, found EOF" % expected)

  parse_error(token, "expected '%s', found '%s'" %
    (expected, tokens.token_string(token)))

def set_block_parent(child, parent):
  if child is None: return None
  else: child_block = child.wrap_block()

  child_block.parent = parent
  return child_block

class Parser:
  # public api
  def __init__(self, scanner):
    self.scanner = scanner
    self.reader = Reader(scanner, matches='token_type', fail=self._fail)

  def parse(self):
    return self.parse_block(None)

  # internal api
  def _fail(self, expected):
    token = self.reader.peek()
    if isinstance(expected, Token):
      expected_error(self.scanner.reader, token, expected)
    elif hasattr(expected, '__call__') and hasattr(expected, '__name__'):
      expected_error(self.scanner.reader, token, expected_fail_map[expected.__name__])
    else:
      expected_error(self.scanner.reader, token, "something")

  def accept(self, token):
    if token is None:
      return self.reader.peek() is None
    return self.reader.accept(token)

  def lookahead(self):
    return self.reader.lookahead()

  # branching point when we expect a statement
  def expect_statement(self):
    reader = self.reader
    token = reader.pop()

    if token is None:
      expected_error(self.scanner.reader, None, "statement")

    token_type = token.token_type

    if token_type == tokens.L_SEMICOLON:
      return None

    if token_type in token_map:
      method = token_map[token_type]
      if hasattr(self, method):
        return getattr(self, method)()
      raise RuntimeError('misconfigured parser for method %s' % method)

    reader.push(token)

    return self.parse_ambiguous_statement()

  # definition generators
  def gen_define_inner(self, allow_init):
    reader = self.reader
    symbol = reader.expect(tokens.L_IDENTIFIER).value

    def symgen():
      yield symbol
      while parse.accept(L_COMMA) is not None:
        yield reader.expect(tokens.L_IDENTIFIER).value

    def gen():
      for symbol in symgen():
        yield symbol, (self.parse_expression() if (allow_init and \
          reader.accept(tokens.L_ASSIGN)) else None)

    return define_type, gen()

  def gen_define(self, allow_init):
    define_type = self.parse_type(False, False).return_type

    if define_type is None:
      raise Exception("expected define statement")

    if define_type == T_VOID:
      # TODO: line numbers
      raise Exception("variables declared void")

    return define_type, self.gen_define_inner(allow_init)


  # type-related definitions
  def parse_class(self):
    reader = self.reader

    class_name = reader.expect(tokens.L_IDENTIFIER).value
    parent_name = None
    if reader.accept(tokens.L_EXTENDS) is not None:
      parent_name = reader.expect(tokens.L_IDENTIFIER).value

    reader.expect(tokens.L_OPEN_BRACE)

    entries = []

    while reader.accept(L_CLOSE_BRACE) is None:
      type, gen = self.gen_define(False)
      for name, value in gen:
        entries.append(Field(type, name))
      reader.expect(tokens.L_SEMICOLON)

    return statement.ClassStatement(Class(class_name, parent_name, entries))

  def parse_typedef(self):
    reader = self.reader
    left = self.parse_type(False, False).return_type
    reader.expect(tokens.L_AS)
    return TypedefStatement(left, reader.expect(tokens.L_IDENTIFIER).value)


  # control flow structures

  # default exists for token_map calls
  def parse_block(self, end_token=tokens.L_CLOSE_BRACE):
    result = statement.BlockStatement(None)
    tail = None

    # self.accept to handle None
    while not self.accept(end_token):
      next = self.expect_statement()

      # noop (;) statement, start again from the top
      if next is None: continue

      if result.body is None: result.body = next
      else: tail.next = next
      next.parent = result
      tail = next

    return result

  def parse_return(self):
    reader = self.reader
    value = reader.accept_delimited(tokens.L_SEMICOLON, self.parse_expression)
    return statement.ReturnStatement(value)

  def parse_if(self):
    reader = self.reader
    reader.expect(tokens.L_OPEN_PAREN)
    condition = self.parse_expression()
    reader.expect(tokens.L_CLOSE_PAREN)

    first = self.expect_statement()
    if reader.accept(tokens.L_ELSE) is None:
      second = None
    else:
      second = self.expect_statement()
    branch = statement.IfStatement(condition, None, None)

    branch.first = set_block_parent(first, branch)
    branch.second = set_block_parent(second, branch)

    return branch

  def parse_loop(self, type, body, condition):
    body_block = None if body is None else body.wrap_block()

    if type == tokens.L_DO:
      loop_type = statement.S_DO_WHILE
    else:
      loop_type = statement.S_WHILE
    loop = statement.LoopStatement(loop_type, condition, body_block)

    if body_block:
      body_block.parent = loop

    return loop

  def parse_while(self):
    reader = self.reader
    reader.expect(tokens.L_OPEN_PAREN)
    condition = self.parse_expression()
    reader.expect(tokens.L_CLOSE_PAREN)

    return self.parse_loop(tokens.L_WHILE, self.expect_statement(), condition)

  def parse_do_while(self):
    reader = self.reader
    body = self.expect_statement()

    reader.expect(tokens.L_WHILE, tokens.L_OPEN_PAREN)
    condition = self.parse_expression()
    reader.expect(tokens.L_CLOSE_PAREN, tokens.L_SEMICOLON)

    return self.parse_loop(tokens.L_DO, body, condition)

  def parse_for(self):
    reader = self.reader
    self.expect(state, tokens.L_OPEN_PAREN)

    def initializer():
      structure = self.parse_ambiguous_statement()
      if structure == G_DEFINE: return self.parse_define()
      if structure == G_EXPRESSION:
        return statement.ExpressionStatement(self.parse_expression())
      raise Exception("expected define or expression")

    init = reader.accept_delimited(tokens.L_SEMICOLON, initializer)
    # TODO: NoneType for token? Better to create a fake token or at least a fake
    # location in the file
    condition = reader.accept_delimited(tokens.L_SEMICOLON,
      self.parse_expression, lambda: BoolLiteral(None, True))
    each = reader.accept_delimited(tokens.L_SEMICOLON, self.parse_expression)
    reader.expect(tokens.tokens.L_SEMICOLON)

    body = self.expect_statement()

    if body is not None: body = body.wrap_block()

    if each is not None:
      each = statement.ExpressionStatement(each)
      if body is None:
        body = statement.BlockStatement(each)
        each.parent = body
      else:
        inner_body = body
        inner_body.next = each
        body = statement.BlockStatement(inner_body)
        inner_body.parent = body
        each.parent = body

    result = statement.LoopStatement(statement.S_WHILE, condition, body)

    if body: body.parent = result

    if init:
      init.next = result
      result = statement.BlockStatement(init)
      init.parent = result
      init.next.parent = result

    return result


  # labeled control flow structures
  def parse_labeled(self, cons):
    label_token = self.reader.expect_terminated(tokens.L_SEMICOLON,
      tokens.L_IDENTIFIER)
    label = label_token.value if label_token else None
    return cons(label)

  def parse_break(self):
    self.reader.pop() # break
    return self.parse_labeled(statement.BreakStatement)

  def parse_continue(self):
    self.reader.pop() # continue
    return self.parse_labeled(statement.ContinueStatement)


  # bindings
  def parse_define(self):
    type, gen = self.gen_define(True)
    clauses = [statement.DefineClause(name, value) for name, value in gen]
    self.reader.expect(tokens.L_SEMICOLON)
    return statement.DefineStatement(type, clauses)

  def parse_let(self):
    clauses = list(self.gen_define_inner(True))
    self.reader.expect(tokens.L_SEMICOLON)
    return LetStatement()


  # ambiguous resolution
  def parse_ambiguous_statement(self):
    reader = self.reader

    if reader.accept(tokens.L_IDENTIFIER, tokens.L_COLON) is not None:
      next = reader.peek()

      if next.token_type not in {tokens.L_DO, tokens.L_WHILE, tokens.L_FOR}:
        parse_error(next, "expected loop after label")

      loop = self.expect_statement()

      # TODO: this doesn't feel right
      try:
        loop.set_label(token.value)
      except Exception:
        raise ParseError(loop.line, loop.offset, "expected loop after label")

      return loop

    structure = disambiguate.disambiguate_statement(self)

    if structure == disambiguate.G_FUNCTION:
      return statement.FunctionStatement(self.parse_function(allow_anon=False))

    if structure == disambiguate.G_DEFINE:
      result = self.parse_define(parser)
    elif structure == disambiguate.G_EXPRESSION:
      result = statement.ExpressionStatement(self.parse_expression())
    else:
      expected_error(self.scanner.reader, self.reader.peek(), "statement")

    self.expect(tokens.L_SEMICOLON)
    return result
