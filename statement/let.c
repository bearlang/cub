#include "../xalloc.h"
#include "../statement.h"

statement *s_let(define_clause *clause) {
  let_statement *let = xmalloc(sizeof(*let));
  let->type = S_LET;
  let->clause = clause;
  let->next = NULL;
  return (statement*) let;
}
