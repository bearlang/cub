#include "../xalloc.h"
#include "../statement.h"

statement *s_continue(char *label) {
  control_statement *cont = xmalloc(sizeof(*cont));
  cont->type = S_CONTINUE;
  cont->label = label;
  cont->next = NULL;
  return (statement*) cont;
}
