#include <stdio.h>
#include <string.h>

#include "analyze.h"
#include "xalloc.h"

#define ST_CLASS 1
#define ST_TYPE 2
#define ST_VARIABLE 4

void numeric_promotion(expression *value, bool allow_floats) {
  type_type ctype;
  cast_type cmethod;
  switch (value->type->type) {
  case T_F32:
  case T_F64:
  case T_F128:
    if (!allow_floats) {
      fprintf(stderr, "floats are not allowed\n");
      exit(1);
    }
    // fallthrough
  case T_S32:
  case T_S64:
    value->type = copy_type(value->value->type);
    break;
  case T_S8:
  case T_S16:
    ctype = T_S32;
    cmethod = O_SIGN_EXTEND;
    break;
  case T_U8:
  case T_U16:
  case T_U32:
    ctype = T_S32;
    cmethod = O_ZERO_EXTEND;
    break;
  case T_U64:
    ctype = T_S64;
    cmethod = O_ZERO_EXTEND;
    break;
  default:
    fprintf(stderr, "must be a numeric expression\n");
    exit(1);
  }

  expression *cast = xmalloc(sizeof(*cast));
  cast->operation.type = O_CAST;
  cast->operation.cast_type = cmethod;
  cast->type = new_type(ctype);
  cast->value = value->value;
  cast->next = NULL;
  value->value = cast;
  value->type = new_type(ctype);
}

static void no_floats() __attribute__ ((noreturn)) {
  fprintf(stderr, "floats are not allowed\n");
  exit(1);
}

void replace_cast(expression **value, cast_type cast, type_type target) {
  expression *new_cast = xmalloc(sizeof(*new_cast));
  new_cast->operation.type = O_CAST;
  new_cast->operation.cast_type = cast;
  new_cast->type = new_type(target);
  new_cast->value = *value;
  new_cast->next = (*value)->next;
  *value = new_cast;
}

