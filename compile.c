#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xalloc.h"
#include "expression.h"
#include "stream.h"
#include "parse.h"
#include "analyze.h"
#include "generate.h"
#include "backend.h"
#include "optimize.h"

#include "out/lib/core.h"

static block_statement *parse_stream(FILE *src) {
  stream *in = open_stream(src);
  block_statement *root = parse(in);
  close_stream(in);
  return root;
}

static block_statement *parse_file(char *filename) {
  FILE *src = fopen(filename, "r");
  if (src == NULL) {
    fprintf(stderr, "cub: no such file or directory\n");
    exit(1);
  }

  block_statement *root = parse_stream(src);
  fclose(src);

  return root;
}

static void backend_write_file(char *filename, code_system *system) {
  FILE *dest;

  dest = fopen(filename, "w");
  if (dest == NULL) {
    fprintf(stderr, "cub: error opening output-file\n");
    exit(1);
  }

  backend_write(system, dest);

  fflush(dest);
  fclose(dest);
}

static block_statement *wrap_core(block_statement *root) {
  FILE *src = fmemopen(lib_core_cub, lib_core_cub_len, "r");
  if (src == NULL) {
    fprintf(stderr, "cub: unable to open core library\n");
    exit(1);
  }
  stream *in = open_stream(src);

  fprintf(stderr, "parsing core\n");
  block_statement *core_block = parse(in);
  close_stream(in);
  fclose(src);

  statement **tail = &core_block->body;
  for (; *tail; tail = &(*tail)->next);
  *tail = (statement*) root;

  root->parent = (statement*) core_block;

  return core_block;
}

int main(int argc, char *argv[]) {
  block_statement *root;
  code_system *system;

  if (argc > 3 || (argc == 2 && (strcmp(argv[1], "-h") == 0 ||
      strcmp(argv[1], "--help") == 0))) {
    fprintf(stderr, "usage: cub [<input-file> [<output-file>]]\n");
    return 0;
  }

  fprintf(stderr, "parsing userspace\n");
  root = argc > 1 ? parse_file(argv[1]) : parse_stream(stdin);
  root = wrap_core(root);

  analyze(root);
  system = generate(root);
  optimize(system);

  // TODO: | llc | gcc -xassembler - out/lib/llvm-harness.s -o argv[2]
  if (argc > 2) {
    backend_write_file(argv[2], system);
  } else {
    backend_write(system, stdout);
  }

  // http://stackoverflow.com/q/31622764/345645
  exit(0);
}
