#include <string.h>

#include "xalloc.h"
#include "optimize.h"

/*static void fill(void *dest, const size_t size, const void *value,
    const size_t width) {
  const void *end = dest + size;
  for (; dest < end; dest += width) {
    memcpy(dest, value, width);
  }
}*/

/*static void resolve_copy(size_t *map, size_t total, size_t *start) {
  size_t now = *start;
  for (;;) {
    size_t next = map[now];
    if (next == total) {
      break;
    }
    now = next;
  }
  *start = now;
}*/

static void optimize_copies(code_block *block) {
  size_t params = block->parameter_count, lines = block->instruction_count;
  size_t total = params + lines, offset = 0;
  size_t map[total];

  for (size_t i = 0; i < params; i++) {
    map[i] = i;
  }

  for (size_t i = 0; i < lines; i++) {
    code_instruction *ins = &block->instructions[i];
    size_t j = i + params;

    size_t *ip = ins->parameters;

    switch (ins->operation.type) {
    case O_BITWISE_NOT:
    case O_CAST:
    case O_GET_FIELD:
    case O_NEGATE:
    case O_NOT:
      ip[0] = map[ip[0]];
      break;
    case O_COMPARE:
    case O_IDENTITY:
    case O_LOGIC:
    case O_NUMERIC:
    case O_SHIFT:
    case O_STR_CONCAT:
      ip[0] = map[ip[0]];
      ip[1] = map[ip[1]];
      break;
    case O_GET_INDEX:
      ip[0] = map[ip[0]];
      ip[1] = map[ip[1]];
      break;
    case O_GET_SYMBOL: {
      map[j] = map[ip[0]];
      free_type(ins->type);
    } continue;
    case O_SET_FIELD:
      ip[0] = map[ip[0]];
      ip[2] = map[ip[2]];
      break;
    case O_SET_INDEX:
      ip[0] = map[ip[0]];
      ip[1] = map[ip[1]];
      ip[2] = map[ip[2]];
      break;

    case O_BLOCKREF:
    case O_LITERAL:
    case O_NEW:
      break;
    case O_CALL:
    case O_FUNCTION:
    case O_NUMERIC_ASSIGN:
    case O_POSTFIX:
    case O_SET_SYMBOL:
    case O_SHIFT_ASSIGN:
    case O_STR_CONCAT_ASSIGN:
    case O_TERNARY:
      abort();
    }

    if (i != offset) {
      memcpy(&block->instructions[offset], ins, sizeof(*ins));
    }

    map[j] = offset + params;
    offset++;
  }

  block->instruction_count = offset;
  xrealloc(block->instructions, sizeof(code_instruction) * offset);

  if (!block->is_final) {
    block->tail.first_block = map[block->tail.first_block];

    if (block->tail.type == BRANCH) {
      block->tail.second_block = map[block->tail.second_block];

      block->tail.condition = map[block->tail.condition];
    }

    for (size_t i = 0; i < block->tail.parameter_count; i++) {
      block->tail.parameters[i] = map[block->tail.parameters[i]];
    }
  }
}

/*static void optimize_copies(code_block *block) {
  size_t params = block->parameter_count, lines = block->instruction_count;
  size_t total = params + lines, newtotal = total;
  size_t map[total];

  fill(map, sizeof(total) * total, (const void*) &total, sizeof(total));

  for (size_t i = 0; i < lines; i++) {
    code_instruction *ins = &block->instructions[i];
    size_t j = i + params;

    switch (ins->operation.type) {
    case O_BITWISE_NOT:
    case O_CAST:
    case O_GET_FIELD:
    case O_NEGATE:
    case O_NOT:
      resolve_copy(map, total, &ins->parameters[0]);
      break;
    case O_COMPARE:
    case O_IDENTITY:
    case O_LOGIC:
    case O_NUMERIC:
    case O_SHIFT:
    case O_STR_CONCAT:
      resolve_copy(map, total, &ins->parameters[0]);
      resolve_copy(map, total, &ins->parameters[1]);
      break;
    case O_GET_INDEX:
      resolve_copy(map, total, &ins->parameters[0]);
      resolve_copy(map, total, &ins->parameters[1]);
      break;
    case O_GET_SYMBOL:
      resolve_copy(map, total, &ins->parameters[0]);
      map[j] = ins->parameters[0];
      newtotal--;
      break;
    case O_SET_FIELD:
      resolve_copy(map, total, &ins->parameters[0]);
      resolve_copy(map, total, &ins->parameters[2]);
      break;
    case O_SET_INDEX:
      resolve_copy(map, total, &ins->parameters[0]);
      resolve_copy(map, total, &ins->parameters[1]);
      resolve_copy(map, total, &ins->parameters[2]);
      break;

    case O_BLOCKREF:
    case O_LITERAL:
    case O_NEW:
      break;
    case O_CALL:
    case O_FUNCTION:
    case O_NUMERIC_ASSIGN:
    case O_SET_SYMBOL:
    case O_SHIFT_ASSIGN:
    case O_STR_CONCAT_ASSIGN:
    case O_TERNARY:
      abort();
    }
  }
}*/

void optimize(code_system *system) {
  for (size_t i = 0; i < system->block_count; i++) {
    code_block *block = &system->blocks[i];
    optimize_copies(block);
  }
}
