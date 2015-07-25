#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "backend.h"

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

struct patchent *fent = NULL;
struct patchvar *fvar = NULL;

void pt_reset() {
	{
		struct patchent *cur = fent;
		while (cur != NULL) {
			struct patchent *next = cur->next;
			if (!cur->is_ref) {
				free(cur->data);
			}
			free(cur);
			cur = next;
		}
	}
	{
		struct patchvar *cur = fvar;
		while (cur != NULL) {
			struct patchvar *next = cur->next;
			free(cur->value);
			free(cur);
			cur = next;
		}
	}
	fent = NULL;
	fvar = NULL;
}

void pt_finalize(FILE *out) {
	struct patchent *cur = fent;
	struct patchent *last = NULL;

	while (cur != NULL) { // reverse the linked list
		struct patchent *next = cur->next;
		cur->next = last;
		last = cur;
		cur = next;
	}
	fent = last;

	cur = fent;
	while (cur != NULL) {
		if (cur->is_ref) {
			if (cur->ref->value == NULL) {
				fputs("variable not completed\n", stderr);
				exit(1);
			}
			fputs(cur->ref->value, out);
		} else {
			fputs(cur->data, out);
		}
		cur = cur->next;
	}

	pt_reset();
}

struct patchvar *pt_def() {
	struct patchvar *out = malloc(sizeof(struct patchvar));
	out->next = fvar;
	fvar = out;
	out->value = NULL;
	return out;
}

void pt_put(struct patchvar *var, char *value) {
	if (value == NULL) {
		fputs("value is NULL\n", stderr);
		exit(1);
	}
	if (var->value != NULL) {
		free(var->value);
	}
	var->value = value;
}

const char *pt_fetch(struct patchvar *var) {
	if (var->value == NULL) {
		fputs("fetch on NULL var\n", stderr);
		exit(1);
	}
	return var->value;
}

struct patchent *pt_new_ent(bool is_ref) {
	struct patchent *out = malloc(sizeof(struct patchent));
	out->next = fent;
	fent = out;
	out->is_ref = is_ref;
	return out;
}

void pt_use(struct patchvar *var) {
	pt_new_ent(true)->ref = var;
}

void pt_write(char *text) {
	pt_new_ent(false)->data = text;
}

#define pf(...) { char *cptr; if (asprintf(&cptr, __VA_ARGS__) == -1) { perror("asprintf"); exit(1); } pt_write(cptr); }
#define pv(var) { pt_use(var); }
#define vf(var, ...) { char *cptr; if (asprintf(&cptr, __VA_ARGS__) == -1) { perror("asprintf"); exit(1); } pt_put(var, cptr); }
#define vfr(var, x) { pt_put(var, x); }

static void wt(type *t) {
	if (t == NULL) {
		abort();
	}
	switch (t->type) {
	case T_ARRAY:
		fprintf(stderr, "array type not implemented\n");
		exit(1);
	case T_BLOCKREF:
		pf("i8*");
		break;
	case T_BOOL:
		pf("i1");
		break;
	case T_F32:
		pf("float");
		break;
	case T_F64:
		pf("double");
		break;
	case T_F128:
		pf("fp128");
		break;
	case T_OBJECT:
		pf("%%struct.%zu*", t->struct_index);
		break;
	case T_REF:
		abort();
	case T_U8:
	case T_S8:
		pf("i8"); // in LLVM, signedness is distinguished on the instructions, not the types
		break;
	case T_U16:
	case T_S16:
		pf("i16");
		break;
	case T_U32:
	case T_S32:
		pf("i32");
		break;
	case T_U64:
	case T_S64:
		pf("i64");
		break;
	case T_STRING:
		pf("i8*");
		break;
	case T_VOID:
		pf("void");
		break;
	default:
		abort();
	}
}

