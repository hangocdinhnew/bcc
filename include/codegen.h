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

namespace bcc {
extern llvm::LLVMContext TheContext;
extern llvm::IRBuilder<> Builder;
extern std::unique_ptr<llvm::Module> TheModule;
extern llvm::FunctionCallee PrintfFunc;
void generateMain(std::unique_ptr<ExprAST> &expr);
llvm::FunctionCallee declarePrintf();
extern std::vector<std::unique_ptr<bcc::FunctionAST>> functions;
extern std::map<std::string, llvm::Function *> FunctionProtos;
uint32_t fnv1a(const std::string &str);

} // namespace bcc
