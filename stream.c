#include "xalloc.h"
#include "stream.h"

stream *open_stream(FILE *src) {
  stream *in = xmalloc(sizeof(*in));
  in->source = src;
  in->was_newline = false;
  in->line = 1;
  in->offset = 1;
  buffer_init(&in->buffer);
  return in;
}

void close_stream(stream *in) {
  buffer_free(&in->buffer);
  free(in);
}

char stream_shift(stream *in) {
  char *chr = buffer_pop(&in->buffer);
  if (chr != NULL) {
    in->offset++;
    return *chr;
  }
  if (in->was_newline) {
    in->was_newline = false;
    in->line++;
    in->offset = 1;
  }
  int value = fgetc(in->source);
  if (value == EOF) {
    return 0;
  }
  if (value == '\n') {
    in->was_newline = true;
  }
  in->offset++;
  return (char) value;
}

void stream_push(stream *in, char chr) {
  in->offset--;
  buffer_append_char(&in->buffer, chr);
}
