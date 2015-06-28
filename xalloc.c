#include <stdio.h>

#include "xalloc.h"

void *xmalloc(const size_t size) {
  // printf("malloc(%zu)\n", size);
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "out of memory\n");
    abort();
  }
  return ptr;
}

void *xrealloc(void *ptr, const size_t size) {
  // printf("realloc(%zu)\n", size);
  ptr = realloc(ptr, size);
  if (ptr == NULL) {
    fprintf(stderr, "out of memory\n");
    abort();
  }
  return ptr;
}
