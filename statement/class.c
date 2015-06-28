#include "../xalloc.h"
#include "../statement.h"

statement *s_class(class *classtype) {
  class_statement *class = xmalloc(sizeof(*class));
  class->type = S_CLASS;
  class->symbol_name = classtype->class_name;
  class->classtype = classtype;
  class->next = NULL;
  return (statement*) class;
}
