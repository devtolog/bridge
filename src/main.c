#include <llvm-c/Core.h>

#include "../include/bridge.h"

/* --- 外部（parse.y / lex.yy.c）で定義されている変数・関数 --- */
extern int yyparse();
extern FILE *yyin;
extern LLVMValueRef last_val; // parse.y で更新される最新の LLVMValueRef

/* --- parse.y で extern 宣言されている実体 --- */
LLVMModuleRef module;
LLVMBuilderRef builder;
LLVMContextRef context;
struct Node* root = NULL;

/* --- ASTノード作成関数の実体 --- */
struct Node* create_val_node(int64_t val) {
    struct Node* node = (struct Node*)GC_MALLOC(sizeof(struct Node));
    node->type = NODE_VAL;
    node->node_val.type = TYPE_INT;
    node->node_val.as.i_val = val;
    node->left = NULL;
    node->right = NULL;
    return node;
}

struct Node* create_str_node(char* s) {
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    node->type = NODE_VAL;
    node->node_val.type = TYPE_STR;
    node->node_val.as.s_val = s; // lex 側で malloc/strdup したものを渡す
    node->left = NULL;
    node->right = NULL;
    return node;
}

struct Node* create_id_node(node_type type, char* name, struct Node* right) {
    struct Node* node = (struct Node*)GC_MALLOC(sizeof(struct Node));
    node->type = type;
    node->name = name;
    node->right = right;
    return node;
}


struct Node* create_node(node_type type, struct Node* left, struct Node* right) {
    struct Node* node = (struct Node*)GC_MALLOC(sizeof(struct Node));
    node->type = type;
    node->left = left;
    node->right = right;
    return node;
}

#include <gc.h>

struct Symbol {
    char* name;
    LLVMValueRef ptr;
};

struct Symbol* sym_table = NULL;
int sym_capacity = 0;
int sym_count = 0;

LLVMValueRef get_or_create_var(const char* name, LLVMTypeRef type) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) {
            return sym_table[i].ptr;
        }
    }

    if (sym_count >= sym_capacity) {
        int new_capacity = (sym_capacity == 0) ? 16 : sym_capacity * 2;
        struct Symbol* new_table = (struct Symbol*)GC_MALLOC(sizeof(struct Symbol) * new_capacity);
        if (sym_table) {
            memcpy(new_table, sym_table, sizeof(struct Symbol) * sym_count);
        }
        sym_table = new_table;
        sym_capacity = new_capacity;
    }

    LLVMValueRef alloca_ptr = LLVMBuildAlloca(builder, type, name);
    
    sym_table[sym_count].name = GC_STRDUP(name); 
    sym_table[sym_count].ptr = alloca_ptr;
    sym_count++;

    return alloca_ptr;
}


/* --- ASTを再帰的に辿って LLVM IR 命令を生成する --- */
LLVMValueRef get_concat_func() {
    LLVMValueRef f = LLVMGetNamedFunction(module, "bridge_concat");
    if (f) return f;
    LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
    LLVMTypeRef params[] = { i8ptr, i8ptr };
    LLVMTypeRef ft = LLVMFunctionType(i8ptr, params, 2, false);
    return LLVMAddFunction(module, "bridge_concat", ft);
}

LLVMValueRef get_repeat_func() {
    LLVMValueRef f = LLVMGetNamedFunction(module, "bridge_repeat");
    if (f) return f;
    LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
    LLVMTypeRef params[] = { i8ptr, LLVMInt64TypeInContext(context) };
    LLVMTypeRef ft = LLVMFunctionType(i8ptr, params, 2, false);
    return LLVMAddFunction(module, "bridge_repeat", ft);
}

LLVMValueRef find_var(const char* name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) {
            return sym_table[i].ptr;
        }
    }
    return NULL;
}


