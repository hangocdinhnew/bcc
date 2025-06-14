#pragma once

// clang-format off
#include <string>
#include <vector>
#include <map>
#include "ast.h"
// clang-format on

class CodeGenerator {
public:
  std::string generate(const ProgramAST &program);

private:
  std::string m_output;
  std::map<std::string, std::string> m_stringLiterals;
  int m_stringCounter = 0;
  int m_labelCounter = 0;
  int m_stackOffset = 0;

  void generateExtern(const ExternAST &ext);
  void generateFunction(const FunctionAST &func);
  void generateStatement(ExprAST *stmt);
  std::string generateExpression(ExprAST *expr);
  std::string generateCall(const CallExprAST &call);
  void resetState();

  std::string m_currentFunctionName;
};
