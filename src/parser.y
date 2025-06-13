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
  std::vector<std::unique_ptr<bcc::ExprAST>> *stmts;
  std::vector<std::string>* str_list;
  bcc::ExternAST* extrn;
}

%token EXTRN

%token <num> NUMBER
%token <str> STRING
%token <str> IDENTIFIER

%token I32
%token VOID
%token PTR
%token ELLIPSIS
%token I64
%token F32
%token F64

%type <expr> expression
%type <args> expr_list

%type <expr> statement
%type <stmts> statements
%type <stmts> block
%type <func> function_def
%type <extrn> extrn_decl
%type <str> base_type
%type <str> type
%type <str_list> param_list
%type <str_list> param_list_nonempty

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
  | input extrn_decl
  ;

extrn_decl:
    EXTRN type IDENTIFIER '(' param_list ')' ';' {
      $$ = new bcc::ExternAST($3, $2, *$5);
      $$->codegen();
      free($2);
      free($3);
      delete $5;
    }
;

param_list:
    /* empty */ { $$ = new std::vector<std::string>(); }
  | param_list_nonempty { $$ = $1; }
;

param_list_nonempty:
    type { $$ = new std::vector<std::string>({$1}); }
  | param_list_nonempty ',' type {
      $1->push_back($3);
      $$ = $1;
  }
  | param_list_nonempty ',' ELLIPSIS {
      $1->push_back("...");
      $$ = $1;
  }
;

type:
    base_type { $$ = $1; }
    | base_type '*' { 
        std::string tmp = std::string($1) + "*";
        $$ = strdup(tmp.c_str());
        free($1);
    }
;

base_type:
    I32 { $$ = strdup("i32"); }
  | I64 { $$ = strdup("i64"); }
  | F32 { $$ = strdup("f32"); }
  | F64 { $$ = strdup("f64"); }
  | VOID { $$ = strdup("void"); }
  | PTR { $$ = strdup("ptr"); }
;

function_def:
    IDENTIFIER '(' ')' block {
      auto func = new bcc::FunctionAST(
        $1,
        std::make_unique<bcc::BlockAST>(std::move(*$4))
      );
      delete $4;
      bcc::functions.push_back(std::unique_ptr<bcc::FunctionAST>(func));
    }
;

block:
    '{' statements '}' { $$ = $2; }
  ;

statements:
    { $$ = new std::vector<std::unique_ptr<bcc::ExprAST>>(); }
    | statements statement  {
      $1->push_back(std::unique_ptr<bcc::ExprAST>($2));
      $$ = $1;
    }
;

statement:
  IDENTIFIER '(' expr_list ')' ';' {
      std::vector<std::unique_ptr<bcc::ExprAST>> argsVec;
      for (auto *arg : *$3) argsVec.emplace_back(arg);
      delete $3;
      $$ = new bcc::CallExprAST($1, std::move(argsVec));
  }
  | IDENTIFIER '(' ')' ';' {
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
