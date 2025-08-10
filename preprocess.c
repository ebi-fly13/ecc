#include "ecc.h"

bool is_hash(struct Token *token) {
    return equal(token, "#") && token->is_begin;
}

struct Token *copy_token(struct Token *src) {
    struct Token *dst = calloc(1, sizeof(struct Token));
    *dst = *src;
    dst->next = NULL;
    return dst;
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

char *format(char *fmt, ...) {
    char *buf;
    size_t buflen;
    FILE *out = open_memstream(&buf, &buflen);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fclose(out);
    return buf;
}

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
                error("expected a filename");
            }
            char *path =
                format("%s/%s", dirname(strdup(token->file->path)), token->str);
            struct Token *include_token = tokenize_file(path);
            if (include_token == NULL) {
                error("cannot tokenize %s: %s\n", path, strerror(errno));
            }
            token = concat(include_token, token->next);
            continue;
        }

        error("invalid preprocessor directive");
    }
    cur->next = token;
    return head.next;
}