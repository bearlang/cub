#include "parser.h"

#include "xalloc.h"

#include "expression.h"

expression *parse_expression_list(parse_state *state);
expression *expect_expression(parse_state *state);
statement *parse_statement(parse_state *state);
statement *next_statement(parse_state *state);
statement *expect_statement(parse_state *state);
static block_statement *parse_block(parse_state *state);

// TODO: support function references like `void(string) beep = `
/*static type *parse_type(token *t, bool array) {
  type *new_type = xmalloc(sizeof(*new_type));

  if (t->type == L_IDENTIFIER) {
    new_type->type = T_REF;
    new_type->symbol_name = t->symbol_name;
  } else {
    new_type->type = t->literal_type;
  }

  if (array) {
    type *outer = xmalloc(sizeof(*outer));
    outer->type = T_ARRAY;
    outer->arraytype = new_type;
    new_type = outer;
  }

  return new_type;
}

static void expect_type_symbol(parse_state *state, type **ret_type,
    char **ret_symbol, token_type *next) {
  token *t = accept(state, L_IDENTIFIER);

  if (t == NULL) {
    t = accept(state, L_TYPE);

    if (t == NULL) {
      fprintf(stderr, "expecting IDENTIFIER or TYPE on line %zi\n",
        state->in->line);
      exit(1);
    }
  }

  bool array = consume(state, L_OPEN_BRACKET);
  if (array) {
    expect_consume(state, L_CLOSE_BRACKET);
  }

  token *symbol = expect(state, L_IDENTIFIER);

  if (next) {
    expect_peek(state, *next);
  }

  *ret_type = parse_type(t, array);
  *ret_symbol = symbol->symbol_name;
  free(t);
  free(symbol);
}

static bool parse_type_symbol(parse_state *state, type **ret_type,
    char **ret_symbol, token_type *next) {
  token *t = accept(state, L_IDENTIFIER);

  if (t == NULL) {
    t = parse_peek(state);

    if (t == NULL || t->type != L_TYPE) {
      goto fail0;
    }

    expect_type_symbol(state, ret_type, ret_symbol, next);
    return true;
  }

  token *a = accept(state, L_OPEN_BRACKET), *b;
  if (a) {
    b = accept(state, L_CLOSE_BRACKET);

    // could be an index expression
    if (!b) {
      goto fail1;
    }
  }

  token *symbol = accept(state, L_IDENTIFIER);

  if (symbol == NULL) {
    goto fail2;
  }

  if (next) {
    token *n = parse_peek(state);
    if (!n || n->type != *next) {
      goto fail3;
    }
  }

  *ret_type = parse_type(t, !!a);
  *ret_symbol = symbol->symbol_name;
  free(t);
  if (a) {
    free(a);
    free(b);
  }
  free(symbol);

  return true;

fail3:
  parse_push(state, symbol);
fail2:
  if (a) {
    parse_push(state, b);
fail1: // pre: !!a
    parse_push(state, a);
  }
  parse_push(state, t);
fail0:
  *ret_type = NULL;
  *ret_symbol = NULL;
  return false;
}*/

block_statement *ensure_block(statement *node) {
  if (node != NULL && node->type != S_BLOCK) {
    statement *parent = s_block(node);
    node->parent = parent;
    node = parent;
  }
  return (block_statement*) node;
}

