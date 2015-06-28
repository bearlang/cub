#include "../xalloc.h"
#include "../expression.h"

expression *new_function_node(function *fn) {
  expression *function = xmalloc(sizeof(*function));
  function->operation.type = O_FUNCTION;
  function->function = fn;
  function->next = NULL;
  return function;
}