LLVMValueRef codegen(struct Node* node) {
    if (!node) return NULL;

    if (node->type == NODE_VAL) {
        if (node->node_val.type == TYPE_INT) {
            return LLVMConstInt(LLVMInt64TypeInContext(context), node->node_val.as.i_val, 0);
        }
        if (node->node_val.type == TYPE_STR) {
            return LLVMBuildGlobalStringPtr(builder, node->node_val.as.s_val, "strtmp");
        }
    }

    switch (node->type) {
        case NODE_PRINT: {
            LLVMValueRef val = codegen(node->left);
            if (!val) return NULL;
            LLVMTypeRef val_type = LLVMTypeOf(val);
            if (LLVMGetTypeKind(val_type) == LLVMPointerTypeKind) {
                LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
                LLVMTypeRef puts_type = LLVMFunctionType(LLVMInt32TypeInContext(context), &i8ptr, 1, false);
                LLVMValueRef puts_func = LLVMGetNamedFunction(module, "puts");
                if (!puts_func) puts_func = LLVMAddFunction(module, "puts", puts_type);
                return LLVMBuildCall2(builder, puts_type, puts_func, &val, 1, "putstmp");
            } else {
                LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
                LLVMTypeRef printf_type = LLVMFunctionType(LLVMInt32TypeInContext(context), &i8ptr, 1, true);
                LLVMValueRef printf_func = LLVMGetNamedFunction(module, "printf");
                if (!printf_func) printf_func = LLVMAddFunction(module, "printf", printf_type);
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(builder, "%lld\n", "fmt");
                LLVMValueRef args[] = { fmt, val };
                return LLVMBuildCall2(builder, printf_type, printf_func, args, 2, "printtmp");
            }
        }
        case NODE_VAR: {
            LLVMValueRef var_ptr = NULL;
            for (int i = 0; i < sym_count; i++) {
                if (strcmp(sym_table[i].name, node->name) == 0) {
                    var_ptr = sym_table[i].ptr;
                    break;
                }
            }
            if (!var_ptr) {
                fprintf(stderr, "Error: Undefined variable '%s'\n", node->name);
                return NULL;
            }
            LLVMTypeRef type = LLVMGetAllocatedType(var_ptr);
            return LLVMBuildLoad2(builder, type, var_ptr, "loadtmp");
        }
        case NODE_ASSIGN: {
            LLVMValueRef val = codegen(node->right);
            if (!val) return NULL;

            LLVMValueRef var_ptr = NULL;
            
            if (node->right->type == NODE_VAR && strcmp(node->name, node->right->name) == 0) {
                for (int i = 0; i < sym_count; i++) {
                    if (strcmp(sym_table[i].name, node->name) == 0) {
                        var_ptr = sym_table[i].ptr;
                        break;
                    }
                }
                LLVMValueRef var_ptr = find_var(node->name);
                if (var_ptr) {
                    return LLVMBuildLoad2(builder, LLVMGetAllocatedType(var_ptr), var_ptr, "loadtmp");
                }
                return NULL; 
            }

            for (int i = 0; i < sym_count; i++) {
                if (strcmp(sym_table[i].name, node->name) == 0) {
                    var_ptr = sym_table[i].ptr;
                    break;
                }
            }

            if (!var_ptr) {
                if (sym_count >= sym_capacity) {
                    int new_cap = (sym_capacity == 0) ? 16 : sym_capacity * 2;
                    struct Symbol* new_tab = (struct Symbol*)GC_MALLOC(sizeof(struct Symbol) * new_cap);
                    if (sym_table) memcpy(new_tab, sym_table, sizeof(struct Symbol) * sym_count);
                    sym_table = new_tab;
                    sym_capacity = new_cap;
                }

                LLVMTypeRef type = LLVMTypeOf(val);
                var_ptr = LLVMBuildAlloca(builder, type, node->name);

                sym_table[sym_count].name = GC_STRDUP(node->name);
                sym_table[sym_count].ptr = var_ptr;
                sym_count++;
            }

            LLVMBuildStore(builder, val, var_ptr);
            return val;
        }

        case NODE_PLUS: {
            LLVMValueRef L = codegen(node->left);
            LLVMValueRef R = codegen(node->right);
            if (!L || !R) return NULL;
            if (LLVMGetTypeKind(LLVMTypeOf(L)) == LLVMPointerTypeKind && 
                LLVMGetTypeKind(LLVMTypeOf(R)) == LLVMPointerTypeKind) {
                LLVMValueRef args[] = { L, R };
                LLVMValueRef func = get_concat_func();
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(func), func, args, 2, "strtmp");
            }
            return LLVMBuildAdd(builder, L, R, "addtmp");
        }
        case NODE_MINUS: {
            LLVMValueRef L = codegen(node->left);
            LLVMValueRef R = codegen(node->right);
            return LLVMBuildSub(builder, L, R, "subtmp");
        }
        case NODE_MUL: {
            LLVMValueRef L = codegen(node->left);
            LLVMValueRef R = codegen(node->right);
            if (!L || !R) return NULL;

            LLVMTypeKind L_kind = LLVMGetTypeKind(LLVMTypeOf(L));
            LLVMTypeKind R_kind = LLVMGetTypeKind(LLVMTypeOf(R));

            // 文字列 * 数値 の場合
            if (L_kind == LLVMPointerTypeKind && R_kind == LLVMIntegerTypeKind) {
                LLVMValueRef args[] = { L, R };
                LLVMValueRef func = get_repeat_func();
                return LLVMBuildCall2(builder, LLVMGlobalGetValueType(func), func, args, 2, "reptmp");
            }
    
            // 通常の数値 * 数値
            return LLVMBuildMul(builder, L, R, "multmp");
        }
        case NODE_DIV: {
            LLVMValueRef L = codegen(node->left);
            LLVMValueRef R = codegen(node->right);
            return LLVMBuildSDiv(builder, L, R, "divtmp");
        }
        default:
            return NULL;
    }
}



