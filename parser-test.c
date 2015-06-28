#include <stdio.h>
#include <stdlib.h>

#include "expression.h"
#include "stream.h"
#include "parser.h"
#include "analyze.h"

static inline void pad(uint8_t depth) {
  printf("%*s", depth, "");
}

void inspect_type(type *type) {
  if (!type) {
    printf("!VOID");
    return;
  }

  switch (type->type) {
  case T_ARRAY:
    printf("ARRAY [");
    inspect_type(type->arraytype);
    printf("]");
    break;
  case T_BOOL: printf("BOOL"); break;
  case T_F32: printf("F32"); break;
  case T_F64: printf("F64"); break;
  case T_F128: printf("F128"); break;
  case T_OBJECT:
    printf("OBJECT {%p}", type->classtype);
    break;
  case T_REF: printf("!!!REF!!!"); break;
  case T_S8: printf("S8"); break;
  case T_S16: printf("S16"); break;
  case T_S32: printf("S32"); break;
  case T_S64: printf("S64"); break;
  case T_STRING: printf("STRING"); break;
  case T_U8: printf("U8"); break;
  case T_U16: printf("U16"); break;
  case T_U32: printf("U32"); break;
  case T_U64: printf("U64"); break;
  case T_VOID: printf("VOID"); break;
  }
}

void inspect_expression(expression *e, uint8_t depth) {
  if (e == NULL) {
    pad(depth);
    printf("NIL\n");
    return;
  }

  switch (e->operation.type) {
  case O_BITWISE_NOT:
    pad(depth);
    printf("~ (\n");
    inspect_expression(e->value, depth + 2);
    pad(depth);
    printf(") -> ");
    inspect_type(e->type);
    printf("\n");
    break;
  case O_CALL:
    pad(depth);
    printf("CALL {%p} (\n", e->function);
    for (expression *value = e->value; value; value = value->next) {
      pad(depth + 2);
      printf("(\n");
      inspect_expression(value, depth + 4);
      pad(depth + 2);
      printf(") -> ");
      inspect_type(value->type);
      printf("\n");
    }
    pad(depth);
    printf(") -> ");
    inspect_type(e->type);
    printf("\n");
    break;
  case O_COMPARE: {
    char *cmp;
    switch (e->operation.compare_type) {
    case O_EQ: cmp = "=="; break;
    case O_GT: cmp = ">"; break;
    case O_GTE: cmp = ">="; break;
    case O_LT: cmp = "<"; break;
    case O_LTE: cmp = "<="; break;
    case O_NE: cmp = "!="; break;
    }
    pad(depth);
    printf("%s (\n", cmp);
    // inspect_expression(e->
    break;
  }
  case O_LITERAL:
    pad(depth);
    printf("LITERAL ");
    switch (e->type->type) {
    case T_BOOL: printf("BOOL %s", e->value_bool ? "true" : "false"); break;
    case T_OBJECT: printf("null"); break;
    case T_STRING: printf("STRING %s", e->value_string); break;
    case T_U8: printf("U8 %hhu", e->value_u8); break;
    case T_U16: printf("U16 %hu", e->value_u16); break;
    case T_U32: printf("U32 %u", e->value_u32); break;
    case T_U64: printf("U64 %lu", e->value_u64); break;
    case T_ARRAY:
    case T_F32:
    case T_F64:
    case T_F128:
    case T_REF:
    case T_S8:
    case T_S16:
    case T_S32:
    case T_S64:
    case T_VOID:
      abort();
    }
    printf("\n");
    break;
  case O_NEGATE:
    pad(depth);
    printf("NEGATE (\n");
    inspect_expression(e->value, depth + 2);
    pad(depth);
    printf(") -> ");
    inspect_type(e->type);
    printf("\n");
    break;
  case O_NEW:
    pad(depth);
    printf("NEW (\n");
    for (expression *argvalue = e->value; argvalue; argvalue = argvalue->next) {
      inspect_expression(argvalue, depth + 2);
    }
    pad(depth);
    printf(") -> ");
    inspect_type(e->type);
    printf("\n");
    break;
  case O_SET_SYMBOL:
    pad(depth);
    printf("SET-SYMBOL %s (\n", e->symbol_name);
    inspect_expression(e->value, depth + 2);
    pad(depth);
    printf(") -> ");
    inspect_type(e->type);
    printf("\n");
    break;
  default:
    printf("%*sNOT IMPLEMENTED\n", depth, "");
  }
}

