#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

#include "primitive-type.h"
#include "stream.h"

typedef enum {
  L_ADD,
  L_ADD_ASSIGN,
  L_AND,
  L_ASHIFT,
  L_ASHIFT_ASSIGN,
  L_ASSIGN,
  L_BITWISE_AND,
  L_BITWISE_AND_ASSIGN,
  L_BITWISE_NOT,
  L_BITWISE_OR,
  L_BITWISE_OR_ASSIGN,
  L_BITWISE_XOR,
  L_BITWISE_XOR_ASSIGN,
  L_BREAK,
  L_CASE,
  L_CLASS,
  L_CLOSE_BRACE,
  L_CLOSE_BRACKET,
  L_CLOSE_PAREN,
  L_COLON,
  L_COMMA,
  L_CONTINUE,
  L_DECREMENT,
  L_DIV,
  L_DIV_ASSIGN,
  L_DO,
  L_DOT,
  L_ELSE,
  L_EQ,
  L_FOR,
  L_GT,
  L_GTE,
  L_IDENTIFIER,
  L_IF,
  L_INCREMENT,
  L_LITERAL,
  L_LSHIFT,
  L_LSHIFT_ASSIGN,
  L_LT,
  L_LTE,
  L_MOD,
  L_MOD_ASSIGN,
  L_MUL,
  L_MUL_ASSIGN,
  L_NE,
  L_NEW,
  L_NOT,
  L_NULL,
  L_OPEN_BRACE,
  L_OPEN_BRACKET,
  L_OPEN_PAREN,
  L_OR,
  L_RETURN,
  L_RSHIFT,
  L_RSHIFT_ASSIGN,
  L_SEMICOLON,
  L_STR_CONCAT,
  L_STR_CONCAT_ASSIGN,
  L_SUB,
  L_SUB_ASSIGN,
  L_TERNARY,
  L_TYPE,
  L_VOID,
  L_WHILE,
  L_XOR
} token_type;

typedef struct {
  token_type type;
  type_type literal_type;
  union {
    bool value_bool;
    char *symbol_name;
    char *value_string;
    uint16_t value_u16;
    uint32_t value_u32;
    uint64_t value_u64;
    uint8_t value_u8;
  };

  size_t line, offset;
} token;

token *new_token(stream*, token_type);
char *token_string(token_type);

#endif
