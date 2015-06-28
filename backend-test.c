#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "expression.h"
#include "stream.h"
#include "parser.h"
#include "analyze.h"
#include "generator.h"
#include "backend.h"
#include "optimize.h"

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: backend-test <input-file> <output-file>\n");
    exit(1);
  }
  FILE *src = fopen(argv[1], "r");
  if (src == NULL) {
    fprintf(stderr, "backend-test: no such file or directory\n");
    exit(1);
  }
  stream *in = open_stream(src);
  block_statement *block = parse(in);
  close_stream(in);
  fclose(src);

  analyze(block);
  code_system *system = generate(block);
  optimize(system);

  FILE *out = fopen(argv[2], "w");
  if (out == NULL) {
    fprintf(stderr, "backend-test: error opening output-file\n");
    exit(1);
  }

  backend_write(system, out);

  fflush(out);
  fclose(out);

  return 0;
}