static type *parse_type(parse_state *state, type **first_type) {
  token *first_token = accept(state, L_TYPE);

  if (!first_token) {
    first_token = accept(state, L_IDENTIFIER);

    if (!first_token) {
      return NULL;
    }
  }

  type *return_type = new_type_from_token(first_token);

  while (consume(state, L_OPEN_PAREN)) {
    if (consume(state, L_CLOSE_PAREN)) {
      return_type = new_function_type(return_type, NULL);
    } else {
      do {
        type *param_type = parse_type(state, NULL);

        token *arg_name = parse_peek(state);

        if (arg_name && arg_name->type == L_IDENTIFIER) {
          if (return_type->blocktype->next || !first_type) {
            unexpected_token(arg_name, "expecting ','");
          } else {
            *first_type = param_type;
            type *real_return_type = return_type->blocktype->argument_type;
            free(return_type->blocktype);
            free(return_type);
            return real_return_type;
          }
        }
      } while (consume(state, L_COMMA));
    }
  }

  if (consume(state, L_OPEN_BRACKET)) {
    expect_consume(state, L_CLOSE_BRACKET);

    return_type = new_array_type(return_type);
  }

  return return_type;
}

void parse_args(parse_state *state, function *fn, type *first_type) {
  size_t count = 0;
  argument **tail = &fn->argument;

  do {
    type *arg_type;

    // ew
    if (first_type) {
      arg_type = first_type;
      first_type = NULL;
    } else {
      arg_type = parse_type(state, NULL);

      if (!arg_type) {
        break;
      }
    }

    token *arg_name = expect(state, L_IDENTIFIER);
    char *symbol = arg_name->symbol_name;
    free(arg_name);

    if (is_void(arg_type)) {
      fprintf(stderr, "variable '%s' declared void on line %zi\n", symbol,
        arg_name->line);
      exit(1);
    }

    argument *arg = xmalloc(sizeof(*arg));
    arg->argument_type = arg_type;
    arg->symbol_name = symbol;
    *tail = arg;
    tail = &arg->next;
    count++;
  } while (consume(state, L_COMMA));

  fn->argument_count = count;
  *tail = NULL;
}

// can expect, guaranteed to find a function if anything
function *parse_function(parse_state *state, bool allow_anonymous) {
  type *first_type = NULL;
  type *return_type = parse_type(state, &first_type);

  if (!return_type) {
    return NULL;
  }

  function *fn = xmalloc(sizeof(*fn));
  fn->function_name = NULL;
  fn->return_type = return_type;

  token *name_token = accept(state, L_IDENTIFIER);

  if (!name_token && !allow_anonymous) {
    unexpected_token(parse_peek(state), "expected function name");
  }

  if (name_token) {
    fn->function_name = name_token->symbol_name;
    free(name_token);
  }

  if (consume(state, L_OPEN_PAREN)) {
    parse_args(state, fn, first_type);
    expect_consume(state, L_CLOSE_PAREN);
  } else if (!name_token && !return_type->blocktype->next) {
    type *real_return_type = return_type->blocktype->argument_type;
    free(return_type->blocktype);
    free(return_type);
    return_type = real_return_type;

    fn->argument_count = 0;
    fn->argument = NULL;
  } else {
    unexpected_token(parse_peek(state), NULL);
  }

  expect_consume(state, L_OPEN_BRACE);
  fn->body = parse_block(state);
  fn->body->fn_parent = fn;
  expect_consume(state, L_CLOSE_BRACE);

  return fn;
}

expression *parse_base_expression(parse_state *state) {
  expression *e;

  if (consume(state, L_OPEN_PAREN)) {
    e = parse_expression(state);
    if (e == NULL) {
      unexpected_token(parse_peek(state), "expecting expression");
    }

    expect_consume(state, L_CLOSE_PAREN);

    return e;
  }

  token *t = accept(state, L_LITERAL);
  if (t != NULL) {
    e = new_literal_node(t->literal_type);

    // TODO: better way to hand-off literal values
    switch (t->literal_type) {
    case T_BOOL: e->value_bool = t->value_bool; break;
    case T_STRING: e->value_string = t->value_string; break;
    case T_U8: e->value_u8 = t->value_u8; break;
    case T_U16: e->value_u16 = t->value_u16; break;
    case T_U32: e->value_u32 = t->value_u32; break;
    case T_U64: e->value_u64 = t->value_u64; break;
    case T_OBJECT:
      // gotta be null
      break;
    default:
      fprintf(stderr, "literal handoff not implemented for this type\n");
      exit(1);
    }

    free(t);

    return e;
  }

  syntax_structure structure = detect_ambiguous_expression(state);

  if (!structure) {
    unexpected_token(parse_peek(state), "expecting function or expression");
  }

  if (structure == G_FUNCTION) {
    return new_function_node(parse_function(state, true));
  }

  t = accept(state, L_IDENTIFIER);

  if (t != NULL) {
    char *name = t->symbol_name;
    free(t);
    return new_get_symbol_node(name);
  }

  return NULL;
}

