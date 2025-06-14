#pragma once

// clang-format off
#include <string>
// clang-format on

enum TokenType {
  END,
  IDENTIFIER,
  NUMBER,
  STRING,
  PERCENT,
  PLUS,
  MINUS,
  STAR,
  SLASH,
  LPAREN,
  RPAREN,
  LBRACE,
  RBRACE,
  SEMI,
  COMMA,
  UNKNOWN
};

struct Token {
  TokenType type;
  std::string value;
};

class Lexer {
public:
  void init(const std::string &src) {
    source = src;
    pos = 0;
  }

  Token nextToken();

private:
  std::string source;
  size_t pos = 0;
};
