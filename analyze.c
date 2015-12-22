#include <stdio.h>
#include <string.h>

#include "analyze.h"
#include "parse.h"
#include "xalloc.h"

static void analyze_expression(block_statement*, expression*);

static void analyze_function(block_statement *block, function *fn) {
  block_statement *body = fn->body;
  body->parent = (statement*) block;

  // add arguments to symbol table
  for (argument *arg = fn->argument; arg; arg = arg->next) {
    type *arg_type = arg->argument_type;
    add_symbol(body, ST_VARIABLE, arg->symbol_name)
      ->type = copy_type(arg_type);
  }

  analyze(body);
}

static expression **analyze_new(block_statement *block, class *the_class,
    expression **param) {
  if (the_class->parent) {
    param = analyze_new(block, the_class->parent, param);
  }

  field *class_field = the_class->field;
  size_t i = 0;
  while (class_field && *param) {
    analyze_expression(block, *param);

    expression *next = (*param)->next;
    (*param)->next = NULL;
    *param = implicit_cast(*param, class_field->field_type);
    (*param)->next = next;
    param = &(*param)->next;
    class_field = class_field->next;
    ++i;
  }

  return param;
}

static void analyze_field_operation(expression *e) {
  char *field_name = e->symbol_name;

  char *class_name;
  if (e->value->type->type == T_ARRAY) {
    if (strcmp(field_name, "length") == 0) {
      free(field_name);

      e->operation.type = e->operation.type == O_GET_FIELD ? O_GET_LENGTH
        : O_SET_LENGTH;
      e->type = new_type(T_U32);
      e->symbol_name = NULL;
      return;
    }

    // TODO: better things
    class_name = "[]";
    goto unknown_field;
  }

  if (e->value->type->type == T_STRING) {
    if (e->operation.type == O_GET_FIELD && strcmp(field_name, "length") == 0) {
      free(field_name);

      e->operation.type = O_GET_LENGTH;
      e->type = new_type(T_U32);
      e->symbol_name = NULL;
      return;
    }

    class_name = "string";
    goto unknown_field;
  }

  if (e->value->type->type != T_OBJECT) {
    fail("field access can only occur on arrays, strings, and objects", e);
  }

  class *the_class = e->value->type->classtype;
  class_name = the_class->class_name;

  do {
    size_t i = 0;
    for (field *class_field = the_class->field; class_field;
        class_field = class_field->next, ++i) {
      if (strcmp(field_name, class_field->symbol_name) == 0) {
        e->field_index = i;
        e->type = copy_type(class_field->field_type);

        if (e->operation.type == O_SET_FIELD) {
          expression *cast = implicit_cast(e->value->next, e->type);

          cast->next = NULL;
          e->value->next = cast;
        }

        free(field_name);
        return;
      }
    }
  } while ((the_class = the_class->parent) != NULL);

unknown_field:
  fail("'%s' has no field named '%s'", e, class_name, field_name);
}

