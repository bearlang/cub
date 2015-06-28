#include "../xalloc.h"
#include "../expression.h"

expression *new_get_field_node(expression *left, char *field) {
  // TODO: field_index vs symbol_name
  expression *get_field = xmalloc(sizeof(*get_field));
  get_field->operation.type = O_GET_FIELD;
  get_field->value = left;
  get_field->symbol_name = field;
  get_field->next = NULL;
  return get_field;
}
