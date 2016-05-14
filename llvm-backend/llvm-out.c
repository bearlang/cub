#include "patch.h"
#include "types.h"

static size_t next_id;

void ll_init() {
	pt_reset();
	pt_write("target datalayout = \"e-m:e-i64:64-f80:128-n8:16:32:64-S128\"\n"
	         "target triple = \"x86_64-unknown-linux-gnu\"\n\n\n");
	next_id = 0;
}

struct llvm_type *ll_declare_struct(void) {
	size_t id = next_id++;
	return LL_INST(id);
}

void ll_define_struct(struct llvm_type *str, struct llvm_type **types) { // types must be NULL-terminated
	pt_printf("%%ls.%zu = type {", str->struct_id.struct_id);
	assert(types[0] != NULL);
	pt_write(llvm_type_string(types[0], false));
	for (int i = 1; types[i] != NULL; i++) {
		pt_write("");
		pt_write(llvm_type_string(types[i], false));
	}
}

