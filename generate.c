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

static code_block *add_block(code_system *system) {
  resize(system->block_count, &system->block_cap, (void**) &system->blocks,
    sizeof(code_block*));

  code_block *block = xmalloc(sizeof(*block));
  system->blocks[system->block_count++] = block;
  return block;
}

static code_struct *add_struct(code_system *system) {
  resize(system->struct_count, &system->struct_cap, (void**) &system->structs,
    sizeof(code_struct));

  code_struct *cstruct = xmalloc(sizeof(*cstruct));
  system->structs[system->struct_count++] = cstruct;
  return cstruct;
}

code_instruction *add_instruction(code_block *block) {
  resize(block->instruction_count, &block->instruction_cap,
    (void**) &block->instructions, sizeof(code_instruction));

  return &block->instructions[block->instruction_count++];
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

code_block *get_code_block(code_system *system, size_t block_index) {
  return system->blocks[block_index];
}

code_struct *get_code_struct(code_system *system, size_t struct_index) {
  return system->structs[struct_index];
}

static type *resolve_type(code_system *system, type *t) {
  if (t->type == T_ARRAY) {
    resolve_type(system, t->arraytype);
  } else if (t->type == T_OBJECT) {
    t->struct_index = t->classtype->struct_index;
  }
  return t;
}

static code_block *create_block(code_system *system) {
  code_block *block = add_block(system);
  block->system = system;
  block->parameter_count = 0;
  block->parameters = NULL;
  block->symbol_count = 0;
  block->symbol_head = NULL;
  block->stack_size = 0;
  block->stack_head = NULL;
  block->accepts_return = false;
  block->has_return = false;
  block->return_instruction = 0;
  block->instruction_count = 0;
  block->instruction_cap = 0;
  block->instructions = NULL;
  block->is_final = false;
  block->tail.parameter_count = 0;
  block->tail.parameters = NULL;
  return block;
}

static code_block *create_child_block(code_block *parent) {
  size_t params = parent->symbol_count + parent->stack_size +
    parent->has_return;

  code_block *block = add_block(parent->system);
  block->system = parent->system;
  block->parameter_count = params;
  block->parameters = NULL;
  block->symbol_count = parent->symbol_count;
  block->symbol_head = NULL;
  block->stack_size = parent->stack_size;
  block->stack_head = NULL;
  block->accepts_return = parent->has_return;
  block->has_return = parent->has_return;
  block->return_instruction = 0;
  block->instruction_count = 0;
  block->instruction_cap = 0;
  block->instructions = NULL;
  block->is_final = false;
  block->tail.parameter_count = 0;
  block->tail.parameters = NULL;

  if (params > 0) {
    block->parameters = xmalloc(sizeof(code_field) * params);

    if (parent->has_return) {
      type *ret_type = instruction_type(parent, parent->return_instruction);
      block->parameters[0].field_type = copy_type(ret_type);
    }

    size_t i = parent->has_return;
    symbol_entry *entry = parent->symbol_head, **tail = &block->symbol_head;
    for (; entry; i++, entry = entry->next) {
      block->parameters[i].field_type = copy_type(entry->type);
      symbol_entry *new = xmalloc(sizeof(*new));
      new->exists = true;
      new->instruction = i;
      new->symbol_name = entry->symbol_name;
      new->type = copy_type(entry->type);
      *tail = new;
      tail = &new->next;
    }
    *tail = NULL;

    instruction_node *node = parent->stack_head;
    for (; node; i++, node = node->next) {
      type *node_type = instruction_type(parent, node->instruction);
      block->parameters[i].field_type = copy_type(node_type);
    }

    i = 1;
    for (node = parent->stack_head; node; i++, node = node->next) {
      instruction_node *new = xmalloc(sizeof(*new));
      new->instruction = params - i;
      new->next = block->stack_head;
      block->stack_head = new;
    }
  }

  return block;
}

static type *get_blockref_type(code_block *block) {
  type *t = xmalloc(sizeof(*t));
  t->type = T_BLOCKREF;

  argument *blocktype = NULL;

  size_t params = block->parameter_count;
  if (params) {
    blocktype = xmalloc(sizeof(*blocktype));
    blocktype->symbol_name = NULL;
    blocktype->argument_type = copy_type(block->parameters[0].field_type);
    argument *tail = blocktype;
    for (size_t i = 1; i < params; i++) {
      argument *next = xmalloc(sizeof(*next));
      next->symbol_name = NULL;
      next->argument_type = copy_type(block->parameters[i].field_type);
      tail->next = next;
      tail = next;
    }
    tail->next = NULL;
  }

  t->blocktype = blocktype;

  return t;
}

static type *get_object_type(code_system *system, size_t struct_index) {
  type *t = xmalloc(sizeof(*t));
  t->type = T_OBJECT;
  t->struct_index = struct_index;
  return t;
}

// precondition: src has at least as many symbols as dest, and the common
// symbols are at the beginning of both symbol tables in the same order
static void set_block_tail_params(code_block *src, code_block *dest) {
  size_t params = dest->parameter_count;
  src->tail.parameter_count = params;
  src->tail.parameters = xmalloc(sizeof(size_t) * params);

  if (dest->accepts_return) {
    if (!src->has_return) {
      fprintf(stderr, "no return struct to forward\n");
      exit(1);
    }

    src->tail.parameters[0] = src->return_instruction;
  }

  size_t i = dest->accepts_return;
  for (symbol_entry *entry = dest->symbol_head, *from = src->symbol_head; entry;
      (entry = entry->next), (from = from->next)) {
    // TODO: verify precondition
    if (strcmp(entry->symbol_name, from->symbol_name) != 0) {
      fprintf(stderr, "symbol tables not aligned\n");
      exit(1);
    }

    src->tail.parameters[i++] = from->instruction;
  }

  for (instruction_node *node = src->stack_head; node; node = node->next) {
    src->tail.parameters[i] = node->instruction;
    i++;
  }

  if (i != params) {
    fprintf(stderr, "block tail params mismatch\n");
    exit(1);
  }
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
  blocktype->argument_type = get_object_type(system, count);
  return_struct->fields[0].field_type->blocktype = blocktype;

  blocktype->next = xmalloc(sizeof(argument));
  blocktype = blocktype->next;
  blocktype->symbol_name = NULL;
  blocktype->argument_type = copy_type(return_type);
  blocktype->next = NULL;

  return count;
}

static code_instruction *new_instruction(code_block *parent,
    size_t parameter_count) {
  code_instruction *ins = add_instruction(parent);
  ins->parameters = xmalloc(sizeof(size_t) * parameter_count);
  return ins;
}

static void make_blockref_index(code_block *src, size_t dest_block) {
  code_instruction *get = add_instruction(src);
  get->operation.type = O_BLOCKREF;
  get->type = get_blockref_type(get_code_block(src->system, dest_block));
  get->block_index = dest_block;
}

static size_t last_instruction(code_block *block) {
  return block->parameter_count + block->instruction_count - 1;
}

static size_t next_instruction(code_block *block) {
  return block->parameter_count + block->instruction_count;
}

static void goto_id(code_block *src, size_t dest) {
  make_blockref_index(src, dest);

  src->tail.type = GOTO;
  src->tail.first_block = last_instruction(src);
  set_block_tail_params(src, get_code_block(src->system, dest));
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
      size_t body_block_id = parent->system->block_count;
      code_block *body_block = create_child_block(parent);
      size_t result_block_id = parent->system->block_count;
      code_block *result_block = create_child_block(parent);

      goto_id(parent, body_block_id);
      body_block = generate_block(body_block, (block_statement*) node);
      goto_id(body_block, result_block_id);

      parent = result_block;
    } break;
    case S_BREAK:
    case S_CONTINUE:
      generate_control(parent, (control_statement*) node);
      parent = NULL;
      break;
    case S_DEFINE:
      parent = generate_define(parent, (define_statement*) node);
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

static void generate_control(code_block *parent, control_statement *control) {
  loop_statement *target = control->target;
  goto_id(parent, control->type == S_BREAK
    ? target->block_tail
    : target->block_head);
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

static code_block *generate_loop(code_block *parent, loop_statement *loop) {
  code_block *condition, *body, *post;

  size_t condition_block = parent->system->block_count;
  condition = create_child_block(parent);
  size_t body_block = parent->system->block_count;
  body = create_child_block(parent);
  post = create_child_block(parent);

  goto_id(parent, loop->type == S_WHILE ? condition_block : body_block);

  // available for the body generation
  loop->block_head = condition_block;
  loop->block_tail = condition_block + 2; // post

  code_block *condition_end = generate_expression(condition, loop->condition);

  condition_end->tail.type = BRANCH;
  condition_end->tail.condition = last_instruction(condition_end);
  set_block_tail_params(condition_end, post);

  code_instruction *ref;

  ref = add_instruction(condition_end);
  ref->operation.type = O_BLOCKREF;
  ref->type = get_blockref_type(body);
  ref->block_index = condition_block + 1; // body

  condition_end->tail.first_block = last_instruction(condition_end);

  ref = add_instruction(condition_end);
  ref->operation.type = O_BLOCKREF;
  ref->type = get_blockref_type(post);
  ref->block_index = condition_block + 2; // post

  condition_end->tail.second_block = last_instruction(condition_end);

  code_block *body_end = generate_block(body, loop->body);

  if (body_end != NULL) {
    goto_id(body_end, condition_block);
    loop->tail_used = true;
  }

  if (!loop->tail_used) {
    fprintf(stderr, "post-loop not reachable\n");
  }

  return post;
}

static code_block *generate_if(code_block *parent, if_statement *branch) {
  code_block *first, *second;

  size_t first_block = parent->system->block_count;
  first = create_child_block(parent);
  second = create_child_block(parent);

  parent = generate_expression(parent, branch->condition);

  parent->tail.type = BRANCH;
  parent->tail.parameter_count = parent->symbol_count;
  parent->tail.condition = last_instruction(parent);

  code_instruction *ref;

  ref = add_instruction(parent);
  ref->operation.type = O_BLOCKREF;
  ref->type = get_blockref_type(first);
  ref->block_index = first_block; // first

  parent->tail.first_block = last_instruction(parent);

  ref = add_instruction(parent);
  ref->operation.type = O_BLOCKREF;
  ref->type = get_blockref_type(second);
  ref->block_index = first_block + 1; // second

  parent->tail.second_block = last_instruction(parent);

  set_block_tail_params(parent, first);

  code_block *first_end, *second_end;
  first_end = branch->first ? generate_block(first, branch->first) : first;
  second_end = branch->second ? generate_block(second, branch->second) : second;

  // reachability check
  if (!first_end && !second_end) {
    return NULL;
  }

  size_t post_block = parent->system->block_count;
  code_block *post = create_child_block(parent);

  if (first_end) {
    goto_id(first_end, post_block);
  }

  if (second_end) {
    goto_id(second_end, post_block);
  }

  return post;
}

static void generate_return(code_block *parent, return_statement *ret) {
  function *fn = ret->target;

  bool non_void = !is_void(fn->return_type);
  if (non_void == !ret->value) {
    fprintf(stderr, "return from void function with return or non-void without "
      "return\n");
    abort();
  }

  size_t param_count = ret->value ? 2 : 1;

  parent->tail.type = GOTO;
  parent->tail.parameter_count = param_count;
  parent->tail.parameters = xmalloc(sizeof(size_t) * param_count);

  if (non_void) {
    parent = generate_expression(parent, ret->value);
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

  start_block->parameters[0].field_type = get_object_type(system, struct_index);
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
    return generate_linear(parent, value, 1);
  case O_BLOCKREF: {
    code_instruction *ref = add_instruction(parent);
    ref->operation.type = O_BLOCKREF;
    type *return_type = value->type->blocktype->argument_type;
    size_t return_struct = get_return_struct(parent->system, return_type);
    type *struct_type = get_object_type(parent->system, return_struct);
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
  case O_GET_INDEX:
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
    fprintf(stderr, "no symbol '%s' post-analysis\n", symbol);
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

      // array value
      code_instruction *get = new_instruction(parent, 2);
      get->operation.type = O_GET_INDEX;
      get->type = left->type;
      get->parameters[0] = peek_stack(parent, 1);
      get->parameters[1] = peek_stack(parent, 0);
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
      code_instruction *set = new_instruction(parent, 3);
      set->operation.type = O_SET_INDEX;
      set->type = NULL;
      set->parameters[2] = last_instruction(parent) - 1;
      set->parameters[1] = pop_stack(parent); // index
      set->parameters[0] = pop_stack(parent); // array

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

      fprintf(stderr, "YOU CHECKED THIS :(\n");
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
  case O_SET_SYMBOL: {
    // TODO: revert to using define instead of declare and expression otherwise
    // we'll have a lot of uninitialized symbol_entry.instruction problems
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

    fprintf(stderr, "symbol not found post-analysis\n");
    abort();
  }
  case O_TERNARY:
    return generate_ternary(parent, value);

  case O_FUNCTION:
    fprintf(stderr, "function expressions not implemented in code generation\n");
    exit(1);

  // gets translated to control flow in analysis
  case O_LOGIC:
    abort();
  }

  // if we get here, we've got some memory corruption
  fprintf(stderr, "memory corrupted, invalid operation type\n");
  abort();
}

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

  return_block->parameters[0].field_type = get_object_type(system,
    return_struct);
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
  new->type = get_object_type(system, context_index);
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
    cast->parameters[0] = cast->type->struct_index;
    cast->parameters[1] = 0;

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
      get->type = NULL;
      get->parameters[0] = return_context;
      get->parameters[1] = 1;
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
    cast->type = get_object_type(system, return_struct);
    cast->parameters[0] = return_struct;
    cast->parameters[1] = object;

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
  case O_UPCAST:
    fprintf(stderr, "class inheritance is not implemented, so object casting "
      "not supported\n");
    exit(1);
  case O_FLOAT_EXTEND:
  case O_FLOAT_TO_SIGNED:
  case O_FLOAT_TO_UNSIGNED:
  case O_FLOAT_TRUNCATE:
  case O_REINTERPRET:
  case O_SIGN_EXTEND:
  case O_SIGNED_TO_FLOAT:
  case O_TRUNCATE:
  case O_UNSIGNED_TO_FLOAT:
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
  new->type = get_object_type(parent->system, the_class->struct_index);
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

  size_t first_block = parent->system->block_count;
  code_block *first = create_child_block(parent);
  size_t second_block = parent->system->block_count;
  code_block *second = create_child_block(parent);

  parent->tail.type = BRANCH;
  parent->tail.condition = last_instruction(parent);
  set_block_tail_params(parent, first);
  make_blockref_index(parent, first_block);
  parent->tail.first_block = last_instruction(parent);
  make_blockref_index(parent, second_block);
  parent->tail.second_block = last_instruction(parent);

  first = generate_expression(first, value->value->next);
  push_stack(first);

  second = generate_expression(second, value->value->next->next);
  push_stack(second);

  size_t after_block = parent->system->block_count;
  code_block *after = create_child_block(first);

  goto_id(first, after_block);
  goto_id(second, after_block);

  size_t result = pop_stack(after);

  mirror_instruction(after, result);

  return after;
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
  blocktype->argument_type = get_object_type(system, 0);
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
