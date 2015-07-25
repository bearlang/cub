#include "../xalloc.h"
#include "../expression.h"

expression *new_new_array_node(type *array_type, expression *size) {
  expression *new = xmalloc(sizeof(*new));
  new->operation.type = O_NEW_ARRAY;
  new->type = new_array_type(array_type);
  new->value = size;
  new->next = NULL;
  return new;
}
