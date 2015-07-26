#include <stdio.h>
#include <string.h>

#include "analyze.h"
#include "parse.h"
#include "xalloc.h"

static void analyze_function(block_statement *block, function *fn) {
  block_statement *body = fn->body;
  body->parent = (statement*) block;

  fn->return_type = resolve_type(block, fn->return_type);

  // add arguments to symbol table
  for (argument *arg = fn->argument; arg; arg = arg->next) {
    type *arg_type = resolve_type(block, arg->argument_type);
    add_symbol(body, ST_VARIABLE, arg->symbol_name)
      ->type = copy_type(arg_type);
    arg->argument_type = arg_type;
  }

  analyze(body);
}

static void analyze_expression(block_statement *block, expression *e) {
  switch (e->operation.type) {
  case O_BITWISE_NOT: {
    analyze_expression(block, e->value);
    e->value = numeric_promotion(e->value, false);
    e->type = copy_type(e->value->type);
  } break;
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
      fprintf(stderr, "type not callable\n");
      exit(1);
    }

    e->type = copy_type(fntype->blocktype->argument_type);

    expression **argvalue = &e->value->next;

    argument *arg = fntype->blocktype->next;
    for (size_t i = 0; arg && *argvalue; i++) {
      analyze_expression(block, *argvalue);

      expression *cast = implicit_cast(*argvalue, arg->argument_type);
      if (!cast) {
        fprintf(stderr, "function called with incompatible argument %zi\n",
          i + 1);
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
      fprintf(stderr, "function called with wrong number of arguments\n");
      exit(1);
    }

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
      fprintf(stderr, "array comparison not supported\n");
      exit(1);
    } else if (ltype == T_BLOCKREF && rtype == T_BLOCKREF) {
      // TODO: optimize for incompatible types
      switch (e->operation.compare_type) {
      case O_GT:
      case O_GTE:
      case O_LT:
      case O_LTE:
        fprintf(stderr, "relative function comparison not supported\n");
        exit(1);
      default:
        break;
      }
    } else if (ltype == T_OBJECT && rtype == T_OBJECT) {
      switch (e->operation.compare_type) {
      case O_GT:
      case O_GTE:
      case O_LT:
      case O_LTE:
        fprintf(stderr, "relative object comparison not supported\n");
        exit(1);
      default:
        break;
      }
    } else if (ltype == T_STRING && rtype == T_STRING) {
      switch (e->operation.compare_type) {
      case O_GT:
      case O_GTE:
      case O_LT:
      case O_LTE:
        fprintf(stderr, "relative string comparison not supported\n");
        exit(1);
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
      fprintf(stderr, "cannot identify non-object\n");
      exit(1);
    }

    if (right->operation.type == O_GET_SYMBOL) {
      uint8_t symbol_type = ST_FUNCTION | ST_VARIABLE | ST_TYPE;
      symbol_entry *entry;

      entry = get_symbol(block, right->symbol_name, &symbol_type);

      if (symbol_type == ST_FUNCTION) {
        // TODO: this should be supported so you can check if two function
        // references have the same closure
        fprintf(stderr, "cannot check identity of function\n");
        exit(1);
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
  case O_SET_FIELD: {
    analyze_expression(block, e->value);

    class *class = e->value->type->classtype;

    char *name = e->symbol_name;
    size_t i = 0;
    for (field *field = class->field; field; field = field->next, ++i) {
      if (strcmp(name, field->symbol_name) == 0) {
        e->field_index = i;
        e->type = copy_type(field->field_type);
        if (e->operation.type == O_SET_FIELD) {
          analyze_expression(block, e->value->next);
          expression *cast = implicit_cast(e->value->next, e->type);

          cast->next = NULL;
          e->value->next = cast;
        }
        free(name);
        return;
      }
    }

    fprintf(stderr, "'%s' has no field named '%s'\n", class->class_name, name);
    exit(1);
  }
  case O_GET_INDEX:
  case O_SET_INDEX: {
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    // TODO: support map indexing
    if (e->value->type->type != T_ARRAY && e->value->type->type != T_STRING) {
      fprintf(stderr, "must index an array or a string\n");
      exit(1);
    }

    if (!is_integer(e->value->next->type)) {
      fprintf(stderr, "cannot index by non-integer\n");
      exit(1);
    }

    if (e->operation.type == O_SET_INDEX) {
      if (e->value->type->type == T_STRING) {
        fprintf(stderr, "strings are immutable\n");
        exit(1);
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
      fprintf(stderr, "unknown symbol '%s'\n", e->symbol_name);
      exit(1);
    }

    if (symbol_type == ST_FUNCTION) {
      if (e->operation.type == O_SET_SYMBOL) {
        fprintf(stderr, "cannot reassign function '%s'\n", e->symbol_name);
        exit(1);
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
          fprintf(stderr, "incompatible type for '%s' assignment\n",
            e->symbol_name);
          exit(1);
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
        fprintf(stderr, "unknown symbol '%s'\n", e->assign);
        exit(1);
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
      fprintf(stderr, "class '%s' not defined\n", class_name);
      exit(1);
    }

    if (entry->type->type != T_OBJECT) {
      fprintf(stderr, "type '%s' not a class\n", class_name);
      exit(1);
    }

    class *the_class = entry->type->classtype;

    e->type->classtype = the_class;

    expression **argvalue = &e->value;
    field *field = the_class->field;
    size_t i = 0;
    while (field && *argvalue) {
      analyze_expression(block, *argvalue);

      // TODO: implicit cast
      expression *next = (*argvalue)->next;
      (*argvalue)->next = NULL;
      *argvalue = implicit_cast(*argvalue, field->field_type);
      (*argvalue)->next = next;
      argvalue = &(*argvalue)->next;
      field = field->next;
      ++i;
    }

    if ((!*argvalue) != (!field)) {
      fprintf(stderr, "constructor for '%s' called with wrong number of"
        " arguments\n", class_name);
      exit(1);
    }

    free(class_name);
  } break;
  case O_NEW_ARRAY:
    analyze_expression(block, e->value);

    if (!is_integer(e->value->type)) {
      fprintf(stderr, "new array size must be numeric\n");
      exit(1);
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
      fprintf(stderr, "numeric assignment lhs must be numeric\n");
      exit(1);
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
      fprintf(stderr, "shift assignment lhs must be numeric\n");
      exit(1);
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
      fprintf(stderr, "string coercion not implemented\n");
      exit(1);
    }
  } break;
  case O_STR_CONCAT_ASSIGN: {
    expression *left = e->value, *right = left->next;
    analyze_expression(block, left);
    analyze_expression(block, right);

    if (left->type->type != T_STRING) {
      fprintf(stderr, "must concatenate a string into a string lvalue\n");
      exit(1);
    }

    if (right->type->type != T_STRING) {
      fprintf(stderr, "string coercion not implemented\n");
      exit(1);
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
      cond->next = NULL;
      e->value = b;
    }

    ternary_numeric_promotion(e);
  } break;
  case O_BLOCKREF:
  case O_CAST: // TODO: implement me!
  case O_INSTANCEOF:
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
    define_clause *clause = define->clause;

    type *define_type = resolve_type(block, define->symbol_type);

    while (clause != NULL) {
      add_symbol(block, ST_VARIABLE, clause->symbol_name)
        ->type = copy_type(define_type);

      if (clause->value) {
        analyze_expression(block, clause->value);

        expression *cast = implicit_cast(clause->value, define_type);
        clause->value = cast;
      }

      clause = clause->next;
    }
  } break;
  case S_IF: {
    if_statement *if_s = (if_statement*) *node;
    analyze_expression(block, if_s->condition);
    assert_condition(if_s->condition->type);

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
    assert_condition(loop->condition->type);

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
      case S_DEFINE:
      case S_EXPRESSION:
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
return_loop_escape:;

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
  case S_CLASS: {
    class *class = ((class_statement*) *node)->classtype;
    for (field *item = class->field; item; item = item->next) {
      resolve_type(block, item->field_type);
    }
  } break;
  }
}

void analyze(block_statement *block) {
  // build the hoisted tables
  for (statement *node = block->body; node != NULL; node = node->next) {
    switch (node->type) {
    case S_CLASS: {
      class_statement *c = (class_statement*) node;
      add_symbol(block, ST_TYPE, c->symbol_name)
        ->type = new_object_type(c->classtype);
      add_symbol(block, ST_CLASS, c->symbol_name)
        ->classtype = c->classtype;
    } break;

    // TODO: functions should be *immutable* variables, not separate
    // as such, the function should be declared as const and then statically
    // resolved in appropriate scopes
    case S_FUNCTION: {
      function_statement *f = (function_statement*) node;
      add_symbol(block, ST_VARIABLE, f->symbol_name)
        ->type = new_blockref_type(f->function);
      add_symbol(block, ST_FUNCTION, f->symbol_name)
        ->function = f->function;
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
