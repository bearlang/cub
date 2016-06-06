import collections

# assumes no access to reader while lookahead still in use
class Lookahead(object):
  def __init__(self, reader):
    self.reader = reader
    self.buffer = reader.buffer
    self.iter = iter(self.buffer)
    self.iterpeek = None

  def __iter__(self):
    return self

  def _pull(self):
    if self.iter:
      try:
        return next(self.iter)
      except StopIteration:
        self.iter = None

    item = next(self.reader.gen)
    self.buffer.append(item)
    return item

  def __next__(self):
    if self.iterpeek:
      item, self.iterpeek = self.iterpeek, None
      return item

    return self._pull()

  def next(self):
    return next(self)

  def peek(self):
    if self.iterpeek:
      return self.iterpeek
    item = self._pull()
    self.iterpeek = item
    return item

def _matcher_fail():
  raise Exception("matcher expectation failed")

def _custom_matcher_fail(string):
  def _fail():
    raise Exception(string)
  return _fail

# earlier items at left end, later items at right end
class Reader(object):
  # fail is called when an expectation fails
  # matches is a function that takes two items and returns whether they match
  #   or a string representing the attribute to compare to the given other item
  def __init__(self, gen, fail=None, matches=None):
    self.gen = gen
    self.buffer = collections.deque()

    # optional args
    if fail is None:
      self.fail = _matcher_fail
    elif isinstance(fail, str):
      self.fail = _custom_matcher_fail(fail)
    else:
      self.fail = fail

    if matches is None:
      self.matches = lambda a, b: a == b
    elif isinstance(matches, str):
      self.matches = lambda a, b: getattr(a, matches) == b
    else:
      self.matches = matches

  def __iter__(self):
    return self

  def _pop(self, raisestop=False):
    if len(self.buffer):
      return self.buffer.popleft()
    if raisestop:
      return next(self.gen)
    try:
      return next(self.gen)
    except StopIteration:
      return None

  def __next__(self):
    return self._pop(True)

  def peek(self):
    if len(self.buffer):
      return self.buffer[0]
    try:
      item = next(self.gen)
    except StopIteration:
      return None
    self.buffer.append(item)
    return item

  def pop(self):
    return self._pop()

  def push(self, item):
    # ignore pushing None, probably pushing back an unmatched "character"
    if item is not None:
      self.buffer.appendleft(item)

  def lookahead(self):
    return Lookahead(self)

  # fancy matching methods
  def accept(self, other):
    item = self.peek()
    if item is not None and self.matches(item, other):
      return self.pop()
    return None

  def expect(self, other):
    item = self.accept(other)
    if item is not None:
      return item
    self.fail()

  # match is used when terminator is not immediately found, no_match is called
  # if terminator is immediately found
  def accept_terminated(self, terminator, match=None, no_match=None):
    if self.accept(terminator) is not None:
      return no_match() if callable(no_match) else no_match
    if match is None:
      value = self.pop()
    else:
      value = self.accept(match)
    if value is not None:
      if self.accept(terminator) is not None:
        return value
      self.push(value)
    return None

  # match is called when terminator is not immediately found, no_match is called
  # if terminator is immediately found
  def expect_terminated(self, terminator, match=None, no_match=None):
    if self.accept(terminator) is not None:
      return no_match() if callable(no_match) else no_match
    if match is None:
      if self.peek() is None: self.fail()
      value = self.pop()
    elif callable(match):
      value = match()
      if value is None: self.fail()
    else:
      value = self.expect(match)
    if self.accept(terminator) is not None:
      return value
    # TODO: what to do with callable? we can't roll-back
    if not callable(match):
      self.push(value)
    self.fail()

class CharReader(Reader):
  def __init__(self, chariter):
    super(CharReader, self).__init__(chariter)
    # the position of the next character to be consumed
    self.line = 1
    self.offset = 1

  def _pop(self, raisestop=False):
    item = super(CharReader, self)._pop(raisestop)
    if item == u'\n':
      self.line += 1
      self.offset = 1
    else:
      self.offset += 1
    return item

  def push(self, item):
    if item is None: return
    if self.offset == 1:
      raise RuntimeError("unable to push back a line")
    super(CharReader, self).push(item)
    self.offset -= 1

  def consume_line(self):
    try:
      while self._pop(True) != '\n': pass
    except StopIteration: pass