type_type binary_numeric_promotion(expression *value, bool allow_floats) {
  expression *left = value->value, *right = left->next;
  type_type ltype = left->type->type, rtype = right->type->type;

  switch ((((uint8_t) ltype) << 8) | (uint8_t) rtype) {
  case (T_F32 << 8) | T_F64:
  case (T_F32 << 8) | T_F128:
  case (T_F64 << 8) | T_F128:
    if (!allow_floats) no_floats();
    replace_cast(&value->value, O_FLOAT_EXTEND, rtype);
    return rtype;
  case (T_F64 << 8) | T_F32:
  case (T_F128 << 8) | T_F32:
  case (T_F128 << 8) | T_F64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_FLOAT_EXTEND, ltype);
    return ltype;
  case (T_F32 << 8) | T_S8:
  case (T_F32 << 8) | T_S16:
  case (T_F32 << 8) | T_S32:
  case (T_F64 << 8) | T_S8:
  case (T_F64 << 8) | T_S16:
  case (T_F64 << 8) | T_S32:
  case (T_F64 << 8) | T_S64:
  case (T_F128 << 8) | T_S8:
  case (T_F128 << 8) | T_S16:
  case (T_F128 << 8) | T_S32:
  case (T_F128 << 8) | T_S64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_SIGNED_TO_FLOAT, ltype);
    return ltype;
  case (T_F32 << 8) | T_U8:
  case (T_F32 << 8) | T_U16:
  case (T_F32 << 8) | T_U32:
  case (T_F64 << 8) | T_U8:
  case (T_F64 << 8) | T_U16:
  case (T_F64 << 8) | T_U32:
  case (T_F64 << 8) | T_U64:
  case (T_F128 << 8) | T_U8:
  case (T_F128 << 8) | T_U16:
  case (T_F128 << 8) | T_U32:
  case (T_F128 << 8) | T_U64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_UNSIGNED_TO_FLOAT, ltype);
    return ltype;
  case (T_S8 << 8) | T_F32:
  case (T_S16 << 8) | T_F32:
  case (T_S32 << 8) | T_F32:
  case (T_S8 << 8) | T_F64:
  case (T_S16 << 8) | T_F64:
  case (T_S32 << 8) | T_F64:
  case (T_S64 << 8) | T_F64:
  case (T_S8 << 8) | T_F128:
  case (T_S16 << 8) | T_F128:
  case (T_S32 << 8) | T_F128:
  case (T_S64 << 8) | T_F128:
    if (!allow_floats) no_floats();
    replace_cast(&value->value, O_SIGNED_TO_FLOAT, rtype);
    return rtype;
  case (T_U8 << 8) | T_F32:
  case (T_U16 << 8) | T_F32:
  case (T_U32 << 8) | T_F32:
  case (T_U8 << 8) | T_F64:
  case (T_U16 << 8) | T_F64:
  case (T_U32 << 8) | T_F64:
  case (T_U64 << 8) | T_F64:
  case (T_U8 << 8) | T_F128:
  case (T_U16 << 8) | T_F128:
  case (T_U32 << 8) | T_F128:
  case (T_U64 << 8) | T_F128:
    if (!allow_floats) no_floats();
    replace_cast(&value->value, O_UNSIGNED_TO_FLOAT, rtype);
    return rtype;
  case (T_F32 << 8) | T_S64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_SIGNED_TO_FLOAT, T_F64);
    replace_cast(&value->value, O_FLOAT_EXTEND, T_F64);
    return T_F64;
  case (T_F32 << 8) | T_U64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_UNSIGNED_TO_FLOAT, T_F64);
    replace_cast(&value->value, O_FLOAT_EXTEND, T_F64);
    return T_F64;
  case (T_S64 << 8) | T_F32:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_FLOAT_EXTEND, T_F64);
    replace_cast(&value->value, O_SIGNED_TO_FLOAT, T_F64);
    return T_F64;
  case (T_U64 << 8) | T_F32:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_FLOAT_EXTEND, T_F64);
    replace_cast(&value->value, O_UNSIGNED_TO_FLOAT, T_F64);
    return T_F64;
  case (T_F128 << 8) | T_F128:
  case (T_F64 << 8) | T_F64:
  case (T_F32 << 8) | T_F32:
    if (!allow_floats) no_floats();
    // fallthrough
  case (T_S32 << 8) | T_S32:
  case (T_S64 << 8) | T_S64:
  case (T_U32 << 8) | T_U32:
  case (T_U64 << 8) | T_U64:
    // no casting necessary!
    return ltype;
  case (T_S8 << 8) | T_S8:
  case (T_S8 << 8) | T_S16:
  case (T_S16 << 8) | T_S8:
  case (T_S16 << 8) | T_S16:
  case (T_S8 << 8) | T_U8:
  case (T_S8 << 8) | T_U16:
  case (T_S16 << 8) | T_U8:
  case (T_S16 << 8) | T_U16:
  case (T_U8 << 8) | T_S8:
  case (T_U8 << 8) | T_S16:
  case (T_U16 << 8) | T_S8:
  case (T_U16 << 8) | T_S16:
    replace_cast(&left->next, O_SIGN_EXTEND, T_S32);
    replace_cast(&value->value, O_SIGN_EXTEND, T_S32);
    return T_S32;
  case (T_S8 << 8) | T_S32:
  case (T_S8 << 8) | T_S64:
  case (T_S16 << 8) | T_S32:
  case (T_S16 << 8) | T_S64:
  case (T_S32 << 8) | T_S64:
    replace_cast(&value->value, O_SIGN_EXTEND, rtype);
    return rtype;
  case (T_S32 << 8) | T_S8:
  case (T_S32 << 8) | T_S16:
  case (T_S64 << 8) | T_S8:
  case (T_S64 << 8) | T_S16:
  case (T_S64 << 8) | T_S32:
    replace_cast(&left->next, O_SIGN_EXTEND, ltype);
    return ltype;
  case (T_U8 << 8) | T_U8:
  case (T_U8 << 8) | T_U16:
  case (T_U16 << 8) | T_U8:
  case (T_U16 << 8) | T_U16:
    replace_cast(&left->next, O_ZERO_EXTEND, T_U32);
    replace_cast(&value->value, O_ZERO_EXTEND, T_U32);
    return T_U32;
  case (T_U8 << 8) | T_S32:
  case (T_U8 << 8) | T_S64:
  case (T_U8 << 8) | T_U32:
  case (T_U8 << 8) | T_U64:
  case (T_U16 << 8) | T_S32:
  case (T_U16 << 8) | T_S64:
  case (T_U16 << 8) | T_U32:
  case (T_U16 << 8) | T_U64:
  case (T_U32 << 8) | T_S64:
  case (T_U32 << 8) | T_U64:
    replace_cast(&value->value, O_ZERO_EXTEND, rtype);
    return rtype;
  case (T_S32 << 8) | T_U8:
  case (T_S32 << 8) | T_U16:
  case (T_S64 << 8) | T_U8:
  case (T_S64 << 8) | T_U16:
  case (T_S64 << 8) | T_U32:
  case (T_U32 << 8) | T_U8:
  case (T_U32 << 8) | T_U16:
  case (T_U64 << 8) | T_U8:
  case (T_U64 << 8) | T_U16:
  case (T_U64 << 8) | T_U32:
    replace_cast(&left->next, O_ZERO_EXTEND, ltype);
    return ltype;
  case (T_S8 << 8) | T_U32:
  case (T_S16 << 8) | T_U32:
    replace_cast(&left->next, O_REINTERPRET, T_S32);
    replace_cast(&value->value, O_SIGN_EXTEND, T_S32);
    return T_S32;
  case (T_S8 << 8) | T_U64:
  case (T_S16 << 8) | T_U64:
  case (T_S32 << 8) | T_U64:
    replace_cast(&left->next, O_REINTERPRET, T_S64);
    replace_cast(&value->value, O_SIGN_EXTEND, T_S64);
    return T_S64;
  case (T_S32 << 8) | T_U32:
  case (T_S64 << 8) | T_U64:
    replace_cast(&left->next, O_REINTERPRET, ltype);
    return ltype;
  case (T_U32 << 8) | T_S8:
  case (T_U32 << 8) | T_S16:
    replace_cast(&left->next, O_SIGN_EXTEND, T_S32);
    replace_cast(&value->value, O_REINTERPRET, T_S32);
    return T_S32;
  case (T_U64 << 8) | T_S8:
  case (T_U64 << 8) | T_S16:
  case (T_U64 << 8) | T_S32:
    replace_cast(&left->next, O_SIGN_EXTEND, T_S64);
    replace_cast(&value->value, O_REINTERPRET, T_S64);
    return T_S64;
  case (T_U32 << 8) | T_S32:
  case (T_U64 << 8) | T_S64:
    replace_cast(&value->value, O_REINTERPRET, rtype);
    return rtype;
  default:
    fprintf(stderr, "unsupported types in binary operation\n");
    exit(1);
  }
}

