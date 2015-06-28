#include "../xalloc.h"
#include "../expression.h"

expression *new_get_symbol_node(char *symbol) {
  expression *get_symbol = xmalloc(sizeof(*get_symbol));
  get_symbol->operation.type = O_GET_SYMBOL;
  get_symbol->symbol_name = symbol;
  // gotta have this for new_assign_node
  get_symbol->value = NULL;
  get_symbol->next = NULL;
  return get_symbol;
}
