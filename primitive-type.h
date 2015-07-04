#ifndef PRIMITIVE_TYPE_H
#define PRIMITIVE_TYPE_H

// TODO: map, list, class, function
typedef enum type_type {
  T_ARRAY,
  T_BLOCKREF,
  T_BOOL,
  T_F32,
  T_F64,
  T_F128,
  // TODO: classes need to be entered into the symbol table before the fields
  // are processed
  T_OBJECT,
  T_REF,
  T_S8,
  T_S16,
  T_S32,
  T_S64,
  T_STRING,
  T_U8,
  T_U16,
  T_U32,
  T_U64,
  T_VOID
} type_type;

#endif