static void assert_integer(type *type, char *operation) {
  switch (type->type) {
  case T_BOOL:
  case T_S8:
  case T_S16:
  case T_S32:
  case T_S64:
  case T_U8:
  case T_U16:
  case T_U32:
  case T_U64:
    return;
  default:
    fprintf(stderr, "cannot %s non-integer\n", operation);
    exit(1);
  }
}

// TODO: limit condition types better
static void assert_condition(type *type) {
  assert_integer(type, "decide by");
}

static uint8_t integer_index(type_type type) {
  switch (type) {
  case T_BOOL: return 0;
  case T_S8: return 1;
  case T_U8: return 2;
  case T_S16: return 3;
  case T_U16: return 4;
  case T_S32: return 5;
  case T_U32: return 6;
  case T_S64: return 7;
  case T_U64: return 8;
  default: return -1;
  }
}

// TODO: floats
static type_type integer_promote(type *left, type *right) {
  type_type result = integer_index(left->type) >= integer_index(right->type)
    ? left->type : right->type;
  return result == T_BOOL ? T_S8 : result;
}

static loop_statement *get_label(control_statement *control) {
  const char *label = control->label;

  for (statement *node = control->parent; node; node = node->parent) {
    switch (node->type) {
    case S_DO_WHILE:
    case S_WHILE: {
      loop_statement *loop = (loop_statement*) node;
      if (loop->label && strcmp(loop->label, label) == 0) {
        return loop;
      }
      // fallthrough
    }
    case S_BLOCK:
    case S_IF:
      break;
    case S_FUNCTION:
      return NULL;
    // these shouldn't ever be a parent
    case S_BREAK:
    case S_CLASS:
    case S_CONTINUE:
    // case S_DECLARE:
    case S_DEFINE:
    case S_EXPRESSION:
    case S_RETURN:
      abort();
    }
  }

  return NULL;
}

