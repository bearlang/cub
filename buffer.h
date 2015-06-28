#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
  size_t used, total;
  char *data;
} buffer;

buffer *new_buffer();
void buffer_free(buffer*);
void buffer_init(buffer*);
void buffer_realloc(buffer*, size_t min_add);
char *buffer_pop(buffer*);
void buffer_append_char(buffer*, char byte);
void buffer_append_mem(buffer*, char *data, size_t bytes);
void buffer_append_str(buffer*, char *data);
char *buffer_flush(buffer*);

#endif
