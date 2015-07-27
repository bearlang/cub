#include <stdio.h>
#include <string.h>

#include "backend.h"

#define pf(...) { fprintf(out, __VA_ARGS__); }
#define wt(t) { wti(out, t); }

static void wti(FILE *out, type *t) {
	if (t == NULL) {
		pf("NULL");
		return;
	}
	switch (t->type) {
	case T_ARRAY:
		fprintf(stderr, "array type not implemented\n");
		exit(1);
	case T_BLOCKREF:
		pf("func(");
		argument *arg = t->blocktype;
		if (arg) {
			wt(arg->argument_type);
			while ((arg = arg->next)) {
				pf(", ");
				wt(arg->argument_type);
			}
		}
		pf(")");
		break;
	case T_BOOL:
		pf("bool");
		break;
	case T_F32:
		pf("fp32");
		break;
	case T_F64:
		pf("fp64");
		break;
	case T_F128:
		pf("fp128");
		break;
	case T_OBJECT:
		pf("S%zu", t->struct_index);
		break;
	case T_REF:
		abort();
	case T_U8:
		pf("u8");
		break;
	case T_S8:
		pf("s8");
		break;
	case T_U16:
		pf("u16");
		break;
	case T_S16:
		pf("s16");
		break;
	case T_U32:
		pf("u32");
		break;
	case T_S32:
		pf("s32");
		break;
	case T_U64:
		pf("u64");
		break;
	case T_S64:
		pf("s64");
		break;
	case T_STRING:
		pf("string");
		break;
	case T_VOID:
		pf("void");
		break;
	default:
		abort();
	}
}

