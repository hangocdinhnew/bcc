#pragma once

// clang-format off
#include <memory>
#include <vector>
#include <string>
// clang-format on

struct ExprAST {
    virtual ~ExprAST() = default;
};

struct NumberExprAST : ExprAST {
    double val;
    NumberExprAST(double val) : val(val) {}
};

struct PositionalParamExprAST : ExprAST {
    int index;
    PositionalParamExprAST(int index) : index(index) {}
};

struct BinaryExprAST : ExprAST {
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

struct StringLiteralExprAST : ExprAST {
    std::string val;
    StringLiteralExprAST(const std::string& val) : val(val) {}
};

struct BlockAST : ExprAST {
    std::vector<std::unique_ptr<ExprAST>> statements;
};

struct CallExprAST : ExprAST {
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    CallExprAST(const std::string& callee, std::vector<std::unique_ptr<ExprAST>> args)
        : callee(callee), args(std::move(args)) {}
};

struct ReturnExprAST : ExprAST {
    std::unique_ptr<ExprAST> expr;
    ReturnExprAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr)) {}
};

struct FunctionAST {
    std::string name;
    std::string retType;
    std::vector<std::string> argTypes;
    std::unique_ptr<BlockAST> body;
    
    FunctionAST(std::string name, std::string retType, 
                std::vector<std::string> argTypes, std::unique_ptr<BlockAST> body)
        : name(std::move(name)), retType(std::move(retType)), 
          argTypes(std::move(argTypes)), body(std::move(body)) {}
};

struct ExternAST {
    std::string name;
    std::string retType;
    std::vector<std::string> argTypes;
    
    ExternAST(std::string name, std::string retType, std::vector<std::string> argTypes)
        : name(std::move(name)), retType(std::move(retType)), 
          argTypes(std::move(argTypes)) {}
};

struct ProgramAST {
    std::vector<std::unique_ptr<ExternAST>> externs;
    std::vector<std::unique_ptr<FunctionAST>> functions;
};
