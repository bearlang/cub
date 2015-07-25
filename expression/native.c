#include "../xalloc.h"
#include "../expression.h"

expression *new_native_node(char *callee, char *assign, expression *args,
    size_t arg_count) {
  expression *native = xmalloc(sizeof(*native));
  native->operation.type = O_NATIVE;
  // native->type not initialized
  native->symbol_name = callee;
  native->assign = assign;
  native->value = args;
  native->arg_count = arg_count;
  native->next = NULL;
  return native;
}
