#include "ecc.h"

// 入力プログラム
char *user_input;

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
struct Token *skip(struct Token *token, char *op) {
    if (strlen(op) != token->len || memcmp(token->loc, op, token->len))
        error_at(token->loc, "expected \"%s\"", op);
    return token->next;
}

struct Token *skip_keyword(struct Token *token, TokenKind kind) {
    if (token->kind != kind) {
        error_at(token->loc, "skip TokenKind error");
    }
    return token->next;
}

long get_number(struct Token *token) {
    if (token->kind != TK_NUM)
        error_at(token->loc, "数ではありません");
    return token->val;
}

bool equal(struct Token *tok, char *name) {
    if (strlen(name) != tok->len || memcmp(name, tok->loc, tok->len))
        return false;
    return true;
}

bool equal_keyword(struct Token *tok, TokenKind kind) {
    return tok->kind == kind;
}

bool is_digit(char c) { return '0' <= c && c <= '9'; }

bool is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

bool is_alnum(char c) { return is_digit(c) || is_alpha(c) || c == '_'; }

bool is_ident_head(char c) { return is_alpha(c) || c == '_'; }

bool is_end(struct Token *token) {
    return equal(token, "}") || equal(token, ",") && equal(token->next, "}");
}

struct Token *skip_end(struct Token *token) {
    assert(is_end(token));
    if (equal(token, "}")) {
        return skip(token, "}");
    }
    token = skip(token, ",");
    return skip(token, "}");
}

int from_hex(char c) {
    assert(isxdigit(c));
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    }
    return c - 'A' + 10;
}

int read_escaped_char(char **new_pos, char *p) {
    if ('0' <= *p && *p <= '7') {
        int c = *p - '0';
        p++;
        for (int i = 0; i < 2; i++) {
            if ('0' <= *p && *p <= '7') {
                c = (c << 3) + *p - '0';
                p++;
            }
        }
        *new_pos = p;
        return c;
    }

    if (*p == 'x') {
        p++;
        int c = 0;
        for (; isxdigit(*p); p++) {
            c = (c << 4) + from_hex(*p);
        }
        *new_pos = p;
        return c;
    }

    *new_pos = p + 1;
    switch (*p) {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'r':
        return '\r';
    case 'e':
        return '\e';
    default:
        return *p;
    }
}

struct Token *new_token(TokenKind kind, struct Token *cur, char *str, int len) {
    struct Token *tok = calloc(1, sizeof(struct Token));
    tok->kind = kind;
    tok->loc = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

struct Token *read_int_literal(struct Token *cur, char *str) {
    char *p = str;
    long base = 10;
    if (!strncasecmp(p, "0x", 2) && is_alnum(p[2])) {
        p += 2;
        base = 16;
    } else if (!strncasecmp(p, "0b", 2) && is_alnum(p[2])) {
        p += 2;
        base = 2;
    } else if (*p == '0') {
        base = 8;
    }
    unsigned long val = strtoul(p, &p, base);

    if (is_alnum(*p)) {
        error_at(p, "invalid digit");
    }

    struct Token *tok = new_token(TK_NUM, cur, str, p - str);
    tok->val = val;
    return tok;
}

struct Token *read_char_literal(struct Token *cur, char *str) {
    assert(*str == '\'');
    char *p = str + 1;
    if (*p == '\0') {
        error_at(str, "unclosed char literal");
    }
    char c;
    if (*p == '\\') {
        c = read_escaped_char(&p, p + 1);
    } else {
        c = *p;
        p++;
    }
    if (*p != '\'') {
        error_at(p, "unclosed char literal");
    }
    p++;
    struct Token *tok = new_token(TK_NUM, cur, str, p - str);
    tok->val = c;
    return tok;
}

char *string_literal_end(char *p) {
    char *str = p;
    for (; *p != '"'; p++) {
        if (*p == '\n' || *p == '\0') {
            error_at(str, "unclosed string literal");
        }
        if (*p == '\\') {
            p++;
        }
    }
    assert(*p == '"');
    return p;
}

struct Token *read_string_literal(struct Token *cur, char *str) {
    assert(*str == '"');
    char *end = string_literal_end(str + 1);
    char *buf = calloc(1, end - str + 1);
    int len = 0;
    for (char *p = str + 1; p < end;) {
        if (*p == '\\') {
            buf[len++] = read_escaped_char(&p, p + 1);
        } else {
            buf[len++] = *p;
            p++;
        }
    }
    struct Token *tok = new_token(TK_STR, cur, str, end - str + 1);
    tok->str = buf;
    tok->ty = array_to(ty_char, len + 1);
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

        if (strncmp(p, "//", 2) == 0) {
            p += 2;
            while (*p != '\n') {
                p++;
            }
            continue;
        }

        if (strncmp(p, "/*", 2) == 0) {
            char *q = strstr(p + 2, "*/");
            if (!q) {
                error_at(p, "コメントが閉じられていません");
            }
            p = q + 2;
            continue;
        }

        if (startswith(p, "<<=") || startswith(p, ">>=")) {
            cur = new_token(TK_RESERVED, cur, p, 3);
            p += 3;
            continue;
        }

