#include "types.h"

enum llvm_type_type {
	LT_VOID, LT_INT, LT_FLOAT, LT_PTR, LT_INSTANCE, LT_BLOCKREF
};

struct llvm_type {
	enum llvm_type_type type;
	union {
		int bits; // for integers
		int struct_id; // for structs - not quite LLVM-level, but close enough
		struct llvm_type *subtype; // for pointers
	};
};

#define check_format(...) { if (asprintf(&cptr, __VA_ARGS__) == -1) { perror("asprintf"); exit(1); } }
#define return_format(...) { check_format(__VA_ARGS__) break; }

char *llvm_type_string(struct llvm_type *type, bool dealloc) {
	// result must be heap-allocated
	char *cptr, *ciptr;
	switch (type->type) {
	case LT_VOID:
		cptr = strdup("void");
		break;
	case LT_INT:
		return_format("i%d", type->bits);
	case LT_FLOAT:
		switch (type->bits) {
		case 32: cptr = strdup("float"); break;
		case 64: cptr = strdup("double"); break;
		case 128: cptr = strdup("fp128"); break;
		default: fprintf(stderr, "invalid number of bits for float: %d\n", type->bits); abort();
		}
		break;
	case LT_PTR:
		ciptr = llvm_type_string(type->subtype);
		check_format("%s*", ciptr);
		free(ciptr);
		break;
	case LT_INSTANCE:
		return_format("%%struct.%zu", type->struct_id);
	case LT_BLOCKREF:
		cptr = strdup("i8*");
		break;
	default:
		fprintf(stderr, "invalid LLVM type ordinal: %d\n", type->type);
		abort();
	}
	free(type);
	return cptr;
}

