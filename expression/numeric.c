#include "../xalloc.h"
#include "../expression.h"

expression *new_numeric_node(numeric_type type, expression *left,
    expression *right) {
  expression *numeric = xmalloc(sizeof(*numeric));
  numeric->operation.type = O_NUMERIC;
  numeric->operation.numeric_type = type;
  numeric->value = left;
  left->next = right;
  numeric->next = NULL;
  return numeric;
}

expression *new_numeric_assign_node(numeric_type type, expression *left,
    expression *right) {
  assert_valid_assign(left);

  expression *numeric = xmalloc(sizeof(*numeric));
  numeric->operation.type = O_NUMERIC_ASSIGN;
  numeric->operation.numeric_type = type;
  numeric->value = left;
  left->next = right;
  numeric->next = NULL;
  return numeric;
}
