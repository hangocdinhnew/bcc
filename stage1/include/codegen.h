#pragma once

// clang-format off
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <map>
#include "ast.h"
// clang-format on

extern llvm::LLVMContext TheContext;
extern llvm::IRBuilder<> Builder;
extern std::unique_ptr<llvm::Module> TheModule;
extern llvm::FunctionCallee PrintfFunc;
llvm::FunctionCallee declarePrintf();
extern std::vector<std::unique_ptr<ExternAST>> externs;
extern std::vector<std::unique_ptr<FunctionAST>> functions;
extern std::map<std::string, llvm::Function *> FunctionProtos;
uint32_t fnv1a(const std::string &str);

llvm::Type *getLLVMTypeFor(const std::string &typeStr, llvm::LLVMContext &ctx);
