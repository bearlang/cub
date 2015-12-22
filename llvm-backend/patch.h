#ifndef LLVM_BACKEND_PATCH_H
#define LLVM_BACKEND_PATCH_H

#include <stdio.h>
#include <stdlib.h>
#include "../bool.h"

struct patchent {
	bool is_ref;
	union {
		char *data;
		struct patchvar *ref;
	};
	struct patchent *next;
};

struct patchvar {
	char *value;
	struct patchvar *next;
};

void pt_reset();
void pt_finalize(FILE *out);

struct patchvar *pt_def();
void pt_put(struct patchvar *var, char *value);
const char *pt_fetch(struct patchvar *var);
void pt_use(struct patchvar *var);
void pt_write(char *text);

#define pt_printf(...) { char *cptr; if (asprintf(&cptr, __VA_ARGS__) == -1) { perror("asprintf"); exit(1); } pt_write(cptr); }

#endif

