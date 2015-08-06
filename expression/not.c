#include "../xalloc.h"
#include "../expression.h"

expression *new_not_node(bool bitwise, expression *value) {
  expression *not = xmalloc(sizeof(*not));
  not->operation.type = bitwise ? O_BITWISE_NOT : O_NOT;
  if (bitwise) {
    not->type = new_type(T_BOOL);
  }
  not->value = value;
  not->next = NULL;
  return not;
}
