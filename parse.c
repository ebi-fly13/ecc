#include "ecc.h"

struct Object *locals = NULL;
struct Object *globals = NULL;
struct Object *functions = NULL;

int align_to(int n, int align) { return (n + align - 1) / align * align; }

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
        return new_node_binary(ND_DIV, node, new_node_num(lhs->ty->ptr_to->size));
    }
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

void add_local_var(char *name, struct Type *ty) {
    struct Object *lvar = find_object(locals, name);
    if (lvar) {
        error("%sはすでに定義されています", name);
    } else {
        lvar = calloc(1, sizeof(struct Object));
        lvar->next = locals;
        lvar->name = name;
        lvar->len = strlen(name);
        lvar->ty = ty;
        if (locals == NULL)
            lvar->offset = ty->size;
        else
            lvar->offset = locals->offset + ty->size;
        locals = lvar;
        return;
    }
}

void add_global_var(struct Object *obj) {
    static struct Object *cur = NULL;
    assert(obj != NULL);
    if(find_object(globals, obj->name) != NULL) {
        printf("%sはすでに定義されています", obj->name);
    }
    if(globals == NULL) {
        globals = obj;
        cur = obj;
    }
    else {
        cur = cur->next = obj;
    }
    return;
}

char *new_unique_name() {
    static int id = 0;
    char *buf = calloc(1, 20);
    sprintf(buf, ".LC%d", id++);
    return buf;
}

struct Object *new_string_literal(char *p) {
    struct Object *obj = calloc(1, sizeof(struct Object));
    obj->name = new_unique_name();
    obj->len = strlen(obj->name);
    obj->ty = array_to(ty_char, strlen(p) + 1);
    obj->is_global_variable = true;
    obj->init_data = p;
    add_global_var(obj);
    return obj;
}

/*
program    = function*
function   = type ident "(" (type ident ("," type ident)*)? ")" "{" stmt* "}" | type ident ("[" num "]")? ";"
stmt       = expr ";" | "return" expr ";" | "if" "(" expr ")" stmt ( "else" stmt
)? | "while" "(" expr ")" stmt | "for" "(" expr? ";" expr? ";" expr? ")" stmt |
"{" stmt* "}" | type ident ("[" num "]")? ";"
expr       = assign
assign     = equality ("=" assign)?
equality   = relational ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add        = mul ("+" mul | "-" mul)*
mul        = unary ("*" unary | "/" unary)*
unary      = "sizeof" unary | "+"? unary | "-"? unary | "*"? unary | "&"? unary 
postfix    = primary ("[" expr "]")? 
primary    = string | num | ident ( "(" (expr ("," expr)*)? ")" )? | "(" expr ")" | "(" "{" stmt+ "}" ")"
type       = int "*"*
*/

void program();
struct Object *function();
struct Node *stmt();
struct Node *compound_stmt();
struct Node *expr();
struct Node *assign();
struct Node *equality();
struct Node *relational();
struct Node *add();
struct Node *mul();
struct Node *unary();
struct Node *postfix();
struct Node *primary();
struct Type *type();

void program() {
    globals = NULL;
    functions = NULL;
    struct Object *cur_function = NULL;
    while (!at_eof()) {
        struct Object *obj = function();
        if (obj->is_function) {
            if (functions == NULL) {
                functions = obj;
                cur_function = obj;
                continue;
            }
            cur_function = cur_function->next = obj;
        } else if (obj->is_global_variable) {
            add_global_var(obj);
        } else {
            assert(0);
        }
    }
    return;
}

struct Object *function() {
    struct Object *obj = calloc(1, sizeof(struct Object));
    obj->ty = type();
    obj->name = strndup(token->str, token->len);
    obj->len = token->len;
    expect_ident();
    if (consume("(")) {
        obj->is_function = true;
        locals = NULL;
        {
            struct Node head = {};
            struct Node *cur = &head;
            while (!consume(")")) {
                if (locals != NULL) expect_op(",");
                struct Type *ty = type();
                char *name = strndup(token->str, token->len);
                expect_ident();
                add_local_var(name, ty);
                cur = cur->next = new_node_var(find_object(locals, name));
            }
            obj->args = head.next;
        }
        if(consume(";")) {
            return obj;
        }
        expect_op("{");
        {  // 関数内の記述
            obj->body = new_node(ND_BLOCK);
            obj->body->body = compound_stmt();
        }
        obj->local_variables = locals;
        if (locals)
            obj->stack_size = locals->offset;
        else
            obj->stack_size = 0;
        obj->stack_size = align_to(obj->stack_size, 16);
        return obj;
    } else {
        obj->is_global_variable = true;
        if (consume("[")) {
            obj->ty = array_to(obj->ty, expect_number());
            expect_op("]");
        }
        expect_op(";");
        return obj;
    }
}

