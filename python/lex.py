import tokens
import type as types

ordinalset = set(map(unicode, xrange(0, 10)))
hexletterset = set(map(unichr, xrange(0x41, 0x47)))
alphaset = set(map(unichr, xrange(0x41, 0x5b)) +
  map(unichr, xrange(0x61, 0x7b)))
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
  u'\\': u'\\',
  u'b': u'\b',
  u'f': u'\f',
  u'n': u'\n',
  u'r': u'\r',
  u't': u'\t',
  u'v': u'\v'
}

class Scanner:
  def __init__(self, upstream):
    self.upstream = upstream
    self.buffer = []
    self.was_newline = False

    self.line = 1
    self.offset = 1

  def __iter__(self):
    return self

  def next(self):
    token = self.scan()
    if token is None: raise StopIteration
    return token

  def _token(self, token_type, offset = None):
    if offset is None: offset = self.offset
    return tokens.Token(self.line, offset, token_type)

  def _error(self, message):
    raise ParseError(self.line, self.offset, message)

  def _shift(self):
    if len(self.buffer): return self.buffer.pop()
    if self.was_newline:
      self.was_newline = False
      self.line += 1
      self.offset = 1
    value = self.upstream.read(1)
    if not len(value): return None
    if value == u'\n':
      self.was_newline = True
    self.offset += 1
    return value

  def _push(self, char):
    self.offset -= 1
    self.buffer.append(char)

  def scan(self):
    # handle whitespace and comments
    while True:
      char = self._shift()
      if char == u'/':
        char = self._shift()
        if char == u'/':
          while True:
            char = self._shift()
            if char == u'\n': break
        elif char == u'*':
          self.consume_comment()
        else:
          self._push(char)
          char = u'/'
          break
      elif char == u' ' or char == u'\t': continue
      elif char is None: return None
      elif char != u'\n': break

    # token offset start
    offset = self.offset - 1

    if char == u'.':
      char = self._shift()
      if char not in ordinalset:
        self._push(char)
        return self._token(tokens.L_DOT)
      self._push(char)
      self._push(u'.')
      return self.scan_number()

    if char in ordinalset:
      self._push(char)
      return self.scan_number()

    if char in idstartset:
      self._push(char)
      return self.scan_word()

    if char in u'\'"':
      # TODO: interpolation
      return self.scan_string(char)

    if char == u'#':
      # accept '='
      return self.token(tokens.L_STR_CONCAT_ASSIGN if self.accept(u'=')
        else tokens.L_STR_CONCAT)

    if char == u'!':
      # accept '='
      token_type = tokens.L_NE if self.accept(u'=') else tokens.L_NOT
    elif char == u'>':
      if self.accept(u'>'):
        if self.accept(u'>'):
          if self.accept(u'='): token_type = tokens.L_RSHIFT_ASSIGN
          else: token_type = tokens.L_RSHIFT
        elif self.accept(u'='): token_type = tokens.L_ASHIFT_ASSIGN
        else: token_type = tokens.L_ASHIFT
      elif self.accept(u'='): token_type = tokens.L_GTE
      else: token_type = tokens.L_GT
    elif char == u'<':
      if self.accept(u'<'):
        if self.accept(u'='): token_type = tokens.L_LSHIFT_ASSIGN
        else: token_type = tokens.L_LSHIFT
      elif self.accept(u'='): token_type = tokens.L_LTE
      else: token_type = tokens.L_LT
    elif char == u'%':
      token_type = tokens.L_MOD_ASSIGN if self.accept(u'=') else tokens.L_MOD
    elif char == u'&':
      if self.accept(u'&'): token_type = tokens.L_AND
      elif self.accept(u'='): token_type = tokens.L_BITWISE_AND_ASSIGN
      else: token_type = tokens.L_BITWISE_AND
    elif char == u'*':
      token_type = tokens.L_MUL_ASSIGN if self.accept(u'=') else tokens.L_MUL
    elif char == u'+':
      if self.accept(u'+'): token_type = tokens.L_INCREMENT
      elif self.accept(u'='): token_type = tokens.L_ADD_ASSIGN
      else: token_type = tokens.L_ADD
    elif char == u'-':
      if self.accept(u'-'): token_type = tokens.L_DECREMENT
      elif self.accept(u'='): token_type = tokens.L_SUB_ASSIGN
      else: token_type = tokens.L_SUB
    elif char == u'/':
      token_type = tokens.L_DIV_ASSIGN if self.accept(u'=') else tokens.L_DIV
    elif char == u'=':
      token_type = tokens.L_EQ if self.accept(u'=') else tokens.L_ASSIGN
    elif char == u'^':
      if self.accept(u'^'): token_type = tokens.L_XOR
      elif self.accept(u'='): token_type = tokens.L_BITWISE_XOR_ASSIGN
      else: token_type = tokens.L_BIWISE_XOR
    elif char == u'|':
      if self.accept(u'|'): token_type = tokens.L_OR
      elif self.accept(u'='): token_type = tokens.L_BITWISE_OR_ASSIGN
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
      self._error("unexpected character '%c'" % char)
    return self._token(token_type, offset)

  def scan_word(self):
    offset = self.offset
    word = self._shift()
    while True:
      char = self._shift()
      if char not in idset:
        self._push(char)
        break
      word += char

    # TODO: don't map yet, map in parser - context sensitive
    # maybe handle reserved words like "import" here?
    # (import is reserved for type names, what about bindings?)
    if word in keywordmap:
      return self._token(keywordmap[word], offset)

    if word in typemap:
      return tokens.TypeToken(self.line, offset, typemap[word])

    if word == u"true":
      token = tokens.BoolToken(self.line, offset, True)
    elif word == u"false":
      token = tokens.BoolToken(self.line, offset, False)
    elif word == u"null":
      token = self._token(tokens.L_LITERAL, offset)
      tokens.literal_type = T_OBJECT
    else:
      token = tokens.IdToken(self.line, offset, word)

    return token

  def scan_string(self, match):
    offset = self.offset
    string = ""
    while True:
      char = self._shift()
      if char == match: break
      if char is None:
        self._error("unexpected EOF, expected character")
      if char != u'\\':
        string += char
        continue
      char = self._shift()
      if char is None:
        self._error("unexpected EOF, expected character")
      if char in escapemap:
        string += escapemap[char]
        continue
      if char == u'u':
        raise NotImplementedError("unicode not implemented")
      if char == u'\n':
        self._error("expected escape sequence, found newline")
      if char == u'x':
        string += unichr((self.expectHexDigit() << 4) | self.expectHexDigit())
      else:
        self._error("unexpected character '%c', expected escape sequence" %
          char)

    return tokens.StrToken(self.line, offset, string)

  def scan_number(self):
    offset = self.offset
    token = self.scan_number_inner()
    tokens.offset = offset
    return token

  # TODO: use maybeDot for floats
  def scan_number_inner(self):
    char = self._shift()
    if char == u'0':
      char = self._shift()

      if char in u".eE":
        raise NotImplementedError("floating point literals are not implemented")

      if char == u'x':
        found = False
        value = 0

        while True:
          char = self._shift()
          if char in ordinalset:
            v2 = value << 4
            if v2 > 0xffffffffffffffff:
              self._error("hex literal too large")
            found = True
            value = v2 | (ord('0' - ord(char)))
            continue

          lower = char.lower()
          if lower in hexletterset:
            v2 = value << 4
            if v2 > 0xffffffffffffffff:
              self._error("hex literal too large")
            found = True
            value = v2 | (ord(lower) - 0x57)
            continue

          self._push(char)
          break

        if not found:
          self._error("expected [0-9a-f]")
        return self._new_integer_token(value)

      if char == u'b':
        found = False
        value = 0

        while True:
          char = self._shift()
          if char not in u"01":
            if char in ordinalset:
              self._error("bad binary literal")

            if not found:
              self._error("expected [01]")

            self._push(char)
            break

          v2 = value << 1
          if v2 > 0xffffffffffffffff:
            self._error("binary literal too large")

          found = True
          value = v2 | (ord(chr) - 0x30)

        return self._new_integer_token(value)

      if char == u'o':
        raise NotImplementedError("octal literals not implemented")

      if char not in ordinalset:
        if char in idstartset:
          self._error("identifiers may not begin with a digit")
        self._push(char)
        return self._new_integer_token(0)

      print "warning: octals are written as 0o7, not 07"

    if char == u'.':
      raise NotImplementedError("floating point literals are not implemented")

    value = 0

    while True:
      if char in u".eE":
        raise NotImplementedError("floating point literals are not implemented")

      if char not in ordinalset:
        if char in idstartset:
          self._error("identifiers may not begin with digits")
        break

      value = value * 10 + (ord(char) - 0x30)
      if value > 0xffffffffffffffff:
        self._error("decimal literal too large")

      char = self._shift()

    self._push(char)
    return self._new_integer_token(value)

  def expect_hex_digit(self):
    char = self._shift()

    if char is None:
      self._error("unexpected EOF, expected hex digit")

    c = ord(char)

    if c >= 0x30 and c <= 0x39: return c - 0x30
    c |= 0x20
    if c >= 0x61 and c <= 0x66: return c - 0x57

    self._error("unexpected character, expected hex digit")

  def accept(self, char):
    found = self._shift()
    if found == char: return True
    self._push(found)
    return False

  def consume_comment(self):
    while True:
      char = self._shift()

      if char is None:
        self._error("unexpected EOF, expected '*/'")

      if char == u'*':
        char = self._shift()
        if char == u'/': break

  def _new_integer_token(self, value):
    return tokens.IntToken(self.line, self.offset, value)
