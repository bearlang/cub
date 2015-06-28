#include <stdio.h>
#include <stdlib.h>

#include "../expression.h"

operation_type assert_valid_assign(expression *left) {
  switch (left->operation.type) {
  case O_GET_FIELD: return O_SET_FIELD;
  case O_GET_INDEX: return O_SET_INDEX;
  case O_GET_SYMBOL: return O_SET_SYMBOL;
  default:
    fprintf(stderr, "invalid left-hand side in assignment\n");
    exit(1);
  }
}

expression *new_assign_node(expression *left, expression *right) {
  left->operation.type = assert_valid_assign(left);

  expression **dest = &left->value;
  while (*dest) dest = &(*dest)->next;
  *dest = right;

  return left;
}
