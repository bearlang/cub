#include <stdio.h>
#include <string.h>

#include "xalloc.h"
#include "generate.h"

// GENERATION UTILITIES

type *instruction_type(code_block *parent, size_t src) {
  size_t params = parent->parameter_count;
  if (params > src) {
    return parent->parameters[src].field_type;
  }
  src -= params;
  if (parent->instruction_count > src) {
    return parent->instructions[src].type;
  }
  fprintf(stderr, "no such instruction, cannot get type\n");
  return NULL;
}

static code_struct *add_struct(code_system *system) {
  resize(system->struct_count, &system->struct_cap, (void**) &system->structs,
    sizeof(code_struct));

  code_struct *cstruct = xmalloc(sizeof(*cstruct));
  system->structs[system->struct_count++] = cstruct;
  return cstruct;
}

static symbol_entry *add_symbol(code_block *block, const char *symbol_name,
    size_t instruction) {
  symbol_entry **entry = &block->symbol_head;

  for (; *entry; entry = &(*entry)->next) {
    if (strcmp(symbol_name, (*entry)->symbol_name) == 0) {
      fprintf(stderr, "symbol '%s' already defined\n", symbol_name);
      exit(1);
    }
  }

  block->symbol_count++;

  *entry = xmalloc(sizeof(symbol_entry));
  (*entry)->instruction = instruction;
  (*entry)->exists = true;
  (*entry)->next = NULL;
  (*entry)->symbol_name = (char*) symbol_name;
  (*entry)->type = NULL;
  return *entry;
}

static type *resolve_type(code_system *system, type *t) {
  if (t->type == T_ARRAY) {
    resolve_type(system, t->arraytype);
  } else if (t->type == T_OBJECT) {
    t->struct_index = t->classtype->struct_index;
  }
  return t;
}

static type *get_object_type(size_t struct_index) {
  type *t = xmalloc(sizeof(*t));
  t->type = T_OBJECT;
  t->struct_index = struct_index;
  return t;
}

static size_t get_return_struct(code_system *system, type *return_type) {
  if (is_void(return_type)) {
    return 0;
  }

  size_t count = system->struct_count;
  code_struct *return_struct;

  for (size_t i = 0; i < count; i++) {
    return_struct = get_code_struct(system, i);
    if (return_struct->field_count != 1) {
      continue;
    }

    code_field *first_field = &return_struct->fields[0];
    type *first = first_field->field_type;

    if (first->type != T_BLOCKREF || !first->blocktype ||
        !first->blocktype->next || first->blocktype->next->next) {
      continue;
    }

    type *second = first->blocktype->next->argument_type;
    first = first->blocktype->argument_type;

    if (first->type == T_OBJECT && first->struct_index == i &&
        equivalent_type(return_type, second)) {
      return i;
    }
  }

  return_struct = add_struct(system);
  return_struct->field_count = 1;
  return_struct->fields = xmalloc(sizeof(code_field));
  return_struct->fields[0].field_type = new_type(T_BLOCKREF);

  argument *blocktype = xmalloc(sizeof(*blocktype));
  blocktype->symbol_name = NULL;
  blocktype->argument_type = get_object_type(count);
  return_struct->fields[0].field_type->blocktype = blocktype;

  blocktype->next = xmalloc(sizeof(argument));
  blocktype = blocktype->next;
  blocktype->symbol_name = NULL;
  blocktype->argument_type = copy_type(return_type);
  blocktype->next = NULL;

  return count;
}

static void push_stack_from(code_block *block, size_t src) {
  instruction_node *node = xmalloc(sizeof(*node));
  node->instruction = src;
  node->next = block->stack_head;
  block->stack_head = node;
  block->stack_size++;
}

static void push_stack(code_block *block) {
  push_stack_from(block, last_instruction(block));
}

static size_t pop_stack(code_block *block) {
  instruction_node *node = block->stack_head;

  if (node == NULL) {
    fprintf(stderr, "no stack to pop\n");
    exit(1);
  }

  size_t instruction = node->instruction;
  block->stack_head = node->next;
  block->stack_size--;
  free(node);

  return instruction;
}

static size_t peek_stack(code_block *block, size_t from_end) {
  instruction_node *node = block->stack_head;

  if (!node) {
    fprintf(stderr, "no stack to peek at %zu\n", from_end);
    exit(1);
  }

  for (size_t i = 0; i < from_end; i++) {
    node = node->next;
    if (!node) {
      fprintf(stderr, "not enough stack to peek at %zu\n", from_end);
      exit(1);
    }
  }

  return node->instruction;
}

static void mirror_instruction(code_block *parent, size_t src) {
  code_instruction *mirror = new_instruction(parent, 1);
  mirror->operation.type = O_GET_SYMBOL;
  mirror->type = copy_type(instruction_type(parent, src));
  mirror->parameters[0] = src;
}

// HEAD

