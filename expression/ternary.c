#include "../xalloc.h"
#include "../expression.h"

expression *new_ternary_node(expression *condition, expression *left,
    expression *right) {
  expression *ternary = xmalloc(sizeof(*ternary));
  ternary->operation.type = O_TERNARY;
  ternary->value = condition;
  condition->next = left;
  left->next = right;
  ternary->next = NULL;
  ternary->line = condition->line;
  ternary->offset = condition->offset;
  return ternary;
}
