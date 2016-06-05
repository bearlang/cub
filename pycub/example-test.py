import codecs

def file_iter(filename):
  # utf-8-sig is the same as utf-8, except that it strips the BOM if present
  with codecs.open(filename, encoding='utf-8-sig') as f:
    char = f.read(1)
    while len(char):
      yield char
      char = f.read(1)

# import lex
# for t in lex.Scanner(file_iter('test.cub')):
#   print str(t)

import lex, parse
print parse.Parser(lex.Scanner(file_iter('test.cub'))).parse()