// TODO: post-increment
expression *parse_interact_expression(parse_state *state) {
  expression *left = parse_base_expression(state);

  if (left == NULL) {
    return NULL;
  }

  for (;;) {
    if (consume(state, L_DOT)) {
      token *right = expect(state, L_IDENTIFIER);
      left = new_get_field_node(left, right->symbol_name);
      free(right);
    } else if (consume(state, L_OPEN_PAREN)) {
      expression *right = parse_expression_list(state);
      expect_consume(state, L_CLOSE_PAREN);
      left = new_call_node(left, right);
    } else if (consume(state, L_OPEN_BRACKET)) {
      expression *right = parse_expression(state);
      if (!right) {
        unexpected_token(parse_peek(state), "expecting expression");
      }
      expect_consume(state, L_CLOSE_BRACKET);
      left = new_get_index_node(left, right);
    } else if (consume(state, L_INCREMENT)) {
      left = new_postfix_node(O_INCREMENT, left);
    } else if (consume(state, L_DECREMENT)) {
      left = new_postfix_node(O_DECREMENT, left);
    } else {
      return left;
    }
  }
}

expression *parse_unary_expression(parse_state *state) {
  token *t = parse_peek(state);
  expression *e, *literal;
  switch (t->type) {
  case L_INCREMENT:
  case L_DECREMENT: {
    numeric_type type = t->type == L_INCREMENT ? O_ADD : O_SUB;
    parse_shift(state);
    free(t);

    e = parse_unary_expression(state);
    check_expression(state, e);

    literal = new_literal_node(T_U8);
    literal->value_u8 = 1;

    return new_numeric_assign_node(type, e, literal);
  }
  case L_SUB:
    parse_shift(state);
    free(t);

    e = parse_unary_expression(state);
    check_expression(state, e);

    return new_negate_node(e);
  case L_NOT:
  case L_BITWISE_NOT: {
    bool bitwise = t->type == L_BITWISE_NOT;
    parse_shift(state);
    free(t);

    e = parse_unary_expression(state);
    check_expression(state, e);

    return new_not_node(bitwise, e);
  }
  case L_NEW: {
    parse_shift(state);
    free(t);

    t = expect(state, L_IDENTIFIER);

    char *class_name = t->symbol_name;
    free(t);

    expect_consume(state, L_OPEN_PAREN);
    expression *args = parse_expression_list(state);
    expect_consume(state, L_CLOSE_PAREN);

    return new_new_node(class_name, args);
  }
  default:
    return parse_interact_expression(state);
  }
}

expression *parse_muldiv_expression(parse_state *state) {
  expression *left = parse_unary_expression(state);

  if (left == NULL) {
    return NULL;
  }

  for (;;) {
    token *t = parse_peek(state);
    numeric_type type;
    switch (t->type) {
    case L_MUL: type = O_MUL; break;
    case L_DIV: type = O_DIV; break;
    case L_MOD: type = O_MOD; break;
    default:
      return left;
    }

    free(t);
    parse_shift(state);

    expression *right = parse_unary_expression(state);
    check_expression(state, right);
    left = new_numeric_node(type, left, right);
  }
}

