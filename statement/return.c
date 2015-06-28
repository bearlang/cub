#include "../xalloc.h"
#include "../statement.h"

statement *s_return(expression *value) {
  return_statement *ret = xmalloc(sizeof(*ret));
  ret->type = S_RETURN;
  ret->value = value;
  ret->next = NULL;
  return (statement*) ret;
}
