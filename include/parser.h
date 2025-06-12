#pragma once

// clang-format off
#include <memory>
#include "lexer.h"
#include "ast.h"
// clang-format on

namespace bcc {
extern TokenInfo CurTok;

TokenInfo getNextTokenWrapped();
std::unique_ptr<ExprAST> parseExpression();
} // namespace bcc
