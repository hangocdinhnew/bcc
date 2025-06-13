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
std::map<std::string, llvm::Function *> FunctionProtos;
std::vector<std::unique_ptr<ExternAST>> externs;
std::vector<std::unique_ptr<bcc::FunctionAST>> functions;

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

llvm::Value *StringLiteralExprAST::codegen() {
  uint32_t hashValue = fnv1a(Val);
  std::string strname = "str." + std::to_string(hashValue);

  return Builder.CreateGlobalString(Val, strname, 0, TheModule.get());
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

llvm::Function *FunctionAST::codegen() {
  auto &ctx = TheContext;
  llvm::FunctionType *fnType;
  if (Name == "main") {
    fnType = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), false);
  } else {
    fnType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
  }

  auto function = llvm::Function::Create(
      fnType, llvm::Function::ExternalLinkage, Name, TheModule.get());

  auto BB = llvm::BasicBlock::Create(ctx, "entry", function);
  Builder.SetInsertPoint(BB);

  Body->codegen();

  if (Name == "main") {
    Builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
  } else {
    Builder.CreateRetVoid();
  }

  FunctionProtos[Name] = function;

  return function;
}

llvm::Value *CallExprAST::codegen() {
  llvm::Function *calleeF = FunctionProtos[Callee];
  if (!calleeF)
    throw std::runtime_error("Unknown function referenced: " + Callee);

  std::vector<llvm::Value *> argsV;
  for (auto &arg : Args)
    argsV.push_back(arg->codegen());

  if (calleeF->getReturnType()->isVoidTy()) {
    return Builder.CreateCall(calleeF, argsV);
  } else {
    return Builder.CreateCall(calleeF, argsV, "calltmp");
  }
}

uint32_t fnv1a(const std::string &str) {
  uint32_t hash = 2166136261u;
  for (char c : str) {
    hash ^= (uint8_t)c;
    hash *= 16777619u;
  }
  return hash;
}

llvm::Function *ExternAST::codegen() {
  auto &ctx = TheContext;
  llvm::FunctionType *funcType = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(ctx), false);

  auto function = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, Name, TheModule.get());

  FunctionProtos[Name] = function;
  return function;
}
} // namespace bcc
