#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdint.h>
#include <stdlib.h>

#include "operation.h"
#include "type.h"

typedef struct expression_node {
  operation operation;
  type *type; // literal object -> null
  union {
    // no array literals for now
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
    // size_t value_string_length;
    struct expression_node *value;
  };
  union {
    function *function;
    char *value_string;
    char *symbol_name;
    size_t field_index;
  };
  struct expression_node *next;
} expression_node;

typedef expression_node expression;

operation_type assert_valid_assign(expression *left);

expression *new_assign_node(expression *left, expression *right);
expression *new_call_node(expression *callee, expression *args);
expression *new_compare_node(compare_type type, expression *left, expression *right);
expression *new_get_field_node(expression *left, char *field);
expression *new_get_index_node(expression *left, expression *index);
expression *new_get_symbol_node(char *symbol);
expression *new_function_node(function *fn);
expression *new_identity_node(expression *left, expression *right);
expression *new_literal_node(type_type type);
expression *new_logic_node(logic_type type, expression *left, expression *right);
expression *new_negate_node(expression *value);
expression *new_new_node(char *class_name, expression *args);
expression *new_not_node(bool bitwise, expression *value);
expression *new_numeric_node(numeric_type type, expression *left, expression *right);
expression *new_numeric_assign_node(numeric_type type, expression *left, expression *right);
expression *new_postfix_node(postfix_type, expression*);
expression *new_shift_node(shift_type type, expression *left, expression *right);
expression *new_shift_assign_node(shift_type type, expression *left, expression *right);
expression *new_str_concat_node(expression *left, expression *right);
expression *new_str_concat_assign_node(expression *left, expression *right);
expression *new_ternary_node(expression *condition, expression *left, expression *right);

#endif
