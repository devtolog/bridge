%{

/*
* Copyright (c) 2026 [tukinyanJP, devtolog]
* Apache LICENSE 2.0
*/

#include <llvm-c/Core.h>
#include "../include/bridge.h"

extern int yylex();
void yyerror(const char *s);

struct Node* create_node(node_type type, struct Node* left, struct Node* right);
struct Node* create_val_node(int64_t val);
struct Node* create_str_node(char* s);


extern struct Node* root; 

extern LLVMValueRef codegen(struct Node* node);
extern LLVMBuilderRef builder;
extern struct Node* root;

extern LLVMContextRef context; 
extern LLVMModuleRef module;
extern LLVMBuilderRef builder;

LLVMValueRef last_val = NULL;

struct Node* create_id_node(node_type type, char* name, struct Node* right);

%}

%union {
    int64_t num;
    char *s_val;
    struct Node* node; 
}
%token PUTS
%token <num> NUMBER 
%token <s_val> STRING IDENTIFIER
%type <node> expr program input end_of_stmt
%left '+' '-'
%left '*' '/'

%%

program:

    | program input

input:
    PUTS '(' expr ')' end_of_stmt { 
        struct Node* n = create_node(NODE_PRINT, $3, NULL);
        codegen(n);
        last_val = LLVMConstInt(LLVMInt64TypeInContext(context), 0, 0);
    }
    | IDENTIFIER '=' expr end_of_stmt { 
        codegen(create_id_node(NODE_ASSIGN, $1, $3)); 
    }
    | expr end_of_stmt { 
          last_val = codegen($1); 
      }
    | end_of_stmt


end_of_stmt:
    '\n' { $$ = NULL; }

    | ';' { $$ = NULL; }


expr:
    NUMBER          { $$ = create_val_node($1); }
    | IDENTIFIER { $$ = create_id_node(NODE_VAR, $1, NULL); }
    | STRING          { $$ = create_str_node($1); }
    | expr '+' expr { $$ = create_node(NODE_PLUS, $1, $3); }
    | expr '-' expr { $$ = create_node(NODE_MINUS, $1, $3); }
    | expr '*' expr { $$ = create_node(NODE_MUL, $1, $3); }

    | expr '/' expr { $$ = create_node(NODE_DIV, $1, $3); }
    | '(' expr ')'  { $$ = $2; }
    ;
%%