#include "../xalloc.h"
#include "../statement.h"

statement *s_function(function *fn) {
  function_statement *function = xmalloc(sizeof(*function));
  function->type = S_FUNCTION;
  function->symbol_name = fn->function_name;
  function->function = fn;
  function->next = NULL;
  return (statement*) function;
}
