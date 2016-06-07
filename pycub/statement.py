import pycub.tokens as tokens

S_WHILE = 0
S_DO_WHILE = 1
S_BREAK = 2
S_CONTINUE = 3

class Statement(object):
  #def __init__(self, type):
  #  self.type = type

  def set_label(self):
    raise Exception("cannot label this statement")

  def wrap_block(self):
    block = BlockStatement(self)
    self.parent = block
    return block

class LoopStatement(Statement):
  # code generation
  #BlockNode breakNode, continueNode;

  def __init__(self, type, condition, body):
    #super(LoopStatement, self).__init__(self, type)
    self.type = type
    self.label = None
    self.condition = condition
    self.body = body

    self.condition_block = None
    self.post_block = None

  def set_label(self, label):
    self.label = label

class BlockStatement(Statement):
  def __init__(self, body):
    #super(BlockStatement, self).__init__(self, S_BLOCK)
    self.body = body
    if body is not None:
      body.parent = self

    self.fn_parent = None
    self.class_list = None
    self.function_list = None
    self.type_list = None
    self.variable_list = None

  def wrap_block(self):
    return self

class ClassStatement(Statement):
  def __init__(self, class_type):
    #super(ClassStatement, self).__init__(self, S_CLASS)
    self.name = class_type.class_name
    self.class_type = class_type

class ControlStatement(Statement):
  def __init__(self, type):
    #super(ControlStatement, self).__init__(self, type)
    self.type = type
    self.label = None
    # TODO: switch statements
    self.target = None

class BreakStatement(ControlStatement):
  def __init__(self):
    super(BreakStatement, self).__init__(self, S_BREAK)

class ContinueStatement(ControlStatement):
  def __init__(self):
    super(ContinueStatement, self).__init__(self, S_CONTINUE)

class DefineClause:
  def __init__(self, name, value):
    self.name = name
    self.value = value
    self.next = None

class DefineStatement(Statement):
  def __init__(self, type, clause):
    #super(DefineStatement, self).__init__(self, S_DEFINE)
    self.symbol_type = type
    self.clause = clause

class ExpressionStatement(Statement):
  def __init__(self, value):
    #super(ExpressionStatement, self).__init__(self, S_EXPRESSION)
    self.value = value

class FunctionStatement(Statement):
  def __init__(self, fn):
    #super(FunctionStatement, self).__init__(self, S_FUNCTION)
    self.name = fn.function_name
    self.function = fn

class IfStatement(Statement):
  def __init__(self, condition, first, second):
    #super(IfStatement, self).__init__(self, S_IF)
    self.condition = condition
    self.first = first
    self.second = second

class LetStatement(Statement):
  def __init__(self, clause):
    #super(LetStatement, self).__init__(self, S_LET)
    self.clause = clause

class ReturnStatement(Statement):
  def __init__(self, value):
    #super(ReturnStatement, self).__init__(self, S_RETURN)
    self.value = value
    self.target = None

class TypedefStatement(Statement):
  def __init__(self, left, alias):
    #super(TypedefStatement, self).__init__(self, S_TYPEDEF)
    self.typedef_type = left
    self.alias = alias
