MAIN_SRCS=xalloc.c buffer.c stream.c type.c token.c lexer.c expression/assign.c expression/call.c expression/compare.c expression/function.c expression/get-field.c expression/get-index.c expression/get-symbol.c expression/identity.c expression/literal.c expression/logic.c expression/negate.c expression/new.c expression/not.c expression/numeric.c expression/shift.c expression/str-concat.c expression/ternary.c statement/block.c statement/break.c statement/class.c statement/continue.c statement/define.c statement/expression.c statement/function.c statement/if.c statement/loop.c statement/return.c parser.c analyze.c generator.c optimize.c

backend-test: $(MAIN_SRCS) backend.c backend-test.c
	gcc -Wall -g3 -std=c11 -lm $(MAIN_SRCS) backend.c backend-test.c -o $@ 2>&1
llvm-backend-test: $(MAIN_SRCS) llvm-backend.c backend-test.c
	gcc -Wall -g3 -std=c11 -lm $(MAIN_SRCS) llvm-backend.c backend-test.c -o $@ 2>&1