void inspect_statement(statement *node, uint8_t depth) {
  if (node == NULL) {
    pad(depth);
    printf("NIL\n");
    return;
  }

  switch (node->type) {
  case S_BLOCK: {
    block_statement *block = (block_statement*) node;
    pad(depth);
    printf("{");
    if (block->fn_parent) {
      printf(":fn {%p}", block->fn_parent);
    }
    printf("\n");
    for (symbol_entry *v = block->variable_head; v; v = v->next) {
      pad(depth + 2);
      printf("VAR ");
      inspect_type(v->type);
      printf(" %s\n", v->symbol_name);
    }
    for (symbol_entry *c = block->class_head; c; c = c->next) {
      pad(depth + 2);
      printf("CLASS {%p}\n", c->classtype);
    }
    for (symbol_entry *f = block->function_head; f; f = f->next) {
      pad(depth + 2);
      printf("FN {%p}\n", f->function);
    }
    for (statement *inner = block->body; inner; inner = inner->next) {
      inspect_statement(inner, depth + 2);
    }
    pad(depth);
    printf("}\n");
    break;
  }
  case S_BREAK:
  case S_CONTINUE: {
    control_statement *control = (control_statement*) node;
    pad(depth);
    printf("%s", node->type == S_BREAK ? "BREAK" : "CONTINUE");
    if (control->target) {
      printf(" {%p}", control->target);
    }
    printf("\n");
    break;
  }
  case S_CLASS: {
    class_statement *cs = (class_statement*) node;
    class *class = cs->classtype;
    pad(depth);
    printf("DECLARE CLASS {%p} {\n", class);
    for (field *field = class->field; field; field = field->next) {
      pad(depth + 2);
      printf("FIELD ");
      inspect_type(field->field_type);
      printf(" %s\n", field->symbol_name);
    }
    pad(depth);
    printf("}\n");
    break;
  }
  case S_DECLARE: {
    declare_statement *decl = (declare_statement*) node;
    pad(depth);
    printf("DECLARE VAR ");
    inspect_type(decl->symbol_type);
    printf(" %s\n", decl->symbol_name);
    break;
  }
  case S_DEFINE:
    fprintf(stderr, "define statement\n");
    exit(1);
  case S_DO_WHILE:
  case S_WHILE: {
    loop_statement *loop = (loop_statement*) node;
    pad(depth);
    printf("%s {%p} (\n", loop->type == S_WHILE ? "WHILE" : "DO-WHILE", loop);
    inspect_expression(loop->condition, depth + 2);
    pad(depth);
    printf(") {\n");
    inspect_statement((statement*) loop->body, depth + 2);
    pad(depth);
    printf("}\n");
    break;
  }
  case S_EXPRESSION: {
    expression_statement *e = (expression_statement*) node;
    pad(depth);
    printf("(\n");
    inspect_expression(e->value, depth + 2);
    pad(depth);
    printf(")\n");
    break;
  }
  case S_FUNCTION: {
    function_statement *fns = (function_statement*) node;
    function *fn = fns->function;
    pad(depth);
    printf("DECLARE FN %s {%p} -> ", fn->function_name, fn);
    inspect_type(fn->return_type);
    printf(" (\n");
    for (argument *argument = fn->argument; argument;
        argument = argument->next) {
      pad(depth + 2);
      printf("ARGUMENT ");
      inspect_type(argument->argument_type);
      printf(" %s\n", argument->symbol_name);
    }
    pad(depth);
    printf(") {\n");
    inspect_statement((statement*) fn->body, depth + 2);
    pad(depth);
    printf("}\n");
    break;
  }
  case S_IF: {
    if_statement *cond = (if_statement*) node;
    pad(depth);
    printf("IF (\n");
    inspect_expression(cond->condition, depth + 2);
    pad(depth);
    printf(") {\n");
    inspect_statement((statement*) cond->first, depth + 2);
    pad(depth);
    printf("} {\n");
    inspect_statement((statement*) cond->second, depth + 2);
    pad(depth);
    printf("}\n");
    break;
  }
  case S_RETURN:
    break;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: parser-test <input-file>\n");
    exit(1);
  }
  FILE *src = fopen(argv[1], "r");
  if (src == NULL) {
    fprintf(stderr, "parser-test: no such file or directory\n");
    exit(1);
  }
  stream *in = open_stream(src);
  block_statement *block = parse(in);
  analyze(block);
  close_stream(in);
  // printf("%p\n", block);
  inspect_statement((statement*) block, 0);
  return 0;
}
