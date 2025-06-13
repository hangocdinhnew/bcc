#pragma once

// clang-format off
#include <memory>
#include <string>
#include <vector>
#include <llvm/IR/Value.h>
// clang-format on

namespace bcc {
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  llvm::Value *codegen() override;
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
  llvm::Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  llvm::Value *codegen() override;
};

class PrintExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Format;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  PrintExprAST(std::unique_ptr<ExprAST> Format,
               std::vector<std::unique_ptr<ExprAST>> Args)
      : Format(std::move(Format)), Args(std::move(Args)) {}

  llvm::Value *codegen() override;
};

class StringLiteralExprAST : public ExprAST {
  std::string Val;

public:
  StringLiteralExprAST(const std::string &val);
  ~StringLiteralExprAST() = default;
  llvm::Value *codegen() override;
};

class BlockAST : public ExprAST {
  std::vector<std::unique_ptr<ExprAST>> Statements;

public:
  BlockAST(std::vector<std::unique_ptr<ExprAST>> stmts)
      : Statements(std::move(stmts)) {}

  llvm::Value *codegen() override {
    for (auto &stmt : Statements)
      stmt->codegen();
    return nullptr;
  }
};

class FunctionAST {
  std::string Name;
  std::unique_ptr<BlockAST> Body;

public:
  FunctionAST(std::string Name, std::unique_ptr<BlockAST> Body)
      : Name(std::move(Name)), Body(std::move(Body)) {}
  llvm::Function *codegen();
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args = {})
      : Callee(Callee), Args(std::move(Args)) {}
  llvm::Value *codegen() override;
};
} // namespace bcc
