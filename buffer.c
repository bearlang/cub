#include <stdlib.h>
#include <string.h>

#include "xalloc.h"
#include "buffer.h"

void buffer_init(buffer *buffer) {
  buffer->used = 0;
  buffer->total = 0;
  buffer->data = NULL;
}

void buffer_free(buffer *buffer) {
  if (buffer->data) {
    free(buffer->data);
  }
}

buffer *new_buffer() {
  buffer *buffer = xmalloc(sizeof(buffer));
  buffer_init(buffer);
  return buffer;
}

// assumes min_add will not exceed reasonable buffer size
void buffer_realloc(buffer *buffer, size_t min_add) {
  size_t new_size = buffer->total || 16, min_size = min_add + buffer->used;

  if (buffer->data != NULL && new_size >= min_size) {
    return;
  }

  do {
    new_size <<= 1;
  } while (new_size < min_size);

  buffer->data = xrealloc(buffer->data, new_size);
}

char *buffer_pop(buffer *buffer) {
  if (buffer->used) {
    return buffer->data + --buffer->used;
  }
  return NULL;
}

void buffer_append_char(buffer *buffer, char byte) {
  buffer_realloc(buffer, 1);
  *(buffer->data + buffer->used) = byte;
  ++buffer->used;
}

void buffer_append_mem(buffer *buffer, char *data, size_t bytes) {
  buffer_realloc(buffer, bytes);
  memcpy(buffer->data + buffer->used, data, bytes);
  buffer->used += bytes;
}

void buffer_append_str(buffer *buffer, char *data) {
  buffer_append_mem(buffer, data, strlen(data));
}

char *buffer_flush(buffer *buffer) {
  char *data = realloc(buffer->data, buffer->used + 1);
  data[buffer->used] = 0;
  buffer->used = 0;
  buffer->total = 0;
  buffer->data = NULL;
  return data;
}
