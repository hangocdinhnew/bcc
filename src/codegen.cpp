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
  llvm::Value *val = Expr->codegen();
  llvm::Value *fmt =
      Builder.CreateGlobalString("%s\n", "fmtStr", 0, TheModule.get());
  return Builder.CreateCall(PrintfFunc, {fmt, val});
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
} // namespace bcc
