import statement

def parse_labeled(reader, cons):
  reader.pop()
  label_token = reader.accept(tokens.L_IDENTIFIER)
  label = label_token.value if label_token else None
  return cons(label)

def parse_break(reader):
  return parse_labeled(reader, statement.BreakStatement)

def parse_continue(reader):
  return parse_labeled(reader, statement.ContinueStatement)

def parse_class(reader):
  reader.pop()
  return statement.ClassStatement

def parse_if(reader):
  reader.pop()

def parse_let(reader):
  reader.pop()

def parse_block(reader):
  reader.pop()

def parse_return(reader):
  reader.pop()

def parse_typedef(reader):
  reader.pop()

def parse_loop(parser, type, body, condition):
  body_block = None if body is None else body.wrap_block()

  loop_type = S_DO_WHILE if type == L_DO else S_WHILE
  loop = statement.LoopStatement(loop_type, condition, body_block)

  if body_block:
    body_block.parent = loop

  return loop

def parse_do_while(parser):
  reader = parser.reader
  body = parser.expect_statement()

  reader.expect(tokens.L_WHILE)
  reader.expect(tokens.L_OPEN_PAREN)
  condition = parser.parse_expression()
  reader.expect(tokens.L_CLOSE_PAREN)
  reader.expect(tokens.L_SEMICOLON)

  return parse_loop(parser, L_DO, body, condition)

def parse_while(parser):
  parser.expect(L_OPEN_PAREN)
  condition = parser.parse_expression()
  parser.expect(L_CLOSE_PAREN)

  return parse_loop(parser, L_WHILE, parser.expect_statement(), condition)

def parse_for(parser):
  parser.expect(state, L_OPEN_PAREN)

  def initializer():
    structure = parser.disambiguate_statement()
    if structure == G_DEFINE: return parse_define(parser, True)
    if structure == G_EXPRESSION:
      return ExpressionStatement(parser.parse_expression())
    raise Exception, "expected define or expression"

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
    raise Exception, "expected define statement"

  if define_type == T_VOID:
    # TODO: line numbers
    raise Exception, "variables declared void"

  return define_type, gen_define_inner(parser, allow_init)

def parse_define(parser):
  type, gen = gen_define(parser, True)
  clauses = [DefineClause(name, value) for name, value in gen]
  parser.expect(L_SEMICOLON)
  return DefineStatement(type, clauses)

def parse_control(cons, parser):
  parser._shift()
  ctrl = cons(parser)
  label_token = parser.accept(L_IDENTIFIER)
  if label_token: ctrl.label = label_token.value
  parser.expect(L_SEMICOLON)

def parse_break(parser):
  return parse_control(BreakStatement, parser)

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

def parse_continue(parser):
  return parse_control(ContinueStatement, parser)

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

def parse_let(parser):
  clauses = list(gen_define_inner(parser, True))

  parser.expect(L_SEMICOLON)

  return LetStatement()

def parse_block(parser):
  return parser.parse_block(L_CLOSE_BRACE)

def parse_return(parser):
  value = parser.accept_delimited(L_SEMICOLON, parser.parse_expression)
  return ReturnStatement(value)

def parse_typedef(parser):
  left = parser.parse_type(False, False).return_type
  parser.expect(L_AS)
  return TypedefStatement(left, parser.expect(tokens.L_IDENTIFIER).value)


