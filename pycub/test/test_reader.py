import unittest

from ..reader import Reader, CharReader

class ExampleException(Exception):
  def __init__(self, message):
    self.message = message

def example_fail():
  raise ExampleException("example_fail")

def alphabet():
  return (chr(a) for a in xrange(ord('a'), ord('z') + 1))

class TestReader(unittest.TestCase):

  def test_basic(self):
    "reader reads and terminates"
    self.assertIsNone(Reader(iter(())).pop())
    self.assertEquals(len(list(Reader(iter(())))), 0)

    self.assertEquals(list(Reader(iter('hello'))), list('hello'))
    items = [{'a': 1}, {'b': 1}]
    self.assertEquals(list(Reader(iter(items))), items)

  def test_peek(self):
    "reader can peek without advancing"
    self.assertIsNone(Reader(iter(())).peek())

    reader = Reader(iter('hello'))
    self.assertEquals(reader.peek(), 'h')
    self.assertEquals(reader.peek(), 'h')
    self.assertEquals(reader.pop(), 'h')
    self.assertEquals(reader.peek(), 'e')
    self.assertEquals(reader.pop(), 'e')

  def test_seek(self):
    "reader can seek back and forth"
    reader = Reader(iter('hello'))
    reader.pop()
    reader.push('h')
    reader.push(None)
    self.assertEquals(reader.peek(), 'h')
    self.assertEquals(list(reader), list('hello'))

  def test_lookahead(self):
    "reader is independent of lookahead"
    reader = Reader(iter('hello'))
    lookahead = reader.lookahead()
    self.assertEquals(list(lookahead), list('hello'))
    self.assertEquals(reader.pop(), 'h')
    lookahead = reader.lookahead()
    self.assertEquals(list(lookahead), list('ello'))
    self.assertEquals(list(reader), list('ello'))

    reader = Reader(iter('hello'))
    lookahead = reader.lookahead()
    self.assertEquals(lookahead.peek(), 'h')
    self.assertEquals(lookahead.next(), 'h')
    self.assertEquals(list(reader), list('hello'))

    reader = Reader(iter('hello'))
    reader.peek()
    lookahead = reader.lookahead()
    self.assertEquals(lookahead.peek(), 'h')
    self.assertEquals(lookahead.next(), 'h')

    reader = Reader(iter(()))
    lookahead = reader.lookahead()
    self.assertEquals(list(lookahead), [])

  def test_matcher(self):
    reader = Reader(iter('hello'), fail=example_fail)
    self.assertEquals(reader.accept('h'), 'h')
    self.assertIsNone(reader.accept('h'))
    self.assertEquals(reader.peek(), 'e')
    self.assertEquals(reader.expect('e'), 'e')
    with self.assertRaises(ExampleException):
      reader.expect('h')
    # failed expect doesn't screw up state
    self.assertEquals(reader.pop(), 'l')

    reader = Reader(iter(()), fail=example_fail)
    self.assertIsNone(reader.accept(1))
    self.assertIsNone(reader.accept(None))
    with self.assertRaises(ExampleException):
      reader.expect(1)
    with self.assertRaises(ExampleException):
      reader.expect(None)

    reader = Reader(iter(()), matches=lambda a, b: a['a'] == b)
    self.assertIsNone(reader.accept(1))
    self.assertIsNone(reader.accept(None))

    reader = Reader(iter(({'a': 4}, {'a': 3}, {'a': 1}, {'a': 1}, {'a': 0})), matches=lambda a, b: a['a'] == b['a'], fail=example_fail)
    self.assertIsNone(reader.accept({'a': 3}))
    self.assertEquals(reader.accept({'a': 4}), {'a': 4})
    with self.assertRaises(ExampleException):
      reader.expect({'a': 4})
    self.assertEquals(reader.expect({'a': 3}), {'a': 3})

    reader = Reader(alphabet(), fail=example_fail)
    self.assertIsNone(reader.accept_terminated('z'))
    self.assertIsNone(reader.accept_terminated('c'))
    self.assertEquals(reader.peek(), 'a')
    self.assertEquals(reader.accept_terminated('a', no_match=lambda: 14), 14)
    self.assertEquals(reader.peek(), 'b')
    self.assertEquals(reader.accept_terminated('c'), 'b')
    self.assertEquals(reader.accept_terminated('e', 'd'), 'd')
    self.assertIsNone(reader.accept_terminated('g', 'd'))
    self.assertEquals(reader.peek(), 'f')

    with self.assertRaises(ExampleException):
      reader.expect_terminated('a')
    with self.assertRaises(ExampleException):
      reader.expect_terminated('h')
    with self.assertRaises(ExampleException):
      reader.expect_terminated('h', 'g')
    self.assertEquals(reader.expect_terminated('f', no_match=14), 14)
    self.assertEquals(reader.expect_terminated('h', 'g'), 'g')
    def fake_parse():
      reader.expect('i')
      reader.expect('j')
      return {'tree': 'here'}
    self.assertEquals(reader.expect_terminated('k', fake_parse), {'tree': 'here'})

class TestCharReader(unittest.TestCase):

  def test_pos(self):
    reader = CharReader(iter(u"some silly\n\nlittle file thin\ng\n"))

    def assert_pos(line, offset):
      self.assertEquals(reader.line, line)
      self.assertEquals(reader.offset, offset)

    assert_pos(1, 1)
    reader.pop()
    assert_pos(1, 2)
    reader.push(u's')
    assert_pos(1, 1)
    reader.pop()
    assert_pos(1, 2)
    reader.consume_line()
    assert_pos(2, 1)
    with self.assertRaises(Exception):
      reader.push(u't')
    assert_pos(2, 1)
    self.assertEquals(reader.pop(), u'\n')
    assert_pos(3, 1)
    self.assertEquals(list(reader), list(u'little file thin\ng\n'))
    assert_pos(5, 1)

    reader = CharReader(iter(u"\u5929\u4e95"))

    assert_pos(1, 1)
    self.assertEquals(reader.pop(), u'\u5929')
    assert_pos(1, 2)
    self.assertEquals(reader.pop(), u'\u4e95')
    assert_pos(1, 3)
    self.assertIsNone(reader.pop())