static block_statement *parent_scope(block_statement *block, bool *escape) {
  statement *parent = block->parent;

  while (parent) {
    switch (parent->type) {
    case S_BLOCK:
      return (block_statement*) parent;
    case S_FUNCTION:
      if (escape) *escape = true;
      // fallthrough
    case S_DEFINE:
    case S_DO_WHILE:
    case S_EXPRESSION:
    case S_IF:
    case S_RETURN:
    case S_WHILE:
      break;
    // shouldn't ever be a parent
    case S_BREAK:
    case S_CONTINUE:
    // shouldn't currently be a parent
    case S_CLASS:
      abort();
    }

    parent = parent->parent;
  }

  // dead end
  return NULL;
}

symbol_entry **find_entry(const symbol_entry **head, const char *symbol_name) {
  symbol_entry **entry = head;
  for (; *entry; entry = &(*entry)->next) {
    if (strcmp(symbol_name, (*entry)->symbol_name) == 0) {
      break;
    }
  }

  return entry;
}

symbol_entry *get_entry(const symbol_entry *head, const char *symbol_name) {
  return *find_entry(&head);
}

static symbol_entry *new_symbol_entry(char *symbol_name, bool constant) {
  symbol_entry *entry = xmalloc(sizeof(*entry));
  entry->symbol_name = symbol_name;
  entry->constant = constant;
  entry->next = NULL;
  return entry;
}

static symbol_entry *add_symbol(block_statement *block, symbol_type type,
    char *symbol_name) {
  symbol_entry **entry, **tail;

  switch (type) {
  case ST_CLASS:
    entry = &block->class_head;
    tail = &block->class_tail;
    break;
  case ST_TYPE:
    entry = &block->type_head;
    tail = &block->type_tail;
  case ST_VARIABLE:
    entry = &block->variable_head;
    tail = &block->variable_tail;
    break;
  }

  entry = find_entry(entry);

  if (*entry) {
    fprintf(stderr, "symbol '%s' already defined\n", symbol_name);
    exit(1);
  }

  return *tail = *entry = new_symbol_entry(symbol_name, false);
}

static symbol_entry *get_symbol(block_statement *block, char *symbol_name,
    uint8_t symbol_type, uint8_t mark_dependency) {
  bool escape = false;
  do {
    uint8_t symbol_types[] = {ST_CLASS, ST_TYPE, ST_VARIABLE};

    for (size_t i = 0; i < sizeof(symbol_types) / sizeof(*symbol_types); i++) {
      uint8_t compare_symbol_type = symbol_types[i]

      if (!(compare_symbol_type & symbol_type)) {
        continue;
      }

      symbol_entry *entry;

      switch (symbol_types) {
      case ST_CLASS: entry = block->class_head; break;
      case ST_TYPE: entry = block->type_head; break;
      case ST_VARIABLE: entry = block->variable_head; break;
      }

      bool mark = (compare_symbol_type & mark_dependency) != 0;

      entry = get_entry(entry, symbol_name);
      if (entry) {
        if (escape && mark && !entry->constant) {
          fprintf(stderr, "non-constant closured variables not supported\n");
          exit(1);
        }

        return entry;
      }

      if (mark) {
        symbol_entry **entry = find_entry(&block->ref_head, symbol_name);
        if (!*entry) {
          *tail = *entry = new_symbol_entry(symbol_name, false);
        }
      }
    }

    block = parent_scope(block, &escape);
  } while (block);

  if (symtype == ST_CLASS) {
    fprintf(stderr, "class '%s' undeclared\n", symbol_name);
  } else {
    fprintf(stderr, "symbol '%s' undeclared\n", symbol_name);
  }
  exit(1);
}

