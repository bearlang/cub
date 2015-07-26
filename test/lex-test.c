#include <stdio.h>
#include <stdlib.h>

#include "../stream.h"
#include "../lex.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: lexer-test <input-file>\n");
    return 1;
  }
  FILE *src = fopen(argv[1], "r");
  if (src == NULL) {
    fprintf(stderr, "lexer-test: no such file or directory\n");
    return 1;
  }
  stream *in = open_stream(src);
  token *token;
  while ((token = scan(in)) != NULL) {
    switch (token->type) {
    case L_ADD:
      printf("token {ADD}");
      break;
    case L_ADD_ASSIGN:
      printf("token {ADD_ASSIGN}");
      break;
    case L_AND:
      printf("token {AND}");
      break;
    case L_ASHIFT:
      printf("token {ASHIFT}");
      break;
    case L_ASHIFT_ASSIGN:
      printf("token {ASHIFT_ASSIGN}");
      break;
    case L_ASSIGN:
      printf("token {ASSIGN}");
      break;
    case L_BITWISE_AND:
      printf("token {BITWISE_AND}");
      break;
    case L_BITWISE_AND_ASSIGN:
      printf("token {BITWISE_AND_ASSIGN}");
      break;
    case L_BITWISE_NOT:
      printf("token {BITWISE_NOT}");
      break;
    case L_BITWISE_OR:
      printf("token {BITWISE_OR}");
      break;
    case L_BITWISE_OR_ASSIGN:
      printf("token {BITWISE_OR_ASSIGN}");
      break;
    case L_BITWISE_XOR:
      printf("token {BITWISE_XOR}");
      break;
    case L_BITWISE_XOR_ASSIGN:
      printf("token {BITWISE_XOR_ASSIGN}");
      break;
    case L_BREAK:
      printf("token {BREAK}");
      break;
    case L_CASE:
      printf("token {CASE}");
      break;
    case L_CLASS:
      printf("token {CLASS}");
      break;
    case L_CLOSE_BRACE:
      printf("token {CLOSE_BRACE}");
      break;
    case L_CLOSE_BRACKET:
      printf("token {CLOSE_BRACKET}");
      break;
    case L_CLOSE_PAREN:
      printf("token {CLOSE_PAREN}");
      break;
    case L_COLON:
      printf("token {COLON}");
      break;
    case L_COMMA:
      printf("token {COMMA}");
      break;
    case L_CONTINUE:
      printf("token {CONTINUE}");
      break;
    case L_DECREMENT:
      printf("token {DECREMENT}");
      break;
    case L_DIV:
      printf("token {DIV}");
      break;
    case L_DIV_ASSIGN:
      printf("token {DIV_ASSIGN}");
      break;
    case L_DO:
      printf("token {DO}");
      break;
    case L_DOT:
      printf("token {DOT}");
      break;
    case L_ELSE:
      printf("token {ELSE}");
      break;
    case L_EQ:
      printf("token {EQ}");
      break;
    case L_FOR:
      printf("token {FOR}");
      break;
    case L_GT:
      printf("token {GT}");
      break;
    case L_GTE:
      printf("token {GTE}");
      break;
    case L_IDENTIFIER:
      printf("identifier token {%s}", token->symbol_name);
      break;
    case L_IF:
      printf("token {IF}");
      break;
    case L_INCREMENT:
      printf("token {INCREMENT}");
      break;
    case L_LITERAL:
      switch (token->literal_type) {
      case T_U8:
        printf("literal token {%hhu}", token->value_u8);
        break;
      case T_U16:
        printf("literal token {%hu}", token->value_u16);
        break;
      case T_U32:
        printf("literal token {%u}", token->value_u32);
        break;
      case T_U64:
        printf("literal token {%lu}", token->value_u64);
        break;
      case T_STRING:
        printf("literal string token '%s'", token->value_string);
        break;
      default:
        printf("literal token");
      }
      break;
    case L_LSHIFT:
      printf("token {LSHIFT}");
      break;
    case L_LSHIFT_ASSIGN:
      printf("token {LSHIFT_ASSIGN}");
      break;
    case L_LT:
      printf("token {LT}");
      break;
    case L_LTE:
      printf("token {LTE}");
      break;
    case L_MOD:
      printf("token {MOD}");
      break;
    case L_MOD_ASSIGN:
      printf("token {MOD_ASSIGN}");
      break;
    case L_MUL:
      printf("token {MUL}");
      break;
    case L_MUL_ASSIGN:
      printf("token {MUL_ASSIGN}");
      break;
    case L_NE:
      printf("token {NE}");
      break;
    case L_NEW:
      printf("token {NEW}");
      break;
    case L_NOT:
      printf("token {NOT}");
      break;
    case L_OPEN_BRACE:
      printf("token {OPEN_BRACE}");
      break;
    case L_OPEN_BRACKET:
      printf("token {OPEN_BRACKET}");
      break;
    case L_OPEN_PAREN:
      printf("token {OPEN_PAREN}");
      break;
    case L_OR:
      printf("token {OR}");
      break;
    case L_RETURN:
      printf("token {RETURN}");
      break;
    case L_RSHIFT:
      printf("token {RSHIFT}");
      break;
    case L_RSHIFT_ASSIGN:
      printf("token {RSHIFT_ASSIGN}");
      break;
    case L_SEMICOLON:
      printf("token {SEMICOLON}");
      break;
    case L_STR_CONCAT:
      printf("token {STR_CONCAT}");
      break;
    case L_SUB:
      printf("token {SUB}");
      break;
    case L_SUB_ASSIGN:
      printf("token {SUB_ASSIGN}");
      break;
    case L_TERNARY:
      printf("token {TERNARY}");
      break;
    case L_TYPE:
      switch (token->literal_type) {
      case T_BOOL:
        printf("type token {BOOL}");
        break;
      case T_F128:
        printf("type token {F128}");
        break;
      case T_F32:
        printf("type token {F32}");
        break;
      case T_F64:
        printf("type token {F64}");
        break;
      case T_S16:
        printf("type token {S16}");
        break;
      case T_S32:
        printf("type token {S32}");
        break;
      case T_S64:
        printf("type token {S64}");
        break;
      case T_S8:
        printf("type token {S8}");
        break;
      case T_STRING:
        printf("type token {STRING}");
        break;
      case T_U16:
        printf("type token {U16}");
        break;
      case T_U32:
        printf("type token {U32}");
        break;
      case T_U64:
        printf("type token {U64}");
        break;
      case T_U8:
        printf("type token {U8}");
        break;
      default:
        printf("type token");
      }
      break;
    case L_VOID:
      printf("token {VOID}");
      break;
    case L_WHILE:
      printf("token {WHILE}");
      break;
    case L_XOR:
      printf("token {XOR}");
      break;
    }
    printf(" at %zu,%zu\n", token->line, token->offset);
  }
  return 0;
}
