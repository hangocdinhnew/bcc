// clang-format off
#include "ast.h"
#include "codegen.h"
#include <map>
#include <iostream>
// clang-format on

LLVMContextRef TheContext = nullptr;
LLVMBuilderRef Builder = nullptr;
LLVMModuleRef TheModule = nullptr;
struct FunctionProtoInfo {
  LLVMValueRef FuncVal;
  LLVMTypeRef FuncType;
};

std::map<std::string, FunctionProtoInfo> FunctionProtos;
static std::map<std::string, LLVMValueRef> NamedValues;
static thread_local FunctionAST *CurrentFunction = nullptr;

LLVMValueRef NumberExprAST::codegen(LLVMModuleRef module,
                                    LLVMBuilderRef builder) {
  if (Val == (int64_t)Val) {
    return LLVMConstInt(LLVMInt32TypeInContext(TheContext), (int64_t)Val, 1);
  } else {
    return LLVMConstReal(LLVMDoubleTypeInContext(TheContext), Val);
  }
}

LLVMValueRef VariableExprAST::codegen(LLVMModuleRef module,
                                      LLVMBuilderRef builder) {
  LLVMValueRef V = NamedValues[Name];
  if (!V) {
    throw std::runtime_error("Unknown variable name: " + Name);
  }

  LLVMTypeRef vTy = LLVMTypeOf(V);
  if (LLVMGetTypeKind(vTy) == LLVMPointerTypeKind) {
    LLVMTypeRef loadTy = nullptr;

    if (LLVMIsAAllocaInst(V)) {
      loadTy = LLVMGetAllocatedType(V);
    } else {
      loadTy = LLVMGetElementType(vTy);
    }

    if (!loadTy) {
      throw std::runtime_error("Cannot determine load type for: " + Name);
    }

    return LLVMBuildLoad2(builder, loadTy, V, (Name + "_val").c_str());
  }

  return V;
}

LLVMValueRef BinaryExprAST::codegen(LLVMModuleRef module,
                                    LLVMBuilderRef builder) {
  LLVMValueRef L = LHS->codegen(module, builder);
  LLVMValueRef R = RHS->codegen(module, builder);

  LLVMTypeKind lTypeKind = LLVMGetTypeKind(LLVMTypeOf(L));
  LLVMTypeKind rTypeKind = LLVMGetTypeKind(LLVMTypeOf(R));

  bool lIsFP =
      (lTypeKind == LLVMFloatTypeKind || lTypeKind == LLVMDoubleTypeKind);
  bool rIsFP =
      (rTypeKind == LLVMFloatTypeKind || rTypeKind == LLVMDoubleTypeKind);

  if (lIsFP || rIsFP) {
    if (!lIsFP)
      L = LLVMBuildSIToFP(builder, L, LLVMDoubleTypeInContext(TheContext),
                          "int_to_double");
    if (!rIsFP)
      R = LLVMBuildSIToFP(builder, R, LLVMDoubleTypeInContext(TheContext),
                          "int_to_double");

    switch (Op) {
    case '+':
      return LLVMBuildFAdd(builder, L, R, "addtmp");
    case '-':
      return LLVMBuildFSub(builder, L, R, "subtmp");
    case '*':
      return LLVMBuildFMul(builder, L, R, "multmp");
    case '/':
      return LLVMBuildFDiv(builder, L, R, "divtmp");
    default:
      throw std::runtime_error("invalid binary operator");
    }
  } else {
    switch (Op) {
    case '+':
      return LLVMBuildAdd(builder, L, R, "addtmp");
    case '-':
      return LLVMBuildSub(builder, L, R, "subtmp");
    case '*':
      return LLVMBuildMul(builder, L, R, "multmp");
    case '/':
      return LLVMBuildSDiv(builder, L, R, "divtmp");
    default:
      throw std::runtime_error("invalid binary operator");
    }
  }
}

LLVMValueRef StringLiteralExprAST::codegen(LLVMModuleRef module,
                                           LLVMBuilderRef builder) {
  uint32_t hashValue = fnv1a(Val);
  std::string strname = "str." + std::to_string(hashValue);
  return LLVMBuildGlobalStringPtr(builder, Val.c_str(), strname.c_str());
}

LLVMValueRef ExternAST::codegen(LLVMModuleRef module) {
  LLVMTypeRef retTy = getLLVMTypeFor(RetType);

  std::vector<LLVMTypeRef> argTys;
  bool isVarArg = false;

  for (const std::string &arg : ArgTypes) {
    if (arg == "...") {
      isVarArg = true;
    } else {
      argTys.push_back(getLLVMTypeFor(arg));
    }
  }

  LLVMTypeRef funcType =
      LLVMFunctionType(retTy, argTys.data(), argTys.size(), isVarArg);
  LLVMValueRef func = LLVMAddFunction(module, Name.c_str(), funcType);
  FunctionProtos[Name] = {func, funcType};

  return func;
}

LLVMValueRef ReturnExprAST::codegen(LLVMModuleRef module,
                                    LLVMBuilderRef builder) {
  if (Expr) {
    LLVMValueRef val = Expr->codegen(module, builder);
    LLVMBuildRet(builder, val);
  } else {
    LLVMBuildRetVoid(builder);
  }
  return nullptr;
}

