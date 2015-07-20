#include "analyze.h"

static void no_floats() __attribute__ ((noreturn)) {
  fprintf(stderr, "floats are not allowed\n");
  exit(1);
}

static void replace_cast(expression **value, cast_type cast, type_type target) {
  expression *new_cast = xmalloc(sizeof(*new_cast));
  new_cast->operation.type = O_CAST;
  new_cast->operation.cast_type = cast;
  new_cast->type = new_type(target);
  new_cast->value = *value;
  new_cast->next = (*value)->next;
  *value = new_cast;
}

void numeric_promotion(expression *value, bool allow_floats) {
  type_type ctype;
  cast_type cmethod;
  switch (value->type->type) {
  case T_F32:
  case T_F64:
  case T_F128:
    if (!allow_floats) no_floats();
    // fallthrough
  case T_S32:
  case T_S64:
  case T_U32:
  case T_U64:
    value->type = copy_type(value->value->type);
    return;
  case T_S8:
  case T_S16:
    ctype = T_S32;
    cmethod = O_SIGN_EXTEND;
    break;
  case T_U8:
  case T_U16:
    ctype = T_U32;
    cmethod = O_ZERO_EXTEND;
    break;
  default:
    fprintf(stderr, "must be a numeric expression\n");
    exit(1);
  }

  replace_cast(&value->value, cmethod, ctype);
  value->type = new_type(ctype);
}