void backend_write(code_system *system, FILE *out) {

	for (size_t i = 0; i < system->struct_count; i++) {
		code_struct *str = get_code_struct(system, i);

		pf("S%zu { ", i);
		for (size_t j = 0; j < str->field_count; j++) {
			if (j != 0) {
				pf(", ");
			}
			wt(str->fields[j].field_type);
		}
		pf(" }\n");
	}
	pf("\n");

#define TYPEOF(x) ((x) < block->parameter_count ? block->parameters[x].field_type : block->instructions[x - block->parameter_count].type)

	for (size_t i = 0; i < system->block_count; i++) {
		code_block *block = get_code_block(system, i);

		pf("B%zu(", i);

		if (block->parameter_count) {
			wt(block->parameters[0].field_type);
			pf(" $0");
			for (size_t p = 1; p < block->parameter_count; p++) {
				pf(", ");
				wt(block->parameters[p].field_type);
				pf(" $%zu", p);
			}
		}

		pf("):\n");

#define AP(n) (ins->parameters[(n)])
#define TP(n) (TYPEOF(ins->parameters[(n)]))

#define SET(x, ...) { pf("  $%zu ", k); wt(ins->type); pf(" := "); pf(x, __VA_ARGS__); }
#define SETR(x) SET("%s", x)

		for (size_t j = 0; j < block->instruction_count; j++) {
			size_t k = j + block->parameter_count;
			code_instruction *ins = &block->instructions[j];

			switch (ins->operation.type) {
			case O_BITWISE_NOT:
				SET("xor $%zu, -1", AP(0));
				break;
			case O_BLOCKREF:
				SET("ref B%zu", ins->block_index);
				break;
			case O_CAST: {
				const char *name = NULL;
				switch (ins->operation.cast_type) {
				case O_UPCAST:
					name = "upcast";
					break;
				case O_DOWNCAST: // TODO: check downcasts
					name = "downcast";
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
				SET("%s $%zu ", name, AP(0));
				wt(TP(0));
			} break;
			case O_COMPARE: {
				const char *op;
				type *left = TP(0), *right = TP(1);
				if (left->type != right->type) {
					fputs("comparison type mismatch\n", stderr);
					exit(1);
				}
				bool is_signed = false;
				switch (left->type) {
				case T_S8:
				case T_S16:
				case T_S32:
				case T_S64:
					is_signed = true;
				case T_U8:
				case T_U16:
				case T_U32:
				case T_U64:
				case T_BOOL:
					switch (ins->operation.compare_type) {
					case O_EQ: op = "icmp eq"; break;
					case O_GT: op = is_signed ? "icmp sgt" : "icmp ugt"; break;
					case O_GTE: op = is_signed ? "icmp sge" : "icmp uge"; break;
					case O_LT: op = is_signed ? "icmp slt" : "icmp ult"; break;
					case O_LTE: op = is_signed ? "icmp sle" : "icmp ule"; break;
					case O_NE: op = "icmp ne"; break;
					}
					break;
				case T_F32:
				case T_F64:
				case T_F128:
					switch (ins->operation.compare_type) {
					case O_EQ: op = "fcmp eq"; break;
					case O_GT: op = "fcmp gt"; break;
					case O_GTE: op = "fcmp ge"; break;
					case O_LT: op = "fcmp lt"; break;
					case O_LTE: op = "fcmp le"; break;
					case O_NE: op = "fcmp ne"; break;
					}
					break;
				case T_OBJECT:
				case T_BLOCKREF:
					switch (ins->operation.compare_type) {
					case O_EQ: op = "icmp eq"; break;
					case O_NE: op = "icmp ne"; break;
					case O_GT:
					case O_GTE:
					case O_LT:
					case O_LTE:
						abort();
					}
					break;
				case T_STRING:
					switch (ins->operation.compare_type) {
					case O_EQ:
						op = "scmp eq"; break;
						break;
					case O_NE:
						op = "scmp ne"; break;
						break;
					case O_GT:
					case O_GTE:
					case O_LT:
					case O_LTE:
						abort();
					}
					break;
				default:
					fputs("unsupported type to compare\n", stderr);
					exit(1);
				}
				SET("$%zu %s $%zu ", AP(0), op, AP(0));
				wt(left);
			} break;
			case O_GET_FIELD:
				SETR("load ");
				wt(TP(0));
				pf(" $%zu->%zu", AP(0), ins->parameters[1]);
				break;
			case O_GET_INDEX:
				SETR("index ");
				wt(TP(0));
				pf(" $%zu[$%zu]", AP(0), AP(1));
				break;
			case O_GET_SYMBOL:
				SET("$%zu", AP(0));
				break;
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
					SETR(ins->value_bool ? "true" : "false");
					break;
				case T_OBJECT:
					SETR("null");
					break;
				case T_STRING:
					SET("string \"%s\"", ins->value_string);
					break;
				case T_U8:
					SET("%hhu", ins->value_u8);
					break;
				case T_U16:
					SET("%hu", ins->value_u16);
					break;
				case T_U32:
					SET("%u", ins->value_u32);
					break;
				case T_U64:
					SET("%lu", ins->value_u64);
					break;
				}
				break;
			case O_NATIVE: {
				if (is_void(ins->type)) {
					pf("  call void");
				} else {
					SETR("call ");
					wt(ins->type);
				}
				pf(" native %s(", ins->native_call);
				size_t count = ins->parameters[0];
				if (count) {
					pf("$%zu", AP(1));
					for (size_t i = 2; i <= count; i++) {
						pf(", $%zu", AP(i));
					}
				}
				pf(")");
			} break;
			case O_NEGATE:
				SET("negate $%zu", AP(0));
				break;
			case O_NEW:
				SETR("new");
				// ins->parameters[0] is the length of O_NEW_ARRAY
				break;
			case O_NOT:
				SET("not $%zu", AP(0));
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
				SET("$%zu %s $%zu", AP(0), op, AP(1));
			} break;
			case O_SET_FIELD:
				pf("  store ");
				wt(TP(0));
				pf(" $%zu->%zu = ", AP(0), ins->parameters[1]);
				wt(TP(2));
				pf(" $%zu", AP(2));
				break;
			case O_SET_INDEX:
				pf("  setindex ");
				wt(TP(0));
				pf(" $%zu[$%zu] = ", AP(0), AP(1));
				wt(TP(2));
				pf(" $%zu", AP(2));
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
				SET("$%zu %s $%zu", AP(0), op, AP(1));
			} break;
			// TODO: implement me!
			case O_NEW_ARRAY:
			case O_STR_CONCAT:
			case O_IDENTITY:
			case O_INSTANCEOF:
			// should not exist post-generation
			case O_CALL:
			case O_FUNCTION:
			case O_LOGIC:
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
			pf("  exit\n\n");
		} else {
			switch (block->tail.type) {
			case GOTO: {
				pf("  goto $%zu(", block->tail.first_block);
				size_t params = block->tail.parameter_count;
				if (params) {
					wt(TYPEOF(block->tail.parameters[0]));
					pf(" $%zu", block->tail.parameters[0]);
					for (size_t i = 1; i < params; i++) {
						pf(", ");
						wt(TYPEOF(block->tail.parameters[i]));
						pf(" $%zu", block->tail.parameters[i]);
					}
				}
				pf(")\n\n");
			} break;
			case BRANCH: {
				pf("  branch $%zu -> T $%zu F $%zu(", block->tail.condition, block->tail.first_block, block->tail.second_block);
				size_t params = block->tail.parameter_count;
				if (params) {
					wt(TYPEOF(block->tail.parameters[0]));
					pf(" $%zu", block->tail.parameters[0]);
					for (size_t i = 1; i < params; i++) {
						pf(", ");
						wt(TYPEOF(block->tail.parameters[i]));
						pf(" $%zu", block->tail.parameters[i]);
					}
				}
				pf(")\n\n");
			} break;
			}
		}
	}
}
