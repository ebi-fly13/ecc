#include "ecc.h"

struct CondIncl {
    struct CondIncl *outer;
    struct Token *token;
    bool cond;
};

struct CondIncl *cond;

bool is_hash(struct Token *token) {
    return equal(token, "#") && token->is_begin;
}

bool is_hash_and_keyword(struct Token *token, char *keyword) {
    return is_hash(token) && token->is_begin && equal(token->next, keyword);
}

struct Token *new_eof_token() {
    struct Token *token = calloc(1, sizeof(struct Token));
    token->kind = TK_EOF;
    return token;
}

struct Token *copy_token(struct Token *src) {
    struct Token *dst = calloc(1, sizeof(struct Token));
    *dst = *src;
    dst->next = NULL;
    return dst;
}

struct Token *copy_line(struct Token **rest, struct Token *src) {
    struct Token head = {};
    struct Token *cur = &head;
    for (; !src->is_begin; src = src->next) {
        cur = cur->next = copy_token(src);
    }
    cur->next = new_eof_token();
    *rest = src;
    return head.next;
}

long eval_const_expr(struct Token **rest, struct Token *token) {
    struct Token *line = copy_line(rest, token);
    return const_expr(&line, line);
}

struct Token *skip_line(struct Token *token) {
    if (token->is_begin) {
        return token;
    }
    warning_token(token, "extra token");
    while (!token->is_begin) {
        token = token->next;
    }
    return token;
}

struct Token *skip_cond_incl(struct Token *token) {
    while (token->kind != TK_EOF) {
        if (is_hash_and_keyword(token, "if")) {
            eval_const_expr(&token, token->next->next);
            token = skip_cond_incl(token);
            assert(is_hash_and_keyword(token, "endif"));
            token = skip_line(token->next->next);
            continue;
        }
        if (is_hash_and_keyword(token, "endif")) {
            break;
        }
        token = token->next;
    }
    return token;
}

struct Token *concat(struct Token *first, struct Token *second) {
    struct Token head = {};
    struct Token *cur = &head;
    while (first != NULL && first->kind != TK_EOF) {
        cur = cur->next = copy_token(first);
        first = first->next;
    }
    cur->next = second;
    return head.next;
}

void push_cond_incl(struct Token **rest, struct Token *token) {
    struct CondIncl *next = calloc(1, sizeof(struct CondIncl));
    next->outer = cond;
    next->token = token;
    next->cond = eval_const_expr(rest, token);
    cond = next;
}

void pop_cond_incl() { cond = cond->outer; }

struct Token *preprocess(struct Token *token) {
    struct Token head = {};
    struct Token *cur = &head;
    while (token->kind != TK_EOF) {
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
                error_token(token, "%s\n", strerror(errno));
            }
            token = skip_line(token->next);
            token = concat(include_token, token);
            continue;
        }

        if (equal(token, "if")) {
            push_cond_incl(&token, token->next);
            if (!cond->cond) {
                token = token->next = skip_cond_incl(token);
            }
            continue;
        }

        if (equal(token, "endif")) {
            pop_cond_incl();
            token = skip_line(token->next);
            continue;
        }

        error_token(token, "invalid preprocessor directive");
    }
    cur->next = token;
    return head.next;
}