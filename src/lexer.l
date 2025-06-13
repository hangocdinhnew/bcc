%option c++
%option noyywrap

%{
  #include <cstdlib>
  #include <cstring>
  #include "parser.tab.h"
  #include "lexer_utils.h"
%}

%%
[ \t\n]+              ;
"extrn"               return EXTRN;
[0-9]+(\.[0-9]+)?     { yylval.num = atof(yytext); return NUMBER; }
\"([^\\\"]|\\.)*\" {
    char* raw = strdup(yytext + 1);
    raw[strlen(raw) - 1] = 0;
    yylval.str = bcc::unescape_string(raw);
    free(raw);
    return STRING;
}
"i32"                 return I32;
"void"                return VOID;
"ptr"                 return PTR;
"i64"                 return I64;
"f32"                 return F32;
"f64"                 return F64;
"..."                 return ELLIPSIS;
[a-zA-Z_][a-zA-Z0-9_]* { yylval.str = strdup(yytext); return IDENTIFIER; }
"+"                   return '+';
"-"                   return '-';
"*"                   return '*';
"/"                   return '/';
"("                   return '(';
")"                   return ')';
";"                   return ';';
.                     return yytext[0];
%%