expression *parse_addsub_expression(parse_state *state) {
  expression *left = parse_muldiv_expression(state);

  if (left == NULL) {
    return NULL;
  }

  for (;;) {
    token *t = parse_peek(state);
    numeric_type type;
    switch (t->type) {
    case L_ADD: type = O_ADD; break;
    case L_SUB: type = O_SUB; break;
    default:
      return left;
    }

    free(t);
    parse_shift(state);

    expression *right = parse_muldiv_expression(state);
    check_expression(state, right);
    left = new_numeric_node(type, left, right);
  }
}

expression *parse_shift_expression(parse_state *state) {
  expression *left = parse_addsub_expression(state);

  if (left == NULL) {
    return NULL;
  }

  for (;;) {
    token *t = parse_peek(state);
    shift_type type;
    switch (t->type) {
    case L_ASHIFT: type = O_ASHIFT; break;
    case L_LSHIFT: type = O_LSHIFT; break;
    case L_RSHIFT: type = O_RSHIFT; break;
    default:
      return left;
    }

    free(t);
    parse_shift(state);

    expression *right = parse_addsub_expression(state);
    check_expression(state, right);
    left = new_shift_node(type, left, right);
  }
}

expression *parse_comparison_expression(parse_state *state) {
  expression *left = parse_shift_expression(state);

  if (left == NULL) {
    return NULL;
  }

  for (;;) {
    token *t = parse_peek(state);
    compare_type type;
    switch (t->type) {
    case L_GT: type = O_GT; break;
    case L_GTE: type = O_GTE; break;
    case L_LT: type = O_LT; break;
    case L_LTE: type = O_LTE; break;
    default:
      return left;
    }

    free(t);
    parse_shift(state);

    expression *right = parse_shift_expression(state);
    check_expression(state, right);
    left = new_compare_node(type, left, right);
  }
}

expression *parse_concat_expression(parse_state *state) {
  expression *left = parse_comparison_expression(state);

  if (left == NULL) {
    return NULL;
  }

  while (consume(state, L_STR_CONCAT)) {
    expression *right = parse_comparison_expression(state);
    check_expression(state, right);

    left = new_str_concat_node(left, right);
  }

  return left;
}

expression *parse_equality_expression(parse_state *state) {
  expression *left = parse_concat_expression(state);

  if (left == NULL) {
    return NULL;
  }

  for (;;) {
    token *t = parse_peek(state);
    compare_type type;
    switch (t->type) {
    case L_EQ: type = O_EQ; break;
    case L_NE: type = O_NE; break;
    default:
      return left;
    }

    free(t);
    parse_shift(state);

    expression *right = parse_concat_expression(state);
    check_expression(state, right);
    left = new_compare_node(type, left, right);
  }
}

expression *parse_band_expression(parse_state *state) {
  expression *left = parse_equality_expression(state);

  if (left == NULL) {
    return NULL;
  }

  while (consume(state, L_BITWISE_AND)) {
    expression *right = parse_equality_expression(state);
    check_expression(state, right);

    left = new_numeric_node(O_BAND, left, right);
  }

  return left;
}

expression *parse_bxor_expression(parse_state *state) {
  expression *left = parse_band_expression(state);

  if (left == NULL) {
    return NULL;
  }

  while (consume(state, L_BITWISE_XOR)) {
    expression *right = parse_band_expression(state);
    check_expression(state, right);

    left = new_numeric_node(O_BXOR, left, right);
  }

  return left;
}

expression *parse_bor_expression(parse_state *state) {
  expression *left = parse_bxor_expression(state);

  if (left == NULL) {
    return NULL;
  }

  while (consume(state, L_BITWISE_OR)) {
    expression *right = parse_bxor_expression(state);
    check_expression(state, right);

    left = new_logic_node(O_BOR, left, right);
  }

  return left;
}

expression *parse_and_expression(parse_state *state) {
  expression *left = parse_bor_expression(state);

  if (left == NULL) {
    return NULL;
  }

  while (consume(state, L_AND)) {
    expression *right = parse_bor_expression(state);
    check_expression(state, right);

    left = new_logic_node(O_AND, left, right);
  }

  return left;
}

