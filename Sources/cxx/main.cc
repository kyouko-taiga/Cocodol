#include <iostream>
#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

extern "C" {
#include "cocodol.h"
}

static llvm::LLVMContext TheLLVMContext;
static std::unique_ptr<llvm::Module>      TheModule;
static std::unique_ptr<llvm::IRBuilder<>> TheBuilder;

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

  using namespace llvm;

  // Create a module.
  TheModule  = std::make_unique<Module>("main", TheLLVMContext);
  TheBuilder = std::make_unique<IRBuilder<>>(TheLLVMContext);

  // Write code generation here ...
  auto i32 = Type::getInt32Ty(TheLLVMContext);

  std::vector<Type*> dom;
  auto mainType = FunctionType::get(i32, dom, false);
  auto mainFunc =
    Function::Create(mainType, Function::ExternalLinkage, "main", *TheModule);
  auto entry = BasicBlock::Create(TheLLVMContext, "entry", mainFunc);
  TheBuilder->SetInsertPoint(entry);
  TheBuilder->CreateRet(ConstantInt::get(i32, 0, true));

  TheModule->print(llvm::errs(), nullptr);

  // Cleanup.
  parser_deinit(&parser);
  context_deinit(&context);
  return 0;
}
