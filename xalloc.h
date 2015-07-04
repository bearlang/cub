#ifndef XMALLOC_H
#define XMALLOC_H

#include <stdlib.h>

void *xmalloc(const size_t size);
void *xrealloc(void *ptr, const size_t size);
void resize(const size_t used, size_t *total, void **ptr, size_t size);

#endif
