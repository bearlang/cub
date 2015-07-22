#include "token.h"
#include "xalloc.h"

token *new_token(stream *in, token_type type) {
  token *t = xmalloc(sizeof(*t));
  t->line = in->line;
  t->offset = in->offset;
  t->type = type;
  return t;
}

char *token_string(token_type type) {
  switch (type) {
  case L_ADD: return "ADD";
  case L_ADD_ASSIGN: return "ADD_ASSIGN";
  case L_AND: return "AND";
  case L_ASHIFT: return "ASHIFT";
  case L_ASHIFT_ASSIGN: return "ASHIFT_ASSIGN";
  case L_ASSIGN: return "ASSIGN";
  case L_BITWISE_AND: return "BITWISE_AND";
  case L_BITWISE_AND_ASSIGN: return "BITWISE_AND_ASSIGN";
  case L_BITWISE_NOT: return "BITWISE_NOT";
  case L_BITWISE_OR: return "BITWISE_OR";
  case L_BITWISE_OR_ASSIGN: return "BITWISE_OR_ASSIGN";
  case L_BITWISE_XOR: return "BITWISE_XOR";
  case L_BITWISE_XOR_ASSIGN: return "BITWISE_XOR_ASSIGN";
  case L_BREAK: return "BREAK";
  case L_CASE: return "CASE";
  case L_CLASS: return "CLASS";
  case L_CLOSE_BRACE: return "CLOSE_BRACE";
  case L_CLOSE_BRACKET: return "CLOSE_BRACKET";
  case L_CLOSE_PAREN: return "CLOSE_PAREN";
  case L_COLON: return "COLON";
  case L_COMMA: return "COMMA";
  case L_CONTINUE: return "CONTINUE";
  case L_DECREMENT: return "DECREMENT";
  case L_DIV: return "DIV";
  case L_DIV_ASSIGN: return "DIV_ASSIGN";
  case L_DO: return "DO";
  case L_DOT: return "DOT";
  case L_ELSE: return "ELSE";
  case L_EQ: return "EQ";
  case L_FOR: return "FOR";
  case L_GT: return "GT";
  case L_GTE: return "GTE";
  case L_IDENTIFIER: return "IDENTIFIER";
  case L_IF: return "IF";
  case L_INCREMENT: return "INCREMENT";
  case L_LITERAL: return "LITERAL";
  case L_LSHIFT: return "LSHIFT";
  case L_LSHIFT_ASSIGN: return "LSHIFT_ASSIGN";
  case L_LT: return "LT";
  case L_LTE: return "LTE";
  case L_MOD: return "MOD";
  case L_MOD_ASSIGN: return "MOD_ASSIGN";
  case L_MUL: return "MUL";
  case L_MUL_ASSIGN: return "MUL_ASSIGN";
  case L_NE: return "NE";
  case L_NEW: return "NEW";
  case L_NOT: return "NOT";
  case L_OPEN_BRACE: return "OPEN_BRACE";
  case L_OPEN_BRACKET: return "OPEN_BRACKET";
  case L_OPEN_PAREN: return "OPEN_PAREN";
  case L_OR: return "OR";
  case L_RETURN: return "RETURN";
  case L_RSHIFT: return "RSHIFT";
  case L_RSHIFT_ASSIGN: return "RSHIFT_ASSIGN";
  case L_SEMICOLON: return "SEMICOLON";
  case L_STR_CONCAT: return "STR_CONCAT";
  case L_SUB: return "SUB";
  case L_SUB_ASSIGN: return "SUB_ASSIGN";
  case L_TERNARY: return "TERNARY";
  case L_TYPE: return "TYPE";
  case L_TYPEDEF: return "TYPEDEF";
  case L_VOID: return "VOID";
  case L_WHILE: return "WHILE";
  case L_XOR: return "XOR";
  default: return "UNKNOWN_TOKEN";
  }
}