static code_block *generate_block(code_block*, block_statement*);
static void generate_control(code_block*, control_statement*);
static code_block *generate_define(code_block*, define_statement*);
static code_block *generate_let(code_block*, let_statement*);
static code_block *generate_loop(code_block*, loop_statement*);
static code_block *generate_if(code_block*, if_statement*);
static void generate_return(code_block*, return_statement*);
static code_block *generate_cast(code_block*, expression*);
static code_struct *generate_class(code_system*, class*);
static code_struct *generate_class_stub(code_system*, class*);
static code_block *generate_function(code_system*, function*);
static code_block *generate_function_stub(code_system*, function*);
static code_block *generate_expression(code_block*, expression*);
static code_block *generate_call(code_block*, expression*);
static code_block *generate_linear(code_block*, expression*,
  size_t ref_param_count);
static code_block *generate_new(code_block*, expression*);
static code_block *generate_ternary(code_block*, expression*);

// STATEMENT-RELATED GENERATION

static code_block *generate_block(code_block *parent, block_statement *block) {
  for (symbol_entry *c = block->class_head; c; c = c->next) {
    generate_class_stub(parent->system, c->classtype);
  }

  for (symbol_entry *f = block->function_head; f; f = f->next) {
    generate_function_stub(parent->system, f->function);
  }

  for (symbol_entry *c = block->class_head; c; c = c->next) {
    generate_class(parent->system, c->classtype);
  }

  for (symbol_entry *f = block->function_head; f; f = f->next) {
    generate_function(parent->system, f->function);
  }

  for (statement *node = block->body; node; node = node->next) {
    switch (node->type) {
    case S_BLOCK: {
      code_block *body = fork_block(parent);
      body = generate_block(body, (block_statement*) node);
      parent = rejoin_block(parent, body);
    } break;
    case S_BREAK:
    case S_CONTINUE:
      generate_control(parent, (control_statement*) node);
      parent = NULL;
      break;
    case S_DEFINE:
      parent = generate_define(parent, (define_statement*) node);
      break;
    case S_LET:
      parent = generate_let(parent, (let_statement*) node);
      break;
    case S_DO_WHILE:
    case S_WHILE:
      parent = generate_loop(parent, (loop_statement*) node);
      break;
    case S_EXPRESSION: {
      expression_statement *express = (expression_statement*) node;
      parent = generate_expression(parent, express->value);
    } break;
    case S_IF:
      parent = generate_if(parent, (if_statement*) node);
      break;
    case S_RETURN:
      generate_return(parent, (return_statement*) node);
      parent = NULL;
      break;
    // already processed
    case S_CLASS:
    case S_FUNCTION:
    case S_TYPEDEF:
      break;
    }

    // most recently processed statement terminates abruptly
    if (parent == NULL) {
      if (node->next) {
        fprintf(stderr, "not reachable\n");
      }

      return NULL;
    }
  }

  return parent;
}

static code_block *generate_define(code_block *parent,
    define_statement *define) {
  type *define_type = define->symbol_type;
  define_clause *clause = define->clause;

  do {
    size_t value = 0;

    if (clause->value) {
      parent = generate_expression(parent, clause->value);
      value = last_instruction(parent);
    }

    symbol_entry *entry = add_symbol(parent, clause->symbol_name, value);
    entry->type = resolve_type(parent->system, copy_type(define_type));
    entry->exists = !!clause->value;

    clause = clause->next;
  } while (clause);

  return parent;
}

static code_block *generate_let(code_block *parent, let_statement *let) {
  code_system *system = parent->system;

  define_clause *clause = let->clause;

  do {
    parent = generate_expression(parent, clause->value);
    symbol_entry *entry = add_symbol(parent, clause->symbol_name,
      last_instruction(parent));
    entry->type = resolve_type(system, copy_type(clause->value->type));
    entry->exists = true;

    clause = clause->next;
  } while (clause);

  return parent;
}

static void generate_control(code_block *parent, control_statement *control) {
  loop_statement *target = control->target;
  if (target->type == S_DO_WHILE) {
    block_node **root = control->type == S_BREAK
      ? &target->break_node
      : &target->continue_node;

    block_node *node = xmalloc(sizeof(*node));
    node->block = parent;
    node->next = *root;
    *root = node;
  } else {
    join_blocks(parent, control->type == S_BREAK
      ? target->post_block
      : target->condition_block);
  }
}

static code_block *generate_do_while(code_block *parent, loop_statement *loop) {
  size_t body_block = parent->system->block_count;
  code_block *body = fork_block(parent);

  if (loop->body) {
    body = generate_block(body, loop->body);
  }

  if (body) {
    block_node *node = xmalloc(sizeof(*node));
    node->block = body;
    node->next = loop->continue_node;
    loop->continue_node = node;
  }

  if (!loop->continue_node && !loop->break_node) {
    fprintf(stderr, "do-while condition not reachable\n");

    return NULL;
  }

  if (loop->continue_node) {
    code_block *condition = tangle_blocks(body, loop->continue_node);

    condition = generate_expression(condition, loop->condition);

    code_block *body_replay, *post;
    weave_blocks(parent, loop->break_node, condition, &body_replay, &post);

    join_blocks(body_replay, body_block);

    return post;
  }

  fprintf(stderr, "do-while condition not reachable\n");

  return tangle_blocks(parent, loop->break_node);
}

