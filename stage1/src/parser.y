%start input
%define parse.error verbose

%left '+' '-'
%left '*' '/'

%union {
  double num;
  char* str;
  ExprAST* expr;
  FunctionAST* func;
  std::vector<FunctionAST*> *funcs;
  std::vector<ExprAST*> *args;
  std::vector<std::unique_ptr<ExprAST>> *stmts;
  std::vector<std::pair<std::string, std::string>>* param_list;
  ExternAST* extrn;
}

%token EXTRN
%token RETURN

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
%token <num> POSITIONAL_PARAM

%type <expr> expression
%type <args> expr_list

%type <expr> statement
%type <stmts> statements
%type <stmts> block
%type <func> function_def
%type <extrn> extrn_decl
%type <str> base_type
%type <str> type
%type <param_list> param_list
%type <param_list> param_list_nonempty

%{
  #include <cstdio>
  #include <cstdlib>
  #include <iostream>
  #include <cstring>
  #include "ast.h"
  #include "codegen.h"

  extern int yylex();
  void yyerror(const char *s);

  std::unique_ptr<ExprAST> root;

  void yyerror(const char *s) {
    std::cerr << "Parse error: " << s << std::endl;
  }

  static std::vector<std::string> extractTypes(const std::vector<std::pair<std::string, std::string>> &params) {
    std::vector<std::string> types;
    for (const auto &p : params) {
      types.push_back(p.first);
    }
    return types;
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
      auto typeList = extractTypes(*$5);
      $$ = new ExternAST($3, $2, typeList);
      $$->codegen(TheModule);
      free($2);
      free($3);
      delete $5;
    }
;

param_list:
    /* empty */ { $$ = new std::vector<std::pair<std::string, std::string>>(); }
  | param_list_nonempty { $$ = $1; }
;

param_list_nonempty:
      type IDENTIFIER {
        $$ = new std::vector<std::pair<std::string, std::string>>();
        $$->emplace_back($1, $2);
        free($1);
        free($2);
      }
    | type {
        $$ = new std::vector<std::pair<std::string, std::string>>();
        $$->emplace_back($1, "");
        free($1);
      }
    | param_list_nonempty ',' type IDENTIFIER {
        $1->emplace_back($3, $4);
        free($3);
        free($4);
        $$ = $1;
      }
    | param_list_nonempty ',' type {
        $1->emplace_back($3, "");
        free($3);
        $$ = $1;
      }
    | param_list_nonempty ',' ELLIPSIS {
        $1->emplace_back("...", "...");
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
    type IDENTIFIER '(' param_list ')' block {
      auto typeList = extractTypes(*$4);
      auto func = new FunctionAST(
        $2,
        $1,
        typeList,
        std::make_unique<BlockAST>(std::move(*$6))
      );
      $$ = func;
      $$->codegen(TheModule, Builder);
      delete $4;
    }
;

block:
    '{' statements '}' { $$ = $2; }
  ;

statements:
    { $$ = new std::vector<std::unique_ptr<ExprAST>>(); }
    | statements statement  {
      $1->push_back(std::unique_ptr<ExprAST>($2));
      $$ = $1;
    }
;

statement:
  IDENTIFIER '(' expr_list ')' ';' {
      std::vector<std::unique_ptr<ExprAST>> argsVec;
      for (auto *arg : *$3) argsVec.emplace_back(arg);
      delete $3;
      $$ = new CallExprAST($1, std::move(argsVec));
  }
  | IDENTIFIER '(' ')' ';' {
      $$ = new CallExprAST($1, {});
  }
  | RETURN expression ';' {
      $$ = new ReturnExprAST(std::unique_ptr<ExprAST>($2));
  }
  | type IDENTIFIER ';' {
      $$ = new VarDeclExprAST($2, $1);
      free($1);
      free($2);
  }
  | type IDENTIFIER '=' expression ';' {
      $$ = new VarDeclExprAST($2, $1, std::unique_ptr<ExprAST>($4));
      free($1);
      free($2);
  }
;

expr_list:
    expression                     { $$ = new std::vector<ExprAST*>(); $$->push_back($1); }
  | expr_list ',' expression       { $$->push_back($3); $$ = $1; }
  ;

expression:
    NUMBER                         { $$ = new NumberExprAST($1); }
  | STRING                         { $$ = new StringLiteralExprAST($1); }
  | IDENTIFIER '(' expr_list ')'   { 
      std::vector<std::unique_ptr<ExprAST>> argsVec;
      for (auto *arg : *$3) argsVec.emplace_back(arg);
      delete $3;
      $$ = new CallExprAST($1, std::move(argsVec));
    }
  | IDENTIFIER '(' ')'             {
      $$ = new CallExprAST($1, {});
    }
  | IDENTIFIER                     {
      $$ = new VariableExprAST($1);
    }
  | expression '+' expression      { $$ = new BinaryExprAST('+',
                                        std::unique_ptr<ExprAST>($1),
                                        std::unique_ptr<ExprAST>($3)); }
  | expression '*' expression      { $$ = new BinaryExprAST('*',
                                        std::unique_ptr<ExprAST>($1),
                                        std::unique_ptr<ExprAST>($3)); }
  | POSITIONAL_PARAM               { $$ = new PositionalParamExprAST((int)$1); }
;

%%
