import pycub.tokens as tokens
import pycub.types as types
from pycub.reader import CharReader

ordinalset = set(map(str, range(0, 10)))
hexletterset = set(map(chr, range(0x41, 0x47)))
alphaset = set(map(chr, range(0x41, 0x5b))) | \
  set(map(chr, range(0x61, 0x7b)))
idstartset = alphaset.union(set(['_']))
idset = ordinalset.union(idstartset)

keywordmap = {
  u"as":       tokens.L_AS,
  u"break":    tokens.L_BREAK,
  u"case":     tokens.L_CASE,
  u"class":    tokens.L_CLASS,
  u"continue": tokens.L_CONTINUE,
  u"do":       tokens.L_DO,
  u"else":     tokens.L_ELSE,
  u"extends":  tokens.L_EXTENDS,
  u"for":      tokens.L_FOR,
  u"if":       tokens.L_IF,
  u"let":      tokens.L_LET,
  u"native":   tokens.L_NATIVE,
  u"new":      tokens.L_NEW,
  u"return":   tokens.L_RETURN,
  u"while":    tokens.L_WHILE
}

typemap = {
  u"bool":   types.T_BOOL,
  u"f128":   types.T_F128,
  u"f32":    types.T_F32,
  u"f64":    types.T_F64,
  u"s16":    types.T_S16,
  u"s32":    types.T_S32,
  u"s64":    types.T_S64,
  u"s8":     types.T_S8,
  u"string": types.T_STRING,
  u"u16":    types.T_U16,
  u"u32":    types.T_U32,
  u"u64":    types.T_U64,
  u"u8":     types.T_U8,
  u"void":   types.T_VOID
}

escapemap = {
  u'"': u'"',
  u'0': u'\0',
  u"'": u"'",
  u'#': u'#',
  u'\\': u'\\',
  u'b': u'\b',
  u'f': u'\f',
  u'n': u'\n',
  u'r': u'\r',
  u't': u'\t',
  u'v': u'\v'
}

