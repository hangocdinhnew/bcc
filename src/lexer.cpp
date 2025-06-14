#include "lexer.h"
#include <cctype>

Token Lexer::nextToken() {
  while (pos < source.size()) {
    if (isspace(source[pos])) {
      pos++;
      continue;
    }

    if (isalpha(source[pos]) || source[pos] == '_') {
      size_t start = pos;
      while (pos < source.size() &&
             (isalnum(source[pos]) || source[pos] == '_')) {
        pos++;
      }
      return {IDENTIFIER, source.substr(start, pos - start)};
    }

    if (isdigit(source[pos])) {
      size_t start = pos;
      while (pos < source.size() &&
             (isdigit(source[pos]) || source[pos] == '.')) {
        pos++;
      }
      return {NUMBER, source.substr(start, pos - start)};
    }

    if (source[pos] == '"') {
      pos++;
      size_t start = pos;
      while (pos < source.size() && source[pos] != '"') {
        pos++;
      }
      std::string str = source.substr(start, pos - start);
      if (pos < source.size())
        pos++;
      return {STRING, str};
    }

    switch (source[pos]) {
    case '%':
      pos++;
      return {PERCENT, "%"};
    case '+':
      pos++;
      return {PLUS, "+"};
    case '-':
      pos++;
      return {MINUS, "-"};
    case '*':
      pos++;
      return {STAR, "*"};
    case '/':
      pos++;
      return {SLASH, "/"};
    case '(':
      pos++;
      return {LPAREN, "("};
    case ')':
      pos++;
      return {RPAREN, ")"};
    case '{':
      pos++;
      return {LBRACE, "{"};
    case '}':
      pos++;
      return {RBRACE, "}"};
    case ';':
      pos++;
      return {SEMI, ";"};
    case ',':
      pos++;
      return {COMMA, ","};
    default:
      pos++;
      return {UNKNOWN, std::string(1, source[pos - 1])};
    }
  }
  return {END, ""};
}
