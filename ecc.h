#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// file.c
char *read_file(char *);
extern char *filename;

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
    TK_SIZEOF,    // sizeof
    TK_STR,       // string
    TK_STATIC,    // static
} TokenKind;

struct Token {
    TokenKind kind;      // トークンの型
    struct Token *next;  // 次の入力トークン
    long val;            // kindがTK_NUMの場合、その数値
    char *loc;           // Tokenの開始位置
    char *str;           // トークンの文字列
    int len;             // トークンの長さ
};

struct VarScope {
    struct VarScope *next;

    char *name;

    struct Object *var;
    struct Type *type_def;
    struct Type *enum_ty;
    int enum_val;
};

struct TagScope {
    struct TagScope *next;
    struct Type *ty;
};

struct Scope {
    struct Scope *next;
    struct VarScope *vars;
    struct TagScope *tags;
};

struct NameTag {
    struct NameTag *next;
    char *name;
    struct Type *ty;
};

struct Object {
    struct Object *next;  // 次の変数かNULL
    char *name;           // 変数名
    int len;              // 変数名の長さ
    struct Type *ty;      // 変数の型

    // local variable
    int offset;  // RBPからのオフセット

    bool is_global_variable;

    char *init_data;

    bool is_function;
    bool is_function_definition;
    bool is_static;
    struct Node *body;
    struct Node *args;
    int stack_size;
};

extern struct Object *locals;
extern struct Object *globals;
extern struct Object *functions;

extern struct Token *token;
extern char *user_input;
void error(char *, ...);

long get_number(struct Token *);
char *get_string(struct Token *);
struct Token *skip(struct Token *, char *);
struct Token *skip_keyword(struct Token *, TokenKind);
bool equal(struct Token *, char *);
bool equal_keyword(struct Token *, TokenKind);
struct Token *tokenize(char *);

// parse.c
typedef enum {
    ND_ADD,        // +
    ND_SUB,        // -
    ND_MUL,        // *
    ND_DIV,        // /
    ND_ASSIGN,     // =
    ND_EQ,         // ==
    ND_NE,         // !=
    ND_LT,         // <
    ND_LE,         // <=
    ND_ADDR,       // &
    ND_DEREF,      // *
    ND_NUM,        // 整数
    ND_LVAR,       // ローカル変数
    ND_GVAR,       // グローバル変数
    ND_RETURN,     // return
    ND_IF,         // if
    ND_WHILE,      // while
    ND_FOR,        // for
    ND_DUMMY,      // ダミー
    ND_BLOCK,      // block
    ND_FUNCALL,    // function call
    ND_STMT_EXPR,  // statement expression
    ND_MEMBER,     // member of struct
    ND_COMMA,      // ,
} NodeKind;

struct Node {
    NodeKind kind;  // ノードの型
    bool is_control_statement;
    struct Node *lhs;  // 左辺
    struct Node *rhs;  // 右辺

    struct Node *next;  // next node
    struct Node *body;  // block

    struct Object *obj;

    struct Node *args;  // 関数の引数

    struct Member *member;  // 構造体のメンバー変数

    long val;         // kindがND_NUMの場合のみ使用
    struct Type *ty;  // 変数の型

    struct Node *cond;  // 条件文
    struct Node *then;  // trueのとき実行するもの
    struct Node *els;   // elseのとき実行するもの
    struct Node *init;  // 初期化文
    struct Node *inc;   // increment文
};

struct Object *program();

// codegen.c
void codegen();

struct Member {
    char *name;
    struct Type *ty;
    int offset;

    struct Member *next;
};

// type.c
typedef enum {
    TY_LONG,    // long
    TY_INT,     // int
    TY_SHORT,   // short
    TY_CHAR,    // char
    TY_VOID,    // void
    TY_PTR,     // pointer
    TY_ARRAY,   // 配列
    TY_FUNC,    // 関数
    TY_STRUCT,  // 構造体
    TY_UNION,   // 共用体
    TY_ENUM,    // 列挙型
} TypeKind;

struct Type {
    TypeKind ty;
    struct Type *ptr_to;
    size_t array_size;
    size_t size;

    char *name;

    // for function type
    struct Type *return_ty;
    struct NameTag *params;

    // for struct
    struct Member *member;
};

extern struct Type *ty_long;
extern struct Type *ty_int;
extern struct Type *ty_short;
extern struct Type *ty_char;
extern struct Type *ty_void;

struct Type *pointer_to(struct Type *);
struct Type *array_to(struct Type *, size_t);
struct Type *func_to(struct Type *, struct NameTag *);
struct Type *struct_type();
struct Type *union_type();
struct Type *enum_type();
bool is_integer(struct Type *);
bool is_pointer(struct Type *);
bool is_void(struct Type *);
bool is_same_type(struct Type *, struct Type *);
void add_type(struct Node *);