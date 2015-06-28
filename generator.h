#ifndef GENERATOR_H
#define GENERATOR_H

#include "type.h"
#include "statement.h"

struct code_system;

typedef struct {
  type *field_type;
} code_field;

typedef struct {
  size_t field_count;
  code_field *fields;
} code_struct;

typedef struct /*extends code_instruction_blank*/ {
  operation operation;
  type *type;
  union {
    // literal value
    bool value_bool;
    float value_f32;
    double value_f64;
    long double value_f128;
    int8_t value_s8;
    int16_t value_s16;
    int32_t value_s32;
    int64_t value_s64;
    uint8_t value_u8;
    uint16_t value_u16;
    uint32_t value_u32;
    uint64_t value_u64;
    char *value_string;
    // parameterized operation array (includes things like field index)
    size_t *parameters;
    // block reference
    size_t block_index;
    // struct reference
    size_t struct_index;
  };
} code_instruction;

// TODO: actually these can't be static: functions have dynamic GOTOs to return
// currently static dispatch: first_block and second_block reference blocks, not
// instructions which contain block references
typedef enum {
  GOTO,
  BRANCH
} code_tail;

typedef struct instruction_node {
  size_t instruction;
  struct instruction_node *next;
} instruction_node;

typedef struct {
  struct code_system *system;
  size_t parameter_count;
  // TODO: is this the right type?
  code_field *parameters;
  size_t symbol_count;
  symbol_entry *symbol_head;
  size_t stack_size;
  instruction_node *stack_head;
  bool accepts_return, has_return;
  size_t return_instruction;
  size_t instruction_count, instruction_cap;
  code_instruction *instructions;

  // tail instruction
  bool is_final;
  struct {
    code_tail type;
    size_t condition_instruction;
    size_t first_block, second_block;
    size_t parameter_count; // TODO: needed?
    size_t *parameters;
  } tail;
} code_block;

typedef struct code_system {
  // TODO: byte arrays in one constant block referenced in slices from the code
  size_t struct_count, struct_cap;
  code_struct *structs;
  size_t block_count, block_cap;
  code_block *blocks;
} code_system;

code_system *generate(block_statement *root);
type *instruction_type(code_block*, size_t);

#endif
