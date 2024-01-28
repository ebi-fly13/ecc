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
        error("ローカル変数%sは既に定義されています", name);
    } else {
        lvar = new_var(name, ty);
        lvar->next = locals;
        lvar->len = strlen(name);
        if (locals == NULL) {
            lvar->offset = (int)ty->size;
        } else {
            lvar->offset = locals->offset + (int)ty->size;
        }
        locals = lvar;
        return lvar;
    }
}

struct Object *new_global_var(char *name, struct Type *ty) {
    struct Object *gvar = find_object(globals, name);
    if (gvar) {
        error("グローバル変数%sは既に定義されています", name);
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

struct Object *new_func(char *name, struct Type *ty) {
    struct Object *fn = find_object(globals, name);
    if (fn) {
        error("関数%sは既に定義されています", name);
    } else {
        fn = new_var(name, ty);
        fn->next = globals;
        fn->len = strlen(name);
        fn->is_function = true;
        fn->next = functions;
        functions = fn;
    }
    return fn;
}

struct Node *expand_func_params(struct Type *ty) {
    struct Node head = {};
    struct Node *cur = &head;
    for (struct Type *p = ty->params; p != NULL; p = p->next) {
        new_local_var(p->name, p);
        cur = cur->next = new_node_var(find_variable_from_scope(p->name));
    }
    return head.next;
}

struct Object *program(struct Token *);
struct Token *function(struct Token *);
struct Token *global_variable(struct Token *);
struct Type *declspec(struct Token **, struct Token *);
struct Type *declarator(struct Token **, struct Token *, struct Type *);
struct Type *type_suffix(struct Token **, struct Token *, struct Type *);
struct Type *func_params(struct Token **, struct Token *, struct Type *);
struct Type *params(struct Token **, struct Token *);
struct Node *stmt(struct Token **, struct Token *);
struct Node *compound_stmt(struct Token **, struct Token *);
struct Node *declaration(struct Token **, struct Token *);
struct Node *expr_stmt(struct Token **, struct Token *);
struct Node *expr(struct Token **, struct Token *);
struct Node *assign(struct Token **, struct Token *);
struct Node *equality(struct Token **, struct Token *);
struct Node *relation(struct Token **, struct Token *);
struct Node *add(struct Token **, struct Token *);
struct Node *mul(struct Token **, struct Token *);
struct Node *unary(struct Token **, struct Token *);
struct Node *postfix(struct Token **, struct Token *);
struct Node *funcall(struct Token **, struct Token *);
struct Node *primary(struct Token **, struct Token *);

bool is_function(struct Token *token) {
    if (equal(token, ";")) return false;
    struct Type dummy = {};
    struct Type *ty = declarator(&token, token->next, &dummy);
    return equal(token, "(");
}

/*
program = (function | global_variable)*
*/
struct Object *program(struct Token *token) {
    globals = NULL;
    while (token->kind != TK_EOF) {
        if (is_function(token)) {
            token = function(token);
        } else {
            token = global_variable(token);
        }
    }
    return globals;
}

/*
function = declspec declarator "(" func_params "{" compound_stmt
*/
struct Token *function(struct Token *token) {
    struct Type *ty = declspec(&token, token);
    ty = declarator(&token, token, ty);
    token = skip(token, "(");
    ty = func_params(&token, token, ty);
    struct Object *fn = new_func(ty->name, ty);
    locals = NULL;
    enter_scope();
    fn->args = expand_func_params(ty);
    if (equal(token, "{")) {
        token = skip(token, "{");
        fn->body = compound_stmt(&token, token);
        if (locals)
            fn->stack_size = locals->offset;
        else
            fn->stack_size = 0;
        fn->stack_size = align_to(fn->stack_size + 8, 16);
        leave_scope();
    } else {
        token = skip(token, ";");
    }
    return token;
}

/*
global_variable = declspec (declarator ("," declarator)* )? ";"
*/
struct Token *global_variable(struct Token *token) {
    struct Type *ty = declspec(&token, token);
    bool is_first = true;
    while (!equal(token, ";")) {
        if (!is_first)
            token = skip(token, ",");
        else
            is_first = false;
        struct Type *gvar = declarator(&token, token, ty);
        new_global_var(gvar->name, gvar);
    }
    return token->next;
}

/*
declspec = "int" | "char"
*/
struct Type *declspec(struct Token **rest, struct Token *token) {
    assert(token->kind == TK_MOLD);
    struct Type *ty = calloc(1, sizeof(struct Type));
    if (equal(token, "int")) {
        *rest = skip(token, "int");
        memcpy(ty, ty_int, sizeof(struct Type));
    } else if (equal(token, "char")) {
        *rest = skip(token, "char");
        memcpy(ty, ty_char, sizeof(struct Type));
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
        struct Type *ty = declspec(&token, token);
        ty = declarator(&token, token, ty);
        cur = cur->next = ty;
    }
    *rest = token->next;
    struct Type *fn = func_to(return_ty, head.next);
    fn->name = return_ty->name;
    return fn;
}

/*
param = declspec declarator
*/
struct Type *param(struct Token **rest, struct Token *token) {
    struct Type *ty = declspec(&token, token);
    ty = declarator(&token, token, ty);
    *rest = token;
    return ty;
}

/*
stmt = "return" expr ";" | "if" "(" expr ")" stmt ("else" stmt)? | "while" "("
expr ")" stmt | "for" "(" expr? ";" expr? ";" expr ")" |  "{" compound_stmt |
expr-stmt
*/
struct Node *stmt(struct Token **rest, struct Token *token) {
    struct Node *node = NULL;
    if (equal_keyword(token, TK_RETURN)) {
        token = skip_keyword(token, TK_RETURN);
        node = new_node_unary(ND_RETURN, expr(&token, token));
        token = skip(token, ";");
    } else if (equal_keyword(token, TK_IF)) {
        token = skip_keyword(token, TK_IF);
        token = skip(token, "(");
        node = new_node(ND_IF);
        node->cond = expr(&token, token);
        token = skip(token, ")");
        node->then = stmt(&token, token);
        if (equal_keyword(token, TK_ELSE)) {
            token = skip_keyword(token, TK_ELSE);
            node->els = stmt(&token, token);
        }
    } else if (equal_keyword(token, TK_WHILE)) {
        token = skip_keyword(token, TK_WHILE);
        node = new_node(ND_WHILE);
        token = skip(token, "(");
        node->cond = expr(&token, token);
        token = skip(token, ")");
        node->then = stmt(&token, token);
    } else if (equal_keyword(token, TK_FOR)) {
        token = skip_keyword(token, TK_FOR);
        node = new_node(ND_FOR);
        token = skip(token, "(");
        if (!equal(token, ";")) {
            node->init = expr(&token, token);
        }
        token = skip(token, ";");
        if (!equal(token, ";")) {
            node->cond = expr(&token, token);
        }
        token = skip(token, ";");
        if (!equal(token, ")")) {
            node->inc = expr(&token, token);
        }
        token = skip(token, ")");
        node->then = stmt(&token, token);
    } else if (equal(token, "{")) {
        node = compound_stmt(&token, token->next);
    } else {
        node = expr_stmt(&token, token);
    }
    *rest = token;
    return node;
}

/*
compound_stmt = (declaration | stmt)* "}"
*/
struct Node *compound_stmt(struct Token **rest, struct Token *token) {
    struct Node *node = new_node(ND_BLOCK);
    struct Node head = {};
    struct Node *cur = &head;

    enter_scope();
    while (!equal(token, "}")) {
        if (token->kind == TK_MOLD) {
            cur->next = declaration(&token, token);
        } else {
            cur->next = stmt(&token, token);
        }
        if (cur->next == NULL) continue;
        cur = cur->next;
        add_type(cur);
    }
    leave_scope();
    *rest = skip(token, "}");
    node->body = head.next;
    return node;
}

/*
declaration = declspec (declarator ("=" expr) ("," declarator "=" expr)* )? ";"
*/
struct Node *declaration(struct Token **rest, struct Token *token) {
    struct Type *base_ty = declspec(&token, token);
    struct Node head = {};
    struct  Node *cur = &head;
    bool is_first = true;
    while (!equal(token, ";")) {
        if(!is_first) token = skip(token, ",");
        is_first = false;
        struct Type *ty = declarator(&token, token, base_ty);
        struct Object *var = new_local_var(ty->name, ty);
        if(!equal(token, "=")) continue; 
        cur->next = new_node_binary(ND_ASSIGN, new_node_var(var), expr(&token, token));
        cur = cur->next;
        add_type(cur);
    }
    *rest = skip(token, ";");
    struct Node *node = new_node(ND_BLOCK);
    node->body = head.next;
    return node;
}

/*
expr-stmt = expr? ";"
*/
struct Node *expr_stmt(struct Token **rest, struct Token *token) {
    struct Node *node = NULL;
    if (!equal(token, ";")) {
        node = expr(&token, token);
    }
    *rest = skip(token, ";");
    return node;
}

/*
expr = assign
*/
struct Node *expr(struct Token **rest, struct Token *token) {
    return assign(rest, token);
}

/*
assign = equality ("=" assign)?
*/
struct Node *assign(struct Token **rest, struct Token *token) {
    struct Node *node = equality(&token, token);
    if (equal(token, "=")) {
        node = new_node_binary(ND_ASSIGN, node, assign(&token, token->next));
    }
    *rest = token;
    return node;
}

/*
equality = relation ("==" relation | "!=" relation)*
*/
struct Node *equality(struct Token **rest, struct Token *token) {
    struct Node *node = relation(&token, token);
    for (;;) {
        if (equal(token, "==")) {
            node = new_node_binary(ND_EQ, node, relation(&token, token->next));
        } else if (equal(token, "!=")) {
            node = new_node_binary(ND_NE, node, relation(&token, token->next));
        } else {
            *rest = token;
            return node;
        }
    }
}

/*
relation = add ("<" add | "<=" add | ">" add | ">=" add)*
*/
struct Node *relation(struct Token **rest, struct Token *token) {
    struct Node *node = add(&token, token);

    for (;;) {
        if (equal(token, "<")) {
            node = new_node_binary(ND_LT, node, add(&token, token->next));
        } else if (equal(token, "<=")) {
            node = new_node_binary(ND_LE, node, add(&token, token->next));
        } else if (equal(token, ">")) {
            node = new_node_binary(ND_LT, add(&token, token->next), node);
        } else if (equal(token, ">=")) {
            node = new_node_binary(ND_LE, add(&token, token->next), node);
        } else {
            *rest = token;
            return node;
        }
    }
}

/*
add = mul ("+" mul | "-" mul)*
*/
struct Node *add(struct Token **rest, struct Token *token) {
    struct Node *node = mul(&token, token);
    for (;;) {
        if (equal(token, "+")) {
            node = new_node_add(node, mul(&token, token->next));
        } else if (equal(token, "-")) {
            node = new_node_sub(node, mul(&token, token->next));
        } else {
            *rest = token;
            return node;
        }
    }
}

/*
mul = unary ("*" unary | "/" unary)*
*/
struct Node *mul(struct Token **rest, struct Token *token) {
    struct Node *node = unary(&token, token);
    for (;;) {
        if (equal(token, "*")) {
            node = new_node_binary(ND_MUL, node, unary(&token, token->next));
        } else if (equal(token, "/")) {
            node = new_node_binary(ND_DIV, node, unary(&token, token->next));
        } else {
            *rest = token;
            return node;
        }
    }
}

/*
unary = ("+" | "-" | "*" | "&") unary | postfix
*/
struct Node *unary(struct Token **rest, struct Token *token) {
    if (equal(token, "+")) {
        return unary(rest, token->next);
    } else if (equal(token, "-")) {
        return new_node_binary(ND_SUB, new_node_num(0),
                               unary(rest, token->next));
    } else if (equal(token, "*")) {
        return new_node_unary(ND_DEREF, unary(rest, token->next));
    } else if (equal(token, "&")) {
        return new_node_unary(ND_ADDR, unary(rest, token->next));
    } else {
        return postfix(rest, token);
    }
}

/*
postfix = primary ( "[" expr() "]" )*
*/
struct Node *postfix(struct Token **rest, struct Token *token) {
    struct Node *node = primary(&token, token);
    while (equal(token, "[")) {
        node = new_node_unary(ND_DEREF,
                              new_node_add(node, expr(&token, token->next)));
        token = skip(token, "]");
    }
    *rest = token;
    return node;
}

/*
funcall = ident "(" (expr ("," expr)*)? ")"
*/
struct Node *funcall(struct Token **rest, struct Token *token) {
    assert(token->kind == TK_IDENT);
    char *name = strndup(token->str, token->len);
    struct Node *node = new_node(ND_FUNCALL);
    struct Object *func = find_object(functions, name);
    if (func == NULL) {
        error("関数 %s は定義されていません", name);
    }
    token = token->next;
    node->ty = func->ty;
    node->obj = func;
    struct Node head = {};
    struct Node *cur = &head;
    assert(equal(token, "("));
    token = skip(token, "(");
    while (!equal(token, ")")) {
        if (cur != &head) token = skip(token, ",");
        cur = cur->next = expr(&token, token);
    }
    node->args = head.next;
    *rest = token->next;
    return node;
}

/*
primary =  "(" "{" compound_stmt ")" | "(" expr ")" | "sizeof" unary | ident
func-args? | string | num
*/
struct Node *primary(struct Token **rest, struct Token *token) {
    struct Node *node = NULL;
    if (equal(token, "(") && equal(token->next, "{")) {
        token = skip(token->next, "{");
        node = new_node(ND_STMT_EXPR);
        node->body = compound_stmt(&token, token)->body;
        *rest = skip(token, ")");
    } else if (equal(token, "(")) {
        token = skip(token, "(");
        node = expr(&token, token);
        *rest = skip(token, ")");
    } else if (equal(token, "sizeof")) {
        node = unary(&token, token->next);
        add_type(node);
        node = new_node_num(node->ty->size);
        *rest = token;
    } else if (token->kind == TK_IDENT) {
        if (equal(token->next, "(")) {
            node = funcall(&token, token);
            *rest = token;
        } else {
            char *name = strndup(token->str, token->len);
            struct Object *var = find_variable_from_scope(name);
            if (var == NULL) {
                error("変数%sは定義されていません", name);
            }
            node = new_node_var(var);
            *rest = token->next;
        }
    } else if (token->kind == TK_STR) {
        node = new_node_var(new_string_literal(get_string(token)));
        *rest = token->next;
    } else if (token->kind == TK_NUM) {
        node = new_node_num(get_number(token));
        *rest = token->next;
    } else {
        error("parseエラー");
    }
    return node;
}