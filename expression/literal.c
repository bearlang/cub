#include "../xalloc.h"
#include "../expression.h"

expression *new_literal_node(type_type type) {
  expression *literal = xmalloc(sizeof(*literal));
  literal->operation.type = O_LITERAL;
  literal->type = new_type(type);
  literal->next = NULL;
  return literal;
}
