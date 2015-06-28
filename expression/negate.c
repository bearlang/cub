#include "../xalloc.h"
#include "../expression.h"

expression *new_negate_node(expression *value) {
  expression *negate = xmalloc(sizeof(*negate));
  negate->operation.type = O_NEGATE;
  negate->value = value;
  negate->next = NULL;
  return negate;
}
