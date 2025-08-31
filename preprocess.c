#include "ecc.h"

typedef enum Cond { If, Elif, Else } Cond;

struct CondIncl {
    struct CondIncl *outer;
    struct Token *token;
    Cond cond;
    bool in;
} *cond;

struct Macro {
    struct Macro *next;
    char *name;
    struct Token *body;
} *macros;

static bool is_hash(struct Token *token) {
    return equal(token, "#") && token->is_begin;
}

static bool is_hash_and_keyword(struct Token *token, char *keyword) {
    return is_hash(token) && equal(token->next, keyword);
}

static struct Token *new_eof_token() {
    struct Token *token = calloc(1, sizeof(struct Token));
    token->kind = TK_EOF;
    return token;
}

static struct Token *copy_token(struct Token *src) {
    struct Token *dst = calloc(1, sizeof(struct Token));
    *dst = *src;
    dst->next = NULL;
    return dst;
}

static struct Token *copy_line(struct Token **rest, struct Token *src) {
    struct Token head = {};
    struct Token *cur = &head;
    for (; !src->is_begin; src = src->next) {
        cur = cur->next = copy_token(src);
    }
    cur->next = new_eof_token();
    *rest = src;
    return head.next;
}

static long eval_const_expr(struct Token **rest, struct Token *token) {
    struct Token *line = copy_line(rest, token);
    return const_expr(&line, line);
}

static struct Token *skip_line(struct Token *token, bool warning_option) {
    if (token->is_begin) {
        return token;
    }
    if (warning_option) {
        warning_token(token, "extra token");
    }
    while (!token->is_begin) {
        token = token->next;
    }
    return token;
}

static struct Token *skip_cond_incl(struct Token *token) {
    while (token->kind != TK_EOF) {
        if (is_hash_and_keyword(token, "if")) {
            token = skip_line(token->next, false);
            token = skip_cond_incl(token);
            while (is_hash_and_keyword(token, "elif")) {
                token = skip_line(token->next, false);
                token = skip_cond_incl(token);
            }
            if (is_hash_and_keyword(token, "else")) {
                token = skip_line(token->next, false);
                token = skip_cond_incl(token);
            }
            if (!is_hash_and_keyword(token, "endif")) {
                error_token(token, "expected endif");
            }
            token = skip_line(token->next->next, false);
            continue;
        }
        if (is_hash_and_keyword(token, "elif") ||
            is_hash_and_keyword(token, "else") ||
            is_hash_and_keyword(token, "endif")) {
            break;
        }
        token = token->next;
    }
    return token;
}

static struct Token *concat(struct Token *first, struct Token *second) {
    struct Token head = {};
    struct Token *cur = &head;
    while (first != NULL && first->kind != TK_EOF) {
        cur = cur->next = copy_token(first);
        first = first->next;
    }
    cur->next = second;
    return head.next;
}

static void push_cond_incl(struct Token *token, int val) {
    struct CondIncl *next = calloc(1, sizeof(struct CondIncl));
    next->outer = cond;
    next->token = token;
    next->cond = If;
    next->in = val != 0;
    cond = next;
}

static void pop_cond_incl() {
    assert(cond != NULL);
    cond = cond->outer;
}

static void add_macro(struct Token **rest, struct Token *token) {
    // assert(token->kind == TK_IDENT);
    struct Macro *macro = calloc(1, sizeof(struct Macro));
    macro->name = strndup(token->loc, token->len);
    macro->body = copy_line(rest, token->next);
    macro->next = macros;
    macros = macro;
}

static struct Macro *find_macro(struct Token *token) {
    // if (token->kind != TK_IDENT) {
    //     return NULL;
    // }
    for (struct Macro *cur = macros; cur != NULL; cur = cur->next) {
        if (strlen(cur->name) == token->len &&
            !strncmp(cur->name, token->loc, token->len)) {
            return cur;
        }
    }
    return NULL;
}

static bool expand_macro(struct Token **rest, struct Token *token) {
    // if (token->kind != TK_IDENT) return false;
    struct Macro *m = find_macro(token);
    if (m == NULL) {
        return false;
    }
    *rest = concat(m->body, token->next);
    return true;
}

struct Token *preprocess(struct Token *token) {
    struct Token head = {};
    struct Token *cur = &head;
    while (token->kind != TK_EOF) {
        struct Token *start = token;

        if (expand_macro(&token, token)) {
            continue;
        }

        if (!is_hash(token)) {
            cur = cur->next = token;
            token = token->next;
            continue;
        }

        token = token->next;

        if (token->is_begin) {
            continue;
        }

        if (equal(token, "include")) {
            token = token->next;
            if (token->kind != TK_STR) {
                error_token(token, "expected a filename");
            }
            char *path =
                format("%s/%s", dirname(strdup(token->file->path)), token->str);
            struct Token *include_token = tokenize_file(path);
            if (include_token == NULL) {
                error_token(start, "%s\n", strerror(errno));
            }
            token = skip_line(token->next, true);
            token = concat(include_token, token);
            continue;
        }

        if (equal(token, "if")) {
            long val = eval_const_expr(&token, token->next);
            push_cond_incl(start, val);
            if (!cond->in) {
                token = token->next = skip_cond_incl(token);
            }
            continue;
        }

        if (equal(token, "elif")) {
            if (cond == NULL || cond->cond == Else) {
                error_token(token, "stray #elif");
            }
            cond->cond = Elif;
            if (cond->in) {
                token = token->next = skip_cond_incl(token->next);
            } else {
                long val = eval_const_expr(&token, token->next);
                if (val != 0) {
                    cond->in = true;
                } else {
                    token = token->next = skip_cond_incl(token);
                }
            }
            continue;
        }

        if (equal(token, "else")) {
            if (cond == NULL || cond->cond == Else) {
                error_token(token, "stray #else");
            }
            token = skip_line(token->next, true);
            if (cond->in) {
                token = skip_cond_incl(token);
            }
            cond->cond = Else;
            cond->in = true;
            continue;
        }

        if (equal(token, "endif")) {
            pop_cond_incl();
            token = skip_line(token->next, true);
            continue;
        }

        if (equal(token, "define")) {
            token = token->next;
            // if (token->kind != TK_IDENT) {
            //     error_token(token, "expected ident");
            // }
            add_macro(&token, token);
            continue;
        }

        error_token(token, "invalid preprocessor directive");
    }
    cur->next = token;
    return head.next;
}