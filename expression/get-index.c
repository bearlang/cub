#include "../xalloc.h"
#include "../expression.h"

expression *new_get_index_node(expression *left, expression *index) {
  // TODO: field_index vs symbol_name
  expression *get_index = xmalloc(sizeof(*get_index));
  get_index->operation.type = O_GET_INDEX;
  get_index->value = left;
  get_index->value->next = index;
  get_index->next = NULL;
  return get_index;
}
