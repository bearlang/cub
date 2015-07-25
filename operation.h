#ifndef OPERATION_H
#define OPERATION_H

typedef enum {
  O_BITWISE_NOT,       // 1
  O_BLOCKREF,          // 1
  O_CALL,              // UNUSED
  O_CAST,              // 2
  O_COMPARE,           // 2
  O_FUNCTION,          // UNUSED
  O_GET_FIELD,         // 2
  O_GET_INDEX,         // 2
  O_GET_SYMBOL,        // 1
  O_IDENTITY,          // 2
  O_INSTANCEOF,        // 2
  O_LITERAL,           // 0
  O_LOGIC,             // 2
  O_NATIVE,
  O_NEGATE,            // 1
  O_NEW,               // 1
  O_NEW_ARRAY,
  O_NOT,               // 1
  O_NUMERIC,           // 2
  O_NUMERIC_ASSIGN,    // UNUSED
  O_POSTFIX,           // 1
  O_SET_FIELD,         // 3
  O_SET_INDEX,         // 3
  O_SET_SYMBOL,        // UNUSED
  O_SHIFT,             // 2
  O_SHIFT_ASSIGN,      // UNUSED
  O_STR_CONCAT,        // 2
  O_STR_CONCAT_ASSIGN, // UNUSED
  O_TERNARY            // UNUSED
} operation_type;

// TODO: truncation, extension, reinterpretation, etc...
typedef enum {
  // between objects
  O_DOWNCAST,
  O_UPCAST,
  // between floats
  O_FLOAT_EXTEND,
  O_FLOAT_TRUNCATE,
  // from floats to integers
  O_FLOAT_TO_SIGNED,
  O_FLOAT_TO_UNSIGNED,
  // from integers to floats
  O_SIGNED_TO_FLOAT,
  O_UNSIGNED_TO_FLOAT,
  // between integers
  O_SIGN_EXTEND,
  O_TRUNCATE,
  O_ZERO_EXTEND,
  // bitwise
  O_REINTERPRET
} cast_type;

typedef enum {
  O_EQ,
  O_GT,
  O_GTE,
  O_LT,
  O_LTE,
  O_NE
} compare_type;

typedef enum {
  O_AND,
  O_OR,
  O_XOR
} logic_type;

typedef enum {
  O_ADD,
  O_BAND,
  O_BOR,
  O_BXOR,
  O_DIV,
  O_MOD,
  O_MUL,
  O_SUB
} numeric_type;

typedef enum {
  O_INCREMENT,
  O_DECREMENT
} postfix_type;

typedef enum {
  O_ASHIFT,
  O_LSHIFT,
  O_RSHIFT
} shift_type;

typedef struct {
  operation_type type;
  union {
    cast_type cast_type;
    compare_type compare_type;
    logic_type logic_type;
    numeric_type numeric_type;
    postfix_type postfix_type;
    shift_type shift_type;
  };
} operation;

#endif
