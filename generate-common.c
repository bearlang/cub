#include "xalloc.h"
#include "generate.h"

code_block *get_code_block(code_system *system, size_t block_index) {
  return system->blocks[block_index];
}

code_struct *get_code_struct(code_system *system, size_t struct_index) {
  return system->structs[struct_index];
}

static code_block *add_block(code_system *system) {
  resize(system->block_count, &system->block_cap, (void**) &system->blocks,
    sizeof(code_block*));

  code_block *block = xmalloc(sizeof(*block));
  system->blocks[system->block_count++] = block;
  return block;
}

code_block *create_block(code_system *system) {
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

type *get_blockref_type(code_block *block) {
  type *t = new_type(T_BLOCKREF);

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

code_instruction *add_instruction(code_block *block) {
  resize(block->instruction_count, &block->instruction_cap,
    (void**) &block->instructions, sizeof(code_instruction));

  return &block->instructions[block->instruction_count++];
}

code_instruction *new_instruction(code_block *parent, size_t parameter_count) {
  code_instruction *ins = add_instruction(parent);
  ins->parameters = xmalloc(sizeof(size_t) * parameter_count);
  return ins;
}

size_t last_instruction(code_block *block) {
  return block->parameter_count + block->instruction_count - 1;
}

size_t next_instruction(code_block *block) {
  return block->parameter_count + block->instruction_count;
}

void add_blockref(code_block *src, size_t dest_block) {
  code_instruction *get = add_instruction(src);
  get->operation.type = O_BLOCKREF;
  get->type = get_blockref_type(get_code_block(src->system, dest_block));
  get->block_index = dest_block;
}
