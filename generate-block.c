#include <string.h>

#include "xalloc.h"
#include "generate.h"

static size_t count_blocks(block_node *head) {
  size_t i = 0;
  for (; head; head = head->next) i++;
  return i;
}

// symbols from parents
static code_block *child_block(code_block *context, block_node *parents,
    size_t parent_count) {
  // analyze common alive symbols from parents that work with context
  // use stack size from parents
  // use has_return from context

  code_block *child = create_block(context->system);

  size_t stack_size = parents->block->stack_size;
  size_t params = context->has_return + context->symbol_count + stack_size;

#ifdef DEBUG
  for (block_node *node = parents->next; node; node = node->next) {
    if (stack_size != node->block->stack_size) {
      fprintf(stderr, "unable to create child: incompatible stacks\n");
      exit(1);
    }
  }
#endif

  child->parameter_count = params;
  child->parameters = params ? xmalloc(sizeof(code_field) * params) : NULL;
  child->symbol_count = context->symbol_count;
  child->stack_size = stack_size;
  child->accepts_return = context->has_return;
  child->has_return = context->has_return;

  if (!params) {
    return child;
  }

  if (context->has_return) {
#ifdef DEBUG
    for (block_node *node = parents; node; node = node->next) {
      if (!node->block->has_return) {
        fprintf(stderr, "unable to create child: missing return struct\n");
        exit(1);
      }
    }
#endif

    child->return_instruction = 0;
    child->parameters[0].field_type = copy_type(instruction_type(context,
      context->return_instruction));
  }

  size_t i = context->has_return;
  if (context->symbol_count) {
    symbol_entry *context_entry = context->symbol_head;
    symbol_entry *parent_entries[parent_count];
    symbol_entry **child_tail = &child->symbol_head;

#ifdef DEBUG
    if (!context_entry) {
      fprintf(stderr, "code block symbol count does not match symbol table\n");
      exit(1);
    }
#endif

    size_t j = 0;
    for (block_node *node = parents; node; node = node->next) {
      parent_entries[j++] = node->block->symbol_head;
    }

    do {
      bool exists = true, couldexist = false;
      for (size_t j = 0; j < parent_count; j++) {
        symbol_entry *entry = parent_entries[j];
#ifdef DEBUG
        if (context_entry->upstream != entry->upstream) {
          fprintf(stderr, "unable to create child: mismatched symbol table\n");
          exit(1);
        }
#endif
        parent_entries[j] = entry->next;
        exists = exists && entry->exists;
        couldexist = couldexist || entry->exists;
      }

      if (!exists && couldexist) {
        // TODO: warn with code coordinates
        fprintf(stderr, "warning loss of value in symbol '%s'\n",
          context_entry->symbol_name);
      }

      child->parameters[i].field_type = copy_type(context_entry->type);

      symbol_entry *tail_entry = xmalloc(sizeof(*tail_entry));
      tail_entry->symbol_name = context_entry->symbol_name;
      tail_entry->upstream = context_entry->upstream;
      tail_entry->type = copy_type(context_entry->type);
      tail_entry->exists = exists;
      if (exists) {
        tail_entry->instruction = i;
        i++;
      }
      *child_tail = tail_entry;
      child_tail = &tail_entry->next;

      context_entry = context_entry->next;
    } while (context_entry);

    *child_tail = NULL;

    size_t alive_diff = context->has_return + context->symbol_count - i;
    if (alive_diff == params) {
      free(child->parameters);
      child->parameter_count = 0;
      child->parameters = NULL;

      return child;
    }

    if (alive_diff) {
      params -= alive_diff;

      child->parameter_count = params;
      child->parameters = xrealloc(child->parameters, sizeof(code_field) *
        params);
    }
  }

  if (i != params) {
    code_block *parent = parents->block;

    // these loops could be rolled together, but are kept separate for clarity
    for (instruction_node *node = parent->stack_head; node; node = node->next) {
      type *node_type = instruction_type(parent, node->instruction);
      child->parameters[i].field_type = copy_type(node_type);
      i++;
    }

    // also who knows maybe this one can be optimized
    size_t end = params - stack_size;
    for (i = params - 1; i >= end; i--) {
      instruction_node *new_node = xmalloc(sizeof(*new_node));
      new_node->instruction = i;
      new_node->next = child->stack_head;
      child->stack_head = new_node;
    }
  }

  return child;
}

