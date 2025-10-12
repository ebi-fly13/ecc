#include "ecc.h"

static bool is_begin;
static bool has_space;

static struct File *current_file;
static struct File **input_files;

// エラー報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void verror_at(char *filename,
               char *input,
               int line_number,
               char *loc,
               char *fmt,
               va_list ap) {
    char *line = loc;
    while (input < line && line[-1] != '\n') {
        line--;
    }
    char *end = loc;
    while (*end != '\n') {
        end++;
    }

    int indent = fprintf(stderr, "%s:%d:", filename, line_number);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);
    int pos = loc - line + indent;
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void error_at(char *loc, char *fmt, ...) {
    int line_number = 1;
    for (char *p = current_file->contents; p < loc; p++) {
        if (*p == '\n') {
            line_number++;
        }
    }
    va_list ap;
    va_start(ap, fmt);
    verror_at(current_file->path, current_file->contents, line_number, loc, fmt,
              ap);
    exit(1);
}

void error_token(struct Token *token, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(token->file->path, token->file->contents, token->line_number,
              token->loc, fmt, ap);
    exit(0);
}

void warning_token(struct Token *token, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(token->file->path, token->file->contents, token->line_number,
              token->loc, fmt, ap);
    va_end(ap);
}

bool startswith(char *p, char *q) { return memcmp(p, q, strlen(q)) == 0; }

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
struct Token *skip(struct Token *token, char *op) {
    if (strlen(op) != token->len || memcmp(token->loc, op, token->len))
        error_token(token, "expected \"%s\"", op);
    return token->next;
}

struct Token *skip_keyword(struct Token *token, TokenKind kind) {
    if (token->kind != kind) {
        error_token(token, "skip TokenKind error");
    }
    return token->next;
}

long get_number(struct Token *token) {
    if (token->kind != TK_NUM)
        error_token(token, "数ではありません");
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
    tok->is_begin = is_begin;
    tok->has_space = has_space;
    tok->kind = kind;
    tok->loc = str;
    tok->len = len;
    tok->file = current_file;
    cur->next = tok;
    is_begin = false;
    has_space = false;
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

    bool l = false;
    bool u = false;

    if (startswith(p, "LLU") || startswith(p, "LLu") || startswith(p, "llU") ||
        startswith(p, "llu") || startswith(p, "ULL") || startswith(p, "Ull") ||
        startswith(p, "uLL") || startswith(p, "ull")) {
        p += 3;
        l = u = true;
    } else if (!strncasecmp(p, "lu", 2) || !strncasecmp(p, "ul", 2)) {
        p += 2;
        l = u = true;
    } else if (startswith(p, "LL") || startswith(p, "ll")) {
        p += 2;
        l = true;
    } else if (*p == 'L' || *p == 'l') {
        p++;
        l = true;
    } else if (*p == 'U' || *p == 'u') {
        p++;
        u = true;
    }

    if (is_alnum(*p)) {
        error_at(p, "invalid digit");
    }

    struct Type *ty;
    if (base == 10) {
        if (l && u) {
            ty = ty_ulong;
        } else if (l) {
            ty = ty_long;
        } else if (u) {
            ty = (val >> 32) ? ty_ulong : ty_uint;
        } else {
            ty = (val >> 31) ? ty_long : ty_int;
        }
    } else {
        if (l && u) {
            ty = ty_ulong;
        } else if (l) {
            ty = (val >> 63) ? ty_ulong : ty_long;
        } else if (u) {
            ty = (val >> 32) ? ty_ulong : ty_uint;
        } else if (val >> 63) {
            ty = ty_ulong;
        } else if (val >> 32) {
            ty = ty_long;
        } else if (val >> 31) {
            ty = ty_uint;
        } else {
            ty = ty_int;
        }
    }

    struct Token *tok = new_token(TK_NUM, cur, str, p - str);
    tok->ty = ty;
    tok->val = val;
    return tok;
}

