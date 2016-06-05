import math
import type as types

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
L_EOF                = 0xff

def token_string(token_type):
  if isinstance(token_type, Token):
    token_type = token_type.token_type
  if token_type == L_ADD: return "ADD"
  elif token_type == L_ADD_ASSIGN: return "ADD_ASSIGN"
  elif token_type == L_AND: return "AND"
  elif token_type == L_AS: return "AS"
  elif token_type == L_ASHIFT: return "ASHIFT"
  elif token_type == L_ASHIFT_ASSIGN: return "ASHIFT_ASSIGN"
  elif token_type == L_ASSIGN: return "ASSIGN"
  elif token_type == L_BITWISE_AND: return "BITWISE_AND"
  elif token_type == L_BITWISE_AND_ASSIGN: return "BITWISE_AND_ASSIGN"
  elif token_type == L_BITWISE_NOT: return "BITWISE_NOT"
  elif token_type == L_BITWISE_OR: return "BITWISE_OR"
  elif token_type == L_BITWISE_OR_ASSIGN: return "BITWISE_OR_ASSIGN"
  elif token_type == L_BITWISE_XOR: return "BITWISE_XOR"
  elif token_type == L_BITWISE_XOR_ASSIGN: return "BITWISE_XOR_ASSIGN"
  elif token_type == L_BREAK: return "BREAK"
  elif token_type == L_CASE: return "CASE"
  elif token_type == L_CLASS: return "CLASS"
  elif token_type == L_CLOSE_BRACE: return "CLOSE_BRACE"
  elif token_type == L_CLOSE_BRACKET: return "CLOSE_BRACKET"
  elif token_type == L_CLOSE_PAREN: return "CLOSE_PAREN"
  elif token_type == L_COLON: return "COLON"
  elif token_type == L_COMMA: return "COMMA"
  elif token_type == L_CONTINUE: return "CONTINUE"
  elif token_type == L_DECREMENT: return "DECREMENT"
  elif token_type == L_DIV: return "DIV"
  elif token_type == L_DIV_ASSIGN: return "DIV_ASSIGN"
  elif token_type == L_DO: return "DO"
  elif token_type == L_DOT: return "DOT"
  elif token_type == L_ELSE: return "ELSE"
  elif token_type == L_EQ: return "EQ"
  elif token_type == L_FOR: return "FOR"
  elif token_type == L_GT: return "GT"
  elif token_type == L_GTE: return "GTE"
  elif token_type == L_IDENTIFIER: return "IDENTIFIER"
  elif token_type == L_IF: return "IF"
  elif token_type == L_INCREMENT: return "INCREMENT"
  elif token_type == L_LET: return "LET"
  elif token_type == L_LITERAL: return "LITERAL"
  elif token_type == L_LSHIFT: return "LSHIFT"
  elif token_type == L_LSHIFT_ASSIGN: return "LSHIFT_ASSIGN"
  elif token_type == L_LT: return "LT"
  elif token_type == L_LTE: return "LTE"
  elif token_type == L_MOD: return "MOD"
  elif token_type == L_MOD_ASSIGN: return "MOD_ASSIGN"
  elif token_type == L_MUL: return "MUL"
  elif token_type == L_MUL_ASSIGN: return "MUL_ASSIGN"
  elif token_type == L_NATIVE: return "NATIVE"
  elif token_type == L_NE: return "NE"
  elif token_type == L_NEW: return "NEW"
  elif token_type == L_NOT: return "NOT"
  elif token_type == L_NULL: return "NULL"
  elif token_type == L_OPEN_BRACE: return "OPEN_BRACE"
  elif token_type == L_OPEN_BRACKET: return "OPEN_BRACKET"
  elif token_type == L_OPEN_PAREN: return "OPEN_PAREN"
  elif token_type == L_OR: return "OR"
  elif token_type == L_RETURN: return "RETURN"
  elif token_type == L_RSHIFT: return "RSHIFT"
  elif token_type == L_RSHIFT_ASSIGN: return "RSHIFT_ASSIGN"
  elif token_type == L_SEMICOLON: return "SEMICOLON"
  elif token_type == L_STR_CONCAT: return "STR_CONCAT"
  elif token_type == L_STR_CONCAT_ASSIGN: return "STR_CONCAT_ASSIGN"
  elif token_type == L_SUB: return "SUB"
  elif token_type == L_SUB_ASSIGN: return "SUB_ASSIGN"
  elif token_type == L_TERNARY: return "TERNARY"
  elif token_type == L_TYPE: return "TYPE"
  elif token_type == L_TYPEDEF: return "TYPEDEF"
  elif token_type == L_VOID: return "VOID"
  elif token_type == L_WHILE: return "WHILE"
  elif token_type == L_XOR: return "XOR"
  else: return "UNKNOWN_TOKEN"

LN2 = math.log(2)

def count_bits(value):
  return 0 if value == 0 else int(math.log(value) / LN2) + 1

class Token(object):
  def __init__(self, line, offset, token_type):
    self.line = line
    self.offset = offset
    self.token_type = token_type

  def __str__(self):
    return "<Token %s>" % token_string(self.token_type)

  def __unicode__(self):
    return unicode(str(self))

class TypeToken(Token):
  def __init__(self, line, offset, literal_type):
    super(TypeToken, self).__init__(line, offset, L_TYPE)
    self.literal_type = literal_type

  def __str__(self):
    return "<TypeToken %d>" % self.literal_type

class IdToken(Token):
  def __init__(self, line, offset, name):
    super(IdToken, self).__init__(line, offset, L_IDENTIFIER)
    self.value = name

  def __str__(self):
    return "<IdToken %s>" % self.value

class LiteralToken(Token):
  def __init__(self, line, offset, literal_type):
    super(LiteralToken, self).__init__(line, offset, L_LITERAL)
    self.literal_type = literal_type

  def __str__(self):
    return "<LiteralToken %d>" % self.literal_type

class BoolToken(LiteralToken):
  def __init__(self, line, offset, value):
    super(IntToken, self).__init__(line, offset, types.T_BOOL)
    self.value = value

  def __str__(self):
    return "<BoolToken %s>" % ("true" if self.value else "false")

class IntToken(LiteralToken):
  def __init__(self, line, offset, value):
    bits = count_bits(value)
    if bits > 32: literal_type = types.T_U64
    elif bits > 16: literal_type = types.T_U32
    elif bits > 8: literal_type = types.T_U16
    else: literal_type = types.T_U8
    super(IntToken, self).__init__(line, offset, literal_type)
    self.value = value

  def __str__(self):
    return "<IntToken %d>" % self.value

class StrToken(LiteralToken):
  def __init__(self, line, offset, string):
    super(StrToken, self).__init__(line, offset, types.T_STRING)
    self.value = string

  def __str__(self):
    return "<StringToken ...>"

  def __unicode__(self):
    return u"<StringToken '%s'>" % self.value.encode('unicode_escape')

