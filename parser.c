#include "parser.h"

#include "lexer.h"
#include "xalloc.h"

#include "expression.h"

expression *parse_expression(parse_state *state);
expression *parse_expression_list(parse_state *state);
expression *expect_expression(parse_state *state);
statement *parse_statement(parse_state *state);
statement *next_statement(parse_state *state);
statement *expect_statement(parse_state *state);
block_statement *parse_block(parse_state *state);

static token *parse_peek(parse_state *state) {
  if (state->count) {
    return state->buffer[state->count - 1];
  }
  token *t = scan(state->in);
  // no more tokens
  if (t == NULL) {
    return NULL;
  }
  state->buffer[0] = t;
  state->count++;
  return t;
}

static token *parse_shift(parse_state *state) {
  if (state->count) {
    return state->buffer[--state->count];
  }
  return scan(state->in);
}

static void parse_push(parse_state *state, token *token) {
  if (state->count == PARSE_LOOKAHEAD_BUFFER) {
    fprintf(stderr, "lookahead buffer limit reached\n");
    exit(1);
  }
  state->buffer[state->count] = token;
  state->count++;
}

static token *accept(parse_state *state, token_type type) {
  token *t = parse_peek(state);
  if (t != NULL && t->type == type) {
    parse_shift(state);
    return t;
  }
  return NULL;
}

static bool consume(parse_state *state, token_type type) {
  token *t = accept(state, type);
  if (t == NULL) {
    return false;
  }
  free(t);
  return true;
}

static token *expect(parse_state *state, token_type type) {
  token *t = accept(state, type);
  if (t == NULL) {
    fprintf(stderr, "expected token %s on line %zi\n", token_string(type),
      state->in->line);
    exit(1);
  }
  return t;
}

static void require(parse_state *state, token_type type) {
  free(expect(state, type));
}

static void check_expression(parse_state *state, expression *e) {
  if (e == NULL) {
    fprintf(stderr, "expecting expression on line %zi\n", state->in->line);
    exit(1);
  }
}

static type *parse_type(token *t) {
  type *type = xmalloc(sizeof(*type));

  if (t->type == L_IDENTIFIER) {
    type->type = T_REF;
    type->symbol_name = t->symbol_name;
  } else {
    type->type = t->literal_type;
  }

  return type;
}

static bool parse_type_symbol(parse_state *state, type **ret_type,
    char **ret_symbol, token_type *next) {
  token *t = accept(state, L_IDENTIFIER);

  if (t == NULL) {
    t = accept(state, L_TYPE);

    if (t == NULL) {
      *ret_type = NULL;
      *ret_symbol = NULL;
      return false;
    }
  }

  token *symbol = accept(state, L_IDENTIFIER);

  if (symbol == NULL) {
    parse_push(state, t);
    *ret_type = NULL;
    *ret_symbol = NULL;
    return false;
  }

  if (next != NULL) {
    token *n = parse_peek(state);
    if (n->type != *next) {
      parse_push(state, symbol);
      parse_push(state, t);
      *ret_type = NULL;
      *ret_symbol = NULL;
      return false;
    }
  }

  *ret_type = parse_type(t);
  *ret_symbol = symbol->symbol_name;
  free(t);
  free(symbol);

  return true;
}

static void expect_type_symbol(parse_state *state, type **ret_type,
    char **ret_symbol) {
  token *t = accept(state, L_IDENTIFIER);

  if (t == NULL) {
    t = accept(state, L_TYPE);

    if (t == NULL) {
      fprintf(stderr, "expecting IDENTIFIER or TYPE on line %zi\n",
        state->in->line);
      exit(1);
    }
  }

  token *symbol = expect(state, L_IDENTIFIER);

  *ret_type = parse_type(t);
  *ret_symbol = symbol->symbol_name;
  free(t);
  free(symbol);
}

block_statement *ensure_block(statement *node) {
  if (node != NULL && node->type != S_BLOCK) {
    statement *parent = s_block(node);
    node->parent = parent;
    node = parent;
  }
  return (block_statement*) node;
}

expression *parse_function_expression(parse_state *state) {
  type *type;
  char *symbol;
  token_type next = L_OPEN_PAREN;
  if (!parse_type_symbol(state, &type, &symbol, &next)) {
    return NULL;
  }

  // basically guaranteed by parse_type_symbol
  require(state, L_OPEN_PAREN);

  function *function = xmalloc(sizeof(*function));
  function->function_name = symbol;
  function->return_type = type;
  function->argument_count = 0;

  if (!consume(state, L_CLOSE_PAREN)) {
    expect_type_symbol(state, &type, &symbol);

    if (type->type == T_VOID) {
      fprintf(stderr, "variable '%s' declared void on line %zi\n", symbol,
        state->in->line);
      exit(1);
    }

    argument *tail = xmalloc(sizeof(*tail));
    tail->argument_type = type;
    tail->symbol_name = symbol;
    function->argument = tail;
    function->argument_count = 1;

    while (!consume(state, L_CLOSE_PAREN)) {
      require(state, L_COMMA);

      expect_type_symbol(state, &type, &symbol);

      if (type->type == T_VOID) {
        fprintf(stderr, "variable '%s' declared void on line %zi\n", symbol,
          state->in->line);
        exit(1);
      }

      argument *tmp = xmalloc(sizeof(*tmp));
      tmp->argument_type = type;
      tmp->symbol_name = symbol;
      function->argument_count++;

      tail->next = tmp;
      tail = tmp;
    }

    tail->next = NULL;
  }

  require(state, L_OPEN_BRACE);
  function->body = parse_block(state);
  function->body->fn_parent = function;
  require(state, L_CLOSE_BRACE);

  return new_function_node(function);
}

