#include "ecc.h"

struct Object *locals = NULL;
struct Object *globals = NULL;
struct Object *functions = NULL;

struct Scope *scope = &(struct Scope){};

int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}

struct Node *new_node(NodeKind kind) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = kind;
    return node;
}

struct Node *new_node_binary(NodeKind kind, struct Node *lhs,
                             struct Node *rhs) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

struct Node *new_node_unary(NodeKind kind, struct Node *cur) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = kind;
    node->lhs = cur;
    return node;
}

struct Node *new_node_num(int val) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

struct Node *new_node_var(struct Object *var) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->obj = var;
    if (var->is_global_variable) {
        node->kind = ND_GVAR;
    } else {
        node->kind = ND_LVAR;
    }
    node->ty = var->ty;
    return node;
}

struct Node *new_node_add(struct Node *lhs, struct Node *rhs) {
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_node_binary(ND_ADD, lhs, rhs);
    }

    if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
        error("ポインタ同士の足し算です");
    }

    if (is_pointer(rhs->ty)) {
        struct Node *buff = lhs;
        lhs = rhs;
        rhs = buff;
    }

    rhs = new_node_binary(ND_MUL, rhs, new_node_num(lhs->ty->ptr_to->size));

    return new_node_binary(ND_ADD, lhs, rhs);
}

struct Node *new_node_sub(struct Node *lhs, struct Node *rhs) {
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_node_binary(ND_SUB, lhs, rhs);
    }

    if (is_integer(lhs->ty) && is_pointer(rhs->ty)) {
        error("整数 - ポインタの演算です");
    }

    if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
        rhs = new_node_binary(ND_MUL, rhs, new_node_num(lhs->ty->ptr_to->size));
        return new_node_binary(ND_SUB, lhs, rhs);
    }

    if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
        struct Node *node = new_node_binary(ND_SUB, lhs, rhs);
        node->ty = ty_int;
        return new_node_binary(ND_DIV, node,
                               new_node_num(lhs->ty->ptr_to->size));
    }
}

void enter_scope() {
    struct Scope *scp = calloc(1, sizeof(struct Scope));
    scp->next = scope;
    scope = scp;
    return;
}

void leave_scope() {
    scope = scope->next;
    return;
}

void push_scope(struct Object *var) {
    struct VarScope *vs = calloc(1, sizeof(struct VarScope));
    vs->name = var->name;
    vs->var = var;
    vs->next = scope->vars;
    scope->vars = vs;
    return;
}

struct Object *find_object(struct Object *map, char *name) {
    for (struct Object *obj = map; obj; obj = obj->next) {
        if (obj->len == strlen(name) &&
            memcmp(name, obj->name, obj->len) == 0) {
            return obj;
        }
    }
    return NULL;
}

struct Object *find_variable_from_scope(char *name) {
    for (struct Scope *scp = scope; scp != NULL; scp = scp->next) {
        for (struct VarScope *var = scp->vars; var != NULL; var = var->next) {
            if (!strcmp(var->name, name)) {
                return var->var;
            }
        }
    }
    return NULL;
}

char *new_unique_name() {
    static int id = 0;
    char *buf = calloc(1, 20);
    sprintf(buf, ".LC%d", id++);
    return buf;
}

struct Object *new_var(char *name, struct Type *ty) {
    struct Object *var = calloc(1, sizeof(struct Object));
    var->name = name;
    var->ty = ty;
    push_scope(var);
    return var;
}

struct Object *new_local_var(char *name, struct Type *ty) {
    struct Object *lvar = find_variable_from_scope(name);
    if (lvar) {
        error("%sは既に定義されています", name);
    } else {
        lvar = new_var(name, ty);
        lvar->next = locals;
        lvar->len = strlen(name);
        if (locals == NULL) {
            lvar->offset = ty->size;
        } else {
            lvar->offset = locals->offset + ty->size;
        }
        locals = lvar;
        return lvar;
    }
}

struct Object *new_global_var(char *name, struct Type *ty) {
    struct Object *gvar = find_object(globals, name);
    if (gvar) {
        error("%sは既に定義されています", name);
    } else {
        gvar = new_var(name, ty);
        gvar->next = globals;
        gvar->len = strlen(name);
        gvar->is_global_variable = true;
        globals = gvar;
        return gvar;
    }
}

struct Object *new_anon_gvar(struct Type *ty) {
    return new_global_var(new_unique_name(), ty);
}

