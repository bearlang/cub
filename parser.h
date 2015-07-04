#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>

#include "statement.h"
#include "stream.h"
#include "token.h"

typedef uint8_t syntax_structure;

const syntax_structure G_ARGUMENT = 1;
const syntax_structure G_DEFINE = 2;
const syntax_structure G_EXPRESSION = 4;
const syntax_structure G_FUNCTION = 8;
const syntax_structure G_PARAMETER = 16;
const syntax_structure G_ANY = 31;

typedef struct {
  stream *in;
  size_t count, cap;
  token **buffer;
} parse_state;

void _expected(token *t) __attribute__ ((noreturn));
#define expected(token, ...) do {\
  fprintf(stderr, "expected " __VA_ARGS__);\
  _expected(token);\
} while(0);

// void expected(token *t, char *expected) __attribute__ ((noreturn));
void unexpected_token(token *t, char *expecting) __attribute__ ((noreturn));

token *parse_peek(parse_state*);
bool parse_peek_compare(parse_state*, token_type);
token *parse_shift(parse_state*);
void shift_consume(parse_state*);
void parse_push(parse_state*, token*);
token *accept(parse_state*, token_type);
bool consume(parse_state*, token_type);
token *expect(parse_state*, token_type);
void expect_peek(parse_state*, token_type);
void expect_consume(parse_state*, token_type);
void check_expression(parse_state*, expression*);
expression *parse_expression(parse_state *state); // TODO: not here
void free_expression(expression*, bool free_strings);
block_statement *parse(stream*);

syntax_structure detect_ambiguous_statement(parse_state*);
syntax_structure detect_ambiguous_expression(parse_state*);

#endif
