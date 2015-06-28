#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

#include "bool.h"
#include "xalloc.h"
#include "buffer.h"
#include "type.h"

static bool consume(stream *in, char chr) {
  char found = stream_shift(in);
  if (found == chr) return true;
  stream_push(in, found);
  return false;
}

static uint8_t expect_hex_digit(stream *in) {
  char chr = stream_shift(in);
  if (chr >= '0' && chr <= '9') {
    return chr - '0';
  }
  chr |= 0x20;
  if (chr >= 'a' && chr <= 'f') {
    return chr - 0x57;
  }
  fprintf(stderr, "unexpected character, expecting hex digit on line %zi\n",
    in->line);
  exit(1);
}

static void consume_comment(stream *in) {
  for (;;) {
    char chr = stream_shift(in);
    if (chr == '*') {
      chr = stream_shift(in);
      if (chr == '/') break;
    }
    if (chr == 0) {
      fprintf(stderr, "unexpected EOF, expecting '*/' on line %zi\n",
        in->line);
      exit(1);
    }
    if (chr == '\n') in->line++;
  }
}

static uint8_t integer_bits(uint64_t value) {
  uint8_t bits = 0;
  while (value) {
    ++bits;
    value >>= 1;
  }
  return bits;
}

static token *new_integer_token(uint64_t value, uint8_t bits) {
  token *t = new_token(L_LITERAL);
  if (bits > 32) {
    t->literal_type = T_U64;
    t->value_u64 = value;
  } else if (bits > 16) {
    t->literal_type = T_U32;
    t->value_u32 = (uint32_t) value;
  } else if (bits > 8) {
    t->literal_type = T_U16;
    t->value_u16 = (uint16_t) value;
  } else {
    t->literal_type = T_U8;
    t->value_u8 = (uint8_t) value;
  }
  return t;
}

static token *scan_number(stream *in) {
  char chr = stream_shift(in);
  if (chr == '0') {
    chr = stream_shift(in);
    if (chr == '.' || chr == 'e' || chr == 'E') {
      // TODO: floating-point numbers
      fprintf(stderr,
        "floating point literals are not implemented on line %zi\n", in->line);
      exit(1);
    }
    if (chr == 'x') {
      bool any = false;
      uint64_t value = 0;
      for (;;) {
        chr = stream_shift(in);
        if (chr >= '0' && chr <= '9') {
          uint64_t v2 = value << 4;
          if ((v2 >> 4) != value) {
            fprintf(stderr, "hex literal too large on line %zi\n", in->line);
            exit(1);
          }
          any = true;
          value = v2 | (chr - '0');
          continue;
        }
        char _chr = chr | 0x20; // lowercase
        if (_chr >= 'a' && _chr <= 'f') {
          uint64_t v2 = value << 4;
          if ((v2 >> 4) != value) {
            fprintf(stderr, "hex literal too large on line %zi\n", in->line);
            exit(1);
          }
          any = true;
          value = v2 | (_chr - 0x57);
          continue;
        }
        // handles EOF
        stream_push(in, chr);
        break;
      }
      if (!any) {
        fprintf(stderr, "unexpected EOF, expecting [0-9a-f] on line %zi\n",
          in->line);
        exit(1);
      }
      return new_integer_token(value, integer_bits(value));
    }
    if (chr == 'b') {
      bool any = false;
      uint64_t value = 0;
      for (;;) {
        chr = stream_shift(in);
        if (chr != '0' && chr != '1') {
          if (chr >= '0' && chr <= '9') {
            fprintf(stderr, "bad binary literal on line %zi\n", in->line);
            exit(1);
          }
          if (!any) {
            fprintf(stderr, "unexpected EOF, expecting [01] on line %zi\n",
              in->line);
            exit(1);
          }
          stream_push(in, chr);
          break;
        }
        uint64_t v2 = value << 1;
        if ((v2 >> 1) != value) {
          fprintf(stderr, "binary literal too large on line %zi\n", in->line);
          exit(1);
        }
        any = true;
        value = v2 | (chr - '0');
      }
      return new_integer_token(value, integer_bits(value));
    }
    if (chr >= '0' && chr <= '9') {
      // TODO: octal literals
      fprintf(stderr, "octal literals are not implemented on line %zi\n",
        in->line);
      exit(1);
    }
    // handles null, too
    stream_push(in, chr);
    return new_integer_token(0, 0);
  }
  if (chr == '.') {
    // TODO: floating-point numbers
    fprintf(stderr, "floating point literals are not implemented on line %zi\n",
      in->line);
    exit(1);
  }
  uint64_t value = 0;
  for (;;) {
    if (chr == '.' || chr == 'e' || chr == 'E') {
      // TODO: floating-point numbers
      fprintf(stderr, "floating point literals are not implemented on line %zi\n",
        in->line);
      exit(1);
    }
    if (chr < '0' || chr > '9') {
      break;
    }
    uint64_t v2 = value * 10;
    if (v2 / 10 != value) {
      fprintf(stderr, "decimal literal too large on line %zi\n", in->line);
      exit(1);
    }
    value = v2 + chr - '0';
    chr = stream_shift(in);
  }
  stream_push(in, chr);
  return new_integer_token(value, integer_bits(value));
}

