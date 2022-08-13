#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// tokenize.c
typedef enum {
    TK_RESERVED,  // 記号
    TK_NUM,       // 整数トークン
    TK_EOF,       // 入力の終わりを表すトークン
} TokenKind;

struct Token {
    TokenKind kind;      // トークンの型
    struct Token *next;  // 次の入力トークン
    int val;             // kindがTK_NUMの場合、その数値
    char *str;           // トークンの文字列
    int len;             // トークンの長さ
};

extern struct Token *token;
extern char *user_input;
bool consume(char *);
void expect(char *);
int expect_number();
bool at_eof();
struct Token *tokenize(char *);

// parse.c
typedef enum {
    ND_ADD,  // +
    ND_SUB,  // -
    ND_MUL,  // *
    ND_DIV,  // /
    ND_EQ,   // ==
    ND_NE,   // !=
    ND_LT,   // <
    ND_LE,   // <=
    ND_NUM,  // 整数
} NodeKind;

struct Node {
    NodeKind kind;     // ノードの型
    struct Node *lhs;  // 左辺
    struct Node *rhs;  // 右辺
    int val;           // kindがND_NUMの場合のみ使用
};

struct Node *expr();

// codegen.c
void gen(struct Node *);