bool check_prototypes(code_block *from, code_block *to, size_t toindex) {
	if (from->is_final || from->tail.parameter_count != to->parameter_count) {
		return false;
	}
	switch (from->tail.type) {
	case GOTO:
		if (from->tail.first_block >= from->parameter_count
		 && from->instructions[from->tail.first_block - from->parameter_count].operation.type == O_BLOCKREF) {
			return from->instructions[from->tail.first_block - from->parameter_count].block_index == toindex;
		}
		break;
	case BRANCH:
		if (from->tail.first_block >= from->parameter_count && from->tail.second_block >= from->parameter_count
		 && from->instructions[from->tail.first_block - from->parameter_count].operation.type == O_BLOCKREF
		 && from->instructions[from->tail.second_block - from->parameter_count].operation.type == O_BLOCKREF) {
			return from->instructions[from->tail.first_block - from->parameter_count].block_index == toindex
			    || from->instructions[from->tail.second_block - from->parameter_count].block_index == toindex;
		}
		break;
	}

	for (size_t p = 0; p < to->parameter_count; p++) {
		type *req = to->parameters[p].field_type;
		size_t iind = from->tail.parameters[p];
		type *found = iind < from->parameter_count ? from->parameters[iind].field_type : from->instructions[iind - from->parameter_count].type;
		if (!equivalent_type(req, found)) {
			return false;
		}
	}
	return true;
}

void backend_write(code_system *system, FILE *out) {
	pt_reset();

	pf("target datalayout = \"e-m:e-i64:64-f80:128-n8:16:32:64-S128\"\n"
		 "target triple = \"x86_64-unknown-linux-gnu\"\n\n\n");

	if (system->struct_count) {
		for (size_t i = 0; i < system->struct_count; i++) {
			code_struct *str = &system->structs[i];

			pf("%%struct.%zu = type { ", i);
			for (size_t j = 0; j < str->field_count; j++) {
				if (j != 0) {
					pf(", ");
				}
				wt(str->fields[j].field_type);
			}
			pf(" };\n");
		}
		pf("\n\n");
	}

	pf("declare i8* @allocate(i64) nounwind\n");
	pf("declare void @exit(i32) nounwind noreturn\n\n");

	size_t natid = 0, natcap = 10;
	char **natives = malloc(natcap * sizeof(char*));
	if (natives == NULL) {
		fputs("alloc failed\n", stderr);
		exit(1);
	}

#define TYPEOF(x) ((x) < block->parameter_count ? block->parameters[x].field_type : block->instructions[x - block->parameter_count].type)

	// constant and prototype extraction
	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = &system->blocks[i];
		for (size_t j = 0; j < block->instruction_count; j++) {
			code_instruction *insr = &block->instructions[j];
			if (insr->operation.type == O_NATIVE) {
				char *name = insr->native_call;
				bool found = false;
				for (size_t k = 0; k < natid; k++) {
					if (strcmp(name, natives[k]) == 0) {
						found = true;
						break;
					}
				}
				if (!found) {
					if (natid >= natcap) {
						natives = realloc(natives, (natcap <<= 1) * sizeof(char*));
					}
					natives[natid++] = name;
					pf("declare ");
					wt(insr->type);
					pf(" @%s(", name);
					size_t count = insr->parameters[0];
					if (count) {
						wt(TYPEOF(insr->parameters[1]));
						for (size_t k = 2; k <= count; k++) {
							pf(", ");
							wt(TYPEOF(insr->parameters[k]));
						}
					}
					pf(") nounwind\n")
				}
			} else if (insr->operation.type == O_LITERAL && insr->type->type == T_STRING) {
				pf("@str_%zu_%zu = private unnamed_addr constant [%zu x i8] c\"", i, j, strlen(insr->value_string) + 1);
				for (char *s = insr->value_string; *s; s++) {
					char c = *s;
					if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
						pf("%c", c);
					} else {
						pf("\\%02hhx", c);
					}
				}
				pf("\\00\"");
			}
		}
	}

	pf("\ndefine i32 @main() {\n");
	pf("  br label %%Block0\n");

	struct patchvar **allrefs[system->block_count];

	for (size_t i = 0; i < system->block_count; i++) {
		size_t len = system->blocks[i].parameter_count + system->blocks[i].instruction_count;
		allrefs[i] = malloc(sizeof(struct patchvar *) * len);
		if (allrefs[i] == NULL) {
			fputs("alloc failed\n", stderr);
			exit(1);
		}
		for (size_t j = 0; j < len; j++) {
			allrefs[i][j] = pt_def();
		}
	}

	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = &system->blocks[i];

		pf("Block%zu:\n", i);

		struct patchvar **ref = allrefs[i];

#define RP(n) pt_fetch(ref[ins->parameters[(n)]])
#define PR(n) pv(ref[ins->parameters[(n)]]);
#define TP(n) (TYPEOF(ins->parameters[(n)]))

		for (size_t k = 0; k < block->parameter_count + block->instruction_count; k++) {
			vf(ref[k], "%%b%zu_%zu", i, k);
		}