type_type binary_numeric_promotion(expression *value, bool allow_floats) {
  expression *left = value->value, *right = left->next;
  type_type ltype = left->type->type, rtype = right->type->type;

  switch ((((uint8_t) ltype) << 8) | (uint8_t) rtype) {
  case (T_F32 << 8) | T_F64:
  case (T_F32 << 8) | T_F128:
  case (T_F64 << 8) | T_F128:
    if (!allow_floats) no_floats();
    replace_cast(&value->value, O_FLOAT_EXTEND, rtype);
    return rtype;
  case (T_F64 << 8) | T_F32:
  case (T_F128 << 8) | T_F32:
  case (T_F128 << 8) | T_F64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_FLOAT_EXTEND, ltype);
    return ltype;
  case (T_F32 << 8) | T_S8:
  case (T_F32 << 8) | T_S16:
  case (T_F32 << 8) | T_S32:
  case (T_F64 << 8) | T_S8:
  case (T_F64 << 8) | T_S16:
  case (T_F64 << 8) | T_S32:
  case (T_F64 << 8) | T_S64:
  case (T_F128 << 8) | T_S8:
  case (T_F128 << 8) | T_S16:
  case (T_F128 << 8) | T_S32:
  case (T_F128 << 8) | T_S64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_SIGNED_TO_FLOAT, ltype);
    return ltype;
  case (T_F32 << 8) | T_U8:
  case (T_F32 << 8) | T_U16:
  case (T_F32 << 8) | T_U32:
  case (T_F64 << 8) | T_U8:
  case (T_F64 << 8) | T_U16:
  case (T_F64 << 8) | T_U32:
  case (T_F64 << 8) | T_U64:
  case (T_F128 << 8) | T_U8:
  case (T_F128 << 8) | T_U16:
  case (T_F128 << 8) | T_U32:
  case (T_F128 << 8) | T_U64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_UNSIGNED_TO_FLOAT, ltype);
    return ltype;
  case (T_S8 << 8) | T_F32:
  case (T_S16 << 8) | T_F32:
  case (T_S32 << 8) | T_F32:
  case (T_S8 << 8) | T_F64:
  case (T_S16 << 8) | T_F64:
  case (T_S32 << 8) | T_F64:
  case (T_S64 << 8) | T_F64:
  case (T_S8 << 8) | T_F128:
  case (T_S16 << 8) | T_F128:
  case (T_S32 << 8) | T_F128:
  case (T_S64 << 8) | T_F128:
    if (!allow_floats) no_floats();
    replace_cast(&value->value, O_SIGNED_TO_FLOAT, rtype);
    return rtype;
  case (T_U8 << 8) | T_F32:
  case (T_U16 << 8) | T_F32:
  case (T_U32 << 8) | T_F32:
  case (T_U8 << 8) | T_F64:
  case (T_U16 << 8) | T_F64:
  case (T_U32 << 8) | T_F64:
  case (T_U64 << 8) | T_F64:
  case (T_U8 << 8) | T_F128:
  case (T_U16 << 8) | T_F128:
  case (T_U32 << 8) | T_F128:
  case (T_U64 << 8) | T_F128:
    if (!allow_floats) no_floats();
    replace_cast(&value->value, O_UNSIGNED_TO_FLOAT, rtype);
    return rtype;
  case (T_F32 << 8) | T_S64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_SIGNED_TO_FLOAT, T_F64);
    replace_cast(&value->value, O_FLOAT_EXTEND, T_F64);
    return T_F64;
  case (T_F32 << 8) | T_U64:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_UNSIGNED_TO_FLOAT, T_F64);
    replace_cast(&value->value, O_FLOAT_EXTEND, T_F64);
    return T_F64;
  case (T_S64 << 8) | T_F32:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_FLOAT_EXTEND, T_F64);
    replace_cast(&value->value, O_SIGNED_TO_FLOAT, T_F64);
    return T_F64;
  case (T_U64 << 8) | T_F32:
    if (!allow_floats) no_floats();
    replace_cast(&left->next, O_FLOAT_EXTEND, T_F64);
    replace_cast(&value->value, O_UNSIGNED_TO_FLOAT, T_F64);
    return T_F64;
  case (T_F128 << 8) | T_F128:
  case (T_F64 << 8) | T_F64:
  case (T_F32 << 8) | T_F32:
    if (!allow_floats) no_floats();
    // fallthrough
  case (T_S32 << 8) | T_S32:
  case (T_S64 << 8) | T_S64:
  case (T_U32 << 8) | T_U32:
  case (T_U64 << 8) | T_U64:
    // no casting necessary!
    return ltype;
  case (T_S8 << 8) | T_S8:
  case (T_S8 << 8) | T_S16:
  case (T_S16 << 8) | T_S8:
  case (T_S16 << 8) | T_S16:
  case (T_S8 << 8) | T_U8:
  case (T_S8 << 8) | T_U16:
  case (T_S16 << 8) | T_U8:
  case (T_S16 << 8) | T_U16:
  case (T_U8 << 8) | T_S8:
  case (T_U8 << 8) | T_S16:
  case (T_U16 << 8) | T_S8:
  case (T_U16 << 8) | T_S16:
    replace_cast(&left->next, O_SIGN_EXTEND, T_S32);
    replace_cast(&value->value, O_SIGN_EXTEND, T_S32);
    return T_S32;
  case (T_S8 << 8) | T_S32:
  case (T_S8 << 8) | T_S64:
  case (T_S16 << 8) | T_S32:
  case (T_S16 << 8) | T_S64:
  case (T_S32 << 8) | T_S64:
    replace_cast(&value->value, O_SIGN_EXTEND, rtype);
    return rtype;
  case (T_S32 << 8) | T_S8:
  case (T_S32 << 8) | T_S16:
  case (T_S64 << 8) | T_S8:
  case (T_S64 << 8) | T_S16:
  case (T_S64 << 8) | T_S32:
    replace_cast(&left->next, O_SIGN_EXTEND, ltype);
    return ltype;
  case (T_U8 << 8) | T_U8:
  case (T_U8 << 8) | T_U16:
  case (T_U16 << 8) | T_U8:
  case (T_U16 << 8) | T_U16:
    replace_cast(&left->next, O_ZERO_EXTEND, T_U32);
    replace_cast(&value->value, O_ZERO_EXTEND, T_U32);
    return T_U32;
  case (T_U8 << 8) | T_S32:
  case (T_U8 << 8) | T_S64:
  case (T_U8 << 8) | T_U32:
  case (T_U8 << 8) | T_U64:
  case (T_U16 << 8) | T_S32:
  case (T_U16 << 8) | T_S64:
  case (T_U16 << 8) | T_U32:
  case (T_U16 << 8) | T_U64:
  case (T_U32 << 8) | T_S64:
  case (T_U32 << 8) | T_U64:
    replace_cast(&value->value, O_ZERO_EXTEND, rtype);
    return rtype;
  case (T_S32 << 8) | T_U8:
  case (T_S32 << 8) | T_U16:
  case (T_S64 << 8) | T_U8:
  case (T_S64 << 8) | T_U16:
  case (T_S64 << 8) | T_U32:
  case (T_U32 << 8) | T_U8:
  case (T_U32 << 8) | T_U16:
  case (T_U64 << 8) | T_U8:
  case (T_U64 << 8) | T_U16:
  case (T_U64 << 8) | T_U32:
    replace_cast(&left->next, O_ZERO_EXTEND, ltype);
    return ltype;
  case (T_S8 << 8) | T_U32:
  case (T_S16 << 8) | T_U32:
    replace_cast(&left->next, O_REINTERPRET, T_S32);
    replace_cast(&value->value, O_SIGN_EXTEND, T_S32);
    return T_S32;
  case (T_S8 << 8) | T_U64:
  case (T_S16 << 8) | T_U64:
  case (T_S32 << 8) | T_U64:
    replace_cast(&left->next, O_REINTERPRET, T_S64);
    replace_cast(&value->value, O_SIGN_EXTEND, T_S64);
    return T_S64;
  case (T_S32 << 8) | T_U32:
  case (T_S64 << 8) | T_U64:
    replace_cast(&left->next, O_REINTERPRET, ltype);
    return ltype;
  case (T_U32 << 8) | T_S8:
  case (T_U32 << 8) | T_S16:
    replace_cast(&left->next, O_SIGN_EXTEND, T_S32);
    replace_cast(&value->value, O_REINTERPRET, T_S32);
    return T_S32;
  case (T_U64 << 8) | T_S8:
  case (T_U64 << 8) | T_S16:
  case (T_U64 << 8) | T_S32:
    replace_cast(&left->next, O_SIGN_EXTEND, T_S64);
    replace_cast(&value->value, O_REINTERPRET, T_S64);
    return T_S64;
  case (T_U32 << 8) | T_S32:
  case (T_U64 << 8) | T_S64:
    replace_cast(&value->value, O_REINTERPRET, rtype);
    return rtype;
  default:
    fprintf(stderr, "unsupported types in binary operation\n");
    exit(1);
  }
}

static void assert_condition(type *type) {
  switch (type->type) {
  case T_BOOL:
  case T_S8:
  case T_S16:
  case T_S32:
  case T_S64:
  case T_U8:
  case T_U16:
  case T_U32:
  case T_U64:
    break;
  default:
    fprintf(stderr, "invalid condition\n");
    exit(1);
  }
}

// mutates
static type *resolve_type(block_statement *block, type *type) {
  // TODO: static types as properties (module support)
  switch (type->type) {
  case T_ARRAY:
    resolve_type(block, type->arraytype);
    break;
  case T_BLOCKREF:
    for (argument *arg = type->blocktype; arg; arg = arg->next) {
      resolve_type(block, arg->argument_type);
    }
    break;
  case T_REF:
  case T_OBJECT: {
    // any type stored in the type symbol chain should already be resolved
    symbol_entry *entry = get_symbol(block, ST_TYPE, type->symbol_name);

    free(type->symbol_name);

    copy_type_into(entry->type, type);
  } break;
  default:
    break;
  }
  return type;
}
