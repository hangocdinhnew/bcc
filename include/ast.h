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
  std::unique_ptr<ExprAST> Expr;

public:
  PrintExprAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {}
  llvm::Value *codegen() override;
};

class StringLiteralExprAST : public ExprAST {
  std::string Val;

public:
  StringLiteralExprAST(const std::string &val);
  ~StringLiteralExprAST() = default;
  llvm::Value *codegen() override;
};
} // namespace bcc