// mutates
static type *resolve_type(block_statement *block, type *type) {
  // TODO: static types as properties (module support)
  switch (type->type) {
  case T_ARRAY:
    resolve_type(block, type->arraytype);
    break;
  case T_BLOCKREF:
    for (argument *arg = type->blocktype; arg; arg = arg->next) {
      resolve_type(block, arg->argument_type);
    }
    break;
  // TODO: support typedefs
  case T_REF:
  case T_OBJECT: {
    // TODO: this may fail because type objects are sometimes reassigned instead
    // of copied
    symbol_entry *entry = get_symbol(block, ST_CLASS, type->symbol_name);

    free(type->symbol_name);

    type->type = T_OBJECT;
    type->classtype = entry->classtype;
  } break;
  default:
    break;
  }
  return type;
}

static void analyze_expression(block_statement *block, expression *e) {
  switch (e->operation.type) {
  case O_BITWISE_NOT:
    analyze_expression(block, e->value);
    numeric_promotion(e->value, false);
    break;
  case O_NEGATE:
    analyze_expression(block, e->value);
    numeric_promotion(e->value, true);
    break;
  case O_CALL: {
    // TODO: function overloading and argument matching
    analyze_expression(block, e->value);

    type *fntype = e->value->type;

    if (fntype->type != T_BLOCKTYPE) {
      fprintf(stderr, "type not callable\n");
      exit(1);
    }

    e->type = copy_type(fntype->blocktype->argument_type);

    expression **argvalue = &e->value->next;

    argument *arg = fntype->blocktype->next;
    for (size_t i = 0; arg && *argvalue; i++) {
      analyze_expression(block, *argvalue);

      expression *cast = implicit_cast(*argvalue, arg->type);
      if (!cast) {
        fprintf(stderr, "function '%s' called with incompatible argument %zi\n",
          fn_name, i + 1);
        exit(1);
      }

      if (cast != *argvalue) {
        cast->next = (*argvalue)->next;
        (*argvalue)->next = NULL;
        *argvalue = cast;
      }
      argvalue = &(*argvalue)->next;
      arg = arg->next;
    }

    if ((!*argvalue) != (!arg)) {
      fprintf(stderr, "function '%s' called with wrong number of arguments\n",
        fn_name);
      exit(1);
    }

  } break;
  // type already declared
  case O_LITERAL:
    break;
  // type already declared
  case O_COMPARE:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);
    binary_numeric_promotion(e, true);
    break;
  case O_IDENTITY: {
    // only applies to objects/classes:
    // obj is Class
    // obj is obj

    expression *left = e->value, *right = left->next;
    analyze_expression(block, left);

    if (left->type->type != T_OBJECT) {
      fprintf(stderr, "cannot identify non-object\n");
      exit(1);
    }

    if (right->operation.type == O_GET_SYMBOL) {
      const char *symbol_name = right->symbol_name;

      for (;;) {
        symbol_entry *entry;

        entry = get_entry(block->variable_head, symbol_name);
        if (entry) {
          // comparing two objects
          right->type = copy_type(entry->type);
          break;
        }

        entry = get_entry(block->class_head, symbol_name);
        if (entry) {
          // checking to see if an object is an instance of a class
          e->operation.type = O_INSTANCEOF;
          e->classtype = entry->classtype;
          free_expression(right);
          return;
        }

        block = parent_scope(block);

        if (!block) {
          fprintf(stderr, "'%s' undeclared\n", symbol_name);
          exit(1);
        }
      }
    } else {
      analyze_expression(block, e->value->next);
    }

    // comparing two objects for identity
    if (right->type->type != T_OBJECT) {
      fprintf(stderr, "cannot identify non-object\n");
      exit(1);
    }
  } break;
  case O_LOGIC: {
    operation *op = &e->operation;
    expression *left = e->value, *right = left->next;
    left->next = NULL;

    analyze_expression(block, left);
    analyze_expression(block, right);

    left = bool_cast(left);
    right = bool_cast(right);

    e->value = left;
    if (op->logic_type == O_XOR) {
      op->type = O_NUMERIC;
      op->numeric_type = O_BXOR;
      left->next = right;
    } else {
      bool is_or = op->logic_type == O_OR;
      op->type = O_TERNARY;
      expression *literal = new_literal_node(T_BOOL);
      literal->value_bool = is_or;
      if (is_or) {
        left->next = literal;
        literal->next = right;
      } else {
        left->next = right;
        right->next = literal;
      }
    }
  } break;
  case O_FUNCTION:
    // TODO: support
    fprintf(stderr, "anonymous functions not supported\n");
    exit(1);
  case O_GET_FIELD:
  case O_SET_FIELD: {
    analyze_expression(block, e->value);

    class *class = e->value->type->classtype;

    char *name = e->symbol_name;
    size_t i = 0;
    for (field *field = class->field; field; field = field->next, ++i) {
      if (strcmp(name, field->symbol_name) == 0) {
        e->field_index = i;
        if (e->operation.type == O_GET_FIELD) {
          e->type = copy_type(field->field_type);
        } else {
          analyze_expression(block, e->value->next);
          e->type = NULL;
          free_type(e->value->next->type);
          e->value->next->type = NULL;
          // TODO: implicit cast
          if (!compatible_type(entry->type, e-value->next->type)) {
            fprintf(stderr, "incompatible types for '%s' field assignment\n",
              name);
            exit(1);
          }
        }
        free(name);
        return;
      }
    }

    fprintf(stderr, "'%s' has no field named '%s'\n", class->class_name, name);
    exit(1);
  }
  // TODO: O_GET_INDEX, O_SET_INDEX
  case O_GET_INDEX:
  case O_SET_INDEX: {
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    // TODO: support map indexing
    if (e->value->type->type != T_ARRAY && e->value->type->type != T_STRING) {
      fprintf(stderr, "must index an array or a string\n");
      exit(1);
    }

    switch (e->value->next->type->type) {
    case T_S8:
    case T_S16:
    case T_S32:
    case T_S64:
    case T_U8:
    case T_U16:
    case T_U32:
    case T_U64:
      break;
    default:
      fprintf(stderr, "cannot index by non-integer\n");
      exit(1);
    }

    e->type = copy_type(e->value->type->arraytype);
  } break;
  case O_GET_SYMBOL:
  case O_SET_SYMBOL: {
    // O_GET_SYMBOL also implemented in O_IDENTITY :|
    symbol_entry *entry = get_symbol(block, ST_VARIABLE, e->symbol_name);

    if (e->operation.type == O_SET_SYMBOL) {
      expression *cast = implicit_cast(e->value, entry->type);
      if (!cast) {
        fprintf(stderr, "incompatible type for '%s' assignment\n",
          e->symbol_name);
        exit(1);
      }

      if (cast != e->value) {
        e->value = cast;
      }
    }

    e->type = copy_type(entry->type);
    break;
  }
  case O_NEW: {
    // TODO: constructor overloading and argument matching
    char *class_name = e->symbol_name;
    e->symbol_name = NULL;

    symbol_entry *entry = get_symbol(block, ST_CLASS, class_name);

    class *class = entry->classtype;

    e->type->classtype = class;

    expression *argvalue = e->value;
    field *field = class->field;
    size_t i = 0;
    while (argvalue && field) {
      analyze_expression(block, argvalue);

      // TODO: implicit cast
      if (!compatible_type(field->field_type, argvalue->type)) {
        fprintf(stderr, "constructor for '%s' called with bad argument %zi\n",
          class_name, i);
        exit(1);
      }

      argvalue = argvalue->next;
      field = field->next;
      ++i;
    }

    if ((!argvalue) != (!field)) {
      fprintf(stderr, "constructor for '%s' called with wrong number of"
        " arguments\n", class_name);
      exit(1);
    }

    free(class_name);
  } break;
  case O_NOT:
    analyze_expression(block, e->value);

    expression *literal;
    switch (e->value->type->type) {
    case T_BOOL:
      literal = new_literal_node(T_BOOL);
      literal->value_bool = false;
      break;
    case T_S8:
    case T_U8:
      literal = new_literal_node(T_U8);
      literal->value_u8 = 0;
      break;
    case T_S16:
    case T_U16:
      literal = new_literal_node(T_U16);
      literal->value_u16 = 0;
      break;
    case T_S32:
    case T_U32:
      literal = new_literal_node(T_U32);
      literal->value_u32 = 0;
      break;
    case T_S64:
    case T_U64:
      literal = new_literal_node(T_U64);
      literal->value_u64 = 0;
      break;
    case T_ARRAY:
      fprintf(stderr, "cannot coerce array to bool\n");
      exit(1);
    case T_BLOCKREF:
      fprintf(stderr, "cannot coerce function to bool\n");
      exit(1);
    case T_OBJECT:
      fprintf(stderr, "cannot coerce object to bool\n");
      exit(1);
    case T_STRING:
      fprintf(stderr, "cannot coerce string to bool\n");
      exit(1);
    case T_REF:
    case T_VOID:
      abort();
    }
    e->operation.type = O_COMPARE;
    e->operation.compare_type = O_EQ;
    e->value->next = literal;
    break;
  case O_NUMERIC:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    bool allow_floats = true;
    switch (e->operation.numeric_type) {
    case O_ADD:
    case O_DIV:
    case O_MOD:
    case O_MUL:
    case O_SUB:
      // allow_floats = true;
      break;
    case O_BAND:
    case O_BOR:
    case O_BXOR:
      allow_floats = false;
      break;
    }
    e->type = new_type(binary_numeric_promotion(e, allow_floats));
    break;
  case O_NUMERIC_ASSIGN:
    abort();

    // TODO: need to handle implicit casting during assignment phase
    // maybe store cast in e->value->next->next?
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    bool allow_floats = true;
    switch (e->operation.numeric_type) {
    case O_ADD:
    case O_DIV:
    case O_MOD:
    case O_MUL:
    case O_SUB:
      // allow_floats = true;
      break;
    case O_BAND:
    case O_BOR:
    case O_BXOR:
      allow_floats = false;
      break;
    }
    type_type new_type = binary_numeric_promotion(e, allow_floats);


    assert_integer(e->value->type, "operate on");
    assert_integer(e->value->next->type, "operate on");

    // TODO: this should be in a common place closer to code generation,
    // specifically when things are more linear and (a = a + b) is the same as
    // (a += b)
    /*if (integer_id(left->type) < integer_id(right->type)) {
      fprintf(stderr, "warning: loss of precision in assignment\n");
    }*/

    // TODO: implicit cast
    e->type = copy_type(e->value->type);
    break;
  case O_POSTFIX:
    abort();

    // TODO: need to handle implicit casting during assignment phase
    analyze_expression(block, e->value);

    numeric_promotion(e, true);
    e->type = copy_type(e->value->type);
    break;
  case O_SHIFT:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    numeric_promotion(e, false);
    numeric_promotion(e->?, false);

    e->type = copy_type(e->value->type);
    break;
  case O_SHIFT_ASSIGN:
    abort();

    // TODO: need to handle implicit casting during assignment phase
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    assert_integer(e->value->type, "shift on");
    assert_integer(e->value->next->type, "shift with");

    // TODO: implicit cast
    e->type = copy_type(e->value->type);
    break;
  case O_STR_CONCAT:
  case O_STR_CONCAT_ASSIGN:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    if (e->operation.type == O_STR_CONCAT_ASSIGN && e->value->type->type != T_STRING) {
      fprintf(stderr, "must concatenate a string into a string lvalue\n");
      exit(1);
    }

    if (e->value->next->type->type != T_STRING) {
      // TODO: coerce to string!
      abort();
    }
    break;
  case O_TERNARY:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);
    analyze_expression(block, e->value->next->next);

    assert_condition(e->value->type);

    // TODO: implicit cast
    if (!compatible_type(e->value->next->type, e->value->next->next->type)) {
      fprintf(stderr, "ternary cannot decide between incompatible values\n");
      exit(1);
    }

    e->type = e->value->next->type;
    break;
  case O_BLOCKREF:
  case O_CAST: // TODO: implement me!
    abort();
  }

  // not always reached when successful, some cases just return
}

