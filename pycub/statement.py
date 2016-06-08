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

  def __eq__(self, other):
    return False

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

  def __eq__(self, other):
    return isinstance(other, LoopStatement) and self.type == other.type and \
      self.label == other.label and self.condition == other.condition and \
      self.body == other.body and \
      self.condition_block == other.condition_block and \
      self.post_block == other.post_block

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

  def __eq__(self, other):
    return isinstance(other, BlockStatement) and self.body == other.body and \
      self.fn_parent == other.fn_parent and \
      self.class_list == other.class_list and \
      self.function_list == other.function_list and \
      self.type_list == other.type_list and \
      self.variable_list == other.variable_list

class ClassStatement(Statement):
  def __init__(self, class_type):
    #super(ClassStatement, self).__init__(self, S_CLASS)
    self.name = class_type.class_name
    self.class_type = class_type

  def __eq__(self, other):
    return isinstance(other, ClassStatement) and self.name == other.name and \
      self.class_type == other.class_type

class ControlStatement(Statement):
  def __init__(self, type):
    #super(ControlStatement, self).__init__(self, type)
    self.type = type
    self.label = None
    # TODO: switch statements
    self.target = None

  def __eq__(self, other):
    return isinstance(other, ControlStatement) and self.type == other.type and \
      self.label == other.label and self.target == other.target

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

  def __eq__(self, other):
    return isinstance(other, DefineClause) and self.name == other.name and \
      self.value == other.value and self.next == other.next

class DefineStatement(Statement):
  def __init__(self, type, clause):
    #super(DefineStatement, self).__init__(self, S_DEFINE)
    self.symbol_type = type
    self.clause = clause

  def __eq__(self, other):
    return isinstance(other, DefineStatement) and \
      self.symbol_type == other.symbol_type and self.value == other.value

class ExpressionStatement(Statement):
  def __init__(self, value):
    #super(ExpressionStatement, self).__init__(self, S_EXPRESSION)
    self.value = value

  def __eq__(self, other):
    return isinstance(other, ExpressionStatement) and self.value == other.value

class FunctionStatement(Statement):
  def __init__(self, fn):
    #super(FunctionStatement, self).__init__(self, S_FUNCTION)
    self.name = fn.function_name
    self.function = fn

  def __eq__(self, other):
    return isinstance(other, FunctionStatement) and self.name == other.name \
      and self.function == other.function

class IfStatement(Statement):
  def __init__(self, condition, first, second):
    #super(IfStatement, self).__init__(self, S_IF)
    self.condition = condition
    self.first = first
    self.second = second

  def __eq__(self, other):
    return isinstance(other, IfStatement) and \
      self.condition == other.condition and self.first == other.first and \
      self.second == other.second

class LetStatement(Statement):
  def __init__(self, clause):
    #super(LetStatement, self).__init__(self, S_LET)
    self.clause = clause

  def __eq__(self, other):
    return isinstance(other, LetStatement) and self.clause == other.clause

class ReturnStatement(Statement):
  def __init__(self, value):
    #super(ReturnStatement, self).__init__(self, S_RETURN)
    self.value = value
    self.target = None

  def __eq__(self, other):
    return isinstance(other, ReturnStatement) and self.value == other.value \
      and self.target == other.target

class TypedefStatement(Statement):
  def __init__(self, left, alias):
    #super(TypedefStatement, self).__init__(self, S_TYPEDEF)
    self.typedef_type = left
    self.alias = alias

  def __eq__(self, other):
    return isinstance(other, TypedefStatement) and \
      self.typedef_type == other.typedef_type and self.alias == other.alias
