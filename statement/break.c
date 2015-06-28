#include "../xalloc.h"
#include "../statement.h"

statement *s_break(char *label) {
  control_statement *brk = xmalloc(sizeof(*brk));
  brk->type = S_BREAK;
  brk->label = label;
  brk->next = NULL;
  return (statement*) brk;
}
