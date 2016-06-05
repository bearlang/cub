import tokens

class Statement(object):
  def __init__(self, type):
    self.type = type

class ControlStatement(Statement):
  def __init__(self, type, parser):
    parser._shift()
    super(BreakStatement, self).__init__(self, type)
    label_token = parser.accept(L_IDENTIFIER)
    self.label = label_token.value if label_token else None
    parser.expect(L_SEMICOLON)

class BreakStatement(ControlStatement):
  def __init__(self, parser):
    super(BreakStatement, self).__init__(self, S_BREAK, parser)

class ContinueStatement(ControlStatement):
  def __init__(self, parser):
    super(ContinueStatement, self).__init__(self, S_CONTINUE, parser)

token_map = {
  tokens.L_BREAK: BreakStatement,
  tokens.L_CLASS: ClassStatement,
  tokens.L_CONTINUE: ContinueStatement,
  tokens.L_IF: IfStatement,
  tokens.L_LET: LetStatement,
  tokens.L_OPEN_BRACE: parse_block,
  tokens.L_RETURN: ReturnStatement,
  tokens.L_TYPEDEF: TypedefStatement,
  tokens.L_DO: DoWhileStatement,
  tokens.L_WHILE: WhileStatement,
  tokens.L_FOR: parse_for,
  tokens.L_IDENTIFIER: parse_identifier
}
