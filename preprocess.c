#include "ecc.h"

bool is_hash(struct Token *token) {
    return equal(token, "#") && token->is_begin;
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
        error("invalid preprocessor directive");
    }
    cur->next = token;
    return head.next;
}