import tokens
from statement import token_map

def parse_error(token, message):
  raise ParseError(token.line, token.offset, message)

def expected_error(token, token_type):
  parse_error(token, "expected '%s', found '%s'" %
    (tokens.token_string(token_type), tokens.token_string(token)))

def set_block_parent(child, parent):
  if child is None: return None

  if child.type == S_BLOCK: child_block = child
  else: child_block = BlockStatement(child)

  child_block.parent = parent
  return child_block

class Parser:
  def __init__(self, scanner):
    self.scanner = scanner
    self.buffer = []

  def _peek(self):
    if len(self.buffer):
      return self.buffer[-1]

    token = self.scanner.scan()

    if token is None:
      return self.scanner._token(L_EOF)

    self.buffer.append(token)
    return token

  def _pull(self):
    if len(self.buffer):
      return self.buffer.pop()

    token = self.scanner.scan()

    if token is None:
      return self.scanner._token(L_EOF)

    return token

  def _push(self, token):
    self.buffer.append(token)

  def accept(self, token_type):
    token = self._peek()

    if token.token_type == token_type:
      self._shift()
      return token

  def expect(self, token_type):
    token = self._shift()

    if token.token_type != token_type:
      expected_error(token, token_type)

    return token

  def expect_identifier(self):
    return expect(L_IDENTIFIER).value

  def parse(self):
    return self.parse_block(tokens.L_EOF)

  def parse_block(self, end_token):
    result = BlockStatement(None)
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
    token = self._peek()
    token_type = token.token_type

    if token_type == L_SEMICOLON:
      self._shift()
      return None

    if token_type in token_map:
      token_map[token_type](self)

    self.parse_ambiguous_statement()

  def expect_statement(self):
    token = self._shift()

    if token is None:
      self.scanner._error("expected statement")

    type = token.token_type

    if type == L_SEMICOLON:
      return None

    if type == L_BREAK or type == L_CONTINUE:
      token = self.accept(L_IDENTIFIER)
      label = None if token is None else token.value
      self.expect(L_SEMICOLON)

      return ControlStatement(S_BREAK if type == L_BREAK else S_CONTINUE, label)

    if type == L_CLASS:
      return self.parse_class()

    if type == L_IF:
      self.expect(L_OPEN_PAREN)
      condition = self.parse_expression()
      self.expect(L_CLOSE_PAREN)

      first = self.expect_statement()
      second = self.expect_statement() if self.accept(L_ELSE) else None

      branch = IfStatement(condition, None, None)

      branch.first = set_block_parent(first, branch)
      branch.second = set_block_parent(second, branch)

      return branch

    if type == L_LET:
      # TODO: what about coords?
      symbol = self.expect_identifier()

      self.expect(L_ASSIGN)
      clauses = [DefineClause(symbol, self.parse_expression(), None)]

      while self.accept(L_COMMA):
        symbol = self.expect_identifier()

        self.expect(L_ASSIGN)

        clauses.append(DefineClause(symbol, self.parse_expression(), None))

      self.expect(L_SEMICOLON)

      return LetStatement(clauses)

    if type == L_OPEN_BRACE:
      return self.parse_block(L_CLOSE_BRACE)

    if type == L_RETURN:
      if self.accept(L_SEMICOLON): value = None
      else:
        value = self.parse_expression()
        self.expect(L_SEMICOLON)

      return ReturnStatement(value)

    if type == L_TYPEDEF:
      left = self.parse_type(False, False)[0]
      self.expect(L_AS)
      return TypedefStatement(left, self.expect_identifier())

    if type == L_DO or type == L_WHILE:
      if type == L_DO:
        body = self.expect_statement()

        self.expect(L_WHILE)
        self.expect(L_OPEN_PAREN)
        condition = self.parse_expression()
        self.expect(L_CLOSE_PAREN)
        self.expect(L_SEMICOLON)
      else:
        self.expect(L_OPEN_PAREN)
        condition = self.parse_expression()
        self.expect(L_CLOSE_PAREN)

        body = self.expect_statement()

      if body is None: body_block = None
      elif body.type == S_BLOCK: body_block = body
      else: body_block = BlockStatement(body)

      loop = LoopStatement(S_DO_WHILE if type == L_DO else S_WHILE, condition,
        body_block)

      if body_block:
        body_block.parent = loop

      return loop

    if type == L_FOR:
      self.expect(state, L_OPEN_PAREN)

      if self.accept(L_SEMICOLON): init = None
      else:
        structure = self.disambiguate_statement()
        if structure == G_DEFINE:
          init = self.expect_define_statement(True)
        elif structure == G_EXPRESSION:
          init = ExpressionStatement(self.parse_expression())
        else:
          parse_error(self._peek(), "expected define or expression")

        self.expect(L_SEMICOLON)

      if self.accept(L_SEMICOLON):
        condition = BoolLiteral(token, True)
      else:
        condition = self.parse_expression()
        self.expect(L_SEMICOLON)

      if self.accept(L_SEMICOLON):
        iterator = None
      else:
        iterator = self.parse_expression()
        self.expect(L_SEMICOLON)

      self.expect(L_CLOSE_PAREN)

      body = self.expect_statement()

      if body and body.type != S_BLOCK:
        body = BlockStatement(body)

      if iterator:
        each = ExpressionStatement(iterator)
        if body is None:
          body = BlockStatement(each)
        else:
          body.next = each
          body = BlockStatement(body)
          each.parent = body

      result = LoopStatement(S_WHILE, condition, body)

      if body: body.parent = result

      if init:
        init.next = result
        result = BlockStatement(init)
        init.next.parent = result

      return result

    if type == L_IDENTIFIER and self.accept(L_COLON):
      next = self._peek()

      if next.token_type not in [L_DO, L_WHILE, L_FOR]:
        parse_error(next, "expected loop after label")

      loop = self.expect_statement()

      if loop.type != S_LOOP:
        raise ParseError(loop.line, loop.offset, "expected loop after label")

      loop.label = token.value
      return loop

    self._push(token)

    return self.parse_ambiguous_statement()

  def parse_ambiguous_statement(self):
    structure = self.disambiguate_statement()

    if structure ==
