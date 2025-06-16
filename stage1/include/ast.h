#pragma once

// clang-format off
#include <memory>
#include <string>
#include <vector>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
// clang-format on

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual LLVMValueRef codegen(LLVMModuleRef module,
                               LLVMBuilderRef builder) = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};

class StringLiteralExprAST : public ExprAST {
  std::string Val;

public:
  StringLiteralExprAST(const std::string &val) : Val(val) {}
  ~StringLiteralExprAST() = default;
  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};

class BlockAST : public ExprAST {
  std::vector<std::unique_ptr<ExprAST>> Statements;

public:
  BlockAST(std::vector<std::unique_ptr<ExprAST>> stmts)
      : Statements(std::move(stmts)) {}

  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override {
    for (auto &stmt : Statements)
      stmt->codegen(module, builder);
    return nullptr;
  }
};

class FunctionAST {
  std::string Name;
  std::string RetType;
  std::vector<std::string> ArgTypes;
  std::unique_ptr<BlockAST> Body;

public:
  FunctionAST(std::string Name, std::string RetType,
              std::vector<std::string> ArgTypes, std::unique_ptr<BlockAST> Body)
      : Name(std::move(Name)), RetType(std::move(RetType)),
        ArgTypes(std::move(ArgTypes)), Body(std::move(Body)) {}

  std::string getName() { return Name; }

  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder);
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args = {})
      : Callee(Callee), Args(std::move(Args)) {}
  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};

class ExternAST {
  std::string Name;
  std::string RetType;
  std::vector<std::string> ArgTypes;

public:
  ExternAST(const std::string &Name, const std::string &RetType,
            const std::vector<std::string> &Args)
      : Name(Name), RetType(RetType), ArgTypes(Args) {}
  LLVMValueRef codegen(LLVMModuleRef module);
};

class ReturnExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Expr;

public:
  ReturnExprAST(std::unique_ptr<ExprAST> expr) : Expr(std::move(expr)) {}
  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};

class PositionalParamExprAST : public ExprAST {
  int Index;

public:
  PositionalParamExprAST(int idx) : Index(idx) {}
  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};

class VarDeclExprAST : public ExprAST {
  std::string Name;
  std::string Type;
  std::unique_ptr<ExprAST> InitExpr;

public:
  VarDeclExprAST(std::string name, std::string type,
                 std::unique_ptr<ExprAST> init = nullptr)
      : Name(std::move(name)), Type(std::move(type)),
        InitExpr(std::move(init)) {}

  LLVMValueRef codegen(LLVMModuleRef module, LLVMBuilderRef builder) override;
};
