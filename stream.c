#include "xalloc.h"
#include "stream.h"

stream *open_stream(FILE *src) {
  stream *in = xmalloc(sizeof(*in));
  in->source = src;
  in->line = 1;
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
    return *chr;
  }
  int value = fgetc(in->source);
  if (value == EOF) {
    return 0;
  }
  return (char) value;
}

void stream_push(stream *in, char chr) {
  buffer_append_char(&in->buffer, chr);
}