void yyerror(const char *s) {
    fprintf(stderr, "Parse Error: %s\n", s);
}

/* --- メイン処理 --- */
int main(int argc, char **argv) {
    GC_INIT();
    char *out_filename = "out.ll";

    // 1. 入力ファイルの処理と出力ファイル名の決定
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            perror("Error opening file");
            return 1;
        }
        yyin = fp;

        // パスを維持したまま拡張子を置換
        char *src_path = GC_STRDUP(argv[1]);
        char *last_dot = strrchr(src_path, '.');
        char *last_slash = strrchr(src_path, '/');
        char *last_bslash = strrchr(src_path, '\\');

        // ドットがパス区切り文字より後ろにある場合のみ拡張子とみなしてカット
        if (last_dot && last_dot > last_slash && last_dot > last_bslash) {
            *last_dot = '\0';
        }

        out_filename = (char *)GC_MALLOC(strlen(src_path) + 4);
        sprintf(out_filename, "%s.ll", src_path);
    } else {
        printf("Reading from stdin... (Ctrl+D to finish)\n");
    }

    // 2. LLVMコンテキスト・モジュール・ビルダーの初期化
    context = LLVMContextCreate();
    module = LLVMModuleCreateWithNameInContext("calc_module", context);
    builder = LLVMCreateBuilderInContext(context);

    // 3. main 関数の作成 (i64 main())
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(context);
    LLVMTypeRef main_func_type = LLVMFunctionType(i64_type, NULL, 0, 0);
    LLVMValueRef main_func = LLVMAddFunction(module, "main", main_func_type);
    
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, main_func, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    // 4. パース実行
    if (yyparse() == 0) {
        // 5. 最後に計算された値を return する (型安全な処理)
        if (last_val) {
            // 文字列(ptr)が最後に来た場合は 0 を返して型不一致を防ぐ
            if (LLVMGetTypeKind(LLVMTypeOf(last_val)) == LLVMPointerTypeKind) {
                LLVMBuildRet(builder, LLVMConstInt(i64_type, 0, 0));
            } else {
                LLVMBuildRet(builder, last_val);
            }
        } else {
            LLVMBuildRet(builder, LLVMConstInt(i64_type, 0, 0));
        }

        // 6. IRをファイルに出力
        char *error = NULL;
        if (LLVMPrintModuleToFile(module, out_filename, &error)) {
            fprintf(stderr, "Could not write to file: %s\n", error);
            LLVMDisposeMessage(error);
        } else {
            printf("Successfully generated %s\n", out_filename);
        }
    } else {
        fprintf(stderr, "Parse failed.\n");
    }

    // 7. 後片付け
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
    if (argc > 1) fclose(yyin);

    return 0;
}