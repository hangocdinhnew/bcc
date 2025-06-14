// clang-format off
#include "parser.h"
#include <cctype>
#include <stdexcept>
// clang-format on

void Parser::skipWhitespace() {
  while (pos < source.size() && isspace(source[pos]))
    pos++;
}

bool Parser::match(const std::string &str) {
  if (source.substr(pos, str.size()) == str) {
    pos += str.size();
    skipWhitespace();
    return true;
  }
  return false;
}

std::string Parser::parseIdentifier() {
  size_t start = pos;
  while (pos < source.size() && (isalnum(source[pos]) || source[pos] == '_'))
    pos++;
  skipWhitespace();
  return source.substr(start, pos - start);
}

std::string Parser::parseType() {
  if (match("i32") || match("ptr") || match("void")) {
    return source.substr(pos - 3, 3);
  }
  return "";
}

std::unique_ptr<ExprAST> Parser::parsePrimary() {
  if (match("%")) {
    size_t start = pos;
    while (isdigit(source[pos]))
      pos++;
    if (start == pos) {
      throw std::runtime_error("Expected digit after '%'");
    }
    int index = std::stoi(source.substr(start, pos - start));
    skipWhitespace();
    return std::make_unique<PositionalParamExprAST>(index);
  } else if (source[pos] == '"') {
    pos++;
    size_t start = pos;
    while (pos < source.size() && source[pos] != '"')
      pos++;
    std::string str = source.substr(start, pos - start);
    if (pos < source.size() && source[pos] == '"')
      pos++;
    skipWhitespace();
    return std::make_unique<StringLiteralExprAST>(str);
  } else if (isalpha(source[pos]) || source[pos] == '_') {
    std::string id = parseIdentifier();
    if (match("(")) {
      std::vector<std::unique_ptr<ExprAST>> args;
      if (!match(")")) {
        do {
          args.push_back(parseExpression());
        } while (match(","));
        if (!match(")")) {
          throw std::runtime_error("Expected ')' after arguments");
        }
      }
      return std::make_unique<CallExprAST>(id, std::move(args));
    }
    return std::make_unique<VariableExprAST>(id);
  } else if (match("(")) {
    auto expr = parseExpression();
    if (!match(")")) {
      throw std::runtime_error("Expected ')' after expression");
    }
    return expr;
  } else if (isdigit(source[pos])) {
    size_t start = pos;
    while (pos < source.size() && (isdigit(source[pos]) || source[pos] == '.'))
      pos++;
    double val = std::stod(source.substr(start, pos - start));
    skipWhitespace();
    return std::make_unique<NumberExprAST>(val);
  }
  throw std::runtime_error("Unexpected token in primary expression");
}

int getPrecedence(char op) {
  switch (op) {
  case '*':
  case '/':
    return 40;
  case '+':
  case '-':
    return 30;
  default:
    return -1;
  }
}

std::unique_ptr<ExprAST> Parser::parseBinOpRHS(int minPrecedence,
                                               std::unique_ptr<ExprAST> lhs) {
  while (true) {
    if (pos >= source.size())
      break;

    char op = source[pos];
    int precedence = getPrecedence(op);

    if (precedence < minPrecedence)
      break;

    // Consume the operator
    pos++;
    skipWhitespace();

    auto rhs = parsePrimary();

    // Check next operator's precedence
    if (pos < source.size()) {
      char nextOp = source[pos];
      int nextPrecedence = getPrecedence(nextOp);

      if (precedence < nextPrecedence) {
        rhs = parseBinOpRHS(precedence + 1, std::move(rhs));
      }
    }

    lhs = std::make_unique<BinaryExprAST>(op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

std::unique_ptr<ExprAST> Parser::parseExpression() {
  auto lhs = parsePrimary();
  return parseBinOpRHS(0, std::move(lhs));
}

std::unique_ptr<FunctionAST> Parser::parseFunction() {
  std::string retType = parseType();
  std::string name = parseIdentifier();

  match("(");
  std::vector<std::string> argTypes;

  if (!match(")")) {
    do {
      argTypes.push_back(parseType());
    } while (match(",") && !match(")"));

    if (!match(")")) {
      throw std::runtime_error("Expected ')' after argument list");
    }
  }

  match("{");
  auto body = std::make_unique<BlockAST>();
  while (!match("}")) {
    if (match("return")) {
      auto expr = parseExpression();
      if (!match(";")) {
        throw std::runtime_error("Expected ';' after return statement");
      }
      body->statements.push_back(
          std::make_unique<ReturnExprAST>(std::move(expr)));
    } else {
      auto expr = parseExpression();
      if (!match(";")) {
        throw std::runtime_error("Expected ';' after expression");
      }
      body->statements.push_back(std::move(expr));
    }
  }

  return std::make_unique<FunctionAST>(name, retType, argTypes,
                                       std::move(body));
}

bool Parser::parse(const std::string &source) {
  this->source = source;
  pos = 0;
  program = ProgramAST();

  while (pos < source.size()) {
    skipWhitespace();
    if (match("extrn")) {
      program.externs.push_back(parseExtern());
    } else {
      program.functions.push_back(parseFunction());
    }
  }
  return true;
}

std::unique_ptr<ExternAST> Parser::parseExtern() {
  std::string retType = parseType();
  std::string name = parseIdentifier();

  match("(");
  std::vector<std::string> argTypes;
  bool lastWasComma = false;

  while (!match(")")) {
    lastWasComma = false;

    if (lastWasComma) {
      throw std::runtime_error("Expected type after comma");
    }

    if (match("...")) {
      argTypes.push_back("...");
    } else {
      argTypes.push_back(parseType());
    }

    if (match(")")) {
      break;
    }

    if (!match(",")) {
      throw std::runtime_error("Expected ',' between arguments");
    }

    lastWasComma = true;
  }

  match(";");
  return std::make_unique<ExternAST>(name, retType, argTypes);
}