struct Token *read_char_literal(struct Token *cur, char *str, char *quote) {
    assert(*quote == '\'');
    char *p = quote + 1;
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

void add_line_number(struct Token *token) {
    char *p = current_file->contents;
    int n = 1;
    do {
        if (p == token->loc) {
            token->line_number = n;
            token = token->next;
        }
        if (*p == '\n') {
            n++;
        }
    } while (*p++);
}

static void remove_backslash_new_line(char *p) {
    int i = 0, j = 0;
    int n = 0;
    while (p[i] != '\0') {
        if (p[i] == '\\' && p[i + 1] == '\n') {
            i += 2;
            n++;
        } else if (p[i] == '\n') {
            p[j++] = p[i++];
            for (; n > 0; n--) {
                p[j++] = '\n';
            }
        } else {
            p[j++] = p[i++];
        }
    }
    for (; n > 0; n--) {
        p[j++] = '\n';
    }
    p[j] = '\0';
}

struct Token *tokenize(struct File *file) {
    current_file = file;
    is_begin = true;
    has_space = false;
    char *p = file->contents;
    remove_backslash_new_line(p);
    struct Token head;
    head.next = NULL;
    struct Token *cur = &head;
    while (*p) {
        if (*p == '\n') {
            is_begin = true;
            has_space = false;
            p++;
            continue;
        }

        if (isspace(*p)) {
            p++;
            has_space = true;
            continue;
        }

        if (strncmp(p, "//", 2) == 0) {
            p += 2;
            while (*p != '\n') {
                p++;
            }
            has_space = true;
            continue;
        }

        if (strncmp(p, "/*", 2) == 0) {
            char *q = strstr(p + 2, "*/");
            if (!q) {
                error_at(p, "コメントが閉じられていません");
            }
            p = q + 2;
            has_space = true;
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
            startswith(p, ">>") || startswith(p, "##")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' ||
            *p == ')' || *p == '<' || *p == '>' || *p == ';' || *p == '=' ||
            *p == '{' || *p == '}' || *p == ',' || *p == '&' || *p == '[' ||
            *p == ']' || *p == '.' || *p == '!' || *p == '~' || *p == '%' ||
            *p == '|' || *p == '^' || *p == ':' || *p == '?' || *p == ':' ||
            *p == '#') {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        if (*p == '"') {
            cur = read_string_literal(cur, p);
            p += cur->len;
            continue;
        }

        if (*p == '\'') {
            cur = read_char_literal(cur, p, p);
            p += cur->len;
            continue;
        }

        if (startswith(p, "L\'")) {
            cur = read_char_literal(cur, p, p + 1);
            p += cur->len;
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

        if (ispunct(*p)) {
            cur = new_token(TK_RESERVED, cur, p, 1);
            p += 1;
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    add_line_number(head.next);
    return head.next;
}

void convert_keywords(struct Token *token) {
    for (struct Token *cur = token; cur != NULL; cur = cur->next) {
        if (cur->kind != TK_IDENT)
            continue;
        if (equal(cur, "return")) {
            cur->kind = TK_RETURN;
        } else if (equal(cur, "if")) {
            cur->kind = TK_IF;
        } else if (equal(cur, "else")) {
            cur->kind = TK_ELSE;
        } else if (equal(cur, "while")) {
            cur->kind = TK_WHILE;
        } else if (equal(cur, "for")) {
            cur->kind = TK_FOR;
        } else if (equal(cur, "long") || equal(cur, "int") ||
                   equal(cur, "short") || equal(cur, "char") ||
                   equal(cur, "_Bool") || equal(cur, "void") ||
                   equal(cur, "signed") || equal(cur, "unsigned") ||
                   equal(cur, "struct") || equal(cur, "union") ||
                   equal(cur, "enum") || equal(cur, "const") ||
                   equal(cur, "restrict") || equal(cur, "__restrict") ||
                   equal(cur, "__restrict__") || equal(cur, "volatile") ||
                   equal(cur, "__Noreturn") || equal(cur, "register") ||
                   equal(cur, "auto") || equal(cur, "float") ||
                   equal(cur, "double")) {
            cur->kind = TK_MOLD;
        } else if (equal(cur, "sizeof")) {
            cur->kind = TK_SIZEOF;
        } else if (equal(cur, "typedef")) {
            cur->kind = TK_RESERVED;
        } else if (equal(cur, "static")) {
            cur->kind = TK_STATIC;
        } else if (equal(cur, "extern")) {
            cur->kind = TK_EXTERN;
        } else if (equal(cur, "goto")) {
            cur->kind = TK_GOTO;
        } else if (equal(cur, "break")) {
            cur->kind = TK_BREAK;
        } else if (equal(cur, "continue")) {
            cur->kind = TK_CONTINUE;
        } else if (equal(cur, "switch")) {
            cur->kind = TK_SWITCH;
        } else if (equal(cur, "case")) {
            cur->kind = TK_CASE;
        } else if (equal(cur, "default")) {
            cur->kind = TK_DEFAULT;
        } else if (equal(cur, "do")) {
            cur->kind = TK_DO;
        } else if (equal(cur, "_Alignof")) {
            cur->kind = TK_ALIGNOF;
        } else if (equal(cur, "_Alignas")) {
            cur->kind = TK_ALIGNAS;
        }
    }
}

struct Token *tokenize_file(char *path) {
    static int file_number = 0;
    char *p = read_file(path);
    if (p == NULL) {
        return NULL;
    }
    struct File *file = new_file(path, file_number + 1, p);
    input_files = realloc(input_files, sizeof(char *) * (file_number + 2));
    input_files[file_number] = file;
    input_files[file_number + 1] = NULL;
    file_number++;
    return tokenize(file);
}