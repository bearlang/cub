#include "../xalloc.h"
#include "../statement.h"

statement *s_block(statement *body) {
  block_statement *block = xmalloc(sizeof(*block));
  block->type = S_BLOCK;
  block->body = body;
  block->fn_parent = NULL;
  block->parent = NULL;
  block->class_head = NULL;
  block->class_tail = NULL;
  block->function_head = NULL;
  block->function_tail = NULL;
  block->variable_head = NULL;
  block->variable_tail = NULL;
  block->next = NULL;
  return (statement*) block;
}
