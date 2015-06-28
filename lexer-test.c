#include <stdio.h>
#include <stdlib.h>

#include "stream.h"
#include "lexer.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: lexer-test <input-file>\n");
    exit(1);
  }
  FILE *src = fopen(argv[1], "r");
  if (src == NULL) {
    fprintf(stderr, "lexer-test: no such file or directory\n");
    exit(1);
  }
  stream *in = open_stream(src);
  token *token;
  while ((token = scan(in)) != NULL) {
    switch (token->type) {
    case L_ADD:
      printf("token {ADD}\n");
      break;
    case L_ADD_ASSIGN:
      printf("token {ADD_ASSIGN}\n");
      break;
    case L_AND:
      printf("token {AND}\n");
      break;
    case L_ASHIFT:
      printf("token {ASHIFT}\n");
      break;
    case L_ASHIFT_ASSIGN:
      printf("token {ASHIFT_ASSIGN}\n");
      break;
    case L_ASSIGN:
      printf("token {ASSIGN}\n");
      break;
    case L_BITWISE_AND:
      printf("token {BITWISE_AND}\n");
      break;
    case L_BITWISE_AND_ASSIGN:
      printf("token {BITWISE_AND_ASSIGN}\n");
      break;
    case L_BITWISE_NOT:
      printf("token {BITWISE_NOT}\n");
      break;
    case L_BITWISE_OR:
      printf("token {BITWISE_OR}\n");
      break;
    case L_BITWISE_OR_ASSIGN:
      printf("token {BITWISE_OR_ASSIGN}\n");
      break;
    case L_BITWISE_XOR:
      printf("token {BITWISE_XOR}\n");
      break;
    case L_BITWISE_XOR_ASSIGN:
      printf("token {BITWISE_XOR_ASSIGN}\n");
      break;
    case L_BREAK:
      printf("token {BREAK}\n");
      break;
    case L_CASE:
      printf("token {CASE}\n");
      break;
    case L_CLASS:
      printf("token {CLASS}\n");
      break;
    case L_CLOSE_BRACE:
      printf("token {CLOSE_BRACE}\n");
      break;
    case L_CLOSE_BRACKET:
      printf("token {CLOSE_BRACKET}\n");
      break;
    case L_CLOSE_PAREN:
      printf("token {CLOSE_PAREN}\n");
      break;
    case L_COLON:
      printf("token {COLON}\n");
      break;
    case L_COMMA:
      printf("token {COMMA}\n");
      break;
    case L_CONTINUE:
      printf("token {CONTINUE}\n");
      break;
    case L_DECREMENT:
      printf("token {DECREMENT}\n");
      break;
    case L_DIV:
      printf("token {DIV}\n");
      break;
    case L_DIV_ASSIGN:
      printf("token {DIV_ASSIGN}\n");
      break;
    case L_DO:
      printf("token {DO}\n");
      break;
    case L_DOT:
      printf("token {DOT}\n");
      break;
    case L_ELSE:
      printf("token {ELSE}\n");
      break;
    case L_EQ:
      printf("token {EQ}\n");
      break;
    case L_FOR:
      printf("token {FOR}\n");
      break;
    case L_GT:
      printf("token {GT}\n");
      break;
    case L_GTE:
      printf("token {GTE}\n");
      break;
    case L_IDENTIFIER:
      printf("identifier token {%s}\n", token->symbol_name);
      break;
    case L_IF:
      printf("token {IF}\n");
      break;
    case L_INCREMENT:
      printf("token {INCREMENT}\n");
      break;
    case L_LITERAL:
      switch (token->literal_type) {
      case T_U8:
        printf("literal token {%hhu}\n", token->value_u8);
        break;
      case T_U16:
        printf("literal token {%hu}\n", token->value_u16);
        break;
      case T_U32:
        printf("literal token {%u}\n", token->value_u32);
        break;
      case T_U64:
        printf("literal token {%lu}\n", token->value_u64);
        break;
      case T_STRING:
        printf("literal string token '%s'\n", token->value_string);
        break;
      default:
        printf("literal token\n");
      }
      break;
    case L_LSHIFT:
      printf("token {LSHIFT}\n");
      break;
    case L_LSHIFT_ASSIGN:
      printf("token {LSHIFT_ASSIGN}\n");
      break;
    case L_LT:
      printf("token {LT}\n");
      break;
    case L_LTE:
      printf("token {LTE}\n");
      break;
    case L_MOD:
      printf("token {MOD}\n");
      break;
    case L_MOD_ASSIGN:
      printf("token {MOD_ASSIGN}\n");
      break;
    case L_MUL:
      printf("token {MUL}\n");
      break;
    case L_MUL_ASSIGN:
      printf("token {MUL_ASSIGN}\n");
      break;
    case L_NE:
      printf("token {NE}\n");
      break;
    case L_NEW:
      printf("token {NEW}\n");
      break;
    case L_NOT:
      printf("token {NOT}\n");
      break;
    case L_OPEN_BRACE:
      printf("token {OPEN_BRACE}\n");
      break;
    case L_OPEN_BRACKET:
      printf("token {OPEN_BRACKET}\n");
      break;
    case L_OPEN_PAREN:
      printf("token {OPEN_PAREN}\n");
      break;
    case L_OR:
      printf("token {OR}\n");
      break;
    case L_RETURN:
      printf("token {RETURN}\n");
      break;
    case L_RSHIFT:
      printf("token {RSHIFT}\n");
      break;
    case L_RSHIFT_ASSIGN:
      printf("token {RSHIFT_ASSIGN}\n");
      break;
    case L_SEMICOLON:
      printf("token {SEMICOLON}\n");
      break;
    case L_STR_CONCAT:
      printf("token {STR_CONCAT}\n");
      break;
    case L_SUB:
      printf("token {SUB}\n");
      break;
    case L_SUB_ASSIGN:
      printf("token {SUB_ASSIGN}\n");
      break;
    case L_TERNARY:
      printf("token {TERNARY}\n");
      break;
    case L_TYPE:
      switch (token->literal_type) {
      case T_BOOL:
        printf("type token {BOOL}\n");
        break;
      case T_F128:
        printf("type token {F128}\n");
        break;
      case T_F32:
        printf("type token {F32}\n");
        break;
      case T_F64:
        printf("type token {F64}\n");
        break;
      case T_S16:
        printf("type token {S16}\n");
        break;
      case T_S32:
        printf("type token {S32}\n");
        break;
      case T_S64:
        printf("type token {S64}\n");
        break;
      case T_S8:
        printf("type token {S8}\n");
        break;
      case T_STRING:
        printf("type token {STRING}\n");
        break;
      case T_U16:
        printf("type token {U16}\n");
        break;
      case T_U32:
        printf("type token {U32}\n");
        break;
      case T_U64:
        printf("type token {U64}\n");
        break;
      case T_U8:
        printf("type token {U8}\n");
        break;
      default:
        printf("type token\n");
      }
      break;
    case L_VOID:
      printf("token {VOID}\n");
      break;
    case L_WHILE:
      printf("token {WHILE}\n");
      break;
    case L_XOR:
      printf("token {XOR}\n");
      break;
    }
  }
  return 0;
}
