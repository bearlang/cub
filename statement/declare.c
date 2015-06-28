#include "../xalloc.h"
#include "../statement.h"

statement *s_declare(type *type, char *name) {
  declare_statement *declare = xmalloc(sizeof(*declare));
  declare->type = S_DECLARE;
  declare->symbol_type = type;
  declare->symbol_name = name;
  declare->next = NULL;
  return (statement*) declare;
}
