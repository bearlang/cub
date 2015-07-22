#include <stdio.h>
#include <string.h>

#include "analyze.h"
#include "xalloc.h"

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
      uint8_t symbol_type = ST_VARIABLE | ST_TYPE;
      symbol_entry *entry;

      // marks all variable dependencies while resolving the symbol
      entry = get_symbol(block, right->symbol_name, &symbol_type, ST_VARIABLE);

      if (symbol_type == ST_TYPE) {
        // TODO: support more than just T_OBJECT
        e->operation.type = O_INSTANCEOF;
        e->classtype = entry->type->classtype;
        free_expression(right);
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
      expression *cast = implicit_cast(e->value->next, e->value->type->arraytype);

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
    symbol_entry *entry = get_variable_symbol(block, e->symbol_name);

    if (e->operation.type == O_SET_SYMBOL) {
      if (entry->constant && entry->exists) {
        fprintf(stderr, "cannot reassign variable '%s'\n", e->symbol_name);
        exit(1);
      }

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
      literal = new_literal_node(T_S8);
      literal->value_s8 = 0;
      break;
    case T_S16:
      literal = new_literal_node(T_S16);
      literal->value_s16 = 0;
      break;
    case T_S32:
      literal = new_literal_node(T_S32);
      literal->value_s32 = 0;
      break;
    case T_S64:
      literal = new_literal_node(T_S64);
      literal->value_s64 = 0;
      break;
    case T_U8:
      literal = new_literal_node(T_U8);
      literal->value_u8 = 0;
      break;
    case T_U16:
      literal = new_literal_node(T_U16);
      literal->value_u16 = 0;
      break;
    case T_U32:
      literal = new_literal_node(T_U32);
      literal->value_u32 = 0;
      break;
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

    expression *b = bool_cast(e->value);

    if (b != e->value) {
      b->next = e->value->next;
      e->value->next = NULL;
      e->value = b;
    }

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
    assert_condition(loop->condition->type);

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

    analyze(body);
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
  case S_CLASS: {
    class *class = ((class_statement*) *node)->classtype;
    for (field *item = class->field; item; item = item->next) {
      resolve_type(item->field_type);
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
