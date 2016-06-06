from pycub.file_iter import file_iter

# import lex
# for t in lex.Scanner(file_iter('test.cub')):
#   print str(t)

import pycub.lex as lex, pycub.parse as parse
print(parse.Parser(lex.Scanner(file_iter('test.cub'))).parse())
