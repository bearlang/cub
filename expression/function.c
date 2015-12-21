#include "../xalloc.h"
#include "../expression.h"

expression *new_function_node(function *fn) {
  expression *function = xmalloc(sizeof(*function));
  function->operation.type = O_FUNCTION;
  function->type = xmalloc(sizeof(type));
  function->type->type = T_BLOCKREF;
  function->type->blocktype = xmalloc(sizeof(argument));
  function->type->blocktype->argument_type = copy_type(fn->return_type);
  function->type->blocktype->symbol_name = NULL;
  function->type->blocktype->next = copy_arguments(fn->argument, false);
  function->function = fn;
  function->next = NULL;
  function->line = fn->line;
  function->offset = fn->offset;
  return function;
}