static code_block *generate_while(code_block *parent, loop_statement *loop) {
  if (loop->condition->operation.type == O_LITERAL &&
      loop->condition->type->type == T_BOOL && loop->condition->value_bool) {
    loop->type = S_DO_WHILE;
    return generate_do_while(parent, loop);
  }

  size_t condition_block = parent->system->block_count;
  code_block *condition = fork_block(parent);

  condition = generate_expression(condition, loop->condition);

  code_block *body, *post;
  branch_into(condition, &body, &post);

  // TODO: this is using undocumented behavior
  size_t /*body_block = parent->system->block_count - 2,
    */post_block = parent->system->block_count - 1;

  loop->condition_block = condition_block;
  loop->post_block = post_block;

  if (loop->body) {
    body = generate_block(body, loop->body);
  }

  if (body) {
    join_blocks(body, condition_block);
  }

  return post;
}

static code_block *generate_loop(code_block *parent, loop_statement *loop) {
  return loop->type == S_DO_WHILE
    ? generate_do_while(parent, loop)
    : generate_while(parent, loop);
}

static code_block *generate_if(code_block *parent, if_statement *branch) {
  parent = generate_expression(parent, branch->condition);

  code_block *first, *second;
  branch_into(parent, &first, &second);

  first = branch->first ? generate_block(first, branch->first) : first;
  second = branch->second ? generate_block(second, branch->second) : second;

  switch (((!first) << 1) | (!second)) {
  case 0: // both exist
    return merge_blocks(parent, first, second);
  case 1: // the first block exists
    return rejoin_block(parent, first);
  case 2: // the second block exists
    return rejoin_block(parent, second);
  case 3:
    return NULL;
  }

  fprintf(stderr, "if statement generation logical failure\n");
  abort();
}

static void generate_return(code_block *parent, return_statement *ret) {
  function *fn = ret->target;

  bool non_void = !is_void(fn->return_type);
  if (non_void == !ret->value) {
    fprintf(stderr, "return from void function with return or non-void without "
      "return\n");
    abort();
  }

  size_t param_count = 1 + non_void;

  if (non_void) {
    parent = generate_expression(parent, ret->value);
  }

  parent->tail.type = GOTO;
  parent->tail.parameter_count = param_count;
  parent->tail.parameters = xmalloc(sizeof(size_t) * param_count);

  if (non_void) {
    parent->tail.parameters[1] = last_instruction(parent);
  }

  parent->tail.parameters[0] = 0;
  parent->tail.first_block = next_instruction(parent);

  size_t return_index = instruction_type(parent, 0)->struct_index;
  code_struct *return_struct = get_code_struct(parent->system, return_index);
  type *return_block_type = return_struct->fields[0].field_type;

  code_instruction *unwrap = new_instruction(parent, 2);
  unwrap->operation.type = O_GET_FIELD;
  unwrap->type = copy_type(return_block_type);
  unwrap->parameters[0] = 0; // return structs are always the first argument
  unwrap->parameters[1] = 0; // blockref position in all return structs
}

// STRUCTURE AND CLASS GENERATION

static code_struct *generate_class(code_system *system, class *the_class) {
  code_struct *cstruct = get_code_struct(system, the_class->struct_index);
  cstruct->field_count = the_class->field_count;

  if (the_class->field_count) {
    cstruct->fields = xmalloc(sizeof(code_field) * the_class->field_count);

    size_t index = 0;
    for (field *entry = the_class->field; entry; entry = entry->next) {
      type *field_type = resolve_type(system, copy_type(entry->field_type));
      cstruct->fields[index].field_type = field_type;
      ++index;
    }
  }

  return cstruct;
}

static code_struct *generate_class_stub(code_system *system, class *the_class) {
  the_class->struct_index = system->struct_count;
  code_struct *cstruct = add_struct(system);
  cstruct->field_count = 0;
  return cstruct;
}

static code_block *generate_function(code_system *system, function *fn) {
  code_block *start_block = get_code_block(system, fn->block_body);

  code_block *block = generate_block(start_block, fn->body);

  if (block) {
    if (fn->return_type && fn->return_type->type != T_VOID) {
      fprintf(stderr, "execution reaches end of non-void function '%s'\n",
        fn->function_name);
      exit(1);
    }

    // to handle exiting a void function
    return_statement ret = {
      .type = S_RETURN,
      .next = NULL,
      .parent = (statement*) fn->body,
      .value = NULL,
      .target = fn
    };

    generate_return(block, &ret);
  }

  return start_block;
}

