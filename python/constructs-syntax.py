import constructs

class Syntax(object):
  def gen(self):
    raise Exception, "this is just a contract"

class Capture(object):
  def get(self):
    raise Exception, "this is just a contract"

  def set(self):
    raise Exception, "this is just a contract"

class Assign(Syntax):
  def __init__(self, left, right, mod = None):
    self.left = left
    self.right = right
    self.mod = mod

  def gen(self):
    capture = self.left.capture()
    value = self.right.gen()
    if self.mod is not None:
      value = constructs.make_binary(self.mod, capture.get(), value)
    return capture.set(value)

class SymbolCapture(Capture):
  def __init__(self, symbol):
    self.symbol = symbol
    
  def get(self):
    return constructs.GetSymbol(self.symbol)
  
  def set(self, right):
    return constructs.SetSymbol(self.symbol, right)

class Symbol(Syntax):
  def __init__(self, symbol):
    self.symbol = symbol

  def gen(self):
    return constructs.GetSymbol(self.symbol)

  def capture(self):
    return SymbolCapture(self.symbol)

class FieldCapture(Capture):
  def __init__(self, obj, field):
    self.obj = obj # construct expression
    self.field = field
    self.uref = constructs.UniqueRef()

  def get(self):
    return constructs.GetField(self.uref, self.field_obj.field)

  def set(self, right):
    setter = constructs.SetField(self.uref, self.field, right)
    return constructs.Capture(self.uref, self.obj, setter)

class Field(Syntax):
  def __init__(self, obj, field):
    self.obj = obj # syntax expression
    self.field = field

  def gen(self):
    return constructs.GetSymbol(self.symbol)

  def capture(self):
    return FieldCapture(self.obj.gen(), self.field)
