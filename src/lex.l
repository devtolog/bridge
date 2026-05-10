%{
/*
* Copyright (c) 2026 [tukinyanJP, devtolog]
* Apache LICENSE 2.0
*/
#include "parse.tab.h"
#include <llvm-c/Core.h>
#include <stdlib.h>
#include "../include/bridge.h"
%}

%%
"puts"      { return PUTS; }
\"[^\"]*\" {
    int len = strlen(yytext) - 2;
    char *res = (char *)GC_MALLOC(len + 1);
    strncpy(res, yytext + 1, len);
    res[len] = '\0';
    yylval.s_val = res;
    return STRING;
}
[a-zA-Z_][a-zA-Z0-9_]* {
    size_t len = strlen(yytext);
    char *s = (char *)GC_MALLOC(len + 1);
    strcpy(s, yytext);
    yylval.s_val = s;
    return IDENTIFIER;
}
"="         { return '='; }
[0-9]+      { yylval.num = atoi(yytext); return NUMBER; }
[ \t]     ;
"\n"        { return '\n'; }
[+\-*/()]   { return yytext[0]; }
";"         { return ';'; }
\/\/.*        ;
.           { printf("Unknown: %s\n", yytext); }
%%

int yywrap() { return 1; }