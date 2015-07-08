#include "xalloc.h"
#include "type.h"

bool is_void(type *t) {
  return !t || t->type == T_VOID;
}

bool compatible_type(type *left, type *right) {
  if (!left) {
    return (!right) || (right->type == T_VOID);
  }

  switch (left->type) {
  case T_ARRAY:
    return right->type == T_ARRAY && compatible_type(left->arraytype,
      right->arraytype);
  case T_OBJECT:
    return right->type == T_OBJECT && left->classtype == right->classtype;
  case T_STRING:
    return right->type == T_STRING;
  case T_VOID:
    return (!right) || (right->type == T_VOID);
  default:
    // TODO: warn for numeric compatibility
    return true;
  }
}

argument *copy_arguments(argument *arg, bool with_names) {
  if (arg == NULL) {
    return NULL;
  }

  argument *new = xmalloc(sizeof(*new));
  new->symbol_name = with_names ? arg->symbol_name : NULL;
  new->argument_type = copy_type(arg->argument_type);

  argument *tail = new;
  for (argument *a = arg->next; a; a = a->next) {
    argument *rep = xmalloc(sizeof(*rep));
    rep->symbol_name = with_names ? a->symbol_name : NULL;
    rep->argument_type = copy_type(a->argument_type);
    tail->next = rep;
    tail = rep;
  }
  tail->next = NULL;

  return new;
}

void free_arguments(argument *arg) {
  while (arg) {
    argument *next = arg->next;
    free_type(arg->argument_type);
    free(arg);
    arg = next;
  }
}

type *new_type(type_type t) {
  type *new = xmalloc(sizeof(*new));
  new->type = t;
  return new;
}

type *new_type_from_token(token *src) {
  if (src->type == L_IDENTIFIER) {
    type *t = new_type(T_REF);
    t->symbol_name = src->symbol_name;
    return t;
  }
  if (src->type == L_TYPE) {
    return new_type(src->literal_type);
  }
  return NULL;
}

type *new_array_type(type *inner) {
  type *new = new_type(T_ARRAY);
  new->arraytype = inner;
  return new;
}

type *new_function_type(type *ret, argument *arg) {
  type *new = new_type(T_BLOCKREF);
  new->blocktype = xmalloc(sizeof(argument));
  new->blocktype->argument_type = ret;
  new->blocktype->symbol_name = NULL;
  new->blocktype->next = copy_arguments(arg, false);
  return new;
}

type *new_blockref_type(function *fn) {
  return new_function_type(copy_type(fn->return_type), fn->argument);
}

type *copy_type(type *t) {
  if (t == NULL) {
    return NULL;
  }

  type *new = new_type(t->type);

  switch (t->type) {
  case T_ARRAY: new->arraytype = copy_type(t->arraytype); break;
  case T_BLOCKREF: new->blocktype = copy_arguments(t->blocktype, false); break;
  case T_OBJECT:
    new->classtype = t->classtype;
    new->struct_index = t->struct_index;
    break;
  case T_REF: new->symbol_name = t->symbol_name; break;
  default: break;
  }

  return new;
}

void free_type(type *t) {
  if (t == NULL) return;

  switch (t->type) {
  case T_ARRAY: free_type(t->arraytype); break;
  case T_BLOCKREF: free_arguments(t->blocktype); break;
  default: break;
  }

  free(t);
}
