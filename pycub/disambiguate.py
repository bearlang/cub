import pycub.tokens as tokens

# an argument part, identifiable by two identifiers in a row
G_ARGUMENT = 1

# a define statement, identifiable by a type followed by an identifier and
# either a comma or an assign token
G_DEFINE = 2

# an expression, identifiable as not being anything else
G_EXPRESSION = 4

# a function definition, identifiable by a type followed by an identifier and
# then an open parenthesis
G_FUNCTION = 8

# a type parameter, basically just a type
G_PARAMETER = 16

G_ANY = G_ARGUMENT | G_DEFINE | G_EXPRESSION | G_FUNCTION | G_PARAMETER

G_NEW_ARRAY = 1
G_NEW_OBJECT = 2

def get_accept(lookahead):
  def accept(token_type):
    token = lookahead.peek()
    if token is None or token.token_type != token_type:
      return False
    lookahead.next()
    return True
  return accept


def disambiguate_expression_inner(lookahead, restrict):
  accept = get_accept(lookahead)

  if not accept(tokens.L_TYPE) and not accept(tokens.L_IDENTIFIER):
    return restrict & G_EXPRESSION

  # could still be an argument, expression, function, or parameter
  was_empty = False
  while accept(tokens.L_OPEN_PAREN):
    if accept(tokens.L_CLOSE_PAREN):
      was_empty = True
      continue

    # could still be an argument, expression, function, or parameter
    while True:
      subrestrict = restrict & G_PARAMETER
      if restrict & G_ARGUMENT: subrestrict |= G_PARAMETER
      if restrict & G_EXPRESSION: subrestrict |= G_EXPRESSION | G_FUNCTION
      if restrict & G_FUNCTION: subrestrict |= G_ARGUMENT | G_PARAMETER

      detect = disambiguate_expression_inner(lookahead, restrict)

      if detect == 0:
        return 0

      if detect == G_ARGUMENT:
        return restrict & G_FUNCTION

      if detect == G_EXPRESSION or detect == G_FUNCTION:
        return restrict & G_EXPRESSION

      if detect == G_PARAMETER:
        restrict &= G_ARGUMENT | G_FUNCTION | G_PARAMETER
        break

      # currently G_PARAMETER | G_EXPRESSION, so keep looking
      if not accept(tokens.L_COMMA):
        break

    if not accept(tokens.L_CLOSE_PAREN):
      return restrict & G_EXPRESSION

    was_empty = False

  has_array = False
  if accept(tokens.L_OPEN_BRACKET):
    if accept(tokens.L_CLOSE_BRACKET):
      # not a normal expression
      # could still be a function expression, argument, or parameter
      has_array = True

      # factor expression out of the trailing return statement
      restrict &= ~G_EXPRESSION
    else:
      # must be an expression
      return restrict & G_EXPRESSION

  if accept(tokens.L_IDENTIFIER):
    # can't be a parameter, define, or expression
    if accept(tokens.L_OPEN_PAREN):
      return restrict & G_FUNCTION

    return restrict & G_ARGUMENT

  if accept(tokens.L_OPEN_BRACE):
    if was_empty and not has_array:
      return restrict & G_FUNCTION

    return 0

  # TODO: can't be an expression if it was a known-typed parameter
  return restrict & (G_EXPRESSION | G_PARAMETER)

