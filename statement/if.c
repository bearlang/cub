#include "../xalloc.h"
#include "../statement.h"

statement *s_if(expression *condition, block_statement *first,
    block_statement *second) {
  if_statement *if_s = xmalloc(sizeof(*if_s));
  if_s->type = S_IF;
  if_s->condition = condition;
  if_s->first = first;
  if_s->second = second;
  if_s->next = NULL;
  return (statement*) if_s;
}
