#include "../xalloc.h"
#include "../expression.h"

expression *new_new_node(char *class_name, expression *args) {
  expression *new = xmalloc(sizeof(*new));
  new->operation.type = O_NEW;
  new->type = xmalloc(sizeof(type));
  new->type->type = T_OBJECT;
  new->type->classtype = NULL;
  new->value = args;
  new->symbol_name = class_name;
  new->next = NULL;
  return new;
}