struct Object *new_string_literal(char *p) {
    struct Object *obj = new_anon_gvar(array_to(ty_char, strlen(p) + 1));
    obj->len = strlen(obj->name);
    obj->is_global_variable = true;
    obj->init_data = p;
    return obj;
}

void program(struct Token *);
struct Token *function(struct Token *);
struct Token *global_variable(struct Token *);
struct Type *declspec(struct Token *);
struct Type *declarator(struct Token **, struct Token *, struct Type *);
struct Type *type_suffix(struct Token **, struct Token *, struct Type *);
struct Type *func_params(struct Token **, struct Token *, struct Type *);
struct Type *params(struct Token **, struct Token *);
struct Node *stmt(struct Token **, struct Token *);
struct Node *compound_stmt(struct Token **, struct Token*);
struct Node *declaration(struct Token **, struct Token *);
struct Node *expr_stmt(struct Token **, struct Token*);
struct Node *expr(struct Token **, struct Token*);
struct Node *assign(struct Token **, struct Token*);
struct Node *equality(struct Token **, struct Token*);
struct Node *relation(struct Token **, struct Token*);
struct Node *add(struct Token **, struct Token*);
struct Node *mul(struct Token **, struct Token*);
struct Node *unary(struct Token **, struct Token*);
struct Node *postfix(struct Token **, struct Token*);
struct Node *funcall(struct Token **, struct Token*);
struct Node *primary(struct Token **, struct Token*);
bool is_function(struct Token *);

/*
program = (function | global_variable)*
*/

/*
function = declspec declarator "(" func_params "{" compound_stmt
*/
struct Token *function(struct Token *token) {}

/*
global_variable = declspec (declarator ("," declarator)* )?
*/
struct Token *global_variable(struct Token *token) {}

/*
declspec = "int" | "char"
*/
struct Type *declspec(struct Token *token) {
    assert(token->kind == TK_MOLD);
    struct Type *ty = NULL;
    if (equal(token, "int")) {
        ty = ty_int;
    } else if (equal(token, "char")) {
        ty = ty_char;
    } else {
        error("既定の型でありません");
    }
    return ty;
}

/*
declarator = "*"* ident type-suffix
*/
struct Type *declarator(struct Token **rest, struct Token *token,
                        struct Type *ty) {
    while (equal(token, "*")) {
        token = skip(token, "*");
        ty = pointer_to(ty);
    }
    assert(token->kind == TK_IDENT);
    char *name = strndup(token->str, token->len);
    ty = type_suffix(rest, token->next, ty);
    ty->name = name;
    return ty;
}

struct Type *type_suffix(struct Token **rest, struct Token *token,
                         struct Type *ty) {
    while (equal(token, "[")) {
        int sz = get_number(token->next);
        token = skip(token->next->next, "]");
        ty = array_to(ty, sz);
    }
    *rest = token;
    return ty;
}

/*
func_params = (param ("," param )? )? ")"
*/
struct Type *func_params(struct Token **rest, struct Token *token,
                         struct Type *return_ty) {
    struct Type head = {};
    struct Type *cur = &head;
    while (!equal(token, ")")) {
        if (cur != &head) token = skip(token, ",");
        struct Type *ty = declspec(token);
        ty = declarator(&token, token->next, ty);
        cur = cur->next = ty;
    }
    *rest = token->next;
    return func_to(return_ty, head.next);
}

/*
param = declspec declarator
*/

/*
stmt = "return" expr ";" | "if" "(" expr ")" stmt ("else" stmt)? | "while" "("
expr ")" stmt | "{" compound_stmt | expr-stmt
*/

/*
compound_stmt = (declaration | stmt)* "}"
*/

/*
declaration = declspec (declarator ("," declarator)* )?
*/
struct Node *declaration(struct Token **rest, struct Token *token) {}

/*
expr-stmt = expr? ";"
*/

/*
expr = assign
*/

/*
assign = equality ("=" assign)?
*/

/*
equality = relation ("==" relation | "!=" relation)*
*/

/*
relation = add ("<" add | "<=" add | ">" add | ">=" add)*
*/

/*
add = mul ("+" mul | "-" mul)*
*/

/*
mul = unary ("*" unary | "/" unary)*
*/

/*
unary = ("+" | "-" | "*" | "&") unary | postfix
*/

/*
postfix = primary ( "[" expr() "]" )*
*/

/*
funcall = ident "(" (assign ("," assign)*)? ")"
*/

/*
primary =  "(" "{" stmt+ "}" ")" | "(" expr ")" | "sizeof" unary | string | num
*/
struct Node *primary(struct Token **rest, struct Token *token) {}