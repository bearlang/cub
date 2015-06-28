#include "../xalloc.h"
#include "../statement.h"

statement *s_expression(expression *value) {
  expression_statement *expression = xmalloc(sizeof(*expression));
  expression->type = S_EXPRESSION;
  expression->value = value;
  expression->next = NULL;
  return (statement*) expression;
}
