#include "../xalloc.h"
#include "../expression.h"

expression *new_identity_node(expression *left, expression *right) {
  expression *identity = xmalloc(sizeof(*identity));
  identity->operation.type = O_IDENTITY;
  identity->type = xmalloc(sizeof(type));
  identity->type->type = T_BOOL;
  identity->value = left;
  left->next = right;
  identity->next = NULL;
  identity->line = left->line;
  identity->offset = left->offset;
  return identity;
}
