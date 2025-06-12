// clang-format off

/*!re2c
re2c:define:YYCTYPE = char;
re2c:yyfill:enable = 0;
*/

#include "lexer.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace bcc {

static const char *cursor;
static const char *limit;
static std::string currentText;

void initLexer(const std::string &source) {
  cursor = source.c_str();
  limit = cursor + source.size();
}

TokenInfo getNextToken() {
  const char *YYCURSOR = cursor;
  const char *YYMARKER;
  const char *YYLIMIT = limit;

  /*!re2c
  [ \t\n]+                           { cursor = YYCURSOR; return getNextToken(); }
  "printf"                           { cursor = YYCURSOR; return {Token::Print, "printf"}; }
  "\"" ( [^"\\] | "\\" . )* "\""     {
                                       currentText = std::string(cursor + 1, YYCURSOR - cursor - 2);
                                       cursor = YYCURSOR;
                                       return {Token::StringLiteral, currentText};
                                     }
  [0-9]+ ("." [0-9]+)?               {
                                       currentText = std::string(cursor, YYCURSOR - cursor);
                                       cursor = YYCURSOR;
                                       return {Token::Number, currentText};
                                     }
  "+"                                { cursor = YYCURSOR; return {Token::Plus, "+"}; }
  "-"                                { cursor = YYCURSOR; return {Token::Minus, "-"}; }
  "*"                                { cursor = YYCURSOR; return {Token::Asterisk, "*"}; }
  "/"                                { cursor = YYCURSOR; return {Token::Slash, "/"}; }
  "("                                { cursor = YYCURSOR; return {Token::LParen, "("}; }
  ")"                                { cursor = YYCURSOR; return {Token::RParen, ")"}; }
  ";"                                { cursor = YYCURSOR; return {Token::Semicolon, ";"}; }
  *                                  { cursor = YYCURSOR; return {Token::Eof, ""}; }
  */
}

} // namespace bcc
