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
  tokens.L_IDENTIFIER: 'parse_ambiguous'
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
    return self.parse_block(tokens.L_EOF)

  # internal api
  def _fail(self, expected):
    token = self.reader.peek()
    if isinstance(expected, Token):
      expected_error(self.reader, token, expected)
    elif hasattr(expected, '__call__') and hasattr(expected, '__name__'):
      expected_error(self.reader, token, expected_fail_map[expected.__name__])
    else:
      expected_error(self.reader, token, "something")

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
      expected_error(self.reader, None, "statement")

    token_type = token.token_type

    if token_type == tokens.L_SEMICOLON:
      return None

    if token_type in statement.token_map:
      method = statement.token_map[token_type]
      if hasattr(self, method):
        return self.getattr(method)()
      raise RuntimeError('misconfigured parser')

    reader.push(token)

    return self.parse_ambiguous_statement()

  # definition generators
  def gen_define_inner(parser, allow_init):
    symbol = parser.expect(tokens.L_IDENTIFIER).value

    def symgen():
      yield symbol
      while parse.accept(L_COMMA) is not None:
        yield parser.expect(tokens.L_IDENTIFIER).value

    def gen():
      for symbol in symgen():
        yield symbol, (parse_expression(parser) if (allow_init and \
          parser.accept(L_ASSIGN)) else None)

    return define_type, gen()

  def gen_define(parser, allow_init):
    define_type = parse_type(parser, False, False).return_type

    if define_type is None:
      raise Exception("expected define statement")

    if define_type == T_VOID:
      # TODO: line numbers
      raise Exception("variables declared void")

    return define_type, gen_define_inner(parser, allow_init)


  # type-related definitions
  def parse_class(parser):
    class_name = parser.expect(tokens.L_IDENTIFIER).value
    parent_name = None
    if parser.accept(L_EXTENDS):
      parent_name = parser.expect(tokens.L_IDENTIFIER).value

    parser.expect(L_OPEN_BRACE)

    entries = []

    while parser.accept(L_CLOSE_BRACE) is None:
      type, gen = gen_define(parser, False)
      for name, value in gen:
        entries.append(Field(type, name))
      parser.expect(L_SEMICOLON)

    return ClassStatement(Class(class_name, parent_name, entries))

  def parse_typedef(parser):
    left = parser.parse_type(False, False).return_type
    parser.expect(L_AS)
    return TypedefStatement(left, parser.expect(tokens.L_IDENTIFIER).value)


  # control flow structures

  # default exists for token_map calls
  def parse_block(self, end_token=tokens.L_CLOSE_BRACE):
    result = statement.BlockStatement(None)
    tail = None

    while not self.accept(end_token):
      next = self.expect_statement()

      # noop (;) statement, start again from the top
      if next is None: continue

      if result.body is None: result.body = next
      else: tail.next = next
      next.parent = result
      tail = next

    return result

  def parse_return(parser):
    value = parser.accept_delimited(L_SEMICOLON, parser.parse_expression)
    return ReturnStatement(value)

  def parse_if(parser):
    parser.expect(L_OPEN_PAREN)
    condition = parser.parse_expression()
    parser.expect(L_CLOSE_PAREN)

    first = parser.expect_statement()
    second = parser.expect_statement() if parser.accept(L_ELSE) else None
    branch = IfStatement(condition, None, None)

    branch.first = set_block_parent(first, branch)
    branch.second = set_block_parent(second, branch)

    return branch

  def parse_loop(parser, type, body, condition):
    body_block = None if body is None else body.wrap_block()

    loop_type = S_DO_WHILE if type == L_DO else S_WHILE
    loop = statement.LoopStatement(loop_type, condition, body_block)

    if body_block:
      body_block.parent = loop

    return loop

  def parse_while(parser):
    parser.expect(L_OPEN_PAREN)
    condition = parser.parse_expression()
    parser.expect(L_CLOSE_PAREN)

    return parse_loop(parser, L_WHILE, parser.expect_statement(), condition)

  def parse_do_while(parser):
    reader = parser.reader
    body = parser.expect_statement()

    reader.expect(tokens.L_WHILE)
    reader.expect(tokens.L_OPEN_PAREN)
    condition = parser.parse_expression()
    reader.expect(tokens.L_CLOSE_PAREN)
    reader.expect(tokens.L_SEMICOLON)

    return parse_loop(parser, L_DO, body, condition)

  def parse_for(parser):
    parser.expect(state, L_OPEN_PAREN)

    def initializer():
      structure = parser.disambiguate_statement()
      if structure == G_DEFINE: return parse_define(parser)
      if structure == G_EXPRESSION:
        return ExpressionStatement(parser.parse_expression())
      raise Exception("expected define or expression")

    init = parser.accept_delimited(L_SEMICOLON, initializer)
    condition = parser.accept_delimited(L_SEMICOLON, self.parse_expression,
      lambda: BoolLiteral(None, True)) # TODO: NoneType for token?
    each = parser.accept_delimited(L_SEMICOLON, parser.parse_expression)
    parser.expect(L_SEMICOLON)

    body = parser.expect_statement()

    if body is not None: body = body.wrap_block()

    if each is not None:
      each = ExpressionStatement(each)
      if body is None:
        body = BlockStatement(each)
        each.parent = body
      else:
        inner_body = body
        inner_body.next = each
        body = BlockStatement(inner_body)
        inner_body.parent = body
        each.parent = body

    result = LoopStatement(S_WHILE, condition, body)

    if body: body.parent = result

    if init:
      init.next = result
      result = BlockStatement(init)
      init.parent = result
      init.next.parent = result

    return result


  # labeled control flow structures
  def parse_labeled(reader, cons):
    reader.pop()
    label_token = reader.accept(tokens.L_IDENTIFIER)
    label = label_token.value if label_token else None
    return cons(label)

  def parse_break(reader):
    return parse_labeled(reader, statement.BreakStatement)

  def parse_continue(reader):
    return parse_labeled(reader, statement.ContinueStatement)


  # bindings
  def parse_define(parser):
    type, gen = gen_define(parser, True)
    clauses = [DefineClause(name, value) for name, value in gen]
    parser.expect(L_SEMICOLON)
    return DefineStatement(type, clauses)

  def parse_let(parser):
    clauses = list(gen_define_inner(parser, True))

    parser.expect(L_SEMICOLON)

    return LetStatement()


  # ambiguous resolution
  def parse_ambiguous(reader):
    structure = disambiguate_statement(reader)

    if structure == G_FUNCTION:
      return statement.FunctionStatement(parse_function(reader, allow_anon=False))

    if structure == G_DEFINE:
      result = expect_define_statement(parse_expression(reader))
    elif structure == G_EXPRESSION:
      result = statement.ExpressionStatement(parse_expression(reader))
    else:
      raise reader.error("expected statement")

    reader.expect(tokens.L_SEMICOLON)
    return result

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
      result = self.ExpressionStatement(self.parse_expression())
    else:
      expected_error(self.reader, self.reader.peek(), "statement")

    self.expect(tokens.L_SEMICOLON)
    return result
