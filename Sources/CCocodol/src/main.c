#include <stdlib.h>
#include <stdio.h>

#include "cocodol.h"

static void report_parse_error(ParseError error, const ParserState* state) {
  printf("%zu: error: %s\n", error.location, error.message);
}

static void report_eval_error(EvalError error, const EvalState* state) {
  printf("%zu: error: %s\n", error.start, error.message);
}

int main(int argc, char** argv) {
  // Get the path of the input file.
  if (argc < 2) {
    fputs("error: no input file\n", stdout);
    return 1;
  }

  FILE* fp = fopen(argv[1], "r");
  if (!fp) {
    printf("error: file not found: '%s'\n", argv[1]);
    return 1;
  }

  // Open and read the input file.
  fseek(fp, 0L, SEEK_END);
  long byte_count = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  char* source = calloc(byte_count, sizeof(char));
  if (source == NULL) {
    printf("error: not enough memory\n");
    return 1;
  }

  fread(source, sizeof(char), byte_count, fp);
  fclose(fp);

  // Parse the program.
  Context context;
  ParserState parser;
  context_init(&context, source);
  parser_init(&parser, &context, NULL);

  NodeID** declv = malloc(sizeof(NodeID*));
  size_t declc = parse(&parser, declv, report_parse_error);

  int status = 0;
  if (declc > 0) {
    // Evaluate the program.
    EvalState eval;
    eval_init(&eval, &context);
    status = eval_program(&eval, *declv, declc, report_eval_error);
    eval_deinit(&eval);
    free(*declv);
  }
  free(declv);

  // Cleanup.
  parser_deinit(&parser);
  context_deinit(&context);
  free(source);

  return status;
}
