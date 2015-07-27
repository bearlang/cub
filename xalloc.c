#include <stdio.h>
#include <string.h>

#include "xalloc.h"

void *xmalloc(const size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "out of memory\n");
    abort();
  }
  return ptr;
}

void *xrealloc(void *ptr, const size_t size) {
  ptr = realloc(ptr, size);
  if (ptr == NULL) {
    fprintf(stderr, "out of memory\n");
    abort();
  }
  return ptr;
}

char *xstrdup(const char *src) {
  if (src == NULL) {
    return NULL;
  }

  char *dest = xmalloc(strlen(src) + sizeof(char));
  if (dest == NULL) {
    fprintf(stderr, "out of memory\n");
    abort();
  }
  strcpy(dest, src);
  return dest;
}

void resize(const size_t used, size_t *total, void **ptr, size_t size) {
  size_t t = *total;
  if (t == 0) {
    *total = t = 16;
  } else if (used >= t) {
    size_t n = t;
    while (used >= n) {
      // not converts to true/false, so cast is needed to prevent warning
      const size_t mask = -(size_t) !(n & (n - 1));
      n = (mask & (n | (n >> 1))) | (((n / 3) << 2) & ~mask);
    }
    *total = t = n;
  } else {
    return;
  }

  void *new_ptr = xrealloc(*ptr, size * t);
  *ptr = new_ptr;
}
