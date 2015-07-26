#ifndef LEXER_H
#define LEXER_H

#include "stream.h"
#include "token.h"

// TODO: token stream :D
/*typedef struct {
  stream *in;
} token_stream;*/

token *scan(stream *in);

#endif
