import collections

# assumes no access to reader while lookahead still in use
class Lookahead:
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
        return self.iter.next()
      except StopIteration:
        self.iter = None

    item = self.reader.chariter.next()
    self.buffer.append(item)
    return item

  def next(self):
    if self.iterpeek:
      item, self.iterpeek = self.iterpeek, None
      return item

    return self._pull()

  def peek(self):
    if self.iterpeek:
      return self.iterpeek
    item = self._pull()
    self.iterpeek = item
    return item

# earlier items at left end, later items at right end
class Reader:
  def __init__(self, chariter):
    self.chariter = chariter
    self.buffer = collections.deque()
    # the position of the next character to be consumed
    self.line = 1
    self.offset = 1

  def __iter__(self):
    return self

  def _pop(self, raisestop=False):
    if len(self.buffer):
      item = self.buffer.popleft()
    elif raisestop:
      item = self.chariter.next()
    else:
      try:
        item = self.chariter.next()
      except StopIteration:
        return None
    if item == u'\n':
      self.line += 1
      self.offset = 1
    else:
      self.offset += 1
    return item

  def next(self):
    return self._pop(True)

  def consume_line(self):
    try:
      while self._pop(True) != '\n': pass
    except StopIteration: pass

  def peek(self):
    if len(self.buffer):
      return self.buffer[0]
    try:
      item = self.chariter.next()
    except
    self.buffer.append(item)
    return item

  def pop(self):
    return self._pop()

  def push(self, item):
    # ignore pushing None, probably pushing back an unmatched "character"
    if item is None: return
    if self.offset == 1:
      raise Exception, "unable to push back a line"
    self.offset -= 1
    self.buffer.appendleft(item)

  def lookahead(self):
    return Lookahead(self)
