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
std::map<std::string, llvm::Function *> FunctionProtos;
std::vector<std::unique_ptr<ExternAST>> externs;
std::vector<std::unique_ptr<bcc::FunctionAST>> functions;

static std::map<std::string, llvm::Value *> NamedValues;

llvm::Value *NumberExprAST::codegen() {
  if (Val == (int64_t)Val) {
    return llvm::ConstantInt::get(TheContext,
                                  llvm::APInt(32, (int64_t)Val, true));
  } else {
    return llvm::ConstantFP::get(TheContext, llvm::APFloat(Val));
  }
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

  if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
    // If either side is float, promote and do fadd
    if (L->getType()->isIntegerTy())
      L = Builder.CreateSIToFP(L, llvm::Type::getDoubleTy(TheContext),
                               "int_to_double");
    if (R->getType()->isIntegerTy())
      R = Builder.CreateSIToFP(R, llvm::Type::getDoubleTy(TheContext),
                               "int_to_double");

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
  } else if (L->getType()->isIntegerTy(32) && R->getType()->isIntegerTy(32)) {
    switch (Op) {
    case '+':
      return Builder.CreateAdd(L, R, "addtmp");
    case '-':
      return Builder.CreateSub(L, R, "subtmp");
    case '*':
      return Builder.CreateMul(L, R, "multmp");
    case '/':
      return Builder.CreateSDiv(L, R, "divtmp");
    default:
      throw std::runtime_error("invalid binary operator");
    }
  } else {
    throw std::runtime_error("Unsupported operand types for binary operator");
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

llvm::Type *getLLVMTypeFor(const std::string &typeStr, llvm::LLVMContext &ctx) {
  if (typeStr == "i32")
    return llvm::Type::getInt32Ty(ctx);
  if (typeStr == "i64")
    return llvm::Type::getInt64Ty(ctx);
  if (typeStr == "f32")
    return llvm::Type::getFloatTy(ctx);
  if (typeStr == "f64")
    return llvm::Type::getDoubleTy(ctx);
  if (typeStr == "void")
    return llvm::Type::getVoidTy(ctx);
  if (typeStr == "ptr")
    return llvm::PointerType::get(llvm::Type::getInt8Ty(ctx), 0);
  if (typeStr.size() > 1 && typeStr.back() == '*') {
    auto base = getLLVMTypeFor(typeStr.substr(0, typeStr.size() - 1), ctx);
    return llvm::PointerType::getUnqual(base);
  }
  throw std::runtime_error("Unsupported type: " + typeStr);
}

llvm::Function *ExternAST::codegen() {
  auto &ctx = TheContext;

  llvm::Type *retTy = getLLVMTypeFor(RetType, ctx);

  std::vector<llvm::Type *> argTys;
  bool isVarArg = false;

  for (const auto &arg : ArgTypes) {
    if (arg == "...") {
      isVarArg = true;
    } else {
      argTys.push_back(getLLVMTypeFor(arg, ctx));
    }
  }

  auto funcType = llvm::FunctionType::get(retTy, argTys, isVarArg);
  auto callee = TheModule->getOrInsertFunction(Name, funcType).getCallee();
  auto func = llvm::cast<llvm::Function>(callee);
  FunctionProtos[Name] = func;
  return func;
}
} // namespace bcc