expression *parse_xor_expression(parse_state *state) {
  expression *left = parse_and_expression(state);

  if (left == NULL) {
    return NULL;
  }

  while (consume(state, L_XOR)) {
    expression *right = parse_and_expression(state);
    check_expression(state, right);

    left = new_logic_node(O_XOR, left, right);
  }

  return left;
}

expression *parse_or_expression(parse_state *state) {
  expression *left = parse_xor_expression(state);

  if (left == NULL) {
    return NULL;
  }

  while (consume(state, L_OR)) {
    expression *right = parse_xor_expression(state);
    check_expression(state, right);

    left = new_logic_node(O_OR, left, right);
  }

  return left;
}

expression *parse_operation_expression(parse_state *state) {
  expression *left = parse_or_expression(state), *right;

  if (left == NULL) {
    return NULL;
  }

  for (;;) {
    const token *t = parse_peek(state);

    switch (t->type) {
    case L_ASSIGN:
      parse_shift(state);

      right = parse_or_expression(state);
      check_expression(state, right);

      left = new_assign_node(left, right);
      break;
    case L_TERNARY:
      parse_shift(state);

      right = expect_expression(state);

      expect_consume(state, L_COLON);

      expression *last = parse_or_expression(state);
      check_expression(state, last);

      left = new_ternary_node(left, right, last);
      break;
    case L_STR_CONCAT_ASSIGN:
      parse_shift(state);

      right = parse_or_expression(state);
      check_expression(state, right);

      left = new_str_concat_assign_node(left, right);
      break;
    case L_ASHIFT_ASSIGN:
    case L_LSHIFT_ASSIGN:
    case L_RSHIFT_ASSIGN: {
      parse_shift(state);

      right = parse_or_expression(state);
      check_expression(state, right);

      shift_type type;
      switch (t->type) {
      case L_ASHIFT_ASSIGN: type = O_ASHIFT; break;
      case L_LSHIFT_ASSIGN: type = O_LSHIFT; break;
      case L_RSHIFT_ASSIGN: type = O_RSHIFT; break;
      default:
        // lol logic
        abort();
      }
      // TODO: do some processing, compact the structure and remove GET_FIELD
      left = new_shift_assign_node(type, left, right);
      break;
    }
    case L_ADD_ASSIGN:
    case L_BITWISE_AND_ASSIGN:
    case L_BITWISE_OR_ASSIGN:
    case L_BITWISE_XOR_ASSIGN:
    case L_DIV_ASSIGN:
    case L_MOD_ASSIGN:
    case L_MUL_ASSIGN:
    case L_SUB_ASSIGN: {
      parse_shift(state);

      right = parse_or_expression(state);
      check_expression(state, right);

      numeric_type type;
      switch (t->type) {
      case L_ADD_ASSIGN: type = O_ADD; break;
      case L_BITWISE_AND_ASSIGN: type = O_BAND; break;
      case L_BITWISE_OR_ASSIGN: type = O_BOR; break;
      case L_BITWISE_XOR_ASSIGN: type = O_BXOR; break;
      case L_DIV_ASSIGN: type = O_DIV; break;
      case L_MOD_ASSIGN: type = O_MOD; break;
      case L_MUL_ASSIGN: type = O_MUL; break;
      case L_SUB_ASSIGN: type = O_SUB; break;
      default:
        // lol logic
        abort();
      }
      left = new_numeric_assign_node(type, left, right);
      break;
    }
    default:
      return left;
    }

    free((void*) t);
  }
}

expression *parse_expression(parse_state *state) {
  return parse_operation_expression(state);
}

