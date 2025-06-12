%start input
%define parse.error verbose

%left '+' '-'
%left '*' '/'

%union {
  double num;
  char* str;
  bcc::ExprAST* expr;
  std::vector<bcc::ExprAST*> *args;
}

%token <num> NUMBER
%token <str> STRING
%token PRINTF

%type <expr> expression
%type <args> expr_list

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

%%

// The main entry point
input:
    /* empty */
  | input statement
  ;

statement:
    PRINTF '(' STRING ')' {
      std::vector<std::unique_ptr<bcc::ExprAST>> argsVec;
      root = std::make_unique<bcc::PrintExprAST>(
          std::make_unique<bcc::StringLiteralExprAST>($3),
          std::move(argsVec));
    }
  | PRINTF '(' STRING ',' expr_list ')' {
      std::vector<std::unique_ptr<bcc::ExprAST>> argsVec;
      for (auto *arg : *$5) argsVec.emplace_back(arg);
      delete $5;

      root = std::make_unique<bcc::PrintExprAST>(
          std::make_unique<bcc::StringLiteralExprAST>($3),
          std::move(argsVec));
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
