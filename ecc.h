#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// file.c
char *read_file(char *);
extern char *filename;

// tokenize.c
typedef enum {
    TK_RESERVED, // 記号
    TK_IDENT,    // 識別子
    TK_NUM,      // 整数トークン
    TK_RETURN,   // return
    TK_IF,       // if
    TK_ELSE,     // else
    TK_WHILE,    // while
    TK_FOR,      // for
    TK_EOF,      // 入力の終わりを表すトークン
    TK_MOLD,     // 型識別子
    TK_SIZEOF,   // sizeof
    TK_STR,      // string
    TK_STATIC,   // static
    TK_EXTERN,   // extern
    TK_GOTO,     // goto
    TK_BREAK,    // break
    TK_CONTINUE, // continue
    TK_SWITCH,   // switch
    TK_CASE,     // case
    TK_DEFAULT,  // default
    TK_DO,       // do
    TK_ALIGNOF,  // _Alignof
    TK_ALIGNAS,  // _Alignas
} TokenKind;

struct Token {
    TokenKind kind;     // トークンの型
    struct Token *next; // 次の入力トークン
    long val;           // kindがTK_NUMの場合、その数値
    char *loc;          // Tokenの開始位置
    char *str;          // トークンの文字列
    int len;            // トークンの長さ
    struct Type *ty;    // 型
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

struct Relocation {
    struct Relocation *next;
    int offset;
    char *label;
    long addend;
};

struct Object {
    struct Object *next; // 次の変数かNULL
    char *name;          // 変数名
    int len;             // 変数名の長さ
    struct Type *ty;     // 変数の型

    // local variable
    int offset; // RBPからのオフセット

    bool is_global_variable;

    char *init_data;
    struct Relocation *rel;

    bool is_function;
    bool is_definition;
    bool is_static;
    struct Node *body;
    struct Node *args;
    int stack_size;

    int align;

    struct Object *locals;
    struct Object *va_area;
};

extern struct Object *locals;
extern struct Object *globals;
extern struct Object *functions;

extern struct Token *token;
extern char *user_input;
void error(char *, ...);
void error_at(char *, char *, ...);

long get_number(struct Token *);
struct Token *skip(struct Token *, char *);
struct Token *skip_keyword(struct Token *, TokenKind);
struct Token *skip_end(struct Token *);
bool equal(struct Token *, char *);
bool equal_keyword(struct Token *, TokenKind);
bool is_end(struct Token *);
bool is_end(struct Token *);
struct Token *tokenize(char *);

// parse.c
typedef enum {
    ND_ADD,         // +
    ND_SUB,         // -
    ND_MUL,         // *
    ND_DIV,         // /
    ND_ASSIGN,      // =
    ND_EQ,          // ==
    ND_NE,          // !=
    ND_LT,          // <
    ND_LE,          // <=
    ND_ADDR,        // &
    ND_DEREF,       // *
    ND_NOT,         // !
    ND_BITNOT,      // ~
    ND_MOD,         // %
    ND_BITOR,       // |
    ND_BITXOR,      // ^
    ND_BITAND,      // &
    ND_LOGOR,       // ||
    ND_LOGAND,      // &&
    ND_SHL,         // <<
    ND_SHR,         // >>
    ND_COND,        // 三項演算子
    ND_CAST,        // cast
    ND_NUM,         // 整数
    ND_VAR,         // 変数
    ND_GOTO,        // goto
    ND_LABEL,       // label
    ND_RETURN,      // return
    ND_IF,          // if
    ND_WHILE,       // while
    ND_FOR,         // for
    ND_BREAK,       // break
    ND_CONTINUE,    // continue
    ND_SWITCH,      // switch
    ND_CASE,        // case
    ND_DO,          // do
    ND_DUMMY,       // ダミー
    ND_BLOCK,       // block
    ND_FUNCALL,     // function call
    ND_STMT_EXPR,   // statement expression
    ND_MEMBER,      // member of struct
    ND_COMMA,       // ,
    ND_ASSIGN_EXPR, // 初期化付き変数定義
    ND_MEMZERO,     // ゼロ埋め
} NodeKind;

struct Node {
    NodeKind kind; // ノードの型
    bool is_control_statement;
    struct Node *lhs; // 左辺
    struct Node *rhs; // 右辺

    struct Node *next; // next node
    struct Node *body; // block

    struct Object *obj;

    struct Node *args; // 関数の引数

    struct Member *member; // 構造体のメンバー変数

    long val;        // kindがND_NUMの場合のみ使用
    struct Type *ty; // 変数の型

    char *label;
    char *unique_label;

    char *continue_label;
    char *break_label;
    char *switch_label;

    struct Node *cond;         // 条件文
    struct Node *then;         // trueのとき実行するもの
    struct Node *els;          // elseのとき実行するもの
    struct Node *init;         // 初期化文
    struct Node *inc;          // increment文
    struct Node *next_case;    // case
    struct Node *default_case; // default

    struct Node *goto_next; // goto
};

struct Node *new_node_cast(struct Node *, struct Type *);

struct Object *program();

// codegen.c
void codegen();

struct Member {
    char *name;
    struct Type *ty;
    int offset;
    int index;
    int align;

    struct Member *next;
};

// type.c
typedef enum {
    TY_LONG,   // long
    TY_INT,    // int
    TY_SHORT,  // short
    TY_CHAR,   // char
    TY_BOOL,   // bool
    TY_VOID,   // void
    TY_PTR,    // pointer
    TY_ARRAY,  // 配列
    TY_FUNC,   // 関数
    TY_STRUCT, // 構造体
    TY_UNION,  // 共用体
    TY_ENUM,   // 列挙型
} TypeKind;

struct Type {
    TypeKind ty;
    struct Type *ptr_to;
    int array_size;
    int size;
    int align;
    bool is_unsigned;
    char *name;

    bool is_flexible;

    // for function type
    struct Type *return_ty;
    struct NameTag *params;
    bool is_variadic;

    // for struct
    struct Member *member;
};

extern struct Type *ty_long;
extern struct Type *ty_int;
extern struct Type *ty_short;
extern struct Type *ty_char;
extern struct Type *ty_bool;
extern struct Type *ty_void;
extern struct Type *ty_ulong;
extern struct Type *ty_uint;
extern struct Type *ty_ushort;
extern struct Type *ty_uchar;

struct Type *pointer_to(struct Type *);
struct Type *array_to(struct Type *, int);
struct Type *func_to(struct Type *, struct NameTag *);
struct Type *struct_type();
struct Type *union_type();
struct Type *enum_type();
struct Type *copy_type(struct Type *);
bool is_integer(struct Type *);
bool is_pointer(struct Type *);
bool is_void(struct Type *);
bool is_same_type(struct Type *, struct Type *);
void add_type(struct Node *);
int align_to(int n, int align);