void free_expression(expression *value, bool free_strings) {
  switch (value->operation.type) {
  case O_BITWISE_NOT:
  case O_NEW:
  case O_NOT:
    free_expression(value->value, free_strings);
    break;
  case O_BLOCKREF:
    // not supposed to exist at this level
    abort();
  case O_CALL: // undefined type
    for (expression *sub = value->value; sub; ) {
      expression *next = sub->next;
      free_expression(sub, free_strings);
      sub = next;
    }
    goto end;
  case O_CAST:
    // do nothing!
    return;
  case O_COMPARE:
    free_expression(value->value->next, free_strings);
    free_expression(value->value, free_strings);
    free(value->type);
    break;
  case O_FUNCTION:
    // TODO: free function
    abort();
  case O_GET_FIELD: // undefined type
    free_expression(value->value, free_strings);
    if (free_strings) {
      free(value->symbol_name);
    }
    goto end;
  case O_GET_INDEX: // undefined type
  case O_NUMERIC: // undefined type
  case O_NUMERIC_ASSIGN: // undefined type
  case O_SHIFT: // undefined type
  case O_SHIFT_ASSIGN: // undefined type
    free_expression(value->value->next, free_strings);
    free_expression(value->value, free_strings);
    goto end;
  case O_GET_SYMBOL: // undefined type
    if (free_strings) {
      free(value->symbol_name);
    }
    goto end;
  case O_IDENTITY:
  case O_LOGIC:
  case O_STR_CONCAT:
  case O_STR_CONCAT_ASSIGN:
    free_expression(value->value->next, free_strings);
    free_expression(value->value, free_strings);
    break;
  case O_LITERAL:
    if (free_strings) {
      free(value->value_string);
    }
    break;
  case O_NEGATE: // undefined type
  case O_POSTFIX: // undefined type
    free_expression(value->value, free_strings);
    goto end;
  case O_SET_FIELD: // undefined type
    free_expression(value->value->next, free_strings);
    free_expression(value->value, free_strings);
    if (free_strings) {
      free(value->symbol_name);
    }
    goto end;
  case O_SET_INDEX: // undefined type
  case O_TERNARY: // undefined type
    free_expression(value->value->next->next, free_strings);
    free_expression(value->value->next, free_strings);
    free_expression(value->value, free_strings);
    goto end;
  case O_SET_SYMBOL: // undefined type
    free_expression(value->value, free_strings);
    if (free_strings) {
      free(value->symbol_name);
    }
    goto end;
  }
  if (value->type) {
    free_type(value->type);
  }
end:
  free(value);
}

expression *expect_expression(parse_state *state) {
  expression *e = parse_expression(state);
  check_expression(state, e);
  return e;
}

expression *parse_expression_list(parse_state *state) {
  expression *e = parse_expression(state), *tail = e;

  if (e == NULL) {
    return NULL;
  }

  while (consume(state, L_COMMA)) {
    tail->next = expect_expression(state);
  }

  return e;
}

// TODO: array types, modifiers (like const)
statement *parse_define_statement(parse_state *state, bool allow_init) {
  type *define_type = parse_type(state, NULL);

  token *symbol_token = accept(state, L_IDENTIFIER);

  if (!symbol_token) {
    return NULL;
  }

  char *symbol = symbol_token->symbol_name;
  if (is_void(define_type)) {
    fprintf(stderr, "variable '%s' declared void on line %zi\n",
      symbol, symbol_token->line);
    exit(1);
  }

  free(symbol_token);

  define_clause *head = xmalloc(sizeof(*head)), *tail = head;

  tail->symbol_name = symbol;

  tail->value = allow_init && consume(state, L_ASSIGN)
    ? expect_expression(state)
    : NULL;

  while (consume(state, L_COMMA)) {
    token *symbol_token = expect(state, L_IDENTIFIER);

    define_clause *tmp = xmalloc(sizeof(*tmp));

    tmp->symbol_name = symbol_token->symbol_name;
    free(symbol_token);

    tmp->value = allow_init && consume(state, L_ASSIGN)
      ? expect_expression(state)
      : NULL;

    tail->next = tmp;
    tail = tmp;
  }

  tail->next = NULL;

  return s_define(define_type, head);
}

