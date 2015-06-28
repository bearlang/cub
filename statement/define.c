#include "../xalloc.h"
#include "../statement.h"

statement *s_define(type *type, define_clause *clause) {
  define_statement *define = xmalloc(sizeof(*define));
  define->type = S_DEFINE;
  define->symbol_type = type;
  define->clause = clause;
  define->next = NULL;
  return (statement*) define;
}
