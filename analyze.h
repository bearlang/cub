#ifndef ANALYZE_H
#define ANALYZE_H

#include "statement.h"

#define ST_CLASS 1
#define ST_TYPE 2
#define ST_VARIABLE 4

expression *bool_cast(expression *value);
expression *implicit_cast(expression *value, type *expected);
void numeric_promotion(expression *value, bool allow_floats);
type_type binary_numeric_promotion(expression *value, bool allow_floats);

loop_statement *get_label(control_statement*);
symbol_entry *get_entry(const symbol_entry*, const char*);
symbol_entry *new_symbol_entry(char*, bool constant);
symbol_entry *add_symbol(block_statement*, symbol_type, char*);
symbol_entry *get_symbol(block_statement*, char*, uint8_t *symbol_type);
symbol_entry *get_variable_symbol(block_statement*, char*);

void analyze(block_statement*);

#endif
