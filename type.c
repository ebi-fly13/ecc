#include "ecc.h"

struct Type *ty_int = &(struct Type){TY_INT, NULL, 0, 8};
struct Type *ty_char = &(struct Type){TY_CHAR, NULL, 0, 1};

bool is_integer(struct Type *ty) {
    return ty->ty == TY_INT || ty->ty == TY_CHAR;
}

bool is_pointer(struct Type *ty) {
    return ty->ty == TY_PTR || ty->ty == TY_ARRAY;
}

struct Type *pointer_to(struct Type *ty) {
    struct Type *pointer = calloc(1, sizeof(struct Type));
    pointer->ty = TY_PTR;
    pointer->ptr_to = ty;
    pointer->size = 8;
    return pointer;
}

struct Type *array_to(struct Type *ty, size_t array_size) {
    struct Type *array = calloc(1, sizeof(struct Type));
    array->ty = TY_ARRAY;
    array->ptr_to = ty;
    array->array_size = array_size;
    array->size = ty->size * array_size;
    return array;
}

struct Type *func_to(struct Type *return_ty, struct Type *params) {
    struct Type *func = calloc(1, sizeof(struct Type));
    func->ty = TY_FUNC;
    func->return_ty = return_ty;
    func->params = params;
    return func;
}

void add_type(struct Node *node) {
    if (node == NULL || node->ty) return;
    add_type(node->lhs);
    add_type(node->rhs);

    add_type(node->cond);
    add_type(node->then);
    add_type(node->els);
    add_type(node->init);
    add_type(node->inc);

    for (struct Node *ret = node->body; ret; ret = ret->next) {
        add_type(ret);
    }

    for (struct Node *ret = node->args; ret; ret = ret->next) {
        add_type(ret);
    }

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
        case ND_NUM:
            node->ty = ty_int;
            return;
        case ND_ADDR:
            node->ty = pointer_to(node->lhs->ty);
            return;
        case ND_DEREF:
            node->ty = node->lhs->ty->ptr_to;
            return;
        case ND_LVAR:
        case ND_GVAR:
            return;
        case ND_FUNCALL:
            node->ty = node->obj->ty->return_ty;
            return;
        case ND_MEMBER:
            node->ty = node->member->ty;
    }
}