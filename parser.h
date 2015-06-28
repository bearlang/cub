#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>

#include "statement.h"
#include "stream.h"
#include "token.h"

// TODO: this _will_ be insufficient when we implement multiple return values
#define PARSE_LOOKAHEAD_BUFFER 16

typedef struct {
  stream *in;
  size_t count;
  token **buffer;
} parse_state;

block_statement *parse(stream *in);

#endif
