MAIN_SRCS=xalloc.c buffer.c stream.c type.c token.c lexer.c expression/assign.c expression/call.c expression/compare.c expression/function.c expression/get-field.c expression/get-index.c expression/get-symbol.c expression/identity.c expression/literal.c expression/logic.c expression/native.c expression/negate.c expression/new.c expression/new-array.c expression/not.c expression/numeric.c expression/postfix.c expression/shift.c expression/str-concat.c expression/ternary.c statement/block.c statement/break.c statement/class.c statement/continue.c statement/define.c statement/expression.c statement/function.c statement/if.c statement/loop.c statement/return.c parser-common.c parser.c parser-detect-ambiguous.c analyze.c analyze-symbol.c analyze-type.c generator.c optimize.c
MAIN_BACKEND=backend.c
CFLAGS=-fdiagnostics-color -Wall -g3 -std=c11 -lm

compiler: $(MAIN_SRCS) $(MAIN_BACKEND) compiler.c
	gcc $(CFLAGS) $(MAIN_SRCS) $(MAIN_BACKEND) compiler.c -o compiler 2>&1
test/backend-test: $(MAIN_SRCS) backend.c test/backend-test.c
	gcc $(CFLAGS) $(MAIN_SRCS) backend.c test/backend-test.c -o $@ 2>&1
test/llvm-backend-test: $(MAIN_SRCS) llvm-backend.c test/backend-test.c
	gcc $(CFLAGS) $(MAIN_SRCS) llvm-backend.c test/backend-test.c -o $@ 2>&1
clean:
	rm -f compiler backend-test llvm-backend-test
