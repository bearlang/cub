#include "../xalloc.h"
#include "../expression.h"

expression *new_logic_node(logic_type type, expression *left,
    expression *right) {
  expression *logic = xmalloc(sizeof(*logic));
  logic->operation.type = O_LOGIC;
  logic->type = xmalloc(sizeof(type));
  logic->type->type = T_BOOL;
  logic->value = left;
  left->next = right;
  logic->next = NULL;
  return logic;
}
