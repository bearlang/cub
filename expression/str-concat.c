#include "../xalloc.h"
#include "../expression.h"

expression *new_str_concat_node(expression *left, expression *right) {
  expression *str_concat = xmalloc(sizeof(*str_concat));
  str_concat->operation.type = O_STR_CONCAT;
  str_concat->type = xmalloc(sizeof(type));
  str_concat->type->type = T_STRING;
  str_concat->value = left;
  left->next = right;
  str_concat->next = NULL;
  return str_concat;
}

expression *new_str_concat_assign_node(expression *left, expression *right) {
  assert_valid_assign(left);

  expression *str_concat = xmalloc(sizeof(*str_concat));
  str_concat->operation.type = O_STR_CONCAT_ASSIGN;
  str_concat->type = xmalloc(sizeof(type));
  str_concat->type->type = T_STRING;
  str_concat->value = left;
  left->next = right;
  str_concat->next = NULL;
  return str_concat;
}
