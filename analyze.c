#include <stdio.h>
#include <string.h>

#include "analyze.h"
#include "xalloc.h"

typedef enum {
  ST_VARIABLE,
  ST_CLASS,
  ST_FUNCTION
} symbol_type;

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
  if (type->type != T_OBJECT) {
    assert_integer(type, "decide by");
  }
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

static symbol_entry *add_symbol(block_statement *block, symbol_type type,
    const char *symbol_name) {
  symbol_entry **entry, **tail;

  switch (type) {
  case ST_CLASS:
    entry = &block->class_head;
    tail = &block->class_tail;
    break;
  case ST_FUNCTION:
    entry = &block->function_head;
    tail = &block->function_tail;
    break;
  case ST_VARIABLE:
    entry = &block->variable_head;
    tail = &block->variable_tail;
    break;
  }

  for (; *entry; entry = &(*entry)->next) {
    if (strcmp(symbol_name, (*entry)->symbol_name) == 0) {
      fprintf(stderr, "symbol '%s' already defined\n", symbol_name);
      exit(1);
    }
  }

  *entry = xmalloc(sizeof(symbol_entry));
  (*entry)->next = NULL;
  (*entry)->symbol_name = (char*) symbol_name;
  *tail = *entry;
  return *entry;
}

static symbol_entry *get_symbol(block_statement *block, symbol_type type,
    const char *symbol_name) {
  for (;;) {
    symbol_entry *entry;

    switch (type) {
    case ST_CLASS: entry = block->class_head; break;
    case ST_FUNCTION: entry = block->function_head; break;
    case ST_VARIABLE: entry = block->variable_head; break;
    }

    for (; entry != NULL; entry = entry->next) {
      if (strcmp(symbol_name, entry->symbol_name) == 0) {
        return entry;
      }
    }

    statement *parent = block->parent;

    while (parent && parent->type != S_BLOCK) {
      switch (parent->type) {
      case S_FUNCTION:
        if (type == ST_VARIABLE) return NULL;
        // fallthrough
      case S_DO_WHILE:
      case S_WHILE:
      case S_IF:
        break;
      // shouldn't ever be a parent
      case S_BREAK:
      case S_CONTINUE:
      // case S_DECLARE:
      case S_DEFINE:
      // these shouldn't currently be a parent
      case S_CLASS:
      case S_EXPRESSION:
      case S_RETURN:
      // this should already be handled
      case S_BLOCK:
        abort();
      }

      parent = parent->parent;
    }

    if (parent == NULL) {
      return NULL;
    }

    block = (block_statement*) parent;
  }
}

// mutates
static type *resolve_type(block_statement *block, type *type) {
  // TODO: static types as properties (module support)
  switch (type->type) {
  case T_ARRAY:
    resolve_type(block, type->arraytype);
    return type;
  // TODO: support typedefs
  case T_REF:
  case T_OBJECT: {
    // TODO: this may fail because type objects are sometimes reassigned instead
    // of copied
    symbol_entry *entry = get_symbol(block, ST_CLASS, type->symbol_name);

    if (entry == NULL) {
      fprintf(stderr, "'%s' undeclared\n", type->symbol_name);
      exit(1);
    }

    free(type->symbol_name);

    type->type = T_OBJECT;
    type->classtype = entry->classtype;
    return type;
  }
  default:
    return type;
  }
}

