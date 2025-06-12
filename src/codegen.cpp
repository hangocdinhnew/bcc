// clang-format off
#include "ast.h"
#include "codegen.h"
#include <llvm/IR/Constants.h>
#include <map>
#include <iostream>
// clang-format on

namespace bcc {
llvm::LLVMContext TheContext;
llvm::IRBuilder<> Builder(TheContext);
std::unique_ptr<llvm::Module> TheModule =
    std::make_unique<llvm::Module>("BCompiler", TheContext);
llvm::FunctionCallee PrintfFunc;

static std::map<std::string, llvm::Value *> NamedValues;

llvm::Value *NumberExprAST::codegen() {
  return llvm::ConstantFP::get(TheContext, llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegen() {
  auto V = NamedValues[Name];
  if (!V) {
    throw std::runtime_error("Unknown variable name");
  }
  return V;
}

llvm::Value *BinaryExprAST::codegen() {
  auto L = LHS->codegen();
  auto R = RHS->codegen();

  switch (Op) {
  case '+':
    return Builder.CreateFAdd(L, R, "addtmp");
  case '-':
    return Builder.CreateFSub(L, R, "subtmp");
  case '*':
    return Builder.CreateFMul(L, R, "multmp");
  case '/':
    return Builder.CreateFDiv(L, R, "divtmp");
  default:
    throw std::runtime_error("invalid binary operator");
  }
}

llvm::Value *PrintExprAST::codegen() {
  std::vector<llvm::Value *> printfArgs;

  // First argument: format string
  llvm::Value *fmt = Format->codegen();
  printfArgs.push_back(fmt);

  // Remaining arguments
  for (auto &arg : Args)
    printfArgs.push_back(arg->codegen());

  return Builder.CreateCall(PrintfFunc, printfArgs, "callprintf");
}

llvm::Value *StringLiteralExprAST::codegen() {
  return Builder.CreateGlobalString(Val, "str", 0, TheModule.get());
}

StringLiteralExprAST::StringLiteralExprAST(const std::string &val) : Val(val) {}

void generateMain(std::unique_ptr<ExprAST> &expr) {
  using namespace llvm;

  FunctionType *mainType =
      FunctionType::get(Type::getInt32Ty(TheContext), false);
  Function *mainFunc = Function::Create(mainType, Function::ExternalLinkage,
                                        "main", TheModule.get());

  BasicBlock *BB = BasicBlock::Create(TheContext, "entry", mainFunc);
  Builder.SetInsertPoint(BB);

  expr->codegen();

  Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(TheContext), 0));
}

llvm::FunctionCallee declarePrintf() {
  auto &ctx = TheContext;

  llvm::FunctionType *printfType = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(ctx),
      llvm::PointerType::get(llvm::Type::getInt8Ty(ctx), 0), true);

  return TheModule->getOrInsertFunction("printf", printfType);
}
} // namespace bcc
