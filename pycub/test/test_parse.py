import unittest#, os.path

from ..file_iter import file_iter
from ..lex import Scanner
from ..parse import Parser
import pycub.statement as statement
import pycub.tokens as tokens
import pycub.types as types

# FIXTURE = os.path.dirname(os.path.realpath(__file__)) + "/fixtures/test.cub"

class TestParse(unittest.TestCase):

  def test_empty(self):
    self.assertEqual(Parser(Scanner(iter(()))).parse(), statement.BlockStatement(None))

  def test_print(self):
    # TODO: add expressions
    one = None # expression.LiteralExpression(types.T_U8, 1)
    callee = None # expression.GetSymbolExpression("print")
    e = None # expression.CallExpression(callee, [one])
    s = statement.ExpressionStatement(e)
    self.assertEqual(Parser(Scanner(iter("print(1);"))).parse())
