#include "ecc.h"

// 現在着目しているToken
struct Token *token;

// 入力プログラム
char *user_input;

// エラー報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    // locが含まれている行の開始地点と終了地点を取得
    char *line = loc;
    while (user_input < line && line[-1] != '\n') line--;

    char *end = loc;
    while (*end != '\n') end++;

    // 見つかった行が全体の何行目なのかを調べる
    int line_num = 1;
    for (char *p = user_input; p < line; p++)
        if (*p == '\n') line_num++;

    // 見つかった行を、ファイル名と行番号と一緒に表示
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のTokenが期待している記号の時には、Tokenを1つ進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op) {
    if (token->kind != TK_RESERVED || strlen(op) != token->len ||
        memcmp(token->str, op, token->len))
        return false;
    token = token->next;
    return true;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect_op(char *op) {
    if (token->kind != TK_RESERVED || strlen(op) != token->len ||
        memcmp(token->str, op, token->len))
        error_at(token->str, "expected \"%s\"", op);
    token = token->next;
}

int expect_number() {
    if (token->kind != TK_NUM) error_at(token->str, "数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

char *expect_string() {
    if (token->kind != TK_STR) error_at(token->str, "文字列ではありません");
    char *p = strndup(token->str + 1, token->len - 2);
    token = token->next;
    return p;
}

void expect_keyword(TokenKind kind) {
    if (token->kind != kind) error_at(token->str, "キーワードではありません");
    token = token->next;
    return;
}

void expect_ident() {
    if (token->kind != TK_IDENT) error_at(token->str, "変数ではありません");
    token = token->next;
    return;
}

bool equal(struct Token *tok, char *name) {
    if (strlen(name) != tok->len || memcmp(name, tok->str, tok->len))
        return false;
    return true;
}

bool at_keyword(TokenKind kind) { return token->kind == kind; }

bool at_ident() { return token->kind == TK_IDENT; }

bool at_number() { return token->kind == TK_NUM; }

bool at_string() { return token->kind == TK_STR; }

bool at_eof() { return token->kind == TK_EOF; }

bool is_alnum(char c) { return isdigit(c) || isalpha(c) || c == '_'; }

struct Token *new_token(TokenKind kind, struct Token *cur, char *str, int len) {
    struct Token *tok = calloc(1, sizeof(struct Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool startswith(char *p, char *q) { return memcmp(p, q, strlen(q)) == 0; }

struct Token *tokenize(char *p) {
    struct Token head;
    head.next = NULL;
    struct Token *cur = &head;
    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }

        if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") ||
            startswith(p, ">=")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' ||
            *p == ')' || *p == '<' || *p == '>' || *p == ';' || *p == '=' ||
            *p == '{' || *p == '}' || *p == ',' || *p == '&' || *p == '[' ||
            *p == ']') {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        if (*p == '"') {
            int len = 0;
            char *q = p + 1;
            while (*q != '"') {
                q++;
            }
            cur = new_token(TK_STR, cur, p, q - p + 1);
            p += cur->len;
            continue;
        }

        if (startswith(p, "return") && !is_alnum(p[6])) {
            cur = new_token(TK_RETURN, cur, p, 6);
            p += 6;
            continue;
        }

        if (startswith(p, "if") && !is_alnum(p[2])) {
            cur = new_token(TK_IF, cur, p, 2);
            p += 2;
            continue;
        }

        if (startswith(p, "else") && !is_alnum(p[4])) {
            cur = new_token(TK_ELSE, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "while") && !is_alnum(p[5])) {
            cur = new_token(TK_WHILE, cur, p, 5);
            p += 5;
            continue;
        }

        if (startswith(p, "for") && !is_alnum(p[3])) {
            cur = new_token(TK_FOR, cur, p, 3);
            p += 3;
            continue;
        }

        if (startswith(p, "int") && !is_alnum(p[3])) {
            cur = new_token(TK_MOLD, cur, p, 3);
            p += 3;
            continue;
        }

        if (startswith(p, "char") && !is_alnum(p[4])) {
            cur = new_token(TK_MOLD, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "sizeof") && !is_alnum(p[6])) {
            cur = new_token(TK_SIZEOF, cur, p, 6);
            p += 6;
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        if (is_alnum(*p)) {
            int len = 0;
            while (is_alnum(*(p + len))) {
                len++;
            }
            cur = new_token(TK_IDENT, cur, p, len);
            p += len;
            continue;
        }

        error_at(token->str, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}