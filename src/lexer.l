%option c++
%option noyywrap

%{
  #include <cstdlib>
  #include <cstring>
  #include "parser.tab.h"
%}

%%
[ \t\n]+              ;
"printf"               return PRINTF;
[0-9]+(\.[0-9]+)?     { yylval.num = atof(yytext); return NUMBER; }
\"([^"]*)\"           { yylval.str = strdup(yytext + 1); yylval.str[strlen(yylval.str) - 1] = 0; return STRING; }
"+"                   return '+';
"-"                   return '-';
"*"                   return '*';
"/"                   return '/';
"("                   return '(';
")"                   return ')';
.                     return yytext[0];
%%
