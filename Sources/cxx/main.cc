#include <iostream>
#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

extern "C" {
#include "cocodol.h"
}

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> Builder(TheContext);
static std::unique_ptr<llvm::Module> TheModule;

static void report_parse_error(ParseError error, const ParserState* state) {
  std::cout << error.location << ": error: " << error.message << std::endl;
}

int main(int argc, char* argv[]) {
  // Define a source input.
  const char* source = "print(40 + 2)";

  // Parse the program.
  Context context;
  ParserState parser;
  context_init(&context, source);
  parser_init(&parser, &context, NULL);

  NodeID** declv = (NodeID**)malloc(sizeof(NodeID*));
  size_t declc = parse(&parser, declv, report_parse_error);

  if (declc > 0) {
    free(*declv);
  }

  // Write code generation here ...

  // Cleanup.
  parser_deinit(&parser);
  context_deinit(&context);
  return 0;
}