struct Node *stmt() {
    struct Node *node = NULL;
    if (at_keyword(TK_RETURN)) {
        expect_keyword(TK_RETURN);
        node = new_node_unary(ND_RETURN, expr());
        expect_op(";");
    } else if (at_keyword(TK_IF)) {
        expect_keyword(TK_IF);
        expect_op("(");
        node = new_node(ND_IF);
        node->cond = expr();
        expect_op(")");
        node->then = stmt();
        if (at_keyword(TK_ELSE)) {
            expect_keyword(TK_ELSE);
            node->els = stmt();
        }
    } else if (at_keyword(TK_WHILE)) {
        node = new_node(ND_WHILE);
        expect_keyword(TK_WHILE);
        expect_op("(");
        node->cond = expr();
        expect_op(")");
        node->then = stmt();
    } else if (at_keyword(TK_FOR)) {
        node = new_node(ND_FOR);
        expect_keyword(TK_FOR);
        expect_op("(");
        if (!consume(";")) {
            node->init = expr();
            expect_op(";");
        }
        if (!consume(";")) {
            node->cond = expr();
            expect_op(";");
        }
        if (!consume(")")) {
            node->inc = expr();
            expect_op(")");
        }
        node->then = stmt();
    } else if (consume("{")) {
        node = new_node(ND_BLOCK);
        node->body = compound_stmt();
    } else if (at_keyword(TK_MOLD)) {
        struct Type *ty = type();
        char *name = strndup(token->str, token->len);
        expect_ident();
        if (consume("[")) {
            ty = array_to(ty, expect_number());
            expect_op("]");
        }
        expect_op(";");
        add_local_var(name, ty);
    } else {
        if (!consume(";")) node = expr();
    }
    add_type(node);
    return node;
}

struct Node *compound_stmt() {
    struct Node head = {};
    struct Node *cur = &head;
    while (!consume("}")) {
        cur->next = stmt();
        if (cur->next == NULL) continue;
        cur = cur->next;
        add_type(cur);
    }
    return head.next;
}

struct Node *expr() {
    return assign();
}

struct Node *assign() {
    struct Node *node = equality();
    if (consume("=")) {
        node = new_node_binary(ND_ASSIGN, node, assign());
    }
    return node;
}

struct Node *equality() {
    struct Node *node = relational();
    for (;;) {
        if (consume("=="))
            node = new_node_binary(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_node_binary(ND_NE, node, relational());
        else
            return node;
    }
}

struct Node *relational() {
    struct Node *node = add();
    for (;;) {
        if (consume("<"))
            node = new_node_binary(ND_LT, node, add());
        else if (consume("<="))
            node = new_node_binary(ND_LE, node, add());
        else if (consume(">"))
            node = new_node_binary(ND_LT, add(), node);
        else if (consume(">="))
            node = new_node_binary(ND_LE, add(), node);
        else
            return node;
    }
}

struct Node *add() {
    struct Node *node = mul();

    for (;;) {
        if (consume("+"))
            node = new_node_add(node, mul());
        else if (consume("-"))
            node = new_node_sub(node, mul());
        else
            return node;
    }
}

struct Node *mul() {
    struct Node *node = unary();
    for (;;) {
        if (consume("*"))
            node = new_node_binary(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_node_binary(ND_DIV, node, unary());
        else
            return node;
    }
}

struct Node *unary() {
    if (consume("+")) {
        return unary();
    }
    if (consume("-")) {
        return new_node_binary(ND_SUB, new_node_num(0), unary());
    }
    if (consume("*")) {
        return new_node_unary(ND_DEREF, unary());
    }
    if (consume("&")) {
        return new_node_unary(ND_ADDR, unary());
    }

    if (at_keyword(TK_SIZEOF)) {
        expect_keyword(TK_SIZEOF);
        struct Node *node = unary();
        add_type(node);
        return new_node_num(node->ty->size);
    }
    return postfix();
}

struct Node *postfix() {
    struct Node *node = primary();

    if (consume("[")) {
        node = new_node_unary(ND_DEREF, new_node_add(node, expr()));
        expect_op("]");
    }

    return node;
}

struct Node *primary() {
    if (consume("(")) {
        struct Node *node = expr();
        expect_op(")");
        return node;
    } else if (at_ident()) {
        char *name = strndup(token->str, token->len);
        expect_ident();
        // function call
        if (consume("(")) {
            struct Node *node = new_node(ND_FUNCALL);
            struct Object *func = find_object(functions, name);
            if (func == NULL) {
                error("関数 %s は定義されていません", name);
            }
            node->ty = func->ty;
            node->obj = func;
            struct Node head = {};
            struct Node *cur = &head;
            while (!consume(")")) {
                if (cur != &head) expect_op(",");
                cur = cur->next = expr();
            }
            node->args = head.next;
            return node;
        }

        struct Object *var = find_object(locals, name);
        if (var == NULL) {
            var = find_object(globals, name);
        }

        if (var == NULL) {
            error("変数%sは定義されていません", name);
        }

        struct Node *node = new_node_var(var);
        return node;
    } else if (at_number()) {
        return new_node_num(expect_number());
    } else if (at_string()) {
        return new_node_var(new_string_literal(expect_string()));
    } else {
        error("parseエラー");
    }
}

struct Type *type() {
    assert(at_keyword(TK_MOLD));
    struct Type *ty = NULL;
    if (equal(token, "int")) {
        ty = ty_int;
    } else if (equal(token, "char")) {
        ty = ty_char;
    } else {
        error("既定の型でありません");
    }

    expect_keyword(TK_MOLD);
    while (consume("*")) {
        ty = pointer_to(ty);
    }
    return ty;
}