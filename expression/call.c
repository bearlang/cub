#include "../xalloc.h"
#include "../expression.h"

expression *new_call_node(expression *callee, expression *args) {
  expression *call = xmalloc(sizeof(*call));
  call->operation.type = O_CALL;
  // call->type not initialized
  call->value = callee;
  callee->next = args;
  call->next = NULL;
  return call;
}
