#pragma once

// clang-format off
#include <vector>
#include <memory>
#include <string>
#include "ast.h"
// clang-format on

class Parser {
public:
  bool parse(const std::string &source);
  const ProgramAST &getProgram() const { return program; }

private:
  ProgramAST program;
  size_t pos = 0;
  std::string source;

  void skipWhitespace();
  bool match(const std::string &str);
  std::string parseIdentifier();
  std::string parseType();
  std::unique_ptr<ExprAST> parseExpression();
  std::unique_ptr<ExprAST> parsePrimary();
  std::unique_ptr<ExprAST> parseBinOpRHS(int minPrecedence, std::unique_ptr<ExprAST> lhs);

  std::unique_ptr<FunctionAST> parseFunction();
  std::unique_ptr<ExternAST> parseExtern();
};
