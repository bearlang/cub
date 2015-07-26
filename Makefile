MAIN_SRCS=xalloc.c buffer.c stream.c type.c token.c lex.c expression/assign.c expression/call.c expression/compare.c expression/function.c expression/get-field.c expression/get-index.c expression/get-symbol.c expression/identity.c expression/literal.c expression/logic.c expression/native.c expression/negate.c expression/new.c expression/new-array.c expression/not.c expression/numeric.c expression/postfix.c expression/shift.c expression/str-concat.c expression/ternary.c statement/block.c statement/break.c statement/class.c statement/continue.c statement/define.c statement/expression.c statement/function.c statement/if.c statement/loop.c statement/return.c parse-common.c parse.c parse-detect-ambiguous.c analyze.c analyze-symbol.c analyze-type.c generate.c optimize.c
MAIN_BACKEND=llvm-backend/llvm-backend.c
CFLAGS=-fdiagnostics-color -Wall -g3 -std=c11 -lm

compiler: $(MAIN_SRCS) $(MAIN_BACKEND) compile.c
	gcc $(CFLAGS) $< -o compiler 2>&1
test/backend-test: $(MAIN_SRCS) backend.c test/backend-test.c
	gcc $(CFLAGS) $< -o $@ 2>&1
test/llvm-backend-test: $(MAIN_SRCS) llvm-backend/llvm-backend.c test/backend-test.c
	gcc $(CFLAGS) $< -o $@ 2>&1
clean:
	rm -f compiler backend-test llvm-backend-test