class Scanner:
  # public api
  def __init__(self, upstream):
    self.upstream = upstream
    self.reader = CharReader(upstream)

  def __iter__(self):
    return self

  def __next__(self):
    token = self.scan()
    if token is None: raise StopIteration
    return token

  def scan(self):
    reader = self.reader

    # handle whitespace and comments
    while True:
      # token offset start
      offset = reader.offset
      char = reader.pop()
      if char == u'/':
        char = reader.peek()
        if char == u'/':
          reader.consume_line()
        elif char == u'*':
          reader.pop()
          self.consume_comment()
        else:
          char = u'/'
          break
      elif char == u' ' or char == u'\t': continue
      elif char is None: return None
      elif char != u'\n': break

    def accept(char):
      return reader.accept(char) is not None

    if char == u'.':
      if reader.peek() not in ordinalset:
        return self.token(tokens.L_DOT, offset)
      reader.push(char)
      return self.scan_number()

    if char in ordinalset:
      reader.push(char)
      return self.scan_number()

    if char in idstartset:
      reader.push(char)
      return self.scan_word()

    if char in u'\'"':
      return self.scan_string(char, offset)

    if char == u'#':
      # accept '='
      return self.token(tokens.L_STR_CONCAT_ASSIGN if accept(u'=')
        else tokens.L_STR_CONCAT, offset)

    if char == u'!':
      # accept '='
      token_type = tokens.L_NE if accept(u'=') else tokens.L_NOT
    elif char == u'>':
      if accept(u'>'):
        if accept(u'>'):
          if accept(u'='): token_type = tokens.L_RSHIFT_ASSIGN
          else: token_type = tokens.L_RSHIFT
        elif accept(u'='): token_type = tokens.L_ASHIFT_ASSIGN
        else: token_type = tokens.L_ASHIFT
      elif accept(u'='): token_type = tokens.L_GTE
      else: token_type = tokens.L_GT
    elif char == u'<':
      if accept(u'<'):
        if accept(u'='): token_type = tokens.L_LSHIFT_ASSIGN
        else: token_type = tokens.L_LSHIFT
      elif accept(u'='): token_type = tokens.L_LTE
      else: token_type = tokens.L_LT
    elif char == u'%':
      token_type = tokens.L_MOD_ASSIGN if accept(u'=') else tokens.L_MOD
    elif char == u'&':
      if accept(u'&'): token_type = tokens.L_AND
      elif accept(u'='): token_type = tokens.L_BITWISE_AND_ASSIGN
      else: token_type = tokens.L_BITWISE_AND
    elif char == u'*':
      token_type = tokens.L_MUL_ASSIGN if accept(u'=') else tokens.L_MUL
    elif char == u'+':
      if accept(u'+'): token_type = tokens.L_INCREMENT
      elif accept(u'='): token_type = tokens.L_ADD_ASSIGN
      else: token_type = tokens.L_ADD
    elif char == u'-':
      if accept(u'-'): token_type = tokens.L_DECREMENT
      elif accept(u'='): token_type = tokens.L_SUB_ASSIGN
      else: token_type = tokens.L_SUB
    elif char == u'/':
      token_type = tokens.L_DIV_ASSIGN if accept(u'=') else tokens.L_DIV
    elif char == u'=':
      token_type = tokens.L_EQ if accept(u'=') else tokens.L_ASSIGN
    elif char == u'^':
      if accept(u'^'): token_type = tokens.L_XOR
      elif accept(u'='): token_type = tokens.L_BITWISE_XOR_ASSIGN
      else: token_type = tokens.L_BIWISE_XOR
    elif char == u'|':
      if accept(u'|'): token_type = tokens.L_OR
      elif accept(u'='): token_type = tokens.L_BITWISE_OR_ASSIGN
      else: token_type = tokens.L_BITWISE_OR
    elif char == u'(': token_type = tokens.L_OPEN_PAREN
    elif char == u')': token_type = tokens.L_CLOSE_PAREN
    elif char == u',': token_type = tokens.L_COMMA
    elif char == u':': token_type = tokens.L_COLON
    elif char == u';': token_type = tokens.L_SEMICOLON
    elif char == u'?': token_type = tokens.L_TERNARY
    elif char == u'[': token_type = tokens.L_OPEN_BRACKET
    elif char == u']': token_type = tokens.L_CLOSE_BRACKET
    elif char == u'{': token_type = tokens.L_OPEN_BRACE
    elif char == u'}': token_type = tokens.L_CLOSE_BRACE
    elif char == u'~': token_type = tokens.L_BITWISE_NOT
    else:
      self.error("unexpected character '%c'" % char)
    return self.token(token_type, offset)

  # internal api
  def scan_word(self):
    reader = self.reader
    offset = reader.offset
    word = reader.pop()
    while True:
      char = reader.pop()
      if char not in idset:
        reader.push(char)
        break
      word += char

    # TODO: don't map yet, map in parser - context sensitive
    # maybe handle reserved words like "import" here?
    # (import is reserved for type names, what about bindings?)
    if word in keywordmap:
      return self.token(keywordmap[word], offset)

    if word in typemap:
      return tokens.TypeToken(reader.line, offset, typemap[word])

    if word == u"true":
      token = tokens.BoolToken(reader.line, offset, True)
    elif word == u"false":
      token = tokens.BoolToken(reader.line, offset, False)
    elif word == u"null":
      token = self.token(tokens.L_LITERAL, offset)
      tokens.literal_type = T_OBJECT
    else:
      token = tokens.IdToken(reader.line, offset, word)

    return token

  # TODO: interpolation
  def scan_string(self, match, offset):
    reader = self.reader
    string = u""
    while True:
      char = reader.pop()
      if char == match: break
      if char is None:
        self.error("unexpected EOF, expected character")
      if char != u'\\':
        string += char
        continue
      char = reader.pop()
      if char is None:
        self.error("unexpected EOF, expected character")
      if char in escapemap:
        string += escapemap[char]
        continue
      if char == u'u':
        raise NotImplementedError("unicode not implemented")
      if char == u'\n':
        self.error("expected escape sequence, found newline")
      if char == u'x':
        string += chr((self.expectHexDigit() << 4) | self.expectHexDigit())
      else:
        self.error("unexpected character '%c', expected escape sequence" %
          char)

    return tokens.StrToken(reader.line, offset, string)

  def scan_number(self):
    offset = self.reader.offset
    token = self.scan_number_inner()
    tokens.offset = offset
    return token

  # TODO: use maybeDot for floats
  def scan_number_inner(self):
    reader = self.reader

    def integer_token(value):
      return tokens.IntToken(reader.line, reader.offset, value)

    char = reader.pop()
    if char == u'0':
      char = reader.pop()

      if char in u".eE":
        raise NotImplementedError("floating point literals are not implemented")

      if char == u'x':
        found = False
        value = 0

        while True:
          char = reader.pop()
          if char in ordinalset:
            v2 = value << 4
            if v2 > 0xffffffffffffffff:
              self.error("hex literal too large")
            found = True
            value = v2 | (ord('0' - ord(char)))
            continue

          lower = char.lower()
          if lower in hexletterset:
            v2 = value << 4
            if v2 > 0xffffffffffffffff:
              self.error("hex literal too large")
            found = True
            value = v2 | (ord(lower) - 0x57)
            continue

          reader.push(char)
          break

        if not found:
          self.error("expected [0-9a-f]")
        return integer_token(value)

      if char == u'b':
        found = False
        value = 0

        while True:
          char = reader.pop()
          if char not in u"01":
            if char in ordinalset:
              self.error("bad binary literal")

            if not found:
              self.error("expected [01]")

            reader.push(char)
            break

          v2 = value << 1
          if v2 > 0xffffffffffffffff:
            self.error("binary literal too large")

          found = True
          value = v2 | (ord(chr) - 0x30)

        return integer_token(value)

      if char == u'o':
        raise NotImplementedError("octal literals not implemented")

      if char not in ordinalset:
        if char in idstartset:
          self.error("identifiers may not begin with a digit")
        reader.push(char)
        return integer_token(0)

      print("warning: octals are written as 0o7, not 07")

    if char == '.':
      raise NotImplementedError("floating point literals are not implemented")

    value = 0

    while True:
      if char in u".eE":
        raise NotImplementedError("floating point literals are not implemented")

      if char not in ordinalset:
        if char in idstartset:
          self.error("identifiers may not begin with digits")
        break

      value = value * 10 + (ord(char) - 0x30)
      if value > 0xffffffffffffffff:
        self.error("decimal literal too large")

      char = reader.pop()

    reader.push(char)
    return integer_token(value)

  def expect_hex_digit(self):
    char = self.reader.pop()

    if char is None:
      self.error("unexpected EOF, expected hex digit")

    c = ord(char)

    if c >= 0x30 and c <= 0x39: return c - 0x30
    c |= 0x20
    if c >= 0x61 and c <= 0x66: return c - 0x57

    self.error("unexpected character, expected hex digit")

  def consume_comment(self):
    reader = self.reader
    depth = 1
    while True:
      char = reader.pop()

      if char is None:
        self.error("unexpected EOF, expected '*/'")

      # don't need to check for None because we'll just catch it in the next
      # iteration
      if char == u'*':
        if reader.pop() == u'/':
          depth -= 1
          if depth == 0: break
      elif char == u'/':
        if reader.pop() == u'*': depth += 1

  def token(self, token_type, offset = None):
    if offset is None: offset = self.reader.offset
    return tokens.Token(self.reader.line, offset, token_type)

  def error(self, message):
    raise ParseError(self.reader.line, self.reader.offset, message)
