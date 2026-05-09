/*
* Copyright (c) 2026 [tukinyanJP, devtolog]
* Apache LICENSE 2.0
*/

#ifndef bridge_h
#define bridge_h

#include "common.h"

typedef enum val_type {
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_BOOL, // 最初なのでこれぐらいあれば十分だと思う
} val_type;

typedef enum node_type {
    NODE_VAL, NODE_PRINT,
    NODE_PLUS, NODE_MINUS, NODE_MUL, NODE_DIV, NODE_NOTMUCH
} node_type;

typedef struct val {
    val_type type;
    union {
        int64_t i_val; // int64_t を使うとよい。どのコンパイラでもマックス値は保証される。
        double d_val;
        bool b_val;
    } as;

    // union はメモリ削減に使うものです。最近は使われませんが、これを使うことによって
    // キャストの回数をうまく減らすことが可能です。
} val;

struct Node {
    node_type type;
    val node_val;
    struct Node *left; 
    struct Node *right;

    // 再帰する。構文木を作成する際に必要になる。
    /*
        例えば、 1 + 1の時は、

        NODE_PLUS (1 + 1 の +)
            left -> NODE_VAL (1 + 1 の左辺の 1。NODE_VALは参照)
            right -> NODE_VAL (1 + 1 の右辺の 1。)

        このような構造になる
    */

    // struct Node *cond; 
    // struct Node *body;
    // struct Node *else;


    //将来 if など実装するときは left right じゃ足りないので cond body else を追加。

    // Q. elseif みたいになったらどうする?

    // A. else に if を再帰的に入れるとelse if ができます。

    /*
        疑似コード:

        if cond1 {
            stmt1
        } elsif cond2 {
            stmt2
        }

        AST:

        node_type -> if (if cond1 のタイプ)
            body -> stmt1...
            else -> node_type if ...
    */

};

#endif