static token *scan_string(stream *in, char match) {
  char chr;
  buffer buf;
  buffer_init(&buf);
  for (;;) {
    chr = stream_shift(in);
    if (chr == 0) {
      fprintf(stderr, "unexpected EOF, expecting character on line %zi\n",
        in->line);
      exit(1);
    }
    if (chr == match) {
      break;
    }
    if (chr != '\\') {
      buffer_append_char(&buf, chr);
      continue;
    }
    chr = stream_shift(in);
    char append;
    switch (chr) {
    case '"': append = '"'; break;
    case '0': append = '\0'; break;
    case '\'': append = '\''; break;
    case '\\': append = '\\'; break;
    case '\n':
      fprintf(stderr, "expected character, found newline on line %zi\n",
        in->line);
      exit(1);
    case 'b': append = 'b'; break;
    case 'f': append = 'f'; break;
    case 'n': append = '\n'; break;
    case 'r': append = 'r'; break;
    case 't': append = 't'; break;
    case 'u':
      // TODO: unicode support
      fprintf(stderr, "unicode not yet supported\n");
      exit(1);
    case 'v': append = 'v'; break;
    case 'x':
      append = (expect_hex_digit(in) << 4) | expect_hex_digit(in);
    default:
      fprintf(stderr, "unexpected character in string on line %zi\n", in->line);
      exit(1);
    }
    buffer_append_char(&buf, append);
  }
  token *t = new_token(L_LITERAL);
  t->literal_type = T_STRING;
  t->value_string = buffer_flush(&buf);
  // TODO: buffer_free not called (not a leak currently, but could be a problem)
  return t;
}

const char *keywords[] = {
  "break",
  "case",
  "class",
  "continue",
  "do",
  "else",
  "for",
  "if",
  "new",
  "return",
  "while"
};
const token_type keyword_map[] = {
  L_BREAK,
  L_CASE,
  L_CLASS,
  L_CONTINUE,
  L_DO,
  L_ELSE,
  L_FOR,
  L_IF,
  L_NEW,
  L_RETURN,
  L_WHILE
};
const size_t keyword_count = sizeof(keywords) / sizeof(char*);

const char *types[] = {
  "bool",
  "f128",
  "f32",
  "f64",
  "s16",
  "s32",
  "s64",
  "s8",
  "string",
  "u16",
  "u32",
  "u64",
  "u8",
  "void",
};
const type_type type_map[] = {
  T_BOOL,
  T_F128,
  T_F32,
  T_F64,
  T_S16,
  T_S32,
  T_S64,
  T_S8,
  T_STRING,
  T_U16,
  T_U32,
  T_U64,
  T_U8,
  T_VOID
};
const size_t type_count = sizeof(types) / sizeof(char*);

