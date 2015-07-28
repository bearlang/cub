#include "xalloc.h"
#include "token.h"
#include "lex.h"
#include "parse.h"

void _expected(token *t) {
  if (t) {
    fprintf(stderr, " at %zu:%zu\n", t->line, t->offset);
  } else {
    fprintf(stderr, "\n");
  }
  exit(1);
}

void unexpected_token(token *t, char *expecting) {
  fprintf(stderr, "unexpected ");
  if (t) {
    fprintf(stderr, "token '%s'", token_string(t->type));
  } else {
    fprintf(stderr, "EOF");
  }
  if (expecting) {
    fprintf(stderr, ", %s", expecting);
  }
  if (t) {
    fprintf(stderr, " at %zu:%zu\n", t->line, t->offset);
  } else {
    fprintf(stderr, "\n");
  }
  exit(1);
}

token *parse_peek(parse_state *state) {
  if (state->count) {
    return state->buffer[state->count - 1];
  }
  token *t = scan(state->in);
  // no more tokens
  if (t == NULL) {
    return NULL;
  }
  resize(state->count, &state->cap, (void**) &state->buffer, sizeof(token*));
  state->buffer[0] = t;
  state->count++;
  return t;
}

bool parse_peek_compare(parse_state *state, token_type type) {
  token *t = parse_peek(state);
  return t && t->type == type;
}

token *parse_shift(parse_state *state) {
  if (state->count) {
    return state->buffer[--state->count];
  }
  return scan(state->in);
}

void shift_consume(parse_state *state) {
  free(parse_shift(state));
}

void parse_push(parse_state *state, token *push_token) {
  resize(state->count, &state->cap, (void**) &state->buffer, sizeof(token*));
  state->buffer[state->count] = push_token;
  state->count++;
}

token *accept(parse_state *state, token_type type) {
  token *t = parse_peek(state);
  if (t != NULL && t->type == type) {
    parse_shift(state);
    return t;
  }
  return NULL;
}

bool consume(parse_state *state, token_type type) {
  token *t = accept(state, type);
  if (t == NULL) {
    return false;
  }
  free(t);
  return true;
}

token *expect(parse_state *state, token_type type) {
  token *t = parse_shift(state);
  if (t == NULL) {
    expected(t, "token '%s', found EOF", token_string(type));
  }
  if (t->type != type) {
    expected(t, "token '%s', found token '%s'", token_string(type),
      token_string(t->type));
  }
  return t;
}

void expect_peek(parse_state *state, token_type type) {
  token *t = parse_peek(state);
  if (!t || t->type != type) {
    expected(t, "token '%s'", token_string(type));
  }
  /*if (t == NULL) {
    fprintf(stderr, "expected token %s on line %zi, found EOF\n",
      token_string(type), state->in->line);
  }
  if (t->type != type) {
    fprintf(stderr, "expected token %s on line %zi, found token %s\n",
      token_string(type), state->in->line, token_string(t->type));
  }*/
}

void expect_consume(parse_state *state, token_type type) {
  free(expect(state, type));
}

void check_expression(parse_state *state, expression *e) {
  if (e == NULL) {
    expected(parse_peek(state), "expression");
  }
}
