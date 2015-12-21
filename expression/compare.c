#include "../xalloc.h"
#include "../expression.h"

expression *new_compare_node(compare_type type, expression *left,
    expression *right) {
  expression *compare = xmalloc(sizeof(*compare));
  compare->operation.type = O_COMPARE;
  compare->operation.compare_type = type;
  compare->type = xmalloc(sizeof(type));
  compare->type->type = T_BOOL;
  compare->value = left;
  left->next = right;
  compare->next = NULL;
  compare->line = left->line;
  compare->offset = left->offset;
  return compare;
}
