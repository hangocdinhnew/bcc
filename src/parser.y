%start input
%define parse.error verbose

%left '+' '-'
%left '*' '/'

%union {
  double num;
  char* str;
  bcc::ExprAST* expr;
  bcc::FunctionAST* func;
  std::vector<bcc::FunctionAST*> *funcs;
  std::vector<bcc::ExprAST*> *args;
}

%token <num> NUMBER
%token <str> STRING
%token PRINTF
%token <str> IDENTIFIER

%type <expr> expression
%type <args> expr_list

%type <expr> statement
%type <expr> block
%type <func> function_def

%{
  #include <cstdio>
  #include <cstdlib>
  #include <iostream>
  #include "ast.h"
  #include "codegen.h"

  extern int yylex();
  void yyerror(const char *s);

  std::unique_ptr<bcc::ExprAST> root;

  void yyerror(const char *s) {
    std::cerr << "Parse error: " << s << std::endl;
  }
%}

%code requires {
  #include <memory>
  #include "ast.h"
}

%%

// The main entry point
input:
    /* empty */
  | input statement
  | input function_def
  ;


function_def:
    IDENTIFIER '(' ')' block {
      auto func = new bcc::FunctionAST($1, std::unique_ptr<bcc::ExprAST>($4));
      bcc::functions.push_back(std::unique_ptr<bcc::FunctionAST>(func));
    }
;

block:
    '{' statement '}' { $$ = $2; }
  ;

statement:
    PRINTF '(' STRING ')' {
      std::vector<std::unique_ptr<bcc::ExprAST>> argsVec;
      $$ = new bcc::PrintExprAST(
          std::make_unique<bcc::StringLiteralExprAST>($3),
          std::move(argsVec));
    }
  | PRINTF '(' STRING ',' expr_list ')' {
      std::vector<std::unique_ptr<bcc::ExprAST>> argsVec;
      for (auto *arg : *$5) argsVec.emplace_back(arg);
      delete $5;
      $$ = new bcc::PrintExprAST(
          std::make_unique<bcc::StringLiteralExprAST>($3),
          std::move(argsVec));
    }
  | IDENTIFIER '(' expr_list ')' {
      std::vector<std::unique_ptr<bcc::ExprAST>> argsVec;
      for (auto *arg : *$3) argsVec.emplace_back(arg);
      delete $3;
      $$ = new bcc::CallExprAST($1, std::move(argsVec));
  }
  | IDENTIFIER '(' ')' {
      $$ = new bcc::CallExprAST($1, {});
  }
  ;

expr_list:
    expression                     { $$ = new std::vector<bcc::ExprAST*>(); $$->push_back($1); }
  | expr_list ',' expression       { $$->push_back($3); $$ = $1; }
  ;

expression:
    NUMBER                         { $$ = new bcc::NumberExprAST($1); }
  | STRING                         { $$ = new bcc::StringLiteralExprAST($1); }
  | expression '+' expression      { $$ = new bcc::BinaryExprAST('+',
                                        std::unique_ptr<bcc::ExprAST>($1),
                                        std::unique_ptr<bcc::ExprAST>($3)); }
  | expression '*' expression      { $$ = new bcc::BinaryExprAST('*',
                                        std::unique_ptr<bcc::ExprAST>($1),
                                        std::unique_ptr<bcc::ExprAST>($3)); }
  ;

%%
