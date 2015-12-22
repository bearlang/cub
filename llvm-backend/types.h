#ifndef LLVM_BACKEND_TYPES_H
#define LLVM_BACKEND_TYPES_H

#include <stdio.h>
#include <stdlib.h>

enum llvm_type_type {
	LT_VOID, LT_INT, LT_FLOAT, LT_PTR, LT_INSTANCE, LT_BLOCKREF
};

struct llvm_type {
	enum llvm_type_type type;
	union {
		int bits; // for integers
		int struct_id; // for structs
		struct llvm_type *subtype; // for pointers
	};
};

#define LTS_KEEP (0)
#define LTS_DEALLOC (1)

char *llvm_type_string(struct llvm_type *type, bool dealloc);

#define LL_ANY(type, ...) (&(struct llvm_type) { .type = type, __VA_ARGS__ })
#define LL_PTR(x) LL_ANY(LLL_PTR, .subtype = x)
#define LL_INT(bits) LL_ANY(LLL_INT, .bits = bits)
#define LL_FLOAT(bits) LL_ANY(LLL_FLOAT, .bits = bits)
#define LL_INST(sid) LL_ANY(LLL_INSTANCE, .struct_id = sid)
#define LL_OBJ(sid) LL_PTR(LL_INST(sid))
#define LL_I1 LL_INT(1)
#define LL_I8 LL_INT(8)
#define LL_I16 LL_INT(16)
#define LL_I32 LL_INT(32)
#define LL_I64 LL_INT(64)
#define LL_F32 LL_FLOAT(32)
#define LL_F64 LL_FLOAT(64)
#define LL_F128 LL_FLOAT(128)
#define LL_VOID LL_ANY(LLL_VOID)
#define LL_BLOCK LL_ANY(LLL_BLOCKREF)

#endif

