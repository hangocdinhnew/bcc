// clang-format off
#include "codegen.h"
#include <sstream>
#include <iomanip>
// clang-format on

std::string escape_string(const std::string &str) {
  std::ostringstream escaped;
  for (char c : str) {
    switch (c) {
    case '\n':
      escaped << "\\n";
      break;
    case '\t':
      escaped << "\\t";
      break;
    case '\"':
      escaped << "\\\"";
      break;
    case '\\':
      escaped << "\\\\";
      break;
    default:
      escaped << c;
    }
  }
  return escaped.str();
}

std::string CodeGenerator::generate(const ProgramAST &program) {
  resetState();
  m_output.clear();

  // Text section with functions
  m_output += ".intel_syntax noprefix\n";
  m_output += ".section .text\n";
  m_output += ".globl main\n\n";

  for (const auto &ext : program.externs) {
    generateExtern(*ext);
  }

  for (const auto &func : program.functions) {
    generateFunction(*func);
  }

  // Generate data section for strings
  if (!m_stringLiterals.empty()) {
    m_output += ".section .rodata\n";
    for (const auto &[str, label] : m_stringLiterals) {
      m_output += label + ": .string \"" + escape_string(str) + "\"\n";
    }
  }

  return m_output;
}

void CodeGenerator::generateExtern(const ExternAST &ext) {
  m_output += ".extern " + ext.name + "\n";
}

void CodeGenerator::generateFunction(const FunctionAST &func) {
  resetState();
  m_currentFunctionName = func.name;
  m_output += func.name + ":\n";

  // Function prologue
  m_output += "    push rbp\n";
  m_output += "    mov rbp, rsp\n";

  // Allocate stack space for parameters
  if (func.argTypes.size() > 0) {
    m_stackOffset =
        8 * (func.argTypes.size() - 6 > 0 ? func.argTypes.size() - 6 : 0);
    if (m_stackOffset > 0) {
      m_output += "    sub rsp, " + std::to_string(m_stackOffset) + "\n";
    }
  }

  // Generate function body
  generateStatement(func.body.get());

  // Function epilogue
  m_output += ".Lret_" + func.name + ":\n";
  m_output += "    mov rsp, rbp\n";
  m_output += "    pop rbp\n";
  m_output += "    ret\n\n";
}

void CodeGenerator::generateStatement(ExprAST *stmt) {
  if (auto block = dynamic_cast<BlockAST *>(stmt)) {
    for (auto &s : block->statements) {
      generateStatement(s.get());
    }
  } else if (auto ret = dynamic_cast<ReturnExprAST *>(stmt)) {
    if (ret->expr) {
      m_output += "    mov rax, " + generateExpression(ret->expr.get()) + "\n";
    }
    m_output += "    jmp .Lret_" + m_currentFunctionName + "\n";
  } else {
    generateExpression(stmt);
  }
}

std::string CodeGenerator::generateExpression(ExprAST *expr) {
  if (auto num = dynamic_cast<NumberExprAST *>(expr)) {
    return std::to_string(static_cast<int>(num->val));
  } else if (auto param = dynamic_cast<PositionalParamExprAST *>(expr)) {
    if (param->index <= 6) {
      const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
      return regs[param->index - 1];
    } else {
      return "QWORD [rbp + " + std::to_string(16 + 8 * (param->index - 7)) +
             "]";
    }
  } else if (auto bin = dynamic_cast<BinaryExprAST *>(expr)) {
    std::string left = generateExpression(bin->lhs.get());
    std::string right = generateExpression(bin->rhs.get());

    m_output += "    mov rax, " + left + "\n";
    m_output += "    mov rbx, " + right + "\n";

    switch (bin->op) {
    case '+':
      m_output += "    add rax, rbx\n";
      break;
    case '-':
      m_output += "    sub rax, rbx\n";
      break;
    case '*':
      m_output += "    imul rax, rbx\n";
      break;
    case '/':
      m_output += "    cqo\n    idiv rbx\n";
      break;
    }
    return "rax";
  } else if (auto call = dynamic_cast<CallExprAST *>(expr)) {
    return generateCall(*call);
  } else if (auto str = dynamic_cast<StringLiteralExprAST *>(expr)) {
    if (m_stringLiterals.count(str->val) == 0) {
      std::string label = "str_" + std::to_string(m_stringCounter++);
      m_stringLiterals[str->val] = label;
    }
    return m_stringLiterals[str->val];
  }
  return "";
}

std::string CodeGenerator::generateCall(const CallExprAST &call) {
  const char *argRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

  // Precompute arguments so we can assign registers in order
  std::vector<std::string> args;
  for (auto &argExpr : call.args) {
    args.push_back(generateExpression(argExpr.get()));
  }

  for (int i = 0; i < args.size(); i++) {
    const std::string &arg = args[i];
    if (i < 6) {
      if (arg.find("str_") == 0) {
        m_output +=
            "    lea " + std::string(argRegs[i]) + ", [rip + " + arg + "]\n";
      } else {
        m_output += "    mov " + std::string(argRegs[i]) + ", " + arg + "\n";
      }
    } else {
      m_output += "    mov rax, " + arg + "\n";
      m_output +=
          "    mov QWORD [rsp + " + std::to_string(8 * (i - 6)) + "], rax\n";
    }
  }

  m_output += "    call " + call.callee + "\n";
  return "rax";
}

void CodeGenerator::resetState() {
  m_stackOffset = 0;
  m_labelCounter = 0;
}
