#include "ecc.h"

struct Type *ty_long = &(struct Type){TY_LONG, NULL, 0, 8, 8};
struct Type *ty_int = &(struct Type){TY_INT, NULL, 0, 4, 4};
struct Type *ty_short = &(struct Type){TY_SHORT, NULL, 0, 2, 2};
struct Type *ty_char = &(struct Type){TY_CHAR, NULL, 0, 1, 1};
struct Type *ty_void = &(struct Type){TY_VOID, NULL, 0, 0, 1};
struct Type *ty_bool = &(struct Type){TY_BOOL, NULL, 0, 1, 1};
struct Type *ty_float = &(struct Type){TY_FLOAT, NULL, 0, 4, 4, true};
struct Type *ty_double = &(struct Type){TY_DOUBLE, NULL, 0, 8, 8, true};

struct Type *ty_ulong = &(struct Type){TY_LONG, NULL, 0, 8, 8, true};
struct Type *ty_uint = &(struct Type){TY_INT, NULL, 0, 4, 4, true};
struct Type *ty_ushort = &(struct Type){TY_SHORT, NULL, 0, 2, 2, true};
struct Type *ty_uchar = &(struct Type){TY_CHAR, NULL, 0, 1, 1, true};

int align_to(int n, int align) { return (n + align - 1) / align * align; }

bool is_integer(struct Type *ty) {
    return ty->ty == TY_LONG || ty->ty == TY_INT || ty->ty == TY_SHORT ||
           ty->ty == TY_CHAR || ty->ty == TY_BOOL || ty->ty == TY_ENUM;
}

bool is_flonum(struct Type *ty) {
    return ty->ty == TY_FLOAT || ty->ty == TY_DOUBLE;
}

bool is_void(struct Type *ty) { return ty->ty == TY_VOID; }

bool is_pointer(struct Type *ty) {
    return ty->ty == TY_PTR || ty->ty == TY_ARRAY;
}

bool is_same_type(struct Type *lhs, struct Type *rhs) {
    if (lhs->ty != rhs->ty)
        return false;
    if (lhs->ty == TY_STRUCT || lhs->ty == TY_UNION) {
        if (strcmp(lhs->name, rhs->name) != 0)
            return false;
    } else if (lhs->ty == TY_FUNC) {
        if (!is_same_type(lhs->return_ty, rhs->return_ty))
            return false;
        struct NameTag *param_lhs = lhs->params;
        struct NameTag *param_rhs = rhs->params;
        while (param_lhs != NULL && param_rhs != NULL) {
            if (!is_same_type(param_lhs->ty, param_rhs->ty))
                return false;
            param_lhs = param_lhs->next;
            param_rhs = param_rhs->next;
        }
        if (param_lhs != NULL && param_rhs != NULL)
            return false;
    }

    return true;
}

struct Type *pointer_to(struct Type *ty) {
    struct Type *pointer = calloc(1, sizeof(struct Type));
    pointer->ty = TY_PTR;
    pointer->ptr_to = ty;
    pointer->size = 8;
    pointer->align = 8;
    return pointer;
}

struct Type *array_to(struct Type *ty, int array_size) {
    struct Type *array = calloc(1, sizeof(struct Type));
    array->ty = TY_ARRAY;
    array->ptr_to = ty;
    array->array_size = array_size;
    array->align = ty->align;

    if (array_size < 0)
        return array;

    array->size = ty->size * array_size;
    return array;
}

struct Type *func_to(struct Type *return_ty, struct NameTag *params) {
    struct Type *func = calloc(1, sizeof(struct Type));
    func->ty = TY_FUNC;
    func->return_ty = return_ty;
    func->params = params;
    return func;
}

struct Type *new_type(TypeKind kind, int size, int align) {
    struct Type *ty = calloc(1, sizeof(struct Type));
    ty->ty = kind;
    ty->size = size;
    ty->align = align;
    return ty;
}

struct Type *struct_type() { return new_type(TY_STRUCT, 0, 1); }

struct Type *union_type() { return new_type(TY_UNION, 0, 1); }

struct Type *enum_type() { return new_type(TY_ENUM, 4, 4); }

struct Type *get_common_type(struct Type *lhs, struct Type *rhs) {
    if (lhs->ptr_to) {
        return pointer_to(lhs->ptr_to);
    }

    if (lhs->size < 4) {
        lhs = ty_int;
    }
    if (rhs->size < 4) {
        rhs = ty_int;
    }

    if (lhs->size != rhs->size) {
        return (lhs->size > rhs->size) ? lhs : rhs;
    }
    if (rhs->is_unsigned) {
        return rhs;
    } else {
        return lhs;
    }
}

struct Type *copy_type(struct Type *ty) {
    struct Type *copy_ty = calloc(1, sizeof(struct Type));
    *copy_ty = *ty;
    return copy_ty;
}

void usual_arith_conv(struct Node **lhs, struct Node **rhs) {
    struct Type *ty = get_common_type((*lhs)->ty, (*rhs)->ty);
    *lhs = new_node_cast(*lhs, ty);
    *rhs = new_node_cast(*rhs, ty);
}

void add_type(struct Node *node) {
    if (node == NULL || node->ty)
        return;
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
    case ND_NUM:
        node->ty = (node->val == (int)node->val) ? ty_int : ty_long;
        return;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
    case ND_BITOR:
    case ND_BITXOR:
    case ND_BITAND:
    case ND_SHL:
    case ND_SHR:
        usual_arith_conv(&node->lhs, &node->rhs);
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_COMMA:
        node->ty = node->rhs->ty;
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        usual_arith_conv(&node->lhs, &node->rhs);
        node->ty = ty_int;
        return;
    case ND_ADDR:
        node->ty = pointer_to(node->lhs->ty);
        return;
    case ND_DEREF:
        node->ty = node->lhs->ty->ptr_to;
        return;
    case ND_NOT:
    case ND_LOGOR:
    case ND_LOGAND:
        node->ty = ty_int;
        return;
    case ND_BITNOT:
        node->ty = node->lhs->ty;
        return;
    case ND_VAR:
    case ND_FUNCALL:
        assert(node->ty != NULL);
        return;
    case ND_MEMBER:
        node->ty = node->member->ty;
        return;
    case ND_COND:
        if (node->then->ty->ty == TY_VOID || node->els->ty->ty == TY_VOID) {
            node->ty = ty_void;
        } else {
            usual_arith_conv(&node->then, &node->els);
            node->ty = node->then->ty;
        }
        return;
    }
}