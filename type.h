#ifndef TYPE_H
#define TYPE_H

#include <stdlib.h>

#include "bool.h"
#include "primitive-type.h"
#include "token.h"

// just in case
struct statement;
struct block_statement;

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
argument *copy_arguments(argument*, bool with_names);
void free_arguments(argument*);
type *new_type(type_type);
type *new_type_from_token(token*);
type *new_array_type(type *inner);
type *new_function_type(type *ret, argument *arg);
type *copy_type(type*);
void free_type(type*);

#endif
