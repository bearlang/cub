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

  def parse_block(self, end_token):
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
      return self.parse_function(self)

    if structure == disambiguate.G_DEFINE:
      result = self.parse_define(parser, True)
    elif structure == disambiguate.G_EXPRESSION:
      result = self.ExpressionStatement(self.parse_expression())
    else:
      expected_error(self.reader, self.reader.peek(), "statement")

    self.expect(tokens.L_SEMICOLON)
    return result
