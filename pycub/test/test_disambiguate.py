import unittest

from pycub.lex import Scanner
from pycub.parse import Parser
from pycub.disambiguate import disambiguate_statement, disambiguate_expression,\
  disambiguate_new

import pycub.disambiguate as disambiguate

def statement(code):
  return disambiguate_statement(Parser(Scanner(iter(code))))

def expression(code):
  return disambiguate_expression(Parser(Scanner(iter(code))))

def new(code):
  return disambiguate_new(Parser(Scanner(iter(code))))

class TestStatementDisambiguation(unittest.TestCase):

  def test_statement_basic(self):
    "disambiguate identifies simple syntax"
    self.assertEquals(statement("obj.value;"), disambiguate.G_EXPRESSION)
    self.assertEquals(statement("func();"), disambiguate.G_EXPRESSION)
    self.assertEquals(statement("void func() {}"), disambiguate.G_FUNCTION)
    self.assertEquals(statement("Type thing;"), disambiguate.G_DEFINE)

  def test_statement_complex(self):
    "disambiguate identifies complex syntax"
    self.assertEquals(statement("Type(Type()) fn() {}"), disambiguate.G_FUNCTION)
    self.assertEquals(statement("Type(Type()) fn;"), disambiguate.G_DEFINE)
    self.assertEquals(statement("Type(Type()) fn = 4;"), disambiguate.G_DEFINE)
    self.assertEquals(statement("Type(Type()) fn, fn2;"), disambiguate.G_DEFINE)
    self.assertEquals(statement("func(func(value + value))"), disambiguate.G_EXPRESSION)

  def test_expression(self):
    "disambiguate identifies expressions"
    self.assertEquals(expression("4"), disambiguate.G_EXPRESSION)
    self.assertEquals(expression("a + b"), disambiguate.G_EXPRESSION)
    self.assertEquals(expression("a b() {}"), disambiguate.G_FUNCTION)
    self.assertEquals(expression("a() {}"), disambiguate.G_FUNCTION)

  def test_new_object(self):
    "disambiguate identifies object creation"
    self.assertEquals(new("Type()"), disambiguate.G_NEW_OBJECT)
    self.assertEquals(new("Type(arg)"), disambiguate.G_NEW_OBJECT)
    self.assertEquals(new("Type(arg1, arg2)"), disambiguate.G_NEW_OBJECT)
    self.assertEquals(new("Type(arg1, arg2)"), disambiguate.G_NEW_OBJECT)
    self.assertEquals(new("Type(arg1 + arg2)"), disambiguate.G_NEW_OBJECT)
    self.assertEquals(new("Type(new Type())"), disambiguate.G_NEW_OBJECT)

  def test_new_array(self):
    "disambiguate identifies array creation"
    self.assertEquals(new("u8[4]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("u8[length]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("u8[new Type()]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type[4]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type[length]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type[new Type()]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type()[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type(Type)[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type(Type())[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type(Type, Type)[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type(Type(), Type)[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type()()[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type(Type)()[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type()(Type)[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type(Type)(Type)[]"), disambiguate.G_NEW_ARRAY)
    self.assertEquals(new("Type(Type, Type)(Type)[]"), disambiguate.G_NEW_ARRAY)