#define SET(x, ...) pf("  %%b%zu_%zu = " x, i, k, __VA_ARGS__)
#define SETR(x) SET("%s", x)

		// parameters via PHI
		size_t offset = block->parameter_count;
		for (size_t k = 0; k < offset; k++) {
			SETR("phi ");
			wt(block->parameters[k].field_type);
			bool first = true;
			for (size_t l = 0; l < system->block_count; l++) {
				code_block *from = &system->blocks[l];
				// we want to limit the number of source possibilities - so we make sure the prototype matches.
				if (check_prototypes(from, block, i)) {
					size_t sourceid = from->tail.parameters[k];
					if (first) {
						first = false;
					} else {
						pf(",");
					}
					pf(" [ ")
					pv(allrefs[l][sourceid]);
					pf(", %%Block%zu ]", l);
				}
			}
			pf("\n");
		}

		for (size_t j = 0; j < block->instruction_count; j++) {
			size_t k = j + offset;
			code_instruction *ins = &block->instructions[j];

			switch (ins->operation.type) {
			case O_BITWISE_NOT:
				SETR("xor ");
				wt(ins->type);
				pf(" %s, -1", RP(0));
				break;
			case O_BLOCKREF:
				vf(ref[k], "blockaddress(@main, %%Block%zu)", ins->block_index);
				break;
			case O_CAST: {
				type *typ = ins->type;
				const char *name = NULL;
				int np = 0;
				switch (ins->operation.cast_type) {
				case O_UPCAST:
				case O_DOWNCAST: // TODO: check downcasts
					name = "bitcast";
					typ = &(type) {.type=T_OBJECT, .struct_index = ins->parameters[np++]};
					break;
				case O_FLOAT_EXTEND:
					name = "fpext";
					break;
				case O_FLOAT_TRUNCATE:
					name = "fptrunc";
					break;
				case O_FLOAT_TO_SIGNED:
					name = "fptosi";
					break;
				case O_FLOAT_TO_UNSIGNED:
					name = "fptoui";
					break;
				case O_SIGNED_TO_FLOAT:
					name = "sitofp";
					break;
				case O_UNSIGNED_TO_FLOAT:
					name = "uitofp";
					break;
				case O_SIGN_EXTEND:
					name = "sext";
					break;
				case O_ZERO_EXTEND:
					name = "zext";
					break;
				case O_TRUNCATE:
					name = "trunc";
					break;
				case O_REINTERPRET:
					name = "bitcast";
					break;
				}
				SET("%s ", name);
				wt(TP(np));
				pf(" %s to ", RP(np));
				wt(typ);
			} break;
			case O_COMPARE: {
				const char *op;
				switch (ins->operation.compare_type) {
				case O_EQ: op = "icmp eq"; break;
				case O_GT: op = "icmp ugt"; break; // TODO: check signs
				case O_GTE: op = "icmp uge"; break;
				case O_LT: op = "icmp ult"; break;
				case O_LTE: op = "icmp ule"; break;
				case O_NE: op = "icmp ne"; break;
				}
				SET("%s ", op);
				wt(TP(0));
				pf(" %s, %s", RP(0), RP(1));
			} break;
			case O_GET_FIELD:
				pf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
//				wt(TP(0));
//				pf(", ");
				wt(TP(0));
				pf(" %s, i64 0, i32 %zu\n", RP(0), ins->parameters[1]);

				SETR("load ");
				wt(ins->type);
				pf("* %%temp.%zu_%zu, align 1", i, k);
				break;
			case O_GET_INDEX:
				pf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
//				wt(TP(0));
//				pf(", ");
				wt(TP(0));
				pf(" %s, i64 %s\n", RP(0), RP(1));

				SETR("load ");
				wt(ins->type);
				pf("* %%temp.%zu_%zu, align 1", i, k);
				break;
			case O_GET_SYMBOL:
				// TODO: fix
				SET("%s", RP(0));
				break;
			case O_IDENTITY:
				fprintf(stderr, "this backend does not support identity checking\n");
				exit(1);
			case O_INSTANCEOF:
				fprintf(stderr, "this backend does not support instanceof checking\n");
				exit(1);
			case O_LITERAL:
			switch (ins->type->type) {
				case T_ARRAY:
				case T_BLOCKREF:
				case T_F32:
				case T_F64:
				case T_F128:
				case T_REF:
				case T_S8:
				case T_S16:
				case T_S32:
				case T_S64:
				case T_VOID:
				default:
					abort();
				case T_BOOL:
					vf(ref[k], ins->value_bool ? "true" : "false");
					break;
				case T_OBJECT:
					vf(ref[k], "null");
					break;
				case T_STRING:
					SET("getelementptr [%zu x i8]* @str_%zu_%zu, i64 0, i64 0", strlen(ins->value_string) + 1, i, j);
					break;
				case T_U8:
					vf(ref[k], "%hhu", ins->value_u8);
					break;
				case T_U16:
					vf(ref[k], "%hu", ins->value_u16);
					break;
				case T_U32:
					vf(ref[k], "%u", ins->value_u32);
					break;
				case T_U64:
					vf(ref[k], "%lu", ins->value_u64);
					break;
				}
				break;
			case O_LOGIC: {
				pf("  %%a.%zu = icmp ne ", k);
				wt(TP(0));
				pf(" %s, 0\n", RP(0));

				pf("  %%b.%zu = icmp ne ", k);
				wt(TP(1));
				pf(" %s, 0\n", RP(1));

				switch (ins->operation.logic_type) {
				case O_AND:
					SETR("and");
					break;
				case O_OR:
					SETR("or");
					break;
				case O_XOR:
					SETR("xor");
					break;
				default:
					abort();
				}
				pf(" i1 %%a.%zu, %%b.%zu", k, k);
			} break;
			case O_NATIVE: {
				if (is_void(ins->type)) {
					pf("  call void");
				} else {
					SETR("call ");
					wt(ins->type);
				}
				pf(" @%s(", ins->native_call);
				size_t count = ins->parameters[0];
				if (count) {
					wt(TP(1));
					pf(" %s", RP(1));
					for (size_t i = 2; i <= count; i++) {
						pf(",");
						wt(TP(i));
						pf(" %s", RP(i));
					}
				}
				pf(")");
			} break;
			case O_NEGATE:
				SETR("sub ");
				wt(ins->type);
				pf(" 0, %s", RP(0));
				break;
			case O_NEW:
				pf("  %%zptr.%zu_%zu = inttoptr i32 0 to %%struct.%zu*\n", i, k, ins->type->struct_index);
				pf("  %%psize.%zu_%zu = getelementptr %%struct.%zu* %%zptr.%zu_%zu, i64 1\n", i, k, ins->type->struct_index, i, k);
				pf("  %%size.%zu_%zu = ptrtoint %%struct.%zu* %%psize.%zu_%zu to i64\n", i, k, ins->type->struct_index, i, k); // TODO: is i64 too long for 32-bit?
				pf("  %%raw.%zu_%zu = call ccc i8* @allocate(i64 %%size.%zu_%zu)\n", i, k, i, k); // TODO: garbage collection
				SET("bitcast i8* %%raw.%zu_%zu to %%struct.%zu*", i, k, ins->type->struct_index);
				break;
			case O_NEW_ARRAY:
				pf("  %%zptr.%zu_%zu = inttoptr i32 0 to ", i, k);
				wt(ins->type->arraytype);
				pf("*\n  %%psize.%zu_%zu = getelementptr ", i, k);
				wt(ins->type->arraytype);
				pf("* %%zptr.%zu_%zu, i64 %zu\n", i, k, ins->parameters[0]);
				pf("  %%size.%zu_%zu = ptrtoint", i, k)
				wt(ins->type->arraytype);
				pf("* %%psize.%zu_%zu to i64\n", i, k); // TODO: is i64 too long for 32-bit?
				pf("  %%raw.%zu_%zu = call ccc i8* @allocate(i64 %%size.%zu_%zu)\n", i, k, i, k); // TODO: garbage collection
				SET("bitcast i8* %%raw.%zu_%zu to ", i, k);
				wt(ins->type);
				break;
			case O_NOT:
				SETR("icmp eq ");
				wt(TP(0));
				pf(" %s, 0", RP(0));
				break;
			case O_NUMERIC: {
				const char *op;
				switch (ins->operation.numeric_type) {
				case O_ADD: op = "add"; break;
				case O_BAND: op = "and"; break;
				case O_BOR: op = "or"; break;
				case O_BXOR: op = "xor"; break;
				case O_DIV: op = "udiv"; break; // TODO: check signs
				case O_MOD: op = "urem"; break; // TODO: check signs
				case O_MUL: op = "mul"; break;
				case O_SUB: op = "sub"; break;
				default: abort();
				}
				SET("%s ", op);
				wt(ins->type);
				pf(" %s, %s", RP(0), RP(1));
			} break;
			case O_SET_FIELD:
				pf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
				type *t = TP(0);
				if (t->type != T_OBJECT) {
					fprintf(stderr, "expected object in SET_FIELD");
					abort();
				}
				wt(t);
				pf(" %s, i64 0, i32 %zu\n", RP(0), ins->parameters[1]);

				type *ft = system->structs[t->struct_index].fields[ins->parameters[1]].field_type;

				pf("  store ");
				wt(ft);
				pf(" %s, ", RP(2));
				wt(ft);
				pf("* %%temp.%zu_%zu, align 1", i, k);
				break;
			case O_SET_INDEX:
				pf("  %%temp.%zu_%zu = getelementptr inbounds ", i, k);
				wt(TP(0));
				pf(" %s, i64 %s\n", RP(0), RP(1));

				type *et = t->arraytype;

				pf("  store ");
				wt(et);
				pf(" %s, ", RP(2));
				wt(et);
				pf("* %%temp.%zu_%zu, align 1", i, k);
				break;
			case O_SHIFT: {
				const char *op;
				switch (ins->operation.shift_type) {
				case O_LSHIFT:
					op = "shl";
					break;
				case O_ASHIFT:
					op = "ashr";
					break;
				case O_RSHIFT:
					op = "lshr";
					break;
				default:
					abort();
				}
				SET("%s ", op);
				wt(ins->type);
				pf(" %s, %s", RP(0), RP(1));
			} break;
			case O_STR_CONCAT:
				fprintf(stderr, "this backend does not support string concatenation\n");
				exit(1);
			case O_CALL:
			case O_FUNCTION:
			case O_NUMERIC_ASSIGN:
			case O_POSTFIX:
			case O_SET_SYMBOL:
			case O_SHIFT_ASSIGN:
			case O_STR_CONCAT_ASSIGN:
			case O_TERNARY:
				abort();
			}
			pf("\n");
		}

		if (block->is_final) {
			pf("  br label %%Done\n");
		} else {
			bool needs_indirection = true;
			switch (block->tail.type) {
			case GOTO: {
				if (block->tail.first_block >= offset
				 && block->instructions[block->tail.first_block - offset].operation.type == O_BLOCKREF) {
					pf("  br label %%Block%zu\n", block->instructions[block->tail.first_block - offset].block_index);
					needs_indirection = false;
				} else {
					pf("  indirectbr i8* %s, [ ", pt_fetch(ref[block->tail.first_block]));
				}
			} break;
			case BRANCH: {
				if (block->tail.first_block >= offset
				 && block->tail.second_block >= offset
				 && block->instructions[block->tail.first_block - offset].operation.type == O_BLOCKREF
				 && block->instructions[block->tail.second_block - offset].operation.type == O_BLOCKREF) {
					pf("  br i1 %s, label %%Block%zu, label %%Block%zu\n",
					   pt_fetch(ref[block->tail.condition]),
					   block->instructions[block->tail.first_block - offset].block_index,
					   block->instructions[block->tail.second_block - offset].block_index);
					needs_indirection = false;
				} else {
					pf("  %%brtarget = select i1 %s, i8* %s, i8* %s\n",
					   pt_fetch(ref[block->tail.condition]),
					   pt_fetch(ref[block->tail.first_block]),
					   pt_fetch(ref[block->tail.second_block]));
					pf("  indirectbr i8* %%brtarget, [ ");
				}
			} break;
			}
			if (needs_indirection) {
				bool first = true;
				for (size_t j = 0; j < system->block_count; j++) {
					if (check_prototypes(block, &system->blocks[j], j)) {
						if (first) {
							first = false;
						} else {
							pf(", ");
						}
						pf("label %%Block%zu", j);
					}
				}
				pf(" ]\n");
			}
		}
	}

	pf("Done:\n  ret i32 0\n}\n");

	for (size_t i = 0; i < system->block_count; i++) {
		free(allrefs[i]);
	}

	pt_finalize(out);
}
