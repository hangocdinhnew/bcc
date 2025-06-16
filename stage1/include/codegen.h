#pragma once

// clang-format off
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <map>
#include "ast.h"
// clang-format on

extern LLVMModuleRef TheModule;
extern LLVMBuilderRef Builder;
extern LLVMContextRef TheContext;

uint32_t fnv1a(const std::string &str);

LLVMTypeRef getLLVMTypeFor(const std::string &typeStr);