static code_block *simple_child_block(code_block *parent) {
  block_node parent_head = {.block = parent, .next = NULL};
  return child_block(parent, &parent_head, 1);
}

static void inject_params(code_block *dest, code_block *src) {
  size_t params = dest->parameter_count;

  code_terminal *term = &src->tail;
  term->parameter_count = params;
  term->parameters = params ? xmalloc(sizeof(size_t) * params) : NULL;

  if (!params) return;

  if (dest->accepts_return) {
    term->parameters[0] = src->return_instruction;
  }

  size_t i = dest->accepts_return;

  symbol_entry *src_entry = src->symbol_head, *dest_entry = dest->symbol_head;
  while (dest_entry) {
    if (dest_entry->exists) {
      term->parameters[i++] = src_entry->instruction;
    }

    src_entry = src_entry->next;
    dest_entry = dest_entry->next;
  }

  for (instruction_node *node = dest->stack_head; node; node = node->next) {
    term->parameters[i++] = node->instruction;
  }
}

static void branch_block(code_block *parent, size_t first_block,
    size_t second_block) {
  size_t condition_instruction = last_instruction(parent);

  size_t first_instruction = next_instruction(parent);
  add_blockref(parent, first_block);
  size_t second_instruction = next_instruction(parent);
  add_blockref(parent, second_block);

  parent->tail.type = BRANCH;
  parent->tail.condition = condition_instruction;
  parent->tail.first_block = first_instruction;
  parent->tail.second_block = second_instruction;

  inject_params(get_code_block(parent->system, first_block), parent);
}

void join_blocks(code_block *parent, size_t dest_block) {
  code_block *dest = get_code_block(parent->system, dest_block);

  if (dest->accepts_return && !parent->has_return) {
    fprintf(stderr, "unable to join block: missing return struct\n");
    exit(1);
  }

  add_blockref(parent, dest_block);

  parent->tail.type = GOTO;
  parent->tail.first_block = last_instruction(parent);

  inject_params(dest, parent);
}

code_block *fork_block(code_block *parent) {
  size_t child_index = parent->system->block_count;
  code_block *child = simple_child_block(parent);
  join_blocks(parent, child_index);
  return child;
}

code_block *rejoin_block(code_block *context, code_block *inner) {
  size_t block_index = context->system->block_count;

  block_node join_head = {.block = inner, .next = NULL};
  code_block *block = child_block(context, &join_head, 1);

  join_blocks(inner, block_index);

  return block;
}

code_block *merge_blocks(code_block *context, code_block *first,
    code_block *second) {
  block_node merge_next = {.block = second, .next = NULL};
  block_node merge_head = {.block = first, .next = &merge_next};
  return tangle_blocks(context, &merge_head);
}

void branch_into(code_block *parent, code_block **first, code_block **second) {
  code_system *system = parent->system;

  size_t first_block = system->block_count;
  *first = simple_child_block(parent);
  size_t second_block = system->block_count;
  *second = simple_child_block(parent);

  branch_block(parent, first_block, second_block);
}

code_block *tangle_blocks(code_block *context, block_node *inputs) {
  size_t child_index = context->system->block_count;
  code_block *child = child_block(context, inputs, count_blocks(inputs));
  for (block_node *node = inputs; node; node = node->next) {
    join_blocks(node->block, child_index);
  }
  return child;
}

void weave_blocks(code_block *context, block_node *inputs,
    code_block *condition, code_block **first, code_block **second) {
  block_node condition_head = {.block = condition, .next = inputs};

  size_t count = count_blocks(&condition_head);

  size_t first_block = context->system->block_count;
  *first = child_block(context, &condition_head, count);
  size_t second_block = context->system->block_count;
  *second = child_block(context, &condition_head, count);

  for (block_node *node = inputs; node; node = node->next) {
    join_blocks(node->block, second_block);
  }

  branch_block(condition, first_block, second_block);
}
