class Expression(object):
  def __init__(self):

class BindExpression(Expression):
  def __init__(self, binding, inner):
    self.binding = binding
    self.value = inner