static code_block *generate_function_stub(code_system *system, function *fn) {
  size_t struct_index = get_return_struct(system, fn->return_type);

  fn->return_struct = struct_index;

  fn->block_body = system->block_count;
  code_block *start_block = create_block(system);

  size_t handoff_size = fn->argument_count + 1;
  start_block->parameter_count = handoff_size;
  start_block->parameters = xmalloc(sizeof(code_field) * handoff_size);

  start_block->parameters[0].field_type = get_object_type(struct_index);
  start_block->has_return = true;
  start_block->return_instruction = 0;

  size_t i = 0;
  for (argument *arg = fn->argument; arg; arg = arg->next) {
    type *arg_type = resolve_type(system, copy_type(arg->argument_type));
    start_block->parameters[++i].field_type = arg_type;
    symbol_entry *entry = add_symbol(start_block, arg->symbol_name, i);
    entry->type = copy_type(arg_type);
  }

  return start_block;
}

// EXPRESSION GENERATION

static code_block *generate_expression(code_block *parent, expression *value) {
  switch (value->operation.type) {
  case O_BITWISE_NOT:
  case O_NOT:
  case O_GET_LENGTH:
    return generate_linear(parent, value, 1);
  case O_BLOCKREF: {
    code_instruction *ref = add_instruction(parent);
    ref->operation.type = O_BLOCKREF;
    type *return_type = value->type->blocktype->argument_type;
    size_t return_struct = get_return_struct(parent->system, return_type);
    type *struct_type = get_object_type(return_struct);
    ref->type = new_type(T_BLOCKREF);
    argument *struct_arg = xmalloc(sizeof(*struct_arg));
    struct_arg->symbol_name = NULL;
    struct_arg->argument_type = struct_type;
    struct_arg->next = copy_arguments(value->type->blocktype->next, false);
    ref->type->blocktype = struct_arg;
    ref->block_index = value->function->block_body;
    return parent;
  }
  case O_NEGATE: {
    parent = generate_linear(parent, value, 1);
    code_instruction *last = &parent->instructions[last_instruction(parent) -
      parent->parameter_count];
    switch (last->type->type) {
    case T_U8: last->type->type = T_S8; break;
    case T_U16: last->type->type = T_S16; break;
    case T_U32: last->type->type = T_S32; break;
    case T_U64: last->type->type = T_S64; break;
    default: break;
    }
    return parent;
  }
  case O_CALL:
    return generate_call(parent, value);
  case O_CAST:
    return generate_cast(parent, value);
  case O_COMPARE:
  case O_GET_INDEX: // TODO: bounds checking
  case O_IDENTITY:
  case O_NUMERIC:
  case O_SHIFT:
  case O_STR_CONCAT:
    return generate_linear(parent, value, 2);
  case O_GET_FIELD: {
    parent = generate_expression(parent, value->value);

    class *class = value->value->type->classtype;
    code_struct *target = get_code_struct(parent->system, class->struct_index);

    code_instruction *get = new_instruction(parent, 2);
    get->operation.type = O_GET_FIELD;
    get->type = copy_type(target->fields[value->field_index].field_type);
    get->parameters[0] = last_instruction(parent) - 1;
    get->parameters[1] = value->field_index;

    return parent;
  }
  case O_GET_SYMBOL: {
    const char *symbol = value->symbol_name;

    for (symbol_entry *entry = parent->symbol_head; entry;
        entry = entry->next) {
      if (strcmp(entry->symbol_name, symbol) == 0) {
        if (!entry->exists) {
          fprintf(stderr, "symbol '%s' not initialized\n", symbol);
          exit(1);
        }

        mirror_instruction(parent, entry->instruction);
        return parent;
      }
    }

    // crap
    fprintf(stderr, "no symbol '%s' in get-symbol post-analysis\n", symbol);
    abort();
  }
  case O_INSTANCEOF:
    abort();
  case O_LITERAL: {
    code_instruction *literal = add_instruction(parent);
    literal->operation.type = O_LITERAL;
    literal->type = copy_type(value->type);
    switch (literal->type->type) {
    case T_BOOL: literal->value_bool = value->value_bool; break;
    case T_STRING: literal->value_string = value->value_string; break;
    case T_U8: literal->value_u8 = value->value_u8; break;
    case T_U16: literal->value_u16 = value->value_u16; break;
    case T_U32: literal->value_u32 = value->value_u32; break;
    case T_U64: literal->value_u64 = value->value_u64; break;
    case T_OBJECT:
      // gotta be null
      break;
    default:
      fprintf(stderr, "literal handoff not implemented for this type\n");
      exit(1);
    }

    return parent;
  }
  case O_NATIVE: {
    for (expression *val = value->value; val; val = val->next) {
      parent = generate_expression(parent, val);
      push_stack(parent);
    }

    code_instruction *native = new_instruction(parent, value->arg_count + 1);
    native->operation.type = O_NATIVE;
    native->type = copy_type(value->type);
    native->native_call = value->symbol_name;

    size_t i = value->arg_count;
    native->parameters[0] = value->arg_count;
    for (expression *val = value->value; val; val = val->next) {
      native->parameters[i] = pop_stack(parent);
      i--;
    }
    return parent;
  }
  case O_NEW:
    return generate_new(parent, value);
  case O_NEW_ARRAY: {
    parent = generate_expression(parent, value->value);

    code_instruction *new = new_instruction(parent, 1);
    new->operation.type = O_NEW_ARRAY;
    new->type = copy_type(value->type);
    new->parameters[0] = last_instruction(parent) - 1;
    return parent;
  }
  case O_NUMERIC_ASSIGN:
  case O_POSTFIX:
  case O_SHIFT_ASSIGN:
  case O_STR_CONCAT_ASSIGN: {
    expression *left = value->value, *right = value->value->next;

    switch (left->operation.type) {
    case O_GET_FIELD:
      parent = generate_expression(parent, left);

      // object
      push_stack_from(parent, last_instruction(parent) - 1);

      // old field value
      push_stack(parent);
      break;
    case O_GET_INDEX: {
      // array
      parent = generate_expression(parent, left->value);
      push_stack(parent);

      // index
      parent = generate_expression(parent, left->value->next);
      push_stack(parent);

      // TODO: bounds checking

      // array value
      code_instruction *get = new_instruction(parent, 2);
      get->operation.type = O_GET_INDEX;
      get->type = left->type;
      get->parameters[0] = peek_stack(parent, 1);
      get->parameters[1] = peek_stack(parent, 0);
      push_stack(parent);
    } break;
    case O_GET_LENGTH: {
      // TODO: ensure we won't get a string here
      parent = generate_expression(parent, left);

      // array
      push_stack_from(parent, last_instruction(parent) - 1);

      // old length
      push_stack(parent);
    } break;
    case O_GET_SYMBOL:
      // old value
      parent = generate_expression(parent, left);
      push_stack(parent);
      break;
    default:
      abort();
    }

    // new value/right value
    if (value->operation.type == O_POSTFIX) {
      code_instruction *literal = add_instruction(parent), *cast = NULL;
      literal->operation.type = O_LITERAL;
      literal->type = new_type(T_U8);
      literal->value_u8 = 1;

      switch (literal->type->type) {
      case T_S8:
        cast = new_instruction(parent, 1);
        cast->operation.cast_type = O_REINTERPRET;
        break;
      case T_S16:
      case T_S32:
      case T_S64:
      case T_U16:
      case T_U32:
      case T_U64:
        cast = new_instruction(parent, 1);
        cast->operation.cast_type = O_ZERO_EXTEND;
        break;
      case T_F32:
      case T_F64:
      case T_F128:
        cast = new_instruction(parent, 1);
        cast->operation.cast_type = O_UNSIGNED_TO_FLOAT;
      case T_U8:
        break;
      default:
        // we only accept numeric types
        abort();
      }

      if (cast) {
        cast->operation.type = O_CAST;
        cast->type = copy_type(value->value->type);
        cast->parameters[0] = last_instruction(parent) - 1;
      }
    } else {
      parent = generate_expression(parent, right);
    }

    code_instruction *compute = new_instruction(parent, 2);
    compute->type = copy_type(value->type);
    compute->parameters[0] = pop_stack(parent);
    compute->parameters[1] = last_instruction(parent) - 1;

    size_t mirror = last_instruction(parent);
    switch (value->operation.type) {
    case O_NUMERIC_ASSIGN:
      compute->operation.type = O_NUMERIC;
      compute->operation.numeric_type = value->operation.numeric_type;
      break;
    case O_SHIFT_ASSIGN:
      compute->operation.type = O_SHIFT;
      compute->operation.shift_type = value->operation.shift_type;
      break;
    case O_STR_CONCAT_ASSIGN:
      compute->operation.type = O_STR_CONCAT;
      break;
    case O_POSTFIX:
      compute->operation.type = O_NUMERIC;
      compute->operation.numeric_type =
        value->operation.postfix_type == O_INCREMENT ? O_ADD : O_SUB;
      mirror = compute->parameters[0];
      break;
    default:
      abort();
    }

    switch (left->operation.type) {
    case O_GET_FIELD: {
      code_instruction *set = new_instruction(parent, 3);
      set->operation.type = O_SET_FIELD;
      set->type = NULL;
      set->parameters[0] = pop_stack(parent);
      set->parameters[1] = left->field_index;
      set->parameters[2] = last_instruction(parent) - 1;

      mirror_instruction(parent, mirror);
    } break;
    case O_GET_INDEX: {
      // TODO: bounds checking
      code_instruction *set = new_instruction(parent, 3);
      set->operation.type = O_SET_INDEX;
      set->type = NULL;
      set->parameters[2] = last_instruction(parent) - 1;
      set->parameters[1] = pop_stack(parent); // index
      set->parameters[0] = pop_stack(parent); // array

      mirror_instruction(parent, mirror);
    } break;
    case O_GET_LENGTH: {
      code_instruction *set = new_instruction(parent, 2);
      set->operation.type = O_SET_LENGTH;
      set->type = NULL;
      set->parameters[0] = pop_stack(parent);
      set->parameters[1] = last_instruction(parent) - 1;

      mirror_instruction(parent, mirror);
    } break;
    case O_GET_SYMBOL: {
      const char *symbol = left->symbol_name;

      for (symbol_entry *entry = parent->symbol_head; entry;
          entry = entry->next) {
        if (strcmp(entry->symbol_name, symbol) == 0) {
          entry->instruction = last_instruction(parent);

          // only need to mirror when we're referencing a previous value because
          // we're just updating the entry's instruction
          if (value->operation.type == O_POSTFIX) {
            mirror_instruction(parent, mirror);
          }
          return parent;
        }
      }

      fprintf(stderr, "no symbol '%s' in get-symbol post-analysis\n", symbol);
      // fallthrough
    }
    default:
      abort();
    }
  } return parent;
  case O_SET_FIELD: {
    parent = generate_expression(parent, value->value);
    push_stack(parent);

    parent = generate_expression(parent, value->value->next);

    code_instruction *set = new_instruction(parent, 3);
    set->operation.type = O_SET_FIELD;
    set->type = NULL;
    set->parameters[0] = pop_stack(parent);
    set->parameters[1] = value->field_index;
    set->parameters[2] = last_instruction(parent) - 1;

    mirror_instruction(parent, last_instruction(parent) - 1);

    return parent;
  }
  case O_SET_INDEX: {
    parent = generate_expression(parent, value->value);
    push_stack(parent);

    parent = generate_expression(parent, value->value->next);
    push_stack(parent);

    // TODO: bounds checking

    parent = generate_expression(parent, value->value->next->next);

    code_instruction *set = new_instruction(parent, 3);
    set->operation.type = O_SET_INDEX;
    set->type = NULL;
    set->parameters[2] = pop_stack(parent);
    set->parameters[0] = pop_stack(parent);
    set->parameters[1] = last_instruction(parent) - 1;

    mirror_instruction(parent, last_instruction(parent) - 1);

    return parent;
  }
  case O_SET_LENGTH: {
    parent = generate_expression(parent, value->value);
    push_stack(parent);

    parent = generate_expression(parent, value->value->next);

    code_instruction *set = new_instruction(parent, 2);
    set->operation.type = O_SET_LENGTH;
    set->type = NULL;
    set->parameters[0] = pop_stack(parent);
    set->parameters[1] = last_instruction(parent) - 1;

    mirror_instruction(parent, last_instruction(parent) - 1);

    return parent;
  }
  case O_SET_SYMBOL: {
    parent = generate_expression(parent, value->value);

    const char *symbol = value->symbol_name;

    for (symbol_entry *entry = parent->symbol_head; entry;
        entry = entry->next) {
      if (strcmp(entry->symbol_name, symbol) == 0) {
        entry->exists = true;
        entry->instruction = last_instruction(parent);
        return parent;
      }
    }

    fprintf(stderr, "no symbol '%s' in set-symbol post-analysis\n", symbol);
    abort();
  }
  case O_TERNARY:
    return generate_ternary(parent, value);

  case O_FUNCTION:
    generate_function_stub(parent->system, value->function);
    generate_function(parent->system, value->function);
    add_blockref(parent, value->function->block_body);
    return parent;

  // gets translated to control flow in analysis
  case O_LOGIC:
    abort();
  }

  // if we get here, we've got some memory corruption
  fprintf(stderr, "memory corrupted, invalid operation type\n");
  abort();
}

