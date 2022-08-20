#include "ecc.h"

struct Type *ty_int = &(struct Type){TY_INT};

bool is_integer(struct Type *ty) {
    return ty->ty == TY_INT;
}

bool is_pointer(struct Type *ty) {
    return ty->ty == TY_PTR;
}

struct Type *pointer_to(struct Type *ty) {
    struct Type *pointer = calloc(1, sizeof(struct Type));
    pointer->ty = TY_PTR;
    pointer->ptr_to = ty;
    return pointer;
}

void add_type(struct Node *node) {
    if(node == NULL || node->ty) return;
    add_type(node->lhs);
    add_type(node->rhs);

    switch (node->kind) {
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_ASSIGN:
            node->ty = node->lhs->ty;
            return;
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_LVAR:
        case ND_NUM:
            node->ty = ty_int;
            return;
        case ND_ADDR:
            node->ty = pointer_to(node->lhs->ty);
            return;
        case ND_DEREF:
            node->ty = node->lhs->ty->ptr_to;
            return;
    }
}