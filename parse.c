#include "ecc.h"

struct Var *locals = NULL;
struct Var *globals = NULL;

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

struct Node *new_node_lvar(struct Var *lvar) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = ND_LVAR;
    node->offset = lvar->offset;
    node->ty = lvar->ty;
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

    rhs = new_node_binary(ND_MUL, rhs, new_node_num(8));

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
        rhs = new_node_binary(ND_MUL, rhs, new_node_num(8));
        return new_node_binary(ND_SUB, lhs, rhs);
    }

    if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
        struct Node *node = new_node_binary(ND_SUB, lhs, rhs);
        node->ty = ty_int;
        return new_node_binary(ND_DIV, node, new_node_num(8));
    }
}

struct Var *find_lvar(char *name) {
    for (struct Var *var = locals; var; var = var->next) {
        if (var->len == strlen(name) &&
            memcmp(name, var->name, var->len) == 0) {
            return var;
        }
    }
    return NULL;
}

void add_local_var(char *name, struct Type *ty) {
    struct Var *lvar = find_lvar(name);
    if (lvar) {
        error("%sはすでに定義されています", name);
    } else {
        lvar = calloc(1, sizeof(struct Var));
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

/*
program    = function*
function   = type ident "(" (type ident ("," type ident)*)? ")" "{" stmt* "}"
stmt       = expr ";" | "return" expr ";" | "if" "(" expr ")" stmt ( "else" stmt
)? | "while" "(" expr ")" stmt | "for" "(" expr? ";" expr? ";" expr? ")" stmt |
"{" stmt* "}" | type ident ("[" num "]")?
expr       = assign
assign     = equality ("=" assign)?
equality   = relational ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add        = mul ("+" mul | "-" mul)*
mul        = unary ("*" unary | "/" unary)*
unary      = "sizeof" unary | "+"? postfix | "-"? postfix | "*"? unary | "&"? unary
postfix    = primary ("[" expr "]")?
primary    = num | ident ( "(" (expr ("," expr)*)? ")" )? | "(" expr ")"
type       = int "*"*
*/

struct Function *program();
struct Function *function();
struct Node *stmt();
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

struct Function *program() {
    struct Function head = {};
    struct Function *cur = &head;
    while (!at_eof()) {
        cur = cur->next = function();
    }
    return head.next;
}

struct Function *function() {
    struct Function *func = calloc(1, sizeof(struct Function));
    func->ty = type();
    func->name = strndup(token->str, token->len);
    expect_ident();
    expect_op("(");
    locals = NULL;
    {
        struct Node head = {};
        struct Node *cur = &head;
        while (!consume(")")) {
            if (locals != NULL) expect_op(",");
            struct Type *ty = type();
            add_local_var(strndup(token->str, token->len), ty);
            cur = cur->next =
                new_node_lvar(find_lvar(strndup(token->str, token->len)));
            expect_ident();
        }
        func->args = head.next;
    }
    expect_op("{");
    {  // 関数内の記述
        struct Node head = {};
        struct Node *cur = &head;
        while (!consume("}")) {
            cur->next = stmt();
            if (cur->next == NULL) continue;
            cur = cur->next;
            add_type(cur);
        }
        func->body = new_node(ND_BLOCK);
        func->body->body = head.next;
    }
    func->local_variables = locals;
    if (locals)
        func->stack_size = locals->offset;
    else
        func->stack_size = 0;
    return func;
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
        struct Node head = {};
        struct Node *cur = &head;
        while (!consume("}")) {
            cur->next = stmt();
            if (cur->next == NULL) continue;
            cur = cur->next;
        }
        node = new_node(ND_BLOCK);
        node->body = head.next;
    } else if (at_keyword(TK_MOLD)) {
        struct Type *ty = type();
        char *name = strndup(token->str, token->len);
        expect_ident();
        if(consume("[")) {
            ty = array_to(ty, expect_number());
            expect_op("]");
        }
        add_local_var(name, ty);
    } else {
        if (!consume(";")) node = expr();
    }
    return node;
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
        return postfix();
    }
    if (consume("-")) {
        return new_node_binary(ND_SUB, new_node_num(0), postfix());
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

    if(consume("[")) {
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
        // function call
        if (equal_op(token->next, "(")) {
            struct Node *node = new_node(ND_FUNCALL);
            node->funcname = strndup(token->str, token->len);
            expect_ident();
            expect_op("(");
            struct Node head = {};
            struct Node *cur = &head;
            while (!consume(")")) {
                if (cur != &head) expect_op(",");
                cur = cur->next = expr();
            }
            node->args = head.next;
            return node;
        }

        struct Var *lvar = find_lvar(strndup(token->str, token->len));
        if (lvar == NULL) {
            error("%sは定義されていません", strndup(token->str, token->len));
        }

        struct Node *node = new_node_lvar(lvar);
        expect_ident();
        return node;
    } else if (at_number()) {
        return new_node_num(expect_number());
    } else {
        error("parseエラー");
    }
}

struct Type *type() {
    expect_keyword(TK_MOLD);
    struct Type *ty = ty_int;
    while (consume("*")) {
        ty = pointer_to(ty);
    }
    return ty;
}