// TODO: don't add symbols with false exist flag to the context struct
static code_block *generate_call(code_block *parent, expression *value) {
  code_system *system = parent->system;
  type *fn_type = value->value->type,
    *return_type = fn_type->blocktype->argument_type;
  bool non_void = !is_void(fn_type->blocktype->argument_type);

  code_instruction *get, *set;

  // create placeholder return struct
  size_t return_struct = get_return_struct(system, return_type);

  size_t return_block_index = system->block_count;

  // create the return block
  size_t param_count = non_void ? 2 : 1;
  code_block *return_block = create_block(system);
  return_block->parameter_count = param_count;
  return_block->parameters = xmalloc(sizeof(code_field) * param_count);
  return_block->symbol_count = parent->symbol_count;
  return_block->stack_size = parent->stack_size;

  return_block->parameters[0].field_type = get_object_type(return_struct);
  if (non_void) {
    return_block->parameters[1].field_type = copy_type(return_type);
  }

  // generate and stack call expressions, including the callee
  size_t value_count = 0;
  for (expression *node = value->value; node; node = node->next) {
    parent = generate_expression(parent, node);
    push_stack(parent);
    value_count++;
  }

  // don't include the callee
  value_count--;

  // verify the parameter count is correct
  /*if (value_count != fn->argument_count) {
    fprintf(stderr, "wrong parameter count to '%s' post-analysis\n",
      fn->function_name);
    abort();
  }*/

  // pop call expressions and add them to the call block's tail
  size_t handoff_size = value_count + 1;

  parent->tail.type = GOTO;
  parent->tail.parameter_count = handoff_size;
  parent->tail.parameters = xmalloc(sizeof(size_t) * handoff_size);

  for (size_t i = value_count; i >= 1; i--) {
    parent->tail.parameters[i] = pop_stack(parent);
  }

  // the instruction representing the callee
  size_t callee_instruction = pop_stack(parent);

  // create the context struct
  size_t context_size = parent->has_return + parent->symbol_count +
    parent->stack_size;

  size_t context_index;
  code_struct *context_struct;
  if (context_size == 0) {
    context_index = return_struct;
  } else {
    size_t field_count = context_size + 1;

    context_index = system->struct_count;

    context_struct = add_struct(system);
    context_struct->field_count = field_count;
    context_struct->fields = xmalloc(sizeof(code_field) * field_count);

    context_struct->fields[0].field_type = get_blockref_type(return_block);
  }

  // add NEW instruction for context struct to call block
  code_instruction *new = new_instruction(parent, 1);
  new->operation.type = O_NEW;
  new->type = get_object_type(context_index);
  new->parameters[0] = context_index;

  // store a reference to the object's instruction
  size_t object = last_instruction(parent);

  // grab blockref to return block and store in context object
  get = add_instruction(parent);
  get->operation.type = O_BLOCKREF;
  get->type = get_blockref_type(return_block);
  get->block_index = return_block_index;

  set = new_instruction(parent, 3);
  set->operation.type = O_SET_FIELD;
  set->type = NULL;
  set->parameters[0] = object;
  set->parameters[1] = 0;
  set->parameters[2] = last_instruction(parent) - 1;

  if (context_size > 0) {
    // add downcast instruction for context object to return block
    code_instruction *cast = new_instruction(return_block, 2);
    cast->operation.type = O_CAST;
    cast->operation.cast_type = O_DOWNCAST;
    cast->type = copy_type(new->type);
    cast->parameters[0] = 0;

    size_t return_context = last_instruction(return_block);

    // add outer context object into call block, context struct, and return block
    if (parent->has_return) {
      type *outer_return = instruction_type(parent, parent->return_instruction);
      context_struct->fields[1].field_type = copy_type(outer_return);

      set = new_instruction(parent, 3);
      set->operation.type = O_SET_FIELD;
      set->type = NULL;
      set->parameters[0] = object;
      set->parameters[1] = 1;
      set->parameters[2] = parent->return_instruction;

      get = new_instruction(return_block, 2);
      get->operation.type = O_GET_FIELD;
      get->type = copy_type(outer_return);
      get->parameters[0] = return_context;
      get->parameters[1] = 1;

      return_block->has_return = true;
      return_block->return_instruction = last_instruction(return_block);
    }

    size_t i = parent->has_return + 1;
    // add stack entries into the call block, context struct, and return block
    instruction_node **return_stack_tail = &return_block->stack_head;
    for (instruction_node *node = parent->stack_head; node; node = node->next) {
      type *node_type = instruction_type(parent, node->instruction);
      context_struct->fields[i].field_type = copy_type(node_type);

      // put the stack entry into the return struct
      set = new_instruction(parent, 3);
      set->operation.type = O_SET_FIELD;
      set->type = NULL;
      set->parameters[0] = object;
      set->parameters[1] = i;
      set->parameters[2] = node->instruction;

      // get the stack entry from the return struct
      get = new_instruction(return_block, 2);
      get->operation.type = O_GET_FIELD;
      get->type = copy_type(node_type);
      get->parameters[0] = return_context;
      get->parameters[1] = i;

      // add a stack entry in the return block
      instruction_node *return_entry = xmalloc(sizeof(*return_entry));
      return_entry->instruction = last_instruction(return_block);
      *return_stack_tail = return_entry;
      return_stack_tail = &return_entry->next;

      i++;
    }
    *return_stack_tail = NULL;

    // add symbol entries into the call block, context struct, and return block
    symbol_entry **symtail = &return_block->symbol_head;
    for (symbol_entry *entry = parent->symbol_head; entry; entry = entry->next) {
      context_struct->fields[i].field_type = copy_type(entry->type);

      // put the variable into the return struct
      set = new_instruction(parent, 3);
      set->operation.type = O_SET_FIELD;
      set->type = NULL;
      set->parameters[0] = object;
      set->parameters[1] = i;
      set->parameters[2] = entry->instruction;

      // get the variable from the return struct
      get = new_instruction(return_block, 2);
      get->operation.type = O_GET_FIELD;
      get->type = copy_type(entry->type);
      get->parameters[0] = return_context;
      get->parameters[1] = i;

      // add an entry to the symbol table representing the variable
      symbol_entry *return_entry = xmalloc(sizeof(*return_entry));
      return_entry->symbol_name = entry->symbol_name;
      return_entry->type = copy_type(entry->type);
      return_entry->exists = true;
      return_entry->instruction = last_instruction(return_block);
      *symtail = return_entry;
      symtail = &return_entry->next;

      i++;
    }
    *symtail = NULL;

    // add upcast instruction for context object to call block
    cast = new_instruction(parent, 2);
    cast->operation.type = O_CAST;
    cast->operation.cast_type = O_UPCAST;
    cast->type = get_object_type(return_struct);
    cast->parameters[0] = object;

    object = last_instruction(parent);
  }

  // finish call block tail with context struct and function block
  parent->tail.first_block = callee_instruction;
  parent->tail.parameters[0] = object;

  // grab return value from parameters
  if (non_void && context_size > 0) {
    mirror_instruction(return_block, 1);
  }

  return return_block;
}

