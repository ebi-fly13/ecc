#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// type.c
struct Type {
    enum { INT, PTR } ty;
    struct Type *ptr_to;
};

// tokenize.c
typedef enum {
    TK_RESERVED,  // 記号
    TK_IDENT,     // 識別子
    TK_NUM,       // 整数トークン
    TK_RETURN,    // return
    TK_IF,        // if
    TK_ELSE,      // else
    TK_WHILE,     // while
    TK_FOR,       // for
    TK_EOF,       // 入力の終わりを表すトークン
    TK_MOLD,      // 型識別子
} TokenKind;

struct Token {
    TokenKind kind;      // トークンの型
    struct Token *next;  // 次の入力トークン
    int val;             // kindがTK_NUMの場合、その数値
    char *str;           // トークンの文字列
    int len;             // トークンの長さ
};

struct LVar {
    struct LVar *next;  // 次の変数かNULL
    char *name;         // 変数名
    int len;            // 変数名の長さ
    int offset;         // RBPからのオフセット
    struct Type *ty;    // 変数の型
};

extern struct LVar *locals;

extern struct Token *token;
extern char *user_input;
void error(char *, ...);
bool consume(char *);
bool consume_type(char *);
void expect_op(char *);
int expect_number();
void expect_keyword();
void expect_ident();
bool equal_op(struct Token *, char *);
bool at_keyword();
bool at_ident();
bool at_number();
bool at_eof();
struct Token *tokenize(char *);

// parse.c
typedef enum {
    ND_ADD,      // +
    ND_SUB,      // -
    ND_MUL,      // *
    ND_DIV,      // /
    ND_ASSIGN,   // =
    ND_EQ,       // ==
    ND_NE,       // !=
    ND_LT,       // <
    ND_LE,       // <=
    ND_ADDR,     // &
    ND_DEREF,    // *
    ND_NUM,      // 整数
    ND_LVAR,     // ローカル変数
    ND_RETURN,   // return
    ND_IF,       // if
    ND_WHILE,    // while
    ND_FOR,      // for
    ND_DUMMY,    // ダミー
    ND_BLOCK,    // block
    ND_FUNCALL,  // function call
} NodeKind;

struct Node {
    NodeKind kind;     // ノードの型
    struct Node *lhs;  // 左辺
    struct Node *rhs;  // 右辺

    struct Node *next;  // next node
    struct Node *body;  // block

    char *funcname;     // 関数名
    struct Node *args;  // 関数の引数

    int val;          // kindがND_NUMの場合のみ使用
    int offset;       // kindがND_LVARの場合のみ使う
    struct Type *ty;  // 変数の型

    struct Node *cond;  // 条件文
    struct Node *then;  // trueのとき実行するもの
    struct Node *els;   // elseのとき実行するもの
    struct Node *init;  // 初期化文
    struct Node *inc;   // increment文
};

struct Function {
    struct Function *next;

    char *name;
    struct Node *body;
    struct LVar *local_variables;
    struct Node *args;
    struct Type *ty;
    int stack_size;
};

struct Function *program();

// codegen.c
void codegen(struct Function *);