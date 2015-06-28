#include "../xalloc.h"
#include "../statement.h"

statement *s_loop(statement_type type, expression *condition,
    block_statement *body) {
  loop_statement *loop_loop = xmalloc(sizeof(*loop_loop));
  loop_loop->type = type;
  loop_loop->label = NULL;
  loop_loop->condition = condition;
  loop_loop->body = body;
  loop_loop->next = NULL;
  return (statement*) loop_loop;
}
