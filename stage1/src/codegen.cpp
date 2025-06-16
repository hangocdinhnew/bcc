// clang-format off
#include "ast.h"
#include "codegen.h"
#include <llvm/IR/Constants.h>
#include <map>
#include <iostream>
// clang-format on

llvm::LLVMContext TheContext;
llvm::IRBuilder<> Builder(TheContext);
std::unique_ptr<llvm::Module> TheModule =
    std::make_unique<llvm::Module>("B+ Compiler", TheContext);
std::map<std::string, llvm::Function *> FunctionProtos;
std::vector<std::unique_ptr<ExternAST>> externs;
std::vector<std::unique_ptr<FunctionAST>> functions;

static std::map<std::string, llvm::Value *> NamedValues;
static thread_local FunctionAST *CurrentFunction = nullptr;

llvm::Value *NumberExprAST::codegen() {
  if (Val == (int64_t)Val) {
    return llvm::ConstantInt::get(TheContext,
                                  llvm::APInt(32, (int64_t)Val, true));
  } else {
    return llvm::ConstantFP::get(TheContext, llvm::APFloat(Val));
  }
}

llvm::Value *VariableExprAST::codegen() {
  auto *V = NamedValues[Name];
  if (!V)
    throw std::runtime_error("Unknown variable name: " + Name);

  if (V->getType()->isPointerTy()) {
    llvm::Type *loadTy = nullptr;
    if (auto *alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
      loadTy = alloca->getAllocatedType();
    }

    if (!loadTy)
      throw std::runtime_error("Cannot determine load type for: " + Name);

    return Builder.CreateLoad(loadTy, V, llvm::Twine(Name) + "_val");
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

llvm::Function *FunctionAST::codegen() {
  CurrentFunction = this;
  auto &ctx = TheContext;

  llvm::Type *retTy = getLLVMTypeFor(RetType, ctx);

  std::vector<llvm::Type *> paramTypes;
  for (const auto &argType : ArgTypes) {
    paramTypes.push_back(getLLVMTypeFor(argType, ctx));
  }

  llvm::FunctionType *fnType =
      llvm::FunctionType::get(retTy, paramTypes, false);

  llvm::Function *function = TheModule->getFunction(Name);
  if (function) {
    if (!function->empty())
      throw std::runtime_error("Function redefinition: " + Name);
  } else {
    function = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                      Name, TheModule.get());
  }

  FunctionProtos[Name] = function;

  auto BB = llvm::BasicBlock::Create(ctx, "entry", function);
  Builder.SetInsertPoint(BB);
  NamedValues.clear();
  unsigned idx = 0;
  for (auto &arg : function->args()) {
    arg.setName("arg" + std::to_string(idx + 1));
    NamedValues["%" + std::to_string(idx + 1)] = &arg;
    idx++;
  }

  Body->codegen();

  CurrentFunction = nullptr;

  if (BB->getTerminator()) {
    if (retTy->isVoidTy()) {
      throw std::runtime_error("Void function calls return: " + Name);
    }
  }

  if (!BB->getTerminator()) {
    if (retTy->isVoidTy()) {
      Builder.CreateRetVoid();
    } else {
      throw std::runtime_error("Non-void function missing return statement: " +
                               Name);
    }
  }

  return function;
}

llvm::Value *CallExprAST::codegen() {
  llvm::Function *calleeF = FunctionProtos[Callee];
  if (!calleeF)
    throw std::runtime_error("Implicit declaration of function not allowed: " +
                             Callee);

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

llvm::Value *ReturnExprAST::codegen() {
  if (Expr) {
    auto val = Expr->codegen();
    Builder.CreateRet(val);
  } else {
    Builder.CreateRetVoid();
  }
  return nullptr;
}

llvm::Value *PositionalParamExprAST::codegen() {
  if (!CurrentFunction)
    throw std::runtime_error("Positional param used outside function");

  auto *func = FunctionProtos[CurrentFunction->getName()];
  auto args = func->arg_begin();

  std::advance(args, Index - 1);

  if (Index <= 0 || Index > func->arg_size())
    throw std::runtime_error("Invalid positional param index");

  return &*args;
}

llvm::Value *VarDeclExprAST::codegen() {
  llvm::Type *llvmType = getLLVMTypeFor(Type, TheContext);
  llvm::Function *func = Builder.GetInsertBlock()->getParent();

  llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(),
                               func->getEntryBlock().begin());
  llvm::AllocaInst *alloca = tmpBuilder.CreateAlloca(llvmType, nullptr, Name);

  if (InitExpr) {
    llvm::Value *initVal = InitExpr->codegen();
    if (initVal->getType() != llvmType) {
      throw std::runtime_error("Initializer type mismatch for " + Name);
    }
    Builder.CreateStore(initVal, alloca);
  }

  NamedValues[Name] = alloca;

  return alloca;
}