statement *parse_statement(parse_state *state) {
  statement *result;
  token *t = parse_shift(state);

  if (t == NULL) {
    return NULL;
  }

  switch (t->type) {
  case L_SEMICOLON:
    free(t);
    return s_expression(NULL);
  case L_BREAK:
  case L_CONTINUE: {
    token_type type = t->type;
    free(t);

    char *label = NULL;
    t = accept(state, L_IDENTIFIER);
    if (t != NULL) {
      label = t->symbol_name;
      free(t);
    }
    expect_consume(state, L_SEMICOLON);

    return type == L_BREAK ? s_break(label) : s_continue(label);
  }
  case L_CLASS: {
    free(t);

    t = expect(state, L_IDENTIFIER);
    expect_consume(state, L_OPEN_BRACE);

    char *class_name = t->symbol_name;
    free(t);

    class *class = xmalloc(sizeof(*class));
    define_statement *define;

    class->class_name = class_name;

    size_t count = 0;
    while ((define = (define_statement*) parse_define_statement(state, false))
        != NULL) {
      expect_consume(state, L_SEMICOLON);

      define_clause *clause = define->clause;
      field *tail = NULL;
      while (clause != NULL) {
        field *field = xmalloc(sizeof(*field));
        field->field_type = define->symbol_type;
        field->symbol_name = clause->symbol_name;
        field->next = NULL;
        if (tail == NULL) {
          class->field = field;
        } else {
          tail->next = field;
        }
        tail = field;

        count++;
        define_clause *tmp = clause->next;
        free(clause);
        clause = tmp;
      }
      free(define);
    }

    class->field_count = count;

    expect_consume(state, L_CLOSE_BRACE);

    return s_class(class);
  }
  case L_RETURN:
    free(t);

    return s_return(consume(state, L_SEMICOLON)
      ? NULL
      : expect_expression(state));
  case L_OPEN_BRACE: {
    free(t);

    result = (statement*) parse_block(state);

    expect_consume(state, L_CLOSE_BRACE);

    return result;
  }
  case L_IF: {
    free(t);

    expect_consume(state, L_OPEN_PAREN);
    expression *condition = parse_expression(state);
    check_expression(state, condition);
    statement *first = expect_statement(state), *second = consume(state, L_ELSE)
      ? expect_statement(state) : NULL;

    result = s_if(condition, ensure_block(first), ensure_block(second));
    if (first) {
      first->parent = result;
    }
    if (second) {
      second->parent = result;
    }
    return result;
  }
  case L_DO: {
    free(t);

    block_statement *body = ensure_block(expect_statement(state));

    expect_consume(state, L_WHILE);

    expect_consume(state, L_OPEN_PAREN);
    expression *condition = expect_expression(state);
    expect_consume(state, L_CLOSE_PAREN);

    expect_consume(state, L_SEMICOLON);

    statement *loop = s_loop(S_DO_WHILE, condition, body);

    if (body) {
      body->parent = loop;
    }

    return loop;
  }
  case L_WHILE: {
    free(t);

    expect_consume(state, L_OPEN_PAREN);
    expression *condition = expect_expression(state);
    expect_consume(state, L_CLOSE_PAREN);

    block_statement *body = ensure_block(expect_statement(state));
    statement *loop = s_loop(S_WHILE, condition, body);

    if (body != NULL) {
      body->parent = loop;
    }

    return loop;
  }
  case L_FOR: {
    free(t);

    expect_consume(state, L_OPEN_PAREN);

    expression *condition, *iter;
    statement *init = parse_define_statement(state, true);
    if (init == NULL) {
      expression *e = parse_expression(state);
      if (e != NULL) {
        init = s_expression(e);
      }
    }

    expect_consume(state, L_SEMICOLON);

    condition = parse_expression(state);

    expect_consume(state, L_SEMICOLON);

    iter = parse_expression(state);

    expect_consume(state, L_CLOSE_PAREN);

    statement* body = expect_statement(state);

    if (condition == NULL) {
      condition = new_literal_node(T_BOOL);
      condition->value_bool = true;
    }

    block_statement *body_block = ensure_block(body);

    if (iter != NULL) {
      if (body_block == NULL) {
        body = s_block(s_expression(iter));
        body_block = (block_statement*) body;
      } else {
        body_block->next = s_expression(iter);
        body = s_block(body);
        body_block = (block_statement*) body;
        body_block->body->next->parent = body;
      }
      body_block->body->parent = body;
    }

    statement *loop = s_loop(S_WHILE, condition, body_block);

    if (body_block != NULL) {
      body_block->parent = loop;
    }

    if (init != NULL) {
      init->next = loop;
      result = s_block(init);
      init->parent = result;
      loop->parent = result;
    } else {
      result = loop;
    }

    return result;
  }
  case L_IDENTIFIER:
    if (consume(state, L_COLON)) {
      token *next = parse_peek(state);
      switch (next->type) {
      case L_DO:
      case L_WHILE:
      case L_FOR:
        result = parse_statement(state);
        if (result != NULL) {
          loop_statement *loop = (loop_statement*) result;
          loop->label = t->symbol_name;
          free(t);
          return result;
        }
      default:
        break;
      }

      fprintf(stderr, "expecting loop after label on line %zi\n",
        state->in->line);
      exit(1);
    }
    // fallthrough
  default:
    parse_push(state, t);
  }

  syntax_structure structure = detect_ambiguous_statement(state);

  switch (structure) {
  case G_DEFINE: {
    result = parse_define_statement(state, true);
    if (!result) {
      unexpected_token(parse_peek(state), "expecting define statement");
    }
    expect_consume(state, L_SEMICOLON);
    return result;
  }
  case G_EXPRESSION: {
    expression *e = parse_expression(state);
    if (!e) {
      unexpected_token(parse_peek(state), "expecting expression");
    }
    expect_consume(state, L_SEMICOLON);
    return s_expression(e);
  }
  case G_FUNCTION: {
    function *fn = parse_function(state, false);
    if (!fn) {
      unexpected_token(parse_peek(state), "expecting function declaration");
    }
    return s_function(fn);
  }
  default:
    return NULL;
  }
}

