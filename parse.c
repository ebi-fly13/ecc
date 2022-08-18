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

struct Node *new_node_lvar(int offset) {
    struct Node *node = calloc(1, sizeof(struct Node));
    node->kind = ND_LVAR;
    node->offset = offset;
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

void add_local_var(struct Token *tok) {
    struct LVar *lvar = find_lvar(tok);
    if (lvar) {
        return;
    } else {
        lvar = calloc(1, sizeof(struct LVar));
        lvar->next = locals;
        lvar->name = tok->str;
        lvar->len = tok->len;
        if (locals == NULL)
            lvar->offset = 8;
        else
            lvar->offset = locals->offset + 8;
        locals = lvar;
        return;
    }
}

/*
program    = stmt*
stmt       = expr ";" | "return" expr ";" | "if" "(" expr ")" stmt ( "else" stmt
)? | "while" "(" expr ")" stmt | "for" "(" expr? ";" expr? ";" expr? ")" stmt |
"{" stmt* "}"
expr       = assign
assign     = equality ("=" assign)?
equality   = relational ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add        = mul ("+" mul | "-" mul)*
mul        = unary ("*" unary | "/" unary)* unary      = ("+" | "-")? primary
primary    = num | ident ( "(" (expr ("," expr)*)? ")" )? | "(" expr ")"
*/

void program();
struct Node *stmt();
struct Node *expr();
struct Node *assign();
struct Node *equality();
struct Node *relational();
struct Node *add();
struct Node *mul();
struct Node *unary();
struct Node *primary();

struct Node *code[100];
void program() {
    int i = 0;
    while (!at_eof()) {
        code[i++] = stmt();
    }
    code[i] = NULL;
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
    return primary();
}

struct Node *primary() {
    if (consume("(")) {
        struct Node *node = expr();
        expect_op(")");
        return node;
    } else if (at_ident()) {

        // function call
        if(equal_op(token->next, "(")) {
            struct Node *node = new_node(ND_FUNCALL, NULL, NULL);
            node->funcname = strndup(token->str, token->len);
            expect_ident();
            expect_op("(");
            struct Node head = {};
            struct Node *cur = &head;
            while(!consume(")")) {
                if(cur != &head) expect_op(",");
                cur = cur->next = expr(); 
            }
            node->args = head.next;
            return node;
        }

        add_local_var(token);
        struct Node *node = new_node_lvar(find_lvar(token)->offset);
        expect_ident();
        return node;
    } else if (at_number()) {
        return new_node_num(expect_number());
    } else {
        error("parseエラー");
    }
}