static code_block *generate_cast(code_block *parent, expression *value) {
  switch (value->operation.cast_type) {
  case O_DOWNCAST:
    // TODO: add downcast check
  case O_FLOAT_EXTEND:
  case O_FLOAT_TO_SIGNED:
  case O_FLOAT_TO_UNSIGNED:
  case O_FLOAT_TRUNCATE:
  case O_REINTERPRET:
  case O_SIGN_EXTEND:
  case O_SIGNED_TO_FLOAT:
  case O_TRUNCATE:
  case O_UNSIGNED_TO_FLOAT:
  case O_UPCAST:
  case O_ZERO_EXTEND: {
    parent = generate_expression(parent, value->value);

    code_instruction *cast = new_instruction(parent, 1);
    cast->operation.type = O_CAST;
    cast->operation.cast_type = value->operation.cast_type;
    cast->type = copy_type(value->type);
    cast->parameters[0] = last_instruction(parent) - 1;
  } break;
  }

  return parent;
}

// TODO: is using the expression value chain good enough?
static code_block *generate_linear(code_block *parent, expression *value,
    size_t ref_param_count) {
  size_t param_count = 0;
  for (expression *p = value->value; p; p = p->next) {
    parent = generate_expression(parent, p);
    push_stack(parent);
    param_count++;
  }

  if (ref_param_count != param_count) {
    fprintf(stderr, "wrong linear parameter count post-analysis\n");
    abort();
  }

  code_instruction *ins = new_instruction(parent, param_count);
  ins->operation = value->operation;
  ins->type = copy_type(value->type);
  while (param_count-- > 0) {
    ins->parameters[param_count] = pop_stack(parent);
  }

  return parent;
}

