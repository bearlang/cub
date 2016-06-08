import math
import pycub.types as types

L_ADD                = 0
L_ADD_ASSIGN         = 1
L_AND                = 2
L_AS                 = 3
L_ASHIFT             = 4
L_ASHIFT_ASSIGN      = 5
L_ASSIGN             = 6
L_BITWISE_AND        = 7
L_BITWISE_AND_ASSIGN = 8
L_BITWISE_NOT        = 9
L_BITWISE_OR         = 10
L_BITWISE_OR_ASSIGN  = 11
L_BITWISE_XOR        = 12
L_BITWISE_XOR_ASSIGN = 13
L_BREAK              = 14
L_CASE               = 15
L_CLASS              = 16
L_CLOSE_BRACE        = 17
L_CLOSE_BRACKET      = 18
L_CLOSE_PAREN        = 19
L_COLON              = 20
L_COMMA              = 21
L_CONTINUE           = 22
L_DECREMENT          = 23
L_DIV                = 24
L_DIV_ASSIGN         = 25
L_DO                 = 26
L_DOT                = 27
L_ELSE               = 28
L_EXTENDS            = 29
L_EQ                 = 30
L_FOR                = 31
L_GT                 = 32
L_GTE                = 33
L_IDENTIFIER         = 34
L_IF                 = 35
L_INCREMENT          = 36
L_LET                = 37
L_LITERAL            = 38
L_LSHIFT             = 39
L_LSHIFT_ASSIGN      = 40
L_LT                 = 41
L_LTE                = 42
L_MOD                = 43
L_MOD_ASSIGN         = 44
L_MUL                = 45
L_MUL_ASSIGN         = 46
L_NATIVE             = 47
L_NE                 = 48
L_NEW                = 49
L_NOT                = 50
L_NULL               = 51
L_OPEN_BRACE         = 52
L_OPEN_BRACKET       = 53
L_OPEN_PAREN         = 54
L_OR                 = 55
L_RETURN             = 56
L_RSHIFT             = 57
L_RSHIFT_ASSIGN      = 58
L_SEMICOLON          = 59
L_STR_CONCAT         = 60
L_STR_CONCAT_ASSIGN  = 61
L_SUB                = 62
L_SUB_ASSIGN         = 63
L_TERNARY            = 64
L_TYPE               = 65
L_TYPEDEF            = 66
L_VOID               = 67
L_WHILE              = 68
L_XOR                = 69
# L_EOF                = 0xff

token_strings = {
  L_ADD: "ADD",
  L_ADD_ASSIGN: "ADD_ASSIGN",
  L_AND: "AND",
  L_AS: "AS",
  L_ASHIFT: "ASHIFT",
  L_ASHIFT_ASSIGN: "ASHIFT_ASSIGN",
  L_ASSIGN: "ASSIGN",
  L_BITWISE_AND: "BITWISE_AND",
  L_BITWISE_AND_ASSIGN: "BITWISE_AND_ASSIGN",
  L_BITWISE_NOT: "BITWISE_NOT",
  L_BITWISE_OR: "BITWISE_OR",
  L_BITWISE_OR_ASSIGN: "BITWISE_OR_ASSIGN",
  L_BITWISE_XOR: "BITWISE_XOR",
  L_BITWISE_XOR_ASSIGN: "BITWISE_XOR_ASSIGN",
  L_BREAK: "BREAK",
  L_CASE: "CASE",
  L_CLASS: "CLASS",
  L_CLOSE_BRACE: "CLOSE_BRACE",
  L_CLOSE_BRACKET: "CLOSE_BRACKET",
  L_CLOSE_PAREN: "CLOSE_PAREN",
  L_COLON: "COLON",
  L_COMMA: "COMMA",
  L_CONTINUE: "CONTINUE",
  L_DECREMENT: "DECREMENT",
  L_DIV: "DIV",
  L_DIV_ASSIGN: "DIV_ASSIGN",
  L_DO: "DO",
  L_DOT: "DOT",
  L_ELSE: "ELSE",
  L_EQ: "EQ",
  L_FOR: "FOR",
  L_GT: "GT",
  L_GTE: "GTE",
  L_IDENTIFIER: "IDENTIFIER",
  L_IF: "IF",
  L_INCREMENT: "INCREMENT",
  L_LET: "LET",
  L_LITERAL: "LITERAL",
  L_LSHIFT: "LSHIFT",
  L_LSHIFT_ASSIGN: "LSHIFT_ASSIGN",
  L_LT: "LT",
  L_LTE: "LTE",
  L_MOD: "MOD",
  L_MOD_ASSIGN: "MOD_ASSIGN",
  L_MUL: "MUL",
  L_MUL_ASSIGN: "MUL_ASSIGN",
  L_NATIVE: "NATIVE",
  L_NE: "NE",
  L_NEW: "NEW",
  L_NOT: "NOT",
  L_NULL: "NULL",
  L_OPEN_BRACE: "OPEN_BRACE",
  L_OPEN_BRACKET: "OPEN_BRACKET",
  L_OPEN_PAREN: "OPEN_PAREN",
  L_OR: "OR",
  L_RETURN: "RETURN",
  L_RSHIFT: "RSHIFT",
  L_RSHIFT_ASSIGN: "RSHIFT_ASSIGN",
  L_SEMICOLON: "SEMICOLON",
  L_STR_CONCAT: "STR_CONCAT",
  L_STR_CONCAT_ASSIGN: "STR_CONCAT_ASSIGN",
  L_SUB: "SUB",
  L_SUB_ASSIGN: "SUB_ASSIGN",
  L_TERNARY: "TERNARY",
  L_TYPE: "TYPE",
  L_TYPEDEF: "TYPEDEF",
  L_VOID: "VOID",
  L_WHILE: "WHILE",
  L_XOR: "XOR"
}

