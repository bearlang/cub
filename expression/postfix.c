#include "../xalloc.h"
#include "../expression.h"

expression *new_postfix_node(postfix_type type, expression *value) {
  assert_valid_assign(value);

  expression *postfix = xmalloc(sizeof(*postfix));
  postfix->operation.type = O_POSTFIX;
  postfix->operation.postfix_type = type;
  postfix->value = value;
  postfix->next = NULL;
  return postfix;
}
