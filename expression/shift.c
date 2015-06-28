#include "../xalloc.h"
#include "../expression.h"

expression *new_shift_node(shift_type type, expression *left,
    expression *right) {
  expression *shift = xmalloc(sizeof(*shift));
  shift->operation.type = O_SHIFT;
  shift->operation.shift_type = type;
  shift->value = left;
  left->next = right;
  shift->next = NULL;
  return shift;
}

expression *new_shift_assign_node(shift_type type, expression *left,
    expression *right) {
  assert_valid_assign(left);

  expression *shift = xmalloc(sizeof(*shift));
  shift->operation.type = O_SHIFT_ASSIGN;
  shift->operation.shift_type = type;
  shift->value = left;
  left->next = right;
  shift->next = NULL;
  return shift;
}