def token_string(token_type):
  if isinstance(token_type, Token):
    token_type = token_type.token_type
  if token_type in token_strings:
    return token_strings[token_type]
  return "UNKNOWN_TOKEN"

def count_bits(value):
  return 0 if value == 0 else int(math.log2(value)) + 1

class Token(object):
  def __init__(self, line, offset, token_type):
    self.line = line
    self.offset = offset
    self.token_type = token_type

  def __repr__(self):
    return "<Token %s>" % token_string(self.token_type)

  def __eq__(self, other):
    return isinstance(other, Token) and self.token_type == other.token_type and\
      self.line == other.line and self.offset == other.offset

class TypeToken(Token):
  def __init__(self, line, offset, literal_type):
    super(TypeToken, self).__init__(line, offset, L_TYPE)
    self.literal_type = literal_type

  def __repr__(self):
    return "<TypeToken %s>" % types.type_to_str(self.literal_type)

  def __eq__(self, other):
    return super(TypeToken, self).__eq__(other) and \
      isinstance(other, TypeToken) and self.literal_type == other.literal_type

class IdToken(Token):
  def __init__(self, line, offset, name):
    super(IdToken, self).__init__(line, offset, L_IDENTIFIER)
    self.value = name

  def __repr__(self):
    return "<IdToken %s>" % self.value

  def __eq__(self, other):
    return super(IdToken, self).__eq__(other) and isinstance(other, IdToken) \
      and self.value == other.value

class LiteralToken(Token):
  def __init__(self, line, offset, literal_type):
    super(LiteralToken, self).__init__(line, offset, L_LITERAL)
    self.literal_type = literal_type

  def __repr__(self):
    return "<LiteralToken %d>" % self.literal_type

  def __eq__(self, other):
    return super(LiteralToken, self).__eq__(other) and \
      isinstance(other, LiteralToken) and \
      self.literal_type == other.literal_type

class BoolToken(LiteralToken):
  def __init__(self, line, offset, value):
    super(IntToken, self).__init__(line, offset, types.T_BOOL)
    self.value = value

  def __repr__(self):
    return "<BoolToken %s>" % ("true" if self.value else "false")

  def __eq__(self, other):
    return super(BoolToken, self).__eq__(other) and \
      isinstance(other, BoolToken) and self.value == other.value

class IntToken(LiteralToken):
  def __init__(self, line, offset, value):
    bits = count_bits(value)
    if bits > 32: literal_type = types.T_U64
    elif bits > 16: literal_type = types.T_U32
    elif bits > 8: literal_type = types.T_U16
    else: literal_type = types.T_U8
    super(IntToken, self).__init__(line, offset, literal_type)
    self.value = value

  def __repr__(self):
    return "<IntToken %d>" % self.value

  def __eq__(self, other):
    return super(IntToken, self).__eq__(other) and \
      isinstance(other, IntToken) and self.value == other.value

class StrToken(LiteralToken):
  def __init__(self, line, offset, string):
    super(StrToken, self).__init__(line, offset, types.T_STRING)
    self.value = string

  def __repr__(self):
    return "<StringToken '%s'>" % self.value

  def __eq__(self, other):
    return super(StrToken, self).__eq__(other) and \
      isinstance(other, StrToken) and self.value == other.value
