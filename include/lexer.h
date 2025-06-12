#pragma once

// clang-format off
#include <cctype>
#include <string>
// clang-format on

namespace bcc {
enum class Token {
  Eof,
  Identifier,
  Number,
  Plus,
  Minus,
  Asterisk,
  Slash,
  Assign,
  Semicolon,
  LParen,
  RParen,
  Print,
  StringLiteral,
};

struct TokenInfo {
  Token type;
  std::string text;
};

TokenInfo getNextToken();
void initLexer(const std::string &source);
} // namespace bcc
