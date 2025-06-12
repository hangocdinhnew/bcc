%{
  #include <cstdio>
  #include <cstdlib>
  #include <iostream>
  #include "ast.h"

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

%define parse.error verbose

%union {
  double num;
  char* str;
  bcc::ExprAST* expr;
}

%token <num> NUMBER
%token <str> STRING
%token PRINT

%type <expr> expression

%%

input:
    PRINT '(' expression ')' { root = std::make_unique<bcc::PrintExprAST>(std::unique_ptr<bcc::ExprAST>($3)); }
  ;

expression:
    NUMBER           { $$ = new bcc::NumberExprAST($1); }
  | STRING           { $$ = new bcc::StringLiteralExprAST($1); }
  | expression '+' expression { $$ = new bcc::BinaryExprAST('+', std::unique_ptr<bcc::ExprAST>($1), std::unique_ptr<bcc::ExprAST>($3)); }
  | expression '*' expression { $$ = new bcc::BinaryExprAST('*', std::unique_ptr<bcc::ExprAST>($1), std::unique_ptr<bcc::ExprAST>($3)); }
  ;

%%
