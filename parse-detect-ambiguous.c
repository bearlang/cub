#include <string.h>

#include "xalloc.h"
#include "lex.h"
#include "parse.h"

typedef struct lookahead_state lookahead_state;

struct lookahead_state {
  parse_state *upstream;
  size_t upstream_index;
  bool unused;
  size_t count, cap;
  token **tokens;
};

// evaluates to zero if `allowed` is not in the restriction value
// evaluates to `allow` if `allowed` is in the restriction value
#define allow_if(allow, allowed) \
  ((allow) & -!!((restriction) & (allowed)))

static void init_lookahead(parse_state *state, lookahead_state *look) {
  look->upstream = state;
  look->upstream_index = state->count;
  look->unused = false;
  look->count = 0;
  look->cap = 0;
  look->tokens = NULL;
}

static void merge_lookahead(lookahead_state *state) {
  size_t count = state->count;

  if (count) {
    parse_state *up = state->upstream;
    size_t upstream_count = up->count;
    size_t merged_count = count + upstream_count;

    resize(merged_count, &up->cap, (void**) &up->buffer,
      sizeof(token*));

    up->count = merged_count;

    token **to = up->buffer, **from = state->tokens;
    memmove(to + count, to, upstream_count * sizeof(token*));

    for (size_t i = 0; i < count; ) {
      size_t dest = i;
      size_t src = count - ++i;
      to[dest] = from[src];
    }

    free(from);
  }
}

static bool lookahead(lookahead_state *state, token_type expected) {
  if (state->upstream_index) {
    if (state->upstream->buffer[state->upstream_index - 1]->type == expected) {
      state->upstream_index--;
      return true;
    }
    return false;
  }

  if (state->unused) {
    if (state->tokens[state->count - 1]->type == expected) {
      state->unused = false;
      return true;
    }

    return false;
  }

  resize(state->count, &state->cap, (void**) &state->tokens, sizeof(token*));

  token *next = scan(state->upstream->in);
  state->tokens[state->count++] = next;
  if (next->type == expected) {
    return true;
  }

  state->unused = true;
  return false;
}

// we're inside something that could be anything except a define
static syntax_structure detect_ambiguous_expression_ambiguous(
    lookahead_state *state, syntax_structure restriction) {
  if (!lookahead(state, L_TYPE) && !lookahead(state, L_IDENTIFIER)) {
    return restriction & G_EXPRESSION;
  } // else could still be argument, expression, function, parameter

  bool wasEmpty = false;
  while (lookahead(state, L_OPEN_PAREN)) {
    if (lookahead(state, L_CLOSE_PAREN)) {
      wasEmpty = true;
      continue;
    } // else could still be argument, expression, function, parameter

    do {
      syntax_structure subrestriction = allow_if(G_PARAMETER, G_ARGUMENT) |
        allow_if(G_EXPRESSION | G_FUNCTION, G_EXPRESSION) |
        allow_if(G_ARGUMENT | G_PARAMETER, G_FUNCTION) |
        allow_if(G_PARAMETER, G_PARAMETER);

      syntax_structure detect = detect_ambiguous_expression_ambiguous(state,
        subrestriction);

      switch (detect) {
      case 0:
        return 0;
      case G_ARGUMENT:
        return restriction & G_FUNCTION;
      case G_EXPRESSION:
      case G_FUNCTION:
        return restriction & G_EXPRESSION;
      case G_PARAMETER:
        restriction &= G_ARGUMENT | G_FUNCTION | G_PARAMETER;
        break;
      default:
        // currently G_PARAMETER | G_EXPRESSION, so keep looking
        break;
      }
    } while (lookahead(state, L_COMMA));

    if (!lookahead(state, L_CLOSE_PAREN)) {
      return restriction & G_EXPRESSION;
    }

    wasEmpty = false;
  }

  bool hasArray = false;
  if (lookahead(state, L_OPEN_BRACKET)) {
    if (lookahead(state, L_CLOSE_BRACKET)) {
      // not a normal expression
      // could still be a function expression, argument, or parameter
      hasArray = true;

      // factors expression out of the trailing return statement
      restriction = restriction & (syntax_structure) ~G_EXPRESSION;
    } else {
      // must be an expression
      return restriction & G_EXPRESSION;
    }
  }

  if (lookahead(state, L_IDENTIFIER)) {
    // can't be a parameter, define, or expression
    if (lookahead(state, L_OPEN_PAREN)) {
      return restriction & G_FUNCTION;
    }

    return restriction & G_ARGUMENT;
  }

  if (lookahead(state, L_OPEN_BRACE)) {
    if (wasEmpty && !hasArray) {
      return restriction & G_FUNCTION;
    }

    return 0;
  }

  // TODO: can't be an expression if it was a known-typed parameter
  return restriction & (G_EXPRESSION | G_PARAMETER);
}