// TODO: have a bunch of O_SET_FIELD instead of O_NEW parameters
static code_block *generate_new(code_block *parent, expression *value) {
  size_t param_count = 0;
  for (expression *p = value->value; p; p = p->next) {
    parent = generate_expression(parent, p);
    push_stack(parent);
    param_count++;
  }

  class *the_class = value->type->classtype;
  size_t field_count = the_class->field_count;

  if (param_count != field_count) {
    fprintf(stderr, "wrong parameter count to '%s' constructor post-analysis\n",
      the_class->class_name);
    abort();
  }

  code_instruction *new = new_instruction(parent, 1);
  new->operation.type = O_NEW;
  new->type = get_object_type(the_class->struct_index);
  new->parameters[0] = the_class->struct_index;

  size_t object = last_instruction(parent);

  for (size_t i = field_count; i > 0; ) {
    size_t value = pop_stack(parent);

    code_instruction *set = new_instruction(parent, 3);
    set->operation.type = O_SET_FIELD;
    set->parameters[0] = object;
    set->parameters[1] = --i;
    set->parameters[2] = value;
    set->type = copy_type(instruction_type(parent, value));
  }

  mirror_instruction(parent, object);

  return parent;
}

static code_block *generate_ternary(code_block *parent, expression *value) {
  parent = generate_expression(parent, value->value);

  code_block *first, *second;
  branch_into(parent, &first, &second);

  // generate the two expressions in their appropriate blocks, then push the
  // result values onto their respective stacks
  first = generate_expression(first, value->value->next);
  second = generate_expression(second, value->value->next->next);
  push_stack(first);
  push_stack(second);

  // merge_blocks will use the stack available in the first and second blocks,
  // and ignore the parent stack
  code_block *post = merge_blocks(parent, first, second);

  // that way the pop_stack can get the result of the first or second expression
  size_t result = pop_stack(post);
  mirror_instruction(post, result);

  return post;
}

// TOP-LEVEL GENERATION

code_system *generate(block_statement *root) {
  code_system *system = xmalloc(sizeof(*system));
  system->struct_count = 0;
  system->struct_cap = 0;
  system->structs = NULL;
  system->block_count = 0;
  system->block_cap = 0;
  system->blocks = NULL;

  code_struct *void_return_struct = add_struct(system);
  void_return_struct->field_count = 1;
  void_return_struct->fields = xmalloc(sizeof(code_field));
  void_return_struct->fields[0].field_type = new_type(T_BLOCKREF);

  argument *blocktype = xmalloc(sizeof(*blocktype));
  blocktype->symbol_name = NULL;
  blocktype->argument_type = get_object_type(0);
  blocktype->next = NULL;
  void_return_struct->fields[0].field_type->blocktype = blocktype;

  code_block *block = create_block(system);

  code_block *tail_block = generate_block(block, root);

  if (tail_block != NULL) {
    // TODO: add exit code when the final block exists
    tail_block->is_final = true;
  }

  return system;
}
