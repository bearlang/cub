import unittest

from ..reader import Reader, CharReader

class ExampleException(Exception):
  def __init__(self, message, expected):
    self.message = message
    self.expected = expected

  def __str__(self):
    return str(self.message)

def example_fail(expected):
  raise ExampleException("example_fail", expected)

def alphabet():
  return (chr(a) for a in range(ord('a'), ord('z') + 1))

class SampleObj(object):
  def __init__(self, value):
    self.value = value

class TestReader(unittest.TestCase):

  def test_basic(self):
    "reader reads and terminates"
    self.assertIsNone(Reader(iter(())).pop())
    self.assertEqual(list(Reader(iter(()))), [])

    self.assertEqual(list(Reader(iter('hello'))), list('hello'))
    items = [{'a': 1}, {'b': 1}]
    self.assertEqual(list(Reader(iter(items))), items)

  def test_peek(self):
    "reader can peek without advancing"
    self.assertIsNone(Reader(iter(())).peek())

    reader = Reader(iter('hello'))
    self.assertEqual(reader.peek(), 'h')
    self.assertEqual(reader.peek(), 'h')
    self.assertEqual(reader.pop(), 'h')
    self.assertEqual(reader.peek(), 'e')
    self.assertEqual(reader.pop(), 'e')

  def test_seek(self):
    "reader can seek back and forth"
    reader = Reader(iter('hello'))
    reader.pop()
    reader.push('h')
    reader.push(None)
    self.assertEqual(reader.peek(), 'h')
    self.assertEqual(list(reader), list('hello'))

  def test_lookahead(self):
    "reader is independent of lookahead"
    reader = Reader(iter('hello'))
    lookahead = reader.lookahead()
    self.assertEqual(list(lookahead), list('hello'))
    self.assertEqual(reader.pop(), 'h')
    lookahead = reader.lookahead()
    self.assertEqual(list(lookahead), list('ello'))
    self.assertEqual(list(reader), list('ello'))

    reader = Reader(iter('hello'))
    lookahead = reader.lookahead()
    self.assertEqual(lookahead.peek(), 'h')
    self.assertEqual(lookahead.next(), 'h')
    self.assertEqual(list(reader), list('hello'))

    reader = Reader(iter('hello'))
    reader.peek()
    lookahead = reader.lookahead()
    self.assertEqual(lookahead.peek(), 'h')
    self.assertEqual(lookahead.peek(), 'h')
    self.assertEqual(lookahead.next(), 'h')

    reader = Reader(iter(()))
    lookahead = reader.lookahead()
    self.assertEqual(list(lookahead), [])

  def test_matcher(self):
    reader = Reader(iter('hello'), fail=example_fail)
    self.assertEqual(reader.accept('h'), 'h')
    self.assertIsNone(reader.accept('h'))
    self.assertEqual(reader.peek(), 'e')
    self.assertEqual(reader.expect('e'), 'e')
    with self.assertRaises(ExampleException) as cm:
      reader.expect('h')
    self.assertEqual(cm.exception.expected, 'h')
    # failed expect doesn't screw up state
    self.assertEqual(reader.pop(), 'l')

    reader = Reader(iter(()), fail=example_fail)
    self.assertIsNone(reader.accept(1))
    self.assertIsNone(reader.accept(None))
    with self.assertRaises(ExampleException) as cm:
      reader.expect(1)
    self.assertEqual(cm.exception.expected, 1)
    with self.assertRaises(ExampleException) as cm:
      reader.expect(None)
    self.assertEqual(cm.exception.expected, None)
    with self.assertRaises(ExampleException) as cm:
      reader.expect_terminated('z')
    self.assertEqual(cm.exception.expected, None)

    reader = Reader(iter(()), matches=lambda a, b: a['a'] == b)
    self.assertIsNone(reader.accept(1))
    self.assertIsNone(reader.accept(None))

    reader = Reader(iter(({'a': 4}, {'a': 3}, {'a': 1}, {'a': 1}, {'a': 0})), matches=lambda a, b: a['a'] == b['a'], fail=example_fail)
    self.assertIsNone(reader.accept({'a': 3}))
    self.assertEqual(reader.accept({'a': 4}), {'a': 4})
    with self.assertRaises(ExampleException) as cm:
      reader.expect({'a': 4})
    self.assertEqual(cm.exception.expected, {'a': 4})
    self.assertEqual(reader.expect({'a': 3}), {'a': 3})

    reader = Reader(alphabet(), fail=example_fail)
    self.assertEqual(reader.accept('a', 'b'), ['a', 'b']);
    self.assertEqual(reader.accept('d', 'c'), None);
    self.assertEqual(reader.peek(), 'c')

    self.assertEqual(reader.expect('c', 'd', 'e'), list('cde'))
    with self.assertRaises(ExampleException) as cm:
      reader.expect('e', 'f', 'g')
    self.assertEqual(cm.exception.expected, 'e')

    reader = Reader(alphabet(), fail=example_fail)
    self.assertIsNone(reader.accept_terminated('z'))
    self.assertIsNone(reader.accept_terminated('c'))
    self.assertEqual(reader.peek(), 'a')
    self.assertEqual(reader.accept_terminated('a', no_match=lambda: 14), 14)
    self.assertEqual(reader.peek(), 'b')
    self.assertEqual(reader.accept_terminated('c'), 'b')
    self.assertEqual(reader.accept_terminated('e', 'd'), 'd')
    self.assertIsNone(reader.accept_terminated('g', 'd'))
    self.assertEqual(reader.peek(), 'f')

    with self.assertRaises(ExampleException) as cm:
      reader.expect_terminated('a')
    self.assertEqual(cm.exception.expected, None)
    with self.assertRaises(ExampleException) as cm:
      reader.expect_terminated('h')
    self.assertEqual(cm.exception.expected, None)
    with self.assertRaises(ExampleException) as cm:
      reader.expect_terminated('h', 'g')
    self.assertEqual(cm.exception.expected, 'g')
    self.assertEqual(reader.expect_terminated('f', no_match=14), 14)
    self.assertEqual(reader.expect_terminated('h', 'g'), 'g')
    def fake_parse():
      reader.expect('i')
      reader.expect('j')
      return {'tree': 'here'}
    self.assertEqual(reader.expect_terminated('k', fake_parse), {'tree': 'here'})

    # this is a destructive exception - we don't currently have a way to roll
    # back
    with self.assertRaises(ExampleException) as cm:
      def faker_parse():
        reader.expect('l')
        return True
      reader.expect_terminated('k', faker_parse)
    self.assertEqual(cm.exception.expected, faker_parse)

    reader = Reader(iter(()))
    with self.assertRaises(Exception):
      reader.expect(0)

    reader = Reader(iter(()), fail="some message")
    with self.assertRaises(Exception) as cm:
      reader.expect(0)

    self.assertEqual(str(cm.exception), "some message")

    samp = SampleObj(4), SampleObj(3), SampleObj(1), SampleObj(1), SampleObj(0)
    reader = Reader(iter(samp), matches="value")
    reader.expect(4)
    reader.expect(3)
    reader.expect(1)
    reader.expect(1)
    reader.expect(0)
    self.assertIsNone(reader.pop())