static void analyze_inner(block_statement *block, statement **node) {
  switch ((*node)->type) {
  case S_BLOCK:
    analyze((block_statement*) *node);
    break;
  case S_BREAK:
  case S_CONTINUE: {
    control_statement *control = (control_statement*) *node;

    if (control->label) {
      loop_statement *loop = get_label(control);

      if (loop == NULL) {
        fprintf(stderr, "label '%s' undeclared\n", control->label);
        exit(1);
      }

      free(control->label);
      control->target = loop;
    } else {
      control->target = NULL;
    }
  } break;
  case S_DEFINE: {
    define_statement *define = (define_statement*) *node;
    define_clause *clause = define->clause;

    type *define_type = resolve_type(block, define->symbol_type);

    while (clause != NULL) {
      add_symbol(block, ST_VARIABLE, clause->symbol_name)
        ->type = copy_type(define_type);

      if (clause->value) {
        analyze_expression(block, clause->value);

        if (!compatible_type(define_type, clause->value->type)) {
          fprintf(stderr, "initialization clause type mismatch\n");
          exit(1);
        }
      }

      clause = clause->next;
    }
  } break;
  case S_IF: {
    if_statement *if_s = (if_statement*) *node;
    analyze_expression(block, if_s->condition);
    assert_condition(if_s->condition->type);

    analyze(if_s->first);
    analyze(if_s->second);
  } break;
  case S_DO_WHILE:
  case S_WHILE: {
    loop_statement *loop = (loop_statement*) *node;
    analyze_expression(block, loop->condition);
    analyze(loop->body);
  } break;
  case S_FUNCTION: {
    function *fn = ((function_statement*) *node)->function;
    block_statement *body = fn->body;
    body->parent = (statement*) block;

    // add arguments to symbol table
    for (argument *arg = fn->argument; arg; arg = arg->next) {
      type *arg_type = resolve_type(block, arg->argument_type);
      add_symbol(body, ST_VARIABLE, arg->symbol_name)
        ->type = arg_type;
      arg->argument_type = arg_type;
    }

    analyze(fn->body);

    // TODO: support all closured variables, not just constant ones
    // TODO: for now, any code_struct with a T_BLOCKTYPE as the first field can
    // be called with the O_CALL operation, but only in the generation phase.
    // also, that process would error out if it discovers that the function
    // field does not accept the code_struct as its first argument
    for (symbol_entry *entry = fn->body->ref_head; entry; entry = entry->next) {

    }
  } break;
  case S_EXPRESSION:
    analyze_expression(block, ((expression_statement*) *node)->value);
    break;
  case S_RETURN: {
    statement *parent = (statement*) block;
    function *fn;
    for (;;) {
      switch (parent->type) {
      case S_BLOCK: {
        block_statement *body = (block_statement*) parent;
        if (body->fn_parent) {
          fn = body->fn_parent;
          goto return_loop_escape;
        }
      }
      case S_DO_WHILE:
      case S_IF:
      case S_WHILE:
        break;
      // this should be handled by the fn_parent check
      case S_FUNCTION:
      // these shouldn't ever be a parent
      case S_BREAK:
      case S_CLASS:
      case S_CONTINUE:
      // case S_DECLARE:
      case S_DEFINE:
      case S_EXPRESSION:
      case S_RETURN:
        abort();
      }

      parent = parent->parent;

      if (!parent) {
        fprintf(stderr, "must return from within a function\n");
        exit(1);
      }
    }
return_loop_escape:;

    return_statement *ret = (return_statement*) *node;
    expression *value = ret->value;
    ret->target = fn;

    if (!compatible_type(fn->return_type, value ? value->type : NULL)) {
      fprintf(stderr, "return type mismatch\n");
      exit(1);
    }
  } break;
  case S_CLASS:
    // do nothing!
    break;
  }
}

void analyze(block_statement *block) {
  // build the hoisted tables
  for (statement *node = block->body; node != NULL; node = node->next) {
    switch (node->type) {
    case S_CLASS: {
      class_statement *c = (class_statement*) node;
      add_symbol(block, ST_CLASS, c->symbol_name)
        ->classtype = c->classtype;
    } break;

    // TODO: functions should be *immutable* variables, not separate
    // as such, the function should be declared as const and then statically
    // resolved in appropriate scopes
    case S_FUNCTION: {
      function_statement *f = (function_statement*) node;
      symbol_entry *fn_entry = add_symbol(block, ST_VARIABLE, f->symbol_name);
      fn_entry->type = new_blockref_type(f->function);
      fn_entry->constant = true;
    } break;

    default:
      break;
    }
  }

  // build the variable and label symbol tables, typecheck
  statement **node = &block->body;
  while ((*node) != NULL) {
    // grab the next node for the event that the inner function changes the tree
    statement *next = (*node)->next;
    analyze_inner(block, node);
    // resolve the links in case the structure changed
    while (next != *node) {
      node = &(*node)->next;
    }
  }
}