static syntax_structure _detect_ambiguous_statement(lookahead_state *state) {
  if (!lookahead(state, L_TYPE) && !lookahead(state, L_IDENTIFIER)) {
    // just represents that it can't be a define or a function declaration
    return G_EXPRESSION;
  }

  syntax_structure restriction = G_DEFINE | G_EXPRESSION | G_FUNCTION;

  while (lookahead(state, L_OPEN_PAREN)) {
    if (lookahead(state, L_CLOSE_PAREN)) {
      continue;
    }

    do {
      syntax_structure subrestriction = allow_if(G_PARAMETER, G_DEFINE) |
        allow_if(G_EXPRESSION | G_FUNCTION, G_EXPRESSION) |
        allow_if(G_ARGUMENT | G_PARAMETER, G_FUNCTION);

      syntax_structure new_restriction =
        detect_ambiguous_expression_ambiguous(state, subrestriction);

      switch (new_restriction) {
      case 0:
        return 0;
      case G_ARGUMENT:
        return restriction & G_FUNCTION;
      case G_EXPRESSION:
      case G_FUNCTION:
        return restriction & G_EXPRESSION;
      case G_PARAMETER:
        restriction &= G_DEFINE | G_FUNCTION;
      default:
        // currently G_PARAMETER | G_EXPRESSION, so keep looking
        break;
      }
    } while (lookahead(state, L_COMMA));

    if (!lookahead(state, L_CLOSE_PAREN)) {
      return restriction & G_EXPRESSION;
    }
  }

  if (lookahead(state, L_OPEN_BRACKET)) {
    if (lookahead(state, L_CLOSE_BRACKET)) {
      // can't be a G_EXPRESSION
      restriction = restriction & (syntax_structure) ~G_EXPRESSION;
    } else {
      return restriction & G_EXPRESSION;
    }
  }

  if (lookahead(state, L_IDENTIFIER)) {
    if (lookahead(state, L_ASSIGN) || lookahead(state, L_COMMA) ||
        lookahead(state, L_SEMICOLON)) {
      return G_DEFINE;
    }

    if (lookahead(state, L_OPEN_PAREN)) {
      return G_FUNCTION;
    }
  }

  return restriction & G_EXPRESSION;
}

static syntax_structure _detect_ambiguous_expression(lookahead_state *state) {
  if (lookahead(state, L_TYPE)) {
    return G_FUNCTION;
  }

  if (!lookahead(state, L_IDENTIFIER)) {
    return G_EXPRESSION;
  }

  bool wasEmpty = false;
  while (lookahead(state, L_OPEN_PAREN)) {
    if (lookahead(state, L_CLOSE_PAREN)) {
      wasEmpty = true;
      continue;
    }

    do {
      syntax_structure new_restriction = detect_ambiguous_expression_ambiguous(
        state, G_ARGUMENT | G_EXPRESSION | G_FUNCTION | G_PARAMETER);

      switch (new_restriction) {
      case 0:
        return 0;
      case G_ARGUMENT:
      case G_PARAMETER:
        return G_FUNCTION;
      case G_EXPRESSION:
      case G_FUNCTION:
        return G_EXPRESSION;
      default:
        // currently G_PARAMETER | G_EXPRESSION, so keep looking
        break;
      }
    } while (lookahead(state, L_COMMA));

    if (!lookahead(state, L_CLOSE_PAREN)) {
      return G_EXPRESSION;
    }

    wasEmpty = false;
  }

  if (lookahead(state, L_OPEN_BRACKET)) {
    if (lookahead(state, L_CLOSE_BRACKET)) {
      return G_FUNCTION;
    }

    return G_EXPRESSION;
  }

  if (lookahead(state, L_IDENTIFIER)) {
    return G_FUNCTION;
  }

  if (lookahead(state, L_OPEN_BRACE)) {
    return G_FUNCTION & -wasEmpty;
  }

  return G_EXPRESSION;
}

static syntax_structure _detect_ambiguous_new(lookahead_state *state) {
  if (lookahead(state, L_TYPE)) {
    return G_NEW_ARRAY;
  }

  if (!lookahead(state, L_IDENTIFIER)) {
    return 0;
  }

  if (lookahead(state, L_OPEN_PAREN)) {
    if (!lookahead(state, L_CLOSE_PAREN)) {
      do {
        syntax_structure restriction = detect_ambiguous_expression_ambiguous(
          state, G_EXPRESSION | G_FUNCTION | G_PARAMETER);

        switch (restriction) {
        case 0:
          return 0;
        case G_PARAMETER:
          return G_NEW_ARRAY;
        case G_EXPRESSION:
        case G_FUNCTION:
          return G_NEW_OBJECT;
        default:
          // currently G_PARAMETER | G_EXPRESSION, so keep looking
          break;
        }
      } while (lookahead(state, L_COMMA));

      if (!lookahead(state, L_CLOSE_PAREN)) {
        return 0;
      }
    }

    if (lookahead(state, L_OPEN_PAREN)) {
      return G_NEW_ARRAY;
    }
  }

  return lookahead(state, L_OPEN_BRACKET) ? G_NEW_ARRAY : G_NEW_OBJECT;
}

syntax_structure detect_ambiguous_statement(parse_state *state) {
  lookahead_state stacklook, *look = &stacklook;
  init_lookahead(state, look);

  syntax_structure result = _detect_ambiguous_statement(look);

  merge_lookahead(look);

  return result;
}

syntax_structure detect_ambiguous_expression(parse_state *state) {
  lookahead_state stacklook, *look = &stacklook;
  init_lookahead(state, look);

  syntax_structure result = _detect_ambiguous_expression(look);

  merge_lookahead(look);

  return result;
}

syntax_structure detect_ambiguous_new(parse_state *state) {
  lookahead_state stacklook, *look = &stacklook;
  init_lookahead(state, look);

  syntax_structure result = _detect_ambiguous_new(look);

  merge_lookahead(look);

  return result;
}
