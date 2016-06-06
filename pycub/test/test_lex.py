import unittest, os.path

from ..file_iter import file_iter
from ..lex import Scanner, tokens as _tokens
import pycub.tokens as tokens
import pycub.types as types

FIXTURE = os.path.dirname(os.path.realpath(__file__)) + "/fixtures/test.cub"

class TestLex(unittest.TestCase):

  def test_empty(self):
    self.assertEqual(list(Scanner(iter(()))), [])

  def test_fixture(self):
    self.assertEqual(_tokens.StrToken.__module__, tokens.StrToken.__module__)

    scanner = Scanner(file_iter(FIXTURE))

    self.assertEqual(scanner.scan(), tokens.StrToken(1, 1, u"\u5929\u4e95"))