// TODO: optimize!
// cannot create symbol table here, types may conflict with variables
static token *scan_word(stream *in) {
  buffer buf;
  buffer_init(&buf);

  char chr = stream_shift(in);
  do {
    buffer_append_char(&buf, chr);
    chr = stream_shift(in);
  } while ((chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') ||
    (chr >= '0' && chr <= '9') || chr == '_');

  stream_push(in, chr);

  const char *word = buffer_flush(&buf);

  for (size_t i = 0; i < keyword_count; i++) {
    if (strcmp(word, keywords[i]) == 0) {
      free((void*) word);
      return new_token(keyword_map[i]);
    }
  }

  for (size_t i = 0; i < type_count; i++) {
    if (strcmp(word, types[i]) == 0) {
      free((void*) word);
      token *t = new_token(L_TYPE);
      t->literal_type = type_map[i];
      return t;
    }
  }

  token *t;
  if (strcmp(word, "true") == 0) {
    free((void*) word);
    t = new_token(L_LITERAL);
    t->literal_type = T_BOOL;
    t->value_bool = true;
  } else if (strcmp(word, "false") == 0) {
    free((void*) word);
    t = new_token(L_LITERAL);
    t->literal_type = T_BOOL;
    t->value_bool = false;
  } else if (strcmp(word, "null") == 0) {
    free((void*) word);
    t = new_token(L_LITERAL);
    t->literal_type = T_OBJECT;
    // TODO: indicate null
  } else {
    t = new_token(L_IDENTIFIER);
    t->symbol_name = (char*) word;
  }
  return t;
}

/*typedef struct {
  size_t choices;
  struct lexnode *head, *tail;
} lexentry;

typedef struct lexnode {
  size_t choices;
  struct lexnode *head, *tail;
  bool has_token;
  char match;
  token_type token;
  struct lexnode *next;
} lexnode;

const char *lexlist[] = {
  ">>>=",
  "<<=",
  ">>=",
  ">>>",
  "!=",
  "%=",
  "&&",
  "&=",
  "*=",
  "++",
  "+=",
  "--",
  "-=",
  "/=",
  "<<",
  "<=",
  "==",
  ">=",
  ">>",
  "^=",
  "^^",
  "|=",
  "||",
  "!",
  "%",
  "&",
  "(",
  ")",
  "*",
  "+",
  ",",
  "-",
  "/",
  ":",
  ";",
  "<",
  "=",
  ">",
  "?",
  "[",
  "]",
  "^",
  "{",
  "|",
  "}",
  "~"
};
const token_type lexmap[] = {
  L_RSHIFT_ASSIGN,
  L_LSHIFT_ASSIGN,
  L_ASHIFT_ASSIGN,
  L_RSHIFT,
  L_NE,
  L_MOD_ASSIGN,
  L_AND,
  L_BITWISE_AND_ASSIGN,
  L_MUL_ASSIGN,
  L_INCREMENT,
  L_ADD_ASSIGN,
  L_DECREMENT,
  L_SUB_ASSIGN,
  L_DIV_ASSIGN,
  L_LSHIFT,
  L_LTE,
  L_EQ,
  L_GTE,
  L_ASHIFT,
  L_BITWISE_XOR_ASSIGN,
  L_XOR,
  L_BITWISE_OR_ASSIGN,
  L_OR,
  L_NOT,
  L_MOD,
  L_BITWISE_AND,
  L_OPEN_PAREN,
  L_CLOSE_PAREN,
  L_MUL,
  L_ADD,
  L_COMMA,
  L_SUB,
  L_DIV,
  L_COLON,
  L_SEMICOLON,
  L_LT,
  L_ASSIGN,
  L_GT,
  L_TERNARY,
  L_OPEN_BRACKET,
  L_CLOSE_BRACKET,
  L_BITWISE_XOR,
  L_OPEN_BRACE,
  L_BITWISE_OR,
  L_CLOSE_BRACE,
  L_BITWISE_NOT
};
const size_t lex_count = sizeof(lexlist) / sizeof(char*);

static lexentry *build_lex() {
  lexentry *entries = xmalloc(sizeof(*entries)) * 0x100;

  for (size_t i = 0; i < 0x100; i++) {
    entries[i].has_token = false;
    entries[i].choices = 0;
    entries[i].entries = NULL;
  }

  for (size_t i = 0; i < lex_count; i++) {
    const char *lexeme = lexlist[i];
    const size_t len = strlen(lexeme);
    lexentry *entry = &entries[lexeme[0]];
    lexnode *node = xmalloc(sizeof(*node));
    node->choices = 0;
    node->next = NULL;
    if (entry->choices) {
      entry
    } else {

    }
    entry->choices++;
    lexloop: for (size_t c = 1; c < len; c++) {
      lexnode node;
      for (node = entry->head; node != NULL; node = node->next) {
        if (node->match == lexeme[c]) {
          entry = (lexentry) node;
          continue lexloop;
        }
      }
      lexnode new = xmalloc(sizeof(*new));
      new->choices = 0;
      new->next = NULL;
      new->head =
      node->next = new;
      node = new;
    }
  }

  return entries;
}*/

static token *scan_inner(stream *in) {
  char chr;
  // handle whitespace and comments
  for (;;) {
    chr = stream_shift(in);
    if (chr == '/') {
      chr = stream_shift(in);
      if (chr == '/') {
        do chr = stream_shift(in);
        while (chr != '\n');
        ++in->line;
      } else if (chr == '*') {
        consume_comment(in);
      } else {
        stream_push(in, chr);
        stream_push(in, '/');
        break;
      }
    } else {
      if (chr == ' ' || chr == '\t') continue;
      if (chr == 0) return NULL;
      if (chr != '\n') break;
      ++in->line;
    }
  }

  token_type type;
  switch (chr) {
  case '.':
    chr = stream_shift(in);
#ifdef DOT_STYLE_CONCAT
    if (chr == '.') {
      return consume(in, '=')
        ? new_token(L_STR_CONCAT_ASSIGN)
        : new_token(L_STR_CONCAT);
    }
#endif
    // works with EOF too
    if (chr < '0' || chr > '9') {
      stream_push(in, chr);
      return new_token(L_DOT);
    }
    stream_push(in, chr);
    stream_push(in, '.');
    return scan_number(in);
  case '0': case '1': case '2': case '3': case '4': case '5': case '6':
  case '7': case '8': case '9':
    stream_push(in, chr);
    return scan_number(in);
  case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
  case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
  case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
  case 'V': case 'W': case 'X': case 'Y': case 'Z': case 'a': case 'b':
  case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i':
  case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
  case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w':
  case 'x': case 'y': case 'z': case '_':
    stream_push(in, chr);
    return scan_word(in);
  case '!':
    type = consume(in, '=') ? L_NE : L_NOT;
    break;
  case '"':
  case '\'':
    // TODO: implement interpolation for '"'
    return scan_string(in, chr);
#ifndef DOT_STYLE_CONCAT
  case '#':
    return consume(in, '=')
      ? new_token(L_STR_CONCAT_ASSIGN)
      : new_token(L_STR_CONCAT);
    break;
#endif
  case '>':
    type = consume(in, '>')
      ? consume(in, '>')
        ? consume(in, '=') ? L_RSHIFT_ASSIGN : L_RSHIFT
        : (consume(in, '=') ? L_ASHIFT_ASSIGN : L_ASHIFT)
      : (consume(in, '=') ? L_GTE : L_GT);
    break;
  case '<':
    type = consume(in, '<')
      ? consume(in, '=') ? L_LSHIFT_ASSIGN : L_LSHIFT
      : (consume(in, '=') ? L_LTE : L_LT);
    break;
  case '%':
    type = consume(in, '=') ? L_MOD_ASSIGN : L_MOD;
    break;
  case '&':
    type = consume(in, '&')
      ? L_AND
      : (consume(in, '=') ? L_BITWISE_AND_ASSIGN : L_BITWISE_AND);
    break;
  case '*':
    type = consume(in, '=') ? L_MUL_ASSIGN : L_MUL;
    break;
  case '+':
    type = consume(in, '+')
      ? L_INCREMENT
      : (consume(in, '=') ? L_ADD_ASSIGN : L_ADD);
    break;
  case '-':
    type = consume(in, '-')
      ? L_DECREMENT
      : (consume(in, '=') ? L_SUB_ASSIGN : L_SUB);
    break;
  case '/':
    type = consume(in, '=') ? L_DIV_ASSIGN : L_DIV;
    break;
  case '=':
    type = consume(in, '=') ? L_EQ : L_ASSIGN;
    break;
  case '^':
    type = consume(in, '^')
      ? L_XOR
      : (consume(in, '=') ? L_BITWISE_XOR_ASSIGN : L_BITWISE_XOR);
    break;
  case '|':
    type = consume(in, '|')
      ? L_OR
      : (consume(in, '=') ? L_BITWISE_OR_ASSIGN : L_BITWISE_OR);
    break;
  case '(': type = L_OPEN_PAREN; break;
  case ')': type = L_CLOSE_PAREN; break;
  case ',': type = L_COMMA; break;
  case ':': type = L_COLON; break;
  case ';': type = L_SEMICOLON; break;
  case '?': type = L_TERNARY; break;
  case '[': type = L_OPEN_BRACKET; break;
  case ']': type = L_CLOSE_BRACKET; break;
  case '{': type = L_OPEN_BRACE; break;
  case '}': type = L_CLOSE_BRACE; break;
  case '~': type = L_BITWISE_NOT; break;
  default:
    fprintf(stderr, "unexpected character '%c' on line %zi\n",
      chr, in->line);
    exit(1);
  }
  return new_token(type);
}

token *scan(stream *in) {
  token *t = scan_inner(in);
  return t;
}