class TestCharReader(unittest.TestCase):

  def test_pos(self):
    reader = CharReader(iter(u"some silly\n\nlittle file thin\ng\n"))

    def assert_pos(line, offset):
      self.assertEqual(reader.line, line)
      self.assertEqual(reader.offset, offset)

    assert_pos(1, 1)
    reader.pop()
    assert_pos(1, 2)
    reader.push(u's')
    assert_pos(1, 1)
    reader.pop()
    assert_pos(1, 2)
    reader.consume_line()
    assert_pos(2, 1)
    reader.push(None)
    with self.assertRaises(Exception):
      reader.push(u't')
    assert_pos(2, 1)
    self.assertEqual(reader.pop(), u'\n')
    assert_pos(3, 1)
    self.assertEqual(list(reader), list(u'little file thin\ng\n'))
    assert_pos(5, 1)

    reader = CharReader(iter(u"\u5929\u4e95"))

    assert_pos(1, 1)
    self.assertEqual(reader.pop(), u'\u5929')
    assert_pos(1, 2)
    self.assertEqual(reader.pop(), u'\u4e95')
    assert_pos(1, 3)
    self.assertIsNone(reader.pop())

  def test_consume_line(self):
    reader = CharReader(iter(u"some silly\n\nlittle file thin\ng\nasdf"))

    def assert_pos(line, offset):
      self.assertEqual(reader.line, line)
      self.assertEqual(reader.offset, offset)

    reader.consume_line()
    assert_pos(2, 1)
    reader.consume_line()
    assert_pos(3, 1)
    reader.consume_line()
    assert_pos(4, 1)
    reader.consume_line()
    assert_pos(5, 1)
    reader.consume_line()
    assert_pos(5, 5)

    self.assertEqual(list(reader), [])