static void analyze_expression(block_statement *block, expression *e) {
  switch (e->operation.type) {
  case O_BITWISE_NOT:
  case O_NEGATE:
    analyze_expression(block, e->value);

    assert_integer(e->value->type, e->operation.type == O_BITWISE_NOT
      ? "bitwise invert" : "negate");

    e->type = e->value->type;
    break;
  case O_CALL: {
    // TODO: dynamic function lookups
    // TODO: function types
    // TODO: function overloading and argument matching
    if (e->value->operation.type != O_GET_SYMBOL) {
      fprintf(stderr, "dynamic function calls not supported\n");
      exit(1);
    }

    char *fn_name = e->value->symbol_name;

    symbol_entry *entry = get_symbol(block, ST_FUNCTION, fn_name);

    if (entry == NULL) {
      fprintf(stderr, "function '%s' undeclared\n", fn_name);
      exit(1);
    }

    function *fn = entry->function;

    e->function = fn;
    e->type = fn->return_type;

    expression *argvalue = e->value->next;

    // e->value->type is not initialized - that's the job of the analyzer
    free(e->value);

    e->value = argvalue;

    argument *arg = fn->argument;
    size_t i = 0;
    while (argvalue && arg) {
      analyze_expression(block, argvalue);

      if (!compatible_type(arg->argument_type, argvalue->type)) {
        fprintf(stderr, "function '%s' called with bad argument %zi\n",
          fn_name, i);
        exit(1);
      }

      argvalue = argvalue->next;
      arg = arg->next;
      ++i;
    }

    if ((!argvalue) != (!arg)) {
      fprintf(stderr, "function '%s' called with wrong number of arguments\n",
        fn_name);
      exit(1);
    }

    break;
  }
  // type already declared
  case O_LITERAL:
    break;
  // type already declared
  case O_COMPARE:
  case O_IDENTITY:
  case O_LOGIC:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);
    break;
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
        free(name);
        e->type = field->field_type;
        e->field_index = i;
        return;
      }
    }

    fprintf(stderr, "'%s' has no field named '%s'\n", class->class_name, name);
    exit(1);
  }
  // TODO: O_GET_INDEX, O_SET_INDEX
  case O_GET_INDEX:
  case O_SET_INDEX:
    fprintf(stderr, "arrays not supported\n");
    exit(1);
  case O_GET_SYMBOL:
  case O_SET_SYMBOL: {
    symbol_entry *entry = get_symbol(block, ST_VARIABLE, e->symbol_name);

    if (entry == NULL) {
      fprintf(stderr, "'%s' undeclared\n", e->symbol_name);
      exit(1);
    }

    if (e->operation.type == O_SET_SYMBOL) {
      analyze_expression(block, e->value);
      if (!compatible_type(entry->type, e->value->type)) {
        fprintf(stderr, "incompatible types for '%s' assignment\n",
          e->symbol_name);
        exit(1);
      }
    }

    e->type = xmalloc(sizeof(type));
    *(e->type) = *(entry->type);
    break;
  }
  case O_NEW: {
    // TODO: constructor overloading and argument matching
    char *class_name = e->symbol_name;
    e->symbol_name = NULL;

    symbol_entry *entry = get_symbol(block, ST_CLASS, class_name);

    if (entry == NULL) {
      fprintf(stderr, "class '%s' undeclared\n", class_name);
      exit(1);
    }

    class *class = entry->classtype;

    e->type->classtype = class;

    expression *argvalue = e->value;
    field *field = class->field;
    size_t i = 0;
    while (argvalue && field) {
      analyze_expression(block, argvalue);

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
    break;
  case O_NUMERIC:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    assert_integer(e->value->type, "operate on");
    assert_integer(e->value->next->type, "operate on");

    e->type = xmalloc(sizeof(type));
    e->type->type = integer_promote(e->value->type, e->value->next->type);
    break;
  case O_NUMERIC_ASSIGN:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    assert_integer(e->value->type, "operate on");
    assert_integer(e->value->next->type, "operate on");

    // TODO: this should be in a common place closer to code generation,
    // specifically when things are more linear and (a = a + b) is the same as
    // (a += b)
    /*if (integer_id(left->type) < integer_id(right->type)) {
      fprintf(stderr, "warning: loss of precision in assignment\n");
    }*/

    e->type = e->value->type;
    break;
  case O_POSTFIX:
    analyze_expression(block, e->value);
    e->type = copy_type(e->value->type);

    assert_integer(e->value->type, e->operation.postfix_type == O_INCREMENT
      ? "increment" : "decrement");

    break;
  case O_SHIFT:
  case O_SHIFT_ASSIGN:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);

    assert_integer(e->value->type, "shift on");
    assert_integer(e->value->next->type, "shift with");

    e->type = e->value->type;
    break;
  case O_STR_CONCAT:
  case O_STR_CONCAT_ASSIGN:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);
    break;
  case O_TERNARY:
    analyze_expression(block, e->value);
    analyze_expression(block, e->value->next);
    analyze_expression(block, e->value->next->next);

    assert_condition(e->value->type);

    if (!compatible_type(e->value->next->type, e->value->next->next->type)) {
      fprintf(stderr, "ternary cannot decide between incompatible values\n");
      exit(1);
    }

    // TODO: promote types between the two cases
    e->type = e->value->next->type;
    break;
  case O_BLOCKREF:
  case O_CAST:
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
  /*case S_DEFINE: {
    define_statement *define = (define_statement*) *node;
    define_clause *clause = define->clause;
    statement *declare = NULL, *declare_tail = NULL;
    statement *assign = NULL, *assign_tail = NULL;

    type *define_type = resolve_type(block, define->symbol_type);

    while (clause != NULL) {
      add_symbol(block, ST_VARIABLE, clause->symbol_name)
        ->type = define_type;

      statement *decl = s_declare(define_type, clause->symbol_name);
      decl->parent = (statement*) block;
      if (declare == NULL) {
        declare = decl;
      } else {
        declare_tail->next = (statement*) decl;
      }
      declare_tail = decl;

      if (clause->value) {
        analyze_expression(block, clause->value);

        if (!compatible_type(define_type, clause->value->type)) {
          fprintf(stderr, "initialization clause type mismatch\n");
          exit(1);
        }

        statement *e = s_expression(new_assign_node(
          new_get_symbol_node(clause->symbol_name), clause->value));
        expression_statement *es = (expression_statement*) e;
        es->value->type = es->value->value->type;
        e->parent = (statement*) block;
        if (assign == NULL) {
          assign = e;
        } else {
          assign_tail->next = e;
        }
        assign_tail = e;
      }

      define_clause *tmp = clause->next;
      free(clause);
      clause = tmp;
    }
    *node = declare;
    if (assign == NULL) {
      declare_tail->next = define->next;
    } else {
      declare_tail->next = assign;
      assign_tail->next = define->next;
    }
    free(define);
    break;
  }*/
  case S_IF: {
    if_statement *if_s = (if_statement*) *node;
    analyze_expression(block, if_s->condition);
    assert_condition(if_s->condition->type);

    analyze(if_s->first);
    analyze(if_s->second);
    break;
  }
  case S_DO_WHILE:
  case S_WHILE: {
    loop_statement *loop = (loop_statement*) *node;
    analyze_expression(block, loop->condition);
    analyze(loop->body);
    break;
  }
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

    // TODO: also look in expressions
    analyze(fn->body);
    break;
  }
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
    break;
  }
  // case S_DECLARE:
  //   abort();
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
      break;
    }

    // TODO: functions should be *immutable* variables, not separate
    // as such, the function should be declared as const and then statically
    // resolved in appropriate scopes
    case S_FUNCTION: {
      function_statement *f = (function_statement*) node;
      add_symbol(block, ST_FUNCTION, f->symbol_name)
        ->function = f->function;
      break;
    }
    default:;
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
