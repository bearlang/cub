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

    self.assertEqual(list(scanner), [
      tokens.StrToken(1, 1, "\u5929\u4e95"),
      tokens.Token(1, 5, tokens.L_SEMICOLON),

      tokens.IdToken(4, 1, "import"),
      tokens.IdToken(4, 8, "thing"),
      tokens.Token(4, 13, tokens.L_SEMICOLON),

      tokens.IdToken(5, 1, "import"),
      tokens.IdToken(5, 8, "thing"),
      tokens.IdToken(5, 14, "as"),
      tokens.IdToken(5, 17, "other"),
      tokens.Token(5, 22, tokens.L_SEMICOLON),

      tokens.IdToken(6, 1, "import"),
      tokens.IdToken(6, 8, "thing"),
      tokens.Token(6, 13, tokens.L_DOT),
      tokens.IdToken(6, 14, "again"),
      tokens.Token(6, 19, tokens.L_SEMICOLON),

      tokens.IdToken(7, 1, "import"),
      tokens.IdToken(7, 8, "thing"),
      tokens.Token(7, 13, tokens.L_DOT),
      tokens.IdToken(7, 14, "again"),
      tokens.IdToken(7, 20, "as"),
      tokens.IdToken(7, 23, "other"),
      tokens.Token(7, 28, tokens.L_SEMICOLON),

      tokens.TypeToken(9, 1, types.T_U8),
      tokens.IdToken(9, 4, "b"),
      tokens.Token(9, 6, tokens.L_ASSIGN),
      tokens.IntToken(9, 8, 4), # hmmm
      tokens.Token(9, 10, tokens.L_ADD),
      tokens.Token(9, 12, tokens.L_OPEN_PAREN),
      tokens.IntToken(9, 13, 3),
      tokens.Token(9, 15, tokens.L_DIV),
      tokens.Token(9, 17, tokens.L_OPEN_PAREN),
      tokens.IntToken(9, 18, 2),
      tokens.Token(9, 20, tokens.L_SUB),
      tokens.IntToken(9, 22, 1),
      tokens.Token(9, 23, tokens.L_CLOSE_PAREN),
      tokens.Token(9, 25, tokens.L_MUL),
      tokens.IntToken(9, 27, 4),
      tokens.Token(9, 28, tokens.L_CLOSE_PAREN),
      tokens.Token(9, 29, tokens.L_SEMICOLON),

      tokens.IdToken(11, 2, "b"),
      tokens.Token(11, 4, tokens.L_ADD_ASSIGN),
      tokens.IntToken(11, 7, 3),
      tokens.Token(11, 8, tokens.L_SEMICOLON),

      tokens.IdToken(12, 1, "b"),
      tokens.Token(12, 3, tokens.L_SUB_ASSIGN),
      tokens.IntToken(12, 6, 3),
      tokens.Token(12, 7, tokens.L_SEMICOLON),

      tokens.IdToken(13, 1, "b"),
      tokens.Token(13, 3, tokens.L_MUL_ASSIGN),
      tokens.IntToken(13, 6, 3),
      tokens.Token(13, 7, tokens.L_SEMICOLON),

      tokens.IdToken(14, 1, "b"),
      tokens.Token(14, 3, tokens.L_DIV_ASSIGN),
      tokens.IntToken(14, 6, 3),
      tokens.Token(14, 7, tokens.L_SEMICOLON),

      tokens.TypeToken(16, 1, types.T_STRING),
      tokens.IdToken(16, 8, "q"),
      tokens.Token(16, 10, tokens.L_ASSIGN),
      tokens.StrToken(16, 12, "hi, me"),
      tokens.Token(16, 21, tokens.L_STR_CONCAT),
      tokens.StrToken(16, 23, "\n"),
      tokens.Token(16, 27, tokens.L_SEMICOLON),

      tokens.IdToken(18, 1, "q"),
      tokens.Token(18, 3, tokens.L_STR_CONCAT_ASSIGN),
      tokens.StrToken(18, 6, "what's up?\n"),
      tokens.Token(18, 20, tokens.L_SEMICOLON)
    ]);