static void analyze_expression(block_statement *block, expression *e) {
  switch (e->operation.type) {
  case O_BITWISE_NOT: {
    analyze_expression(block, e->value);

    switch (e->value->type->type) {
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
      fail("cannot bitwise invert non-integer", e);
    }

    e->type = copy_type(e->value->type);
  } break;
  // TODO: better negative literal support
  case O_NEGATE:
    analyze_expression(block, e->value);

    e->value = numeric_promotion(e->value, true);
    e->type = copy_type(e->value->type);
    break;
  case O_CALL: {
    // TODO: function overloading and argument matching
    analyze_expression(block, e->value);

    type *fntype = e->value->type;

    if (fntype->type != T_BLOCKREF) {
      fail("type not callable", e);
    }

    e->type = copy_type(fntype->blocktype->argument_type);

    expression **argvalue = &e->value->next;

    argument *arg = fntype->blocktype->next;
    for (size_t i = 0; arg && *argvalue; i++) {
      analyze_expression(block, *argvalue);

      expression *cast = implicit_cast(*argvalue, arg->argument_type);
      if (!cast) {
        fail("function called with incompatible argument %zu", e, i + 1);
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
      fail("function called with wrong number of arguments", e);
    }

  } break;
  case O_CAST: {
    analyze_expression(block, e->value);
    explicit_cast(e);
  } break;
  // type already declared
  case O_LITERAL:
    break;
  // type already declared
  case O_COMPARE: {
    expression *left = e->value, *right = left->next;
    analyze_expression(block, left);
    analyze_expression(block, right);

    type *lt = left->type, *rt = right->type;
    type_type ltype = lt->type, rtype = rt->type;

    if (ltype == T_ARRAY && rtype == T_ARRAY) {
      fail("array comparison not supported", e);
    } else if (ltype == T_BLOCKREF && rtype == T_BLOCKREF) {
      // TODO: optimize for incompatible types
      switch (e->operation.compare_type) {
      case O_GT:
      case O_GTE:
      case O_LT:
      case O_LTE:
        fail("relative function comparison not supported", e);
      default:
        break;
      }
    } else if (ltype == T_OBJECT && rtype == T_OBJECT) {
      switch (e->operation.compare_type) {
      case O_GT:
      case O_GTE:
      case O_LT:
      case O_LTE:
        fail("relative object comparison not supported", e);
      default:
        break;
      }
    } else if (ltype == T_STRING && rtype == T_STRING) {
      switch (e->operation.compare_type) {
      case O_GT:
      case O_GTE:
      case O_LT:
      case O_LTE:
        fail("relative string comparison not supported", e);
      default:
        break;
      }
    } else if (ltype != T_BOOL && rtype != T_BOOL) {
      binary_numeric_promotion(e, true);
    }
  } break;
  case O_IDENTITY: {
    // only applies to objects/classes:
    // obj is Class
    // obj is obj

    expression *left = e->value, *right = left->next;
    analyze_expression(block, left);

    if (left->type->type != T_OBJECT) {
      fail("cannot identify non-object", e);
    }

    if (right->operation.type == O_GET_SYMBOL) {
      uint8_t symbol_type = ST_FUNCTION | ST_VARIABLE | ST_TYPE;
      symbol_entry *entry;

      entry = get_symbol(block, right->symbol_name, &symbol_type);

      if (symbol_type == ST_FUNCTION) {
        // TODO: should this be supported so you can check if two function
        // references have the same closure?
        fail("cannot check identity of function", e);
      }

      if (symbol_type == ST_TYPE) {
        // TODO: support more than just T_OBJECT
        e->operation.type = O_INSTANCEOF;
        e->classtype = entry->type->classtype;
        free_expression(right, true);
        break;
      }

      right->type = copy_type(entry->type);
    } else {
      analyze_expression(block, e->value->next);
    }

    // comparing two objects for identity
    if (right->type->type != T_OBJECT) {
      fail("cannot identify non-object", e);
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
      // TODO: reinterpret_cast for this?
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
    analyze_function(block, e->function);
    break;
  case O_GET_FIELD:
  case O_SET_FIELD:
    analyze_expression(block, e->value);

    if (e->operation.type == O_SET_FIELD) {
      analyze_expression(block, e->value->next);
    }

    analyze_field_operation(e);
    break;
  case O_GET_INDEX:
  case O_SET_INDEX: {
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    // TODO: support map indexing
    if (e->value->type->type != T_ARRAY && e->value->type->type != T_STRING) {
      fail("must index an array or a string", e);
    }

    if (!is_integer(e->value->next->type)) {
      fail("cannot index by non-integer", e);
    }

    if (e->operation.type == O_SET_INDEX) {
      if (e->value->type->type == T_STRING) {
        fail("strings are immutable", e);
      }

      analyze_expression(block, e->value->next->next);
      expression *cast = implicit_cast(e->value->next->next,
        e->value->type->arraytype);

      cast->next = NULL;
      e->value->next->next = cast;
    }

    if (e->value->type->type == T_STRING) {
      e->type = new_type(T_U8);
    } else {
      e->type = copy_type(e->value->type->arraytype);
    }
  } break;
  case O_GET_SYMBOL:
  case O_SET_SYMBOL: {
    // O_GET_SYMBOL also implemented in O_IDENTITY :|
    uint8_t symbol_type = ST_FUNCTION | ST_VARIABLE;
    symbol_entry *entry = get_symbol(block, e->symbol_name, &symbol_type);

    if (!entry) {
      fail("unknown symbol '%s'", e, e->symbol_name);
    }

    if (symbol_type == ST_FUNCTION) {
      if (e->operation.type == O_SET_SYMBOL) {
        fail("cannot reassign function '%s'", e, e->symbol_name);
      }

      free(e->symbol_name);

      e->operation.type = O_BLOCKREF;
      e->function = entry->function;

      e->type = new_blockref_type(entry->function);
    } else {
      if (e->operation.type == O_SET_SYMBOL) {
        analyze_expression(block, e->value);

        expression *cast = implicit_cast(e->value, entry->type);
        if (!cast) {
          fail("incompatible type for '%s' assignment", e, e->symbol_name);
        }

        // not initialized by implicit_cast
        cast->next = NULL;

        // might already be the same
        e->value = cast;
      }

      e->type = copy_type(entry->type);
    }
    break;
  }
  case O_NATIVE:
    for (expression *val = e->value; val; val = val->next) {
      analyze_expression(block, val);
    }

    if (e->assign) {
      uint8_t symbol_type = ST_VARIABLE;
      symbol_entry *entry = get_symbol(block, e->assign, &symbol_type);

      if (!entry) {
        fail("unknown symbol '%s'", e, e->assign);
      }

      e->type = copy_type(entry->type);
    } else {
      e->type = new_type(T_VOID);
    }
    break;
  case O_NEW: {
    // TODO: constructor overloading and argument matching
    char *class_name = e->symbol_name;
    e->symbol_name = NULL;

    uint8_t symbol_type = ST_TYPE;
    symbol_entry *entry = get_symbol(block, class_name, &symbol_type);

    if (!entry) {
      fail("class '%s' not defined", e, class_name);
    }

    if (entry->type->type != T_OBJECT) {
      fail("type '%s' not a class", e, class_name);
    }

    class *the_class = entry->type->classtype;

    e->type->classtype = the_class;

    if (*analyze_new(block, the_class, &e->value)) {
      fail("constructor for '%s' called with wrong number of arguments", e,
        class_name);
    }

    free(class_name);
  } break;
  case O_NEW_ARRAY:
    analyze_expression(block, e->value);

    if (!is_integer(e->value->type)) {
      fail("new array size must be numeric", e);
    }

    resolve_type(block, e->type);
    break;
  case O_NOT: {
    analyze_expression(block, e->value);
    expression *b = bool_cast(e->value);

    expression *literal = new_literal_node(T_BOOL);
    literal->value_bool = false;

    e->operation.type = O_COMPARE;
    e->operation.compare_type = O_EQ;
    e->value = b;
    b->next = literal;
  } break;
  case O_NUMERIC:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    type_type expression_type;
    switch (e->operation.numeric_type) {
    case O_ADD:
    case O_DIV:
    case O_MOD:
    case O_MUL:
    case O_SUB:
      expression_type = binary_numeric_promotion(e, true);
      break;
    case O_BAND:
    case O_BOR:
    case O_BXOR:
      expression_type = binary_numeric_conversion(e);
      break;
    }

    e->type = new_type(expression_type);
    break;
  case O_NUMERIC_ASSIGN: {
    expression *left = e->value, *right = left->next;

    analyze_expression(block, left);
    analyze_expression(block, right);

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

    if (!is_integer(left->type) && (!allow_floats ||
        !is_float(left->type))) {
      fail("numeric assignment lhs must be numeric", e);
    }

    expression *cast = implicit_cast(right, left->type);
    cast->next = NULL;
    left->next = cast;

    e->type = copy_type(left->type);
  } break;
  case O_POSTFIX:
    analyze_expression(block, e->value);

    e->type = copy_type(e->value->type);
    break;
  case O_SHIFT: {
    expression *left = e->value, *right = left->next;
    left->next = NULL;
    analyze_expression(block, left);
    analyze_expression(block, right);

    left = numeric_promotion(left, false);
    right = numeric_promotion(right, false);

    e->value = left;
    left->next = right;

    e->type = copy_type(e->value->type);
  } break;
  case O_SHIFT_ASSIGN: {
    expression *left = e->value, *right = left->next;
    analyze_expression(block, left);
    analyze_expression(block, right);

    if (!is_integer(left->type)) {
      fail("shift assignment lhs must be numeric", e);
    }

    expression *cast = numeric_promotion(right, false);
    left->next = cast;

    e->type = copy_type(left->type);
  } break;
  case O_STR_CONCAT: {
    expression *left = e->value, *right = left->next;
    analyze_expression(block, left);
    analyze_expression(block, right);

    if (left->type->type != T_STRING || right->type->type != T_STRING) {
      fail("string coercion not implemented", e);
    }
  } break;
  case O_STR_CONCAT_ASSIGN: {
    expression *left = e->value, *right = left->next;
    analyze_expression(block, left);
    analyze_expression(block, right);

    if (left->type->type != T_STRING) {
      fail("must concatenate a string into a string lvalue", e);
    }

    if (right->type->type != T_STRING) {
      fail("string coercion not implemented", e);
    }
  } break;
  case O_TERNARY: {
    expression *cond = e->value, *left = cond->next, *right = left->next;
    analyze_expression(block, cond);
    analyze_expression(block, left);
    analyze_expression(block, right);

    expression *b = bool_cast(cond);

    if (b != cond) {
      b->next = left;
      e->value = b;
    }

    ternary_numeric_promotion(e);
  } break;
  case O_BLOCKREF:
  case O_GET_LENGTH:
  case O_INSTANCEOF:
  case O_SET_LENGTH:
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
      block_statement *parent = block;
      control->target = NULL;
      do {
        if (parent->parent && (parent->parent->type == S_WHILE ||
            parent->parent->type == S_DO_WHILE)) {
          control->target = (loop_statement*) parent->parent;
          break;
        }

        bool escape = false;
        parent = parent_scope(parent, &escape);
        if (escape) {
          break;
        }
      } while (parent);

      if (control->target == NULL) {
        fprintf(stderr, "%s statement must be inside a loop\n",
          (*node)->type == S_BREAK ? "break" : "continue");
        exit(1);
      }
    }
  } break;
  case S_DEFINE: {
    define_statement *define = (define_statement*) *node;
    type *define_type = resolve_type(block, define->symbol_type);

    define_clause *clause = define->clause;

    do {
      add_symbol(block, ST_VARIABLE, clause->symbol_name)
        ->type = copy_type(define_type);

      if (clause->value) {
        analyze_expression(block, clause->value);

        expression *cast = implicit_cast(clause->value, define_type);
        clause->value = cast;
      }

      clause = clause->next;
    } while (clause != NULL);
  } break;
  case S_LET: {
    let_statement *let = (let_statement*) *node;

    define_clause *clause = let->clause;

    do {
      analyze_expression(block, clause->value);

      if (is_void(clause->value->type)) {
        fprintf(stderr, "variable '%s' implied void\n", clause->symbol_name);
        exit(1);
      }

      add_symbol(block, ST_VARIABLE, clause->symbol_name)
        ->type = copy_type(clause->value->type);

      clause = clause->next;
    } while (clause);
  } break;
  case S_IF: {
    if_statement *if_s = (if_statement*) *node;
    analyze_expression(block, if_s->condition);
    assert_condition(if_s->condition);

    if (if_s->first) {
      analyze(if_s->first);
    }
    if (if_s->second) {
      analyze(if_s->second);
    }
  } break;
  case S_DO_WHILE:
  case S_WHILE: {
    loop_statement *loop = (loop_statement*) *node;
    analyze_expression(block, loop->condition);
    assert_condition(loop->condition);

    analyze(loop->body);
  } break;
  case S_FUNCTION:
    analyze_function(block, ((function_statement*) *node)->function);
    break;
  case S_EXPRESSION:
    analyze_expression(block, ((expression_statement*) *node)->value);
    break;
  case S_RETURN: {
    statement *parent = (statement*) block;
    function *fn;
    for (;;) {
      if (parent->type == S_BLOCK) {
        block_statement *block_parent = (block_statement*) parent;
        if (block_parent->fn_parent) {
          fn = block_parent->fn_parent;
          break;
        }
      }

      switch (parent->type) {
      case S_BLOCK:
      case S_DO_WHILE:
      case S_IF:
      case S_WHILE:
        break;
      // this should be handled by the if statement
      case S_FUNCTION:
      // these shouldn't ever be a parent
      case S_BREAK:
      case S_CLASS:
      case S_CONTINUE:
      case S_DEFINE:
      case S_EXPRESSION:
      case S_LET:
      case S_RETURN:
      case S_TYPEDEF:
        abort();
      }

      parent = parent->parent;

      if (!parent) {
        fprintf(stderr, "must return from within a function\n");
        exit(1);
      }
    }

    return_statement *ret = (return_statement*) *node;
    ret->target = fn;

    analyze_expression(block, ret->value);
    ret->value = implicit_cast(ret->value, fn->return_type);
  } break;
  case S_TYPEDEF: {
    typedef_statement *tdef = (typedef_statement*) *node;
    add_symbol(block, ST_TYPE, tdef->alias)
      ->type = resolve_type(block, copy_type(tdef->typedef_type));
  } break;

  case S_CLASS:
    break;
  }
}

void analyze(block_statement *block) {
  // hoist classes
  for (statement *node = block->body; node != NULL; node = node->next) {
    if (node->type == S_CLASS) {
      class_statement *c = (class_statement*) node;
      add_symbol(block, ST_TYPE, c->symbol_name)
        ->type = new_object_type(c->classtype);
      add_symbol(block, ST_CLASS, c->symbol_name)
        ->classtype = c->classtype;
    }
  }

  // resolve class types
  for (statement *node = block->body; node != NULL; node = node->next) {
    if (node->type == S_CLASS) {
      class *c = ((class_statement*) node)->classtype;

      char *class_name = c->parent_name;
      if (class_name != NULL) {
        uint8_t symbol_type = ST_TYPE;
        symbol_entry *entry = get_symbol(block, class_name, &symbol_type);

        if (!entry) {
          fprintf(stderr, "class '%s' not defined\n", class_name);
          exit(1);
        }

        if (entry->type->type != T_OBJECT) {
          fprintf(stderr, "type '%s' not a class\n", class_name);
          exit(1);
        }

        c->parent = entry->type->classtype;
      }

      for (field *c_field = c->field; c_field; c_field = c_field->next) {
        c_field->field_type = resolve_type(block, c_field->field_type);
      }
    }
  }

  // hoist functions and resolve function types
  for (statement *node = block->body; node != NULL; node = node->next) {
    if (node->type == S_FUNCTION) {
      // TODO: functions should be *immutable* variables, not separate
      // as such, the function should be declared as const and then statically
      // resolved in appropriate scopes

      function_statement *f = (function_statement*) node;
      function *fn = f->function;

      fn->return_type = resolve_type(block, fn->return_type);

      // add arguments to symbol table
      for (argument *arg = fn->argument; arg; arg = arg->next) {
        arg->argument_type = resolve_type(block, arg->argument_type);
      }

      add_symbol(block, ST_VARIABLE, f->symbol_name)
        ->type = new_blockref_type(fn);
      add_symbol(block, ST_FUNCTION, f->symbol_name)
        ->function = f->function;
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
