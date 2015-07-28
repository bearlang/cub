#include "../xalloc.h"
#include "../expression.h"

expression *new_function_node(function *fn) {
  expression *func = xmalloc(sizeof(*func));
  func->operation.type = O_FUNCTION;
  func->type = xmalloc(sizeof(type));
  func->type->type = T_BLOCKREF;
  func->type->blocktype = xmalloc(sizeof(argument));
  func->type->blocktype->argument_type = copy_type(fn->return_type);
  func->type->blocktype->symbol_name = NULL;
  func->type->blocktype->next = copy_arguments(fn->argument, false);
  func->function = fn;
  func->next = NULL;
  return func;
}
