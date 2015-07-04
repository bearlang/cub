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

// *used never modified
void resize(const size_t used, size_t *total, void **ptr, size_t size) {
  size_t t = *total;
  if (t == 0) {
    *total = t = 16;
  } else if (used >= t) {
    size_t n = t;
    while (used >= t) {
      const size_t mask = -!(n & (n - 1));
      n = (mask & (n | (n >> 1))) | (((n / 3) << 2) & ~mask);
    }
    *total = t = n;
  } else {
    return;
  }

  void *new_ptr = xrealloc(*ptr, size * t);
  *ptr = new_ptr;
}
