#include "../xalloc.h"
#include "../expression.h"

expression *new_cast_node(type *cast_type, expression *value) {
  expression *cast = xmalloc(sizeof(*cast));
  cast->operation.type = O_CAST;
  // TODO: cast->operation.cast_type needs to be initialized in analysis phase
  cast->type = cast_type;
  cast->value = value;
  cast->next = NULL;
  return cast;
}