def disambiguate_statement(parser):
  lookahead = parser.lookahead()
  accept = get_accept(lookahead)

  if not accept(tokens.L_TYPE) and not accept(tokens.L_IDENTIFIER):
    # just represents that it can't be a define or a function declaration
    return G_EXPRESSION

  restrict = G_DEFINE | G_EXPRESSION | G_FUNCTION

  while accept(tokens.L_OPEN_PAREN):
    if accept(tokens.L_CLOSE_PAREN):
      continue

    while True:
      subrestrict = 0
      if restrict & G_DEFINE: subrestrict |= G_PARAMETER
      if restrict & G_EXPRESSION: subrestrict |= G_EXPRESSION | G_FUNCTION
      if restrict & G_FUNCTION: subrestrict |= G_ARGUMENT | G_PARAMETER

      detect = disambiguate_expression_inner(lookahead, subrestrict)

      if detect == 0:
        return 0

      # this must be a top-level anonymous function, which isn't supported
      if detect == G_ARGUMENT:
        return G_FUNCTION

      if detect == G_EXPRESSION or detect == G_FUNCTION:
        return G_EXPRESSION

      if detect == G_PARAMETER:
        restrict &= G_DEFINE | G_FUNCTION

      # currently G_PARAMETER | G_EXPRESSION, so keep looking
      if not accept(tokens.L_COMMA):
        break

    if not accept(tokens.L_CLOSE_PAREN):
      return G_EXPRESSION

  if accept(tokens.L_OPEN_BRACKET):
    if not accept(tokens.L_CLOSE_BRACKET):
      return G_EXPRESSION

    # can't be a G_EXPRESSION
    restrict &= ~G_EXPRESSION

  if accept(tokens.L_IDENTIFIER):
    if accept(tokens.L_ASSIGN) or accept(tokens.L_COMMA) or accept(tokens.L_SEMICOLON):
      return G_DEFINE

    if accept(tokens.L_OPEN_PAREN):
      return G_FUNCTION

  return G_EXPRESSION

def disambiguate_expression(parser):
  lookahead = parser.lookahead()
  accept = get_accept(lookahead)

  if accept(tokens.L_TYPE):
    return G_FUNCTION

  if not accept(tokens.L_IDENTIFIER):
    return G_EXPRESSION

  was_empty = False
  while accept(tokens.L_OPEN_PAREN):
    if accept(tokens.L_CLOSE_PAREN):
      was_empty = True
      continue

    while True:
      subrestrict = G_ARGUMENT | G_EXPRESSION | G_FUNCTION | G_PARAMETER

      detect = disambiguate_expression_inner(lookahead, subrestrict)

      if detect == 0:
        return 0

      if detect == G_ARGUMENT or detect == G_PARAMETER:
        return G_FUNCTION

      if detect == G_EXPRESSION or detect == G_FUNCTION:
        return G_EXPRESSION

      # currently G_PARAMETER | G_EXPRESSION, so keep looking
      if not accept(tokens.L_COMMA):
        break

    if not accept(tokens.L_CLOSE_PAREN):
      return G_EXPRESSION

    was_empty = False

  if accept(tokens.L_OPEN_BRACKET):
    if accept(tokens.L_CLOSE_BRACKET):
      return G_FUNCTION
    return G_EXPRESSION

  if accept(tokens.L_IDENTIFIER):
    return G_FUNCTION

  if accept(tokens.L_OPEN_BRACE):
    return G_FUNCTION if was_empty else 0

  return G_EXPRESSION

def disambiguate_new(parser):
  lookahead = parser.lookahead()
  accept = get_accept(lookahead)

  if accept(tokens.L_TYPE):
    return G_NEW_ARRAY

  if not accept(tokens.L_IDENTIFIER):
    # it's not valid, but will be appropriately dealt with in the actual parser
    return G_NEW_OBJECT

  if accept(tokens.L_OPEN_PAREN):
    if not accept(tokens.L_CLOSE_PAREN):
      while True:
        subrestrict = G_EXPRESSION | G_FUNCTION | G_PARAMETER
        restrict = disambiguate_expression_inner(lookahead, subrestrict)

        if restrict == 0:
          # it's clearly wrong, might as well try
          return G_NEW_OBJECT

        if restrict == G_PARAMETER:
          return G_NEW_ARRAY

        if restrict == G_EXPRESSION or restrict == G_FUNCTION:
          return G_NEW_OBJECT

        # currently G_PARAMETER | G_EXPRESSION, so keep looking
        if not accept(tokens.L_COMMA):
          break

      if not accept(tokens.L_CLOSE_PAREN):
        return G_NEW_OBJECT

    if accept(tokens.L_OPEN_PAREN):
      return G_NEW_ARRAY

  return G_NEW_ARRAY if accept(tokens.L_OPEN_BRACKET) else G_NEW_OBJECT