statement *next_statement(parse_state *state) {
  statement *s;

  while ((s = parse_statement(state)) != NULL && s->type == S_EXPRESSION &&
      ((expression_statement*) s)->value == NULL) {
    free(s);
  }

  return s;
}

statement *expect_statement(parse_state *state) {
  statement *s = parse_statement(state);

  if (s == NULL) {
    fprintf(stderr, "expected statement on line %zi\n", state->in->line);
    exit(1);
  }

  if (s->type == S_EXPRESSION && ((expression_statement*) s)->value == NULL) {
    free(s);
    return NULL;
  }

  return s;
}

static block_statement *parse_block(parse_state *state) {
  statement *result = s_block(NULL),
    *node = next_statement(state), *tail = node;

  block_statement *body = (block_statement*) result;

  if (node != NULL) {
    body->body = node;
    node->parent = result;

    while ((node = next_statement(state)) != NULL) {
      tail->next = node;
      tail = node;
      tail->parent = result;
    }
  }

  return body;
}

block_statement *parse(stream *in) {
  parse_state state;
  state.in = in;
  state.count = 0;
  state.cap = 16;
  state.buffer = xmalloc(sizeof(token*) * state.cap);
  block_statement *body = parse_block(&state);

  token *t = parse_peek(&state);
  if (t == NULL) {
    free(state.buffer);
    return body;
  }

  unexpected_token(parse_peek(&state), "expecting EOF");
}
