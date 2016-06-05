def parse_labeled(token_stream, cons):
  token_stream.shift()
  label_token = token_stream.accept(tokens.L_IDENTIFIER)
  label = label_token.value if label_token else None
  return cons(label)

def parse_break(token_stream):
  return parse_labeled(token_stream, BreakStatement)

def parse_continue(token_stream):
  return parse_labeled(token_stream, ContinueStatement)

def parse_class(token_stream):
  token_stream.shift()
  return ClassStatement

def parse_if(token_stream):
  token_stream.shift()

def parse_let(token_stream):
  token_stream.shift()

def parse_block(token_stream):
  token_stream.shift()

def parse_return(token_stream):
  token_stream.shift()

def parse_typedef(token_stream):
  token_stream.shift()

def parse_do_while(token_stream):
  token_stream.shift()

def parse_while(token_stream):
  token_stream.shift()

def parse_for(token_stream):
  token_stream.shift()

def parse_ambiguous(token_stream):
  structure = disambiguate_statement(token_stream)

  if structure == G_FUNCTION:
    return FunctionStatement(parse_function(token_stream, allow_anon=False))

  if structure == G_DEFINE:
    result = expect_define_statement(parse_expression(token_stream))
  elif structure == G_EXPRESSION:
    result = ExpressionStatement(parse_expression(token_stream))
  else:
    raise token_stream.error("expected statement")

  token_stream.expect(tokens.L_SEMICOLON)
  return result

token_map = {
  tokens.L_BREAK: parse_break,
  tokens.L_CLASS: parse_class,
  tokens.L_CONTINUE: parse_continue,
  tokens.L_IF: parse_if,
  tokens.L_LET: parse_let,
  tokens.L_OPEN_BRACE: parse_block,
  tokens.L_RETURN: parse_return,
  tokens.L_TYPEDEF: parse_typedef,
  tokens.L_DO: parse_do_while,
  tokens.L_WHILE: parse_while,
  tokens.L_FOR: parse_for,
  tokens.L_IDENTIFIER: parse_ambiguous
}
