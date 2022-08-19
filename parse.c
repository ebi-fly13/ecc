#include "ecc.h"

struct LVar *locals = NULL;

struct Node *new_node(NodeKind kind, struct Node *lhs, struct Node *rhs) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

struct Node *new_node_num(int val) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

struct Node *new_node_lvar(struct LVar *lvar) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = ND_LVAR;
    node->offset = lvar->offset;
    node->ty = lvar->ty;
    return node;
}

struct LVar *find_lvar(struct Token *tok) {
    for (struct LVar *var = locals; var; var = var->next) {
        if (var->len == tok->len &&
            memcmp(tok->str, var->name, var->len) == 0) {
            return var;
        }
    }
    return NULL;
}

void add_local_var(struct Token *tok, struct Type *ty) {
    struct LVar *lvar = find_lvar(tok);
    if (lvar) {
        error("%sはすでに定義されています", strndup(tok->str, tok->len));
    } else {
        lvar = calloc(1, sizeof(struct LVar));
        lvar->next = locals;
        lvar->name = tok->str;
        lvar->len = tok->len;
        lvar->ty = ty;
        if (locals == NULL)
            lvar->offset = 8;
        else
            lvar->offset = locals->offset + 8;
        locals = lvar;
        return;
    }
}

/*
program    = function*
function   = type ident "(" (type ident ("," type ident)*)? ")" "{" stmt* "}"
stmt       = expr ";" | "return" expr ";" | "if" "(" expr ")" stmt ( "else" stmt
)? | "while" "(" expr ")" stmt | "for" "(" expr? ";" expr? ";" expr? ")" stmt |
"{" stmt* "}" | type ident
expr       = assign
assign     = equality ("=" assign)?
equality   = relational ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add        = mul ("+" mul | "-" mul)*
mul        = unary ("*" unary | "/" unary)*
unary      = "+"? primary | "-"? primary | "*"? unary | "&"? unary;
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
            add_local_var(token, type());
            cur = cur->next = new_node_lvar(find_lvar(token));
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
        }
        func->body = new_node(ND_BLOCK, NULL, NULL);
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
        node = new_node(ND_RETURN, expr(), NULL);
        expect_op(";");
    } else if (at_keyword(TK_IF)) {
        expect_keyword(TK_IF);
        expect_op("(");
        node = new_node(ND_IF, expr(), NULL);
        expect_op(")");
        node->rhs = stmt();
        if (at_keyword(TK_ELSE)) {
            expect_keyword(TK_ELSE);
            node = new_node(ND_ELSE, node, stmt());
        }
    } else if (at_keyword(TK_WHILE)) {
        expect_keyword(TK_WHILE);
        expect_op("(");
        node = new_node(ND_WHILE, expr(), NULL);
        expect_op(")");
        node->rhs = stmt();
    } else if (at_keyword(TK_FOR)) {
        expect_keyword(TK_FOR);
        expect_op("(");
        struct Node *ret[3];
        for (int i = 0; i < 3; i++) {
            ret[i] = NULL;
            if (i < 2) {
                if (!consume(";")) {
                    ret[i] = expr();
                    expect_op(";");
                }
            } else {
                if (!consume(")")) {
                    ret[i] = expr();
                    expect_op(")");
                }
            }
        }
        struct Node *lhs = new_node(ND_DUMMY, ret[0], ret[1]);
        struct Node *rhs = new_node(ND_DUMMY, ret[2], stmt());
        node = new_node(ND_FOR, lhs, rhs);
    } else if (consume("{")) {
        struct Node head = {};
        struct Node *cur = &head;
        while (!consume("}")) {
            cur = cur->next = stmt();
        }
        node = new_node(ND_BLOCK, NULL, NULL);
        node->body = head.next;
    } else if (at_keyword(TK_MOLD)) {
        add_local_var(token, type());
        expect_ident();
    } else {
        if (!consume(";"))
            node = expr();
        else
            node = new_node(ND_BLOCK, NULL, NULL);
    }
    return node;
}

struct Node *expr() {
    return assign();
}

struct Node *assign() {
    struct Node *node = equality();
    if (consume("=")) {
        node = new_node(ND_ASSIGN, node, assign());
    }
    return node;
}

struct Node *equality() {
    struct Node *node = relational();
    for (;;) {
        if (consume("=="))
            node = new_node(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_node(ND_NE, node, relational());
        else
            return node;
    }
}

struct Node *relational() {
    struct Node *node = add();
    for (;;) {
        if (consume("<"))
            node = new_node(ND_LT, node, add());
        else if (consume("<="))
            node = new_node(ND_LE, node, add());
        else if (consume(">"))
            node = new_node(ND_LT, add(), node);
        else if (consume(">="))
            node = new_node(ND_LE, add(), node);
        else
            return node;
    }
}

struct Node *add() {
    struct Node *node = mul();

    for (;;) {
        if (consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if (consume("-"))
            node = new_node(ND_SUB, node, mul());
        else
            return node;
    }
}

struct Node *mul() {
    struct Node *node = unary();
    for (;;) {
        if (consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

struct Node *unary() {
    if (consume("+")) {
        return primary();
    }
    if (consume("-")) {
        return new_node(ND_SUB, new_node_num(0), primary());
    }
    if (consume("*")) {
        return new_node(ND_DEREF, unary(), NULL);
    }
    if (consume("&")) {
        return new_node(ND_ADDR, unary(), NULL);
    }
    return primary();
}

struct Node *primary() {
    if (consume("(")) {
        struct Node *node = expr();
        expect_op(")");
        return node;
    } else if (at_ident()) {
        // function call
        if (equal_op(token->next, "(")) {
            struct Node *node = new_node(ND_FUNCALL, NULL, NULL);
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

        struct LVar *lvar = find_lvar(token);
        if (lvar == NULL) {
            error("%sは定義されていません", lvar->name);
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
    struct Type *ty = calloc(1, sizeof(struct Type));
    ty->ty = INT;
    while (consume("*")) {
        struct Type *cur = calloc(1, sizeof(struct Type));
        cur->ty = PTR;
        cur->ptr_to = ty;
        ty = cur;
    }
    return ty;
}