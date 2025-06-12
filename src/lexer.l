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
"printf"               return PRINTF;
[0-9]+(\.[0-9]+)?     { yylval.num = atof(yytext); return NUMBER; }
\"([^\\\"]|\\.)*\" {
    char* raw = strdup(yytext + 1);
    raw[strlen(raw) - 1] = 0;
    yylval.str = bcc::unescape_string(raw);
    free(raw);
    return STRING;
}
"+"                   return '+';
"-"                   return '-';
"*"                   return '*';
"/"                   return '/';
"("                   return '(';
")"                   return ')';
.                     return yytext[0];
%%