        if (startswith(p, "...")) {
            cur = new_token(TK_RESERVED, cur, p, 3);
            p += 3;
            continue;
        }

        if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") ||
            startswith(p, ">=") || startswith(p, "->") || startswith(p, "+=") ||
            startswith(p, "-=") || startswith(p, "*=") || startswith(p, "/=") ||
            startswith(p, "++") || startswith(p, "--") || startswith(p, "%=") ||
            startswith(p, "|=") || startswith(p, "^=") || startswith(p, "&=") ||
            startswith(p, "||") || startswith(p, "&&") || startswith(p, "<<") ||
            startswith(p, ">>")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' ||
            *p == ')' || *p == '<' || *p == '>' || *p == ';' || *p == '=' ||
            *p == '{' || *p == '}' || *p == ',' || *p == '&' || *p == '[' ||
            *p == ']' || *p == '.' || *p == '!' || *p == '~' || *p == '%' ||
            *p == '|' || *p == '^' || *p == ':' || *p == '?' || *p == ':') {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        if (*p == '"') {
            cur = read_string_literal(cur, p);
            p += cur->len;
            continue;
        }

        if (*p == '\'') {
            cur = read_char_literal(cur, p);
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

        if (startswith(p, "long") && !is_alnum(p[4])) {
            cur = new_token(TK_MOLD, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "int") && !is_alnum(p[3])) {
            cur = new_token(TK_MOLD, cur, p, 3);
            p += 3;
            continue;
        }

        if (startswith(p, "short") && !is_alnum(p[5])) {
            cur = new_token(TK_MOLD, cur, p, 5);
            p += 5;
            continue;
        }

        if (startswith(p, "char") && !is_alnum(p[4])) {
            cur = new_token(TK_MOLD, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "_Bool") && !is_alnum(p[5])) {
            cur = new_token(TK_MOLD, cur, p, 5);
            p += 5;
            continue;
        }

        if (startswith(p, "void") && !is_alnum(p[4])) {
            cur = new_token(TK_MOLD, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "signed") && !is_alnum(p[6])) {
            cur = new_token(TK_MOLD, cur, p, 6);
            p += 6;
            continue;
        }

        if (startswith(p, "unsigned") && !is_alnum(p[8])) {
            cur = new_token(TK_MOLD, cur, p, 8);
            p += 8;
            continue;
        }

        if (startswith(p, "sizeof") && !is_alnum(p[6])) {
            cur = new_token(TK_SIZEOF, cur, p, 6);
            p += 6;
            continue;
        }

        if (startswith(p, "struct") && !is_alnum(p[6])) {
            cur = new_token(TK_MOLD, cur, p, 6);
            p += 6;
            continue;
        }

        if (startswith(p, "union") && !is_alnum(p[5])) {
            cur = new_token(TK_MOLD, cur, p, 5);
            p += 5;
            continue;
        }

        if (startswith(p, "enum") && !is_alnum(p[4])) {
            cur = new_token(TK_MOLD, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "typedef") && !is_alnum(p[7])) {
            cur = new_token(TK_RESERVED, cur, p, 7);
            p += 7;
            continue;
        }

        if (startswith(p, "static") && !is_alnum(p[6])) {
            cur = new_token(TK_STATIC, cur, p, 6);
            p += 6;
            continue;
        }

        if (startswith(p, "extern") && !is_alnum(p[6])) {
            cur = new_token(TK_EXTERN, cur, p, 6);
            p += 6;
            continue;
        }

        if (startswith(p, "goto") && !is_alnum(p[4])) {
            cur = new_token(TK_GOTO, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "break") && !is_alnum(p[5])) {
            cur = new_token(TK_BREAK, cur, p, 5);
            p += 5;
            continue;
        }

        if (startswith(p, "continue") && !is_alnum(p[8])) {
            cur = new_token(TK_CONTINUE, cur, p, 8);
            p += 8;
            continue;
        }

        if (startswith(p, "switch") && !is_alnum(p[6])) {
            cur = new_token(TK_SWITCH, cur, p, 6);
            p += 6;
            continue;
        }

        if (startswith(p, "case") && !is_alnum(p[4])) {
            cur = new_token(TK_CASE, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "default") && !is_alnum(p[7])) {
            cur = new_token(TK_DEFAULT, cur, p, 7);
            p += 7;
            continue;
        }

        if (startswith(p, "do") && !is_alnum(p[2])) {
            cur = new_token(TK_DO, cur, p, 2);
            p += 2;
            continue;
        }

        if (startswith(p, "_Alignof") && !is_alnum(p[8])) {
            cur = new_token(TK_ALIGNOF, cur, p, 8);
            p += 8;
            continue;
        }

        if (startswith(p, "_Alignas") && !is_alnum(p[8])) {
            cur = new_token(TK_ALIGNAS, cur, p, 8);
            p += 8;
            continue;
        }

        if (isdigit(*p)) {
            cur = read_int_literal(cur, p);
            p += cur->len;
            continue;
        }

        if (is_ident_head(*p)) {
            int len = 0;
            while (is_alnum(*(p + len))) {
                len++;
            }
            cur = new_token(TK_IDENT, cur, p, len);
            p += len;
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}