LLVMValueRef FunctionAST::codegen(LLVMModuleRef module,
                                  LLVMBuilderRef builder) {
  CurrentFunction = this;
  LLVMTypeRef retTy = getLLVMTypeFor(RetType);

  std::vector<LLVMTypeRef> paramTypes;
  for (const std::string &argType : ArgTypes)
    paramTypes.push_back(getLLVMTypeFor(argType));

  LLVMTypeRef fnType =
      LLVMFunctionType(retTy, paramTypes.data(), paramTypes.size(), 0);
  LLVMValueRef function = LLVMGetNamedFunction(module, Name.c_str());
  if (function) {
    if (LLVMCountBasicBlocks(function) != 0)
      throw std::runtime_error("Function redefinition: " + Name);
  } else {
    function = LLVMAddFunction(module, Name.c_str(), fnType);
  }

  FunctionProtos[Name] = {function, fnType};
  LLVMBasicBlockRef entry =
      LLVMAppendBasicBlockInContext(TheContext, function, "entry");
  LLVMPositionBuilderAtEnd(builder, entry);

  NamedValues.clear();
  unsigned idx = 0;
  for (LLVMValueRef arg = LLVMGetFirstParam(function); arg;
       arg = LLVMGetNextParam(arg)) {
    std::string argName = "arg" + std::to_string(++idx);
    LLVMSetValueName(arg, argName.c_str());
    NamedValues["%" + std::to_string(idx)] = arg;
  }

  Body->codegen(module, builder);

  if (LLVMGetBasicBlockTerminator(entry) != nullptr) {
    if (LLVMGetReturnType(fnType) == LLVMVoidTypeInContext(TheContext)) {
      throw std::runtime_error("Void function cannot return: " + Name);
    }
  }

  if (LLVMGetBasicBlockTerminator(entry) == nullptr) {
    if (LLVMGetReturnType(fnType) == LLVMVoidTypeInContext(TheContext)) {
      LLVMBuildRetVoid(builder);
    } else {
      throw std::runtime_error("Non-void function missing return statement: " +
                               Name);
    }
  }

  CurrentFunction = nullptr;
  return function;
}

LLVMValueRef CallExprAST::codegen(LLVMModuleRef module,
                                  LLVMBuilderRef builder) {
  auto it = FunctionProtos.find(Callee);
  if (it == FunctionProtos.end()) {
    throw std::runtime_error("Implicit declaration of function not allowed: " +
                             Callee);
  }

  LLVMValueRef calleeF = it->second.FuncVal;
  LLVMTypeRef calleeType = it->second.FuncType;

  std::vector<LLVMValueRef> argsV;
  for (auto &arg : Args) {
    argsV.push_back(arg->codegen(module, builder));
  }

  LLVMTypeRef retType = LLVMGetReturnType(calleeType);

  if (LLVMGetTypeKind(retType) == LLVMVoidTypeKind) {
    return LLVMBuildCall2(builder, calleeType, calleeF, argsV.data(),
                          argsV.size(), "");
  } else {
    return LLVMBuildCall2(builder, calleeType, calleeF, argsV.data(),
                          argsV.size(), "calltmp");
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

LLVMTypeRef getLLVMTypeFor(const std::string &typeStr) {
  if (typeStr == "i32")
    return LLVMInt32TypeInContext(TheContext);
  if (typeStr == "i64")
    return LLVMInt64TypeInContext(TheContext);
  if (typeStr == "f32")
    return LLVMFloatTypeInContext(TheContext);
  if (typeStr == "f64")
    return LLVMDoubleTypeInContext(TheContext);
  if (typeStr == "void")
    return LLVMVoidTypeInContext(TheContext);
  if (typeStr == "ptr")
    return LLVMPointerType(LLVMInt8TypeInContext(TheContext), 0);
  if (typeStr.size() > 1 && typeStr.back() == '*') {
    LLVMTypeRef base = getLLVMTypeFor(typeStr.substr(0, typeStr.size() - 1));
    return LLVMPointerType(base, 0);
  }
  throw std::runtime_error("Unsupported type: " + typeStr);
}

LLVMValueRef PositionalParamExprAST::codegen(LLVMModuleRef module,
                                             LLVMBuilderRef builder) {
  std::string paramName = "%" + std::to_string(Index);
  auto it = NamedValues.find(paramName);
  if (it == NamedValues.end()) {
    throw std::runtime_error("Unknown positional parameter: " + paramName);
  }
  return it->second;
}

LLVMValueRef VarDeclExprAST::codegen(LLVMModuleRef module,
                                     LLVMBuilderRef builder) {
  LLVMTypeRef llvmType = getLLVMTypeFor(Type);
  LLVMBasicBlockRef entryBlock = LLVMGetEntryBasicBlock(
      LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder)));

  LLVMBuilderRef tmpBuilder = LLVMCreateBuilderInContext(TheContext);

  if (LLVMGetFirstInstruction(entryBlock)) {
    LLVMPositionBuilderBefore(tmpBuilder, LLVMGetFirstInstruction(entryBlock));
  } else {
    LLVMPositionBuilderAtEnd(tmpBuilder, entryBlock);
  }

  LLVMValueRef alloca = LLVMBuildAlloca(tmpBuilder, llvmType, Name.c_str());
  LLVMDisposeBuilder(tmpBuilder);

  if (InitExpr) {
    LLVMValueRef initVal = InitExpr->codegen(module, builder);
    LLVMTypeRef initTy = LLVMTypeOf(initVal);

    if (LLVMTypeOf(initVal) != llvmType) {
      throw std::runtime_error("Initializer type mismatch for " + Name);
    }
    LLVMBuildStore(builder, initVal, alloca);
  }

  NamedValues[Name] = alloca;
  return alloca;
}
