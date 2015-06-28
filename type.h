#ifndef TYPE_H
#define TYPE_H

#include <stdlib.h>

#include "bool.h"

// just in case
struct statement;
struct block_statement;

// TODO: map, list, class, function
typedef enum {
  T_ARRAY,
  T_BLOCKREF, // TODO: replace with T_LITERAL?
  T_BOOL,
  T_F32,
  T_F64,
  T_F128,
  // TODO: classes need to be entered into the symbol table before the fields
  // are processed
  T_OBJECT,
  T_REF,
  T_S8,
  T_S16,
  T_S32,
  T_S64,
  T_STRING,
  T_U8,
  T_U16,
  T_U32,
  T_U64,
  T_VOID
} type_type;

struct class;
struct arguments;

// note that this struct may be used, do not modify post-parsing :3
typedef struct type {
  type_type type;
  union {
    char *symbol_name;
    struct type *arraytype;
    struct class *classtype;
    struct argument *blocktype;

    // code generation
    size_t struct_index;
  };
} type;

typedef struct argument {
  char *symbol_name;
  type *argument_type;
  struct argument *next;
} argument;

typedef struct function {
  char *function_name;
  type *return_type;
  size_t argument_count; // TODO: use this (store, don't forget stack values)
  argument *argument;
  struct block_statement *body;

  // code generation
  size_t block_body;
  size_t return_struct;
} function;

typedef struct field {
  type *field_type;
  char *symbol_name;
  struct field *next;
} field;

typedef struct class {
  char *class_name;
  size_t field_count; // TODO: use this
  field *field;

  // code generation
  size_t struct_index;
} class;

bool compatible_type(type *left, type *right);
bool is_void(type*);
argument *copy_arguments(argument *arg, bool with_names);
type *copy_type(type*);

#endif