expression *parse_base_expression(parse_state *state) {
  expression *e;

  if (consume(state, L_OPEN_PAREN)) {
    e = parse_expression(state);
    if (e == NULL) {
      fprintf(stderr, "unexpected token %s on line %zi\n",
        token_string(parse_peek(state)->type), state->in->line);
      exit(1);
    }

    require(state, L_CLOSE_PAREN);

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

  // function expression
  e = parse_function_expression(state);
  if (e != NULL) {
    return e;
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
      require(state, L_CLOSE_PAREN);
      left = new_call_node(left, right);
    } else if (consume(state, L_OPEN_BRACKET)) {
      expression *right = parse_expression(state);
      require(state, L_CLOSE_BRACKET);
      left = new_get_index_node(left, right);
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

    require(state, L_OPEN_PAREN);
    expression *args = parse_expression_list(state);
    require(state, L_CLOSE_PAREN);

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

      require(state, L_COLON);

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
      // TODO: do some processing, compact the structure and remove GET_FIELD, etc
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
  type *type;
  char *symbol;
  if (!parse_type_symbol(state, &type, &symbol, NULL)) {
    return NULL;
  }

  if (type->type == T_VOID) {
    fprintf(stderr, "variable '%s' declared void on line %zi\n",
      symbol, state->in->line);
    exit(1);
  }

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

  return s_define(type, head);
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
    require(state, L_SEMICOLON);

    return type == L_BREAK ? s_break(label) : s_continue(label);
  }
  case L_CLASS: {
    free(t);

    t = expect(state, L_IDENTIFIER);
    require(state, L_OPEN_BRACE);

    char *class_name = t->symbol_name;
    free(t);

    class *class = xmalloc(sizeof(*class));
    define_statement *define;

    class->class_name = class_name;

    size_t count = 0;
    while ((define = (define_statement*) parse_define_statement(state, false))
        != NULL) {
      require(state, L_SEMICOLON);

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

    require(state, L_CLOSE_BRACE);

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

    require(state, L_CLOSE_BRACE);

    return result;
  }
  case L_IF: {
    free(t);

    require(state, L_OPEN_PAREN);
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

    require(state, L_WHILE);

    require(state, L_OPEN_PAREN);
    expression *condition = expect_expression(state);
    require(state, L_CLOSE_PAREN);

    require(state, L_SEMICOLON);

    statement *loop = s_loop(S_DO_WHILE, condition, body);

    if (body) {
      body->parent = loop;
    }

    return loop;
  }
  case L_WHILE: {
    free(t);

    require(state, L_OPEN_PAREN);
    expression *condition = expect_expression(state);
    require(state, L_CLOSE_PAREN);

    block_statement *body = ensure_block(expect_statement(state));
    statement *loop = s_loop(S_WHILE, condition, body);

    if (body != NULL) {
      body->parent = loop;
    }

    return loop;
  }
  case L_FOR: {
    free(t);

    require(state, L_OPEN_PAREN);

    expression *condition, *iter;
    statement *init = parse_define_statement(state, true);
    if (init == NULL) {
      expression *e = parse_expression(state);
      if (e != NULL) {
        init = s_expression(e);
      }
    }

    require(state, L_SEMICOLON);

    condition = parse_expression(state);

    require(state, L_SEMICOLON);

    iter = parse_expression(state);

    require(state, L_CLOSE_PAREN);

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

  expression *e = parse_function_expression(state);
  if (e != NULL) {
    result = s_function(e->function);
    free(e);
    return result;
  }

  result = parse_define_statement(state, true);
  if (result != NULL) {
    require(state, L_SEMICOLON);
    return result;
  }

  e = parse_expression(state);
  if (e != NULL) {
    require(state, L_SEMICOLON);
    return s_expression(e);
  }

  return NULL;
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

block_statement *parse_block(parse_state *state) {
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
  state.buffer = xmalloc(sizeof(token*) * PARSE_LOOKAHEAD_BUFFER);
  block_statement *body = parse_block(&state);

  token *t = parse_peek(&state);
  if (t == NULL) {
    free(state.buffer);
    return body;
  }

  fprintf(stderr, "unexpected token %s on line %zi\n", token_string(t->type),
    state.in->line);
  exit(1);
}
