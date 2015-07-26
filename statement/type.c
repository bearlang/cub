#include "../xalloc.h"
#include "../statement.h"

statement *s_typedef(type *left, char *alias) {
  typedef_statement *typedef_s = xmalloc(sizeof(*typedef_s));
  typedef_s->type = S_TYPEDEF;
  typedef_s->typedef_type = left;
  typedef_s->alias = alias;
  typedef_s->next = NULL;
  return (statement*) typedef_s;
}
