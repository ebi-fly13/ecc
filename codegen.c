#include "ecc.h"

int label = 100;

void prologue() {
    int offset = 0;
    if (locals) offset = locals->offset;
    // 変数の領域を確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", offset);
}

void epilogue() {
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void gen_lval(struct Node *node) {
    if (node->kind != ND_LVAR) error("代入の左辺値が変数でありません");

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->offset);
    printf("  push rax\n");
}

void gen(struct Node *node) {
    if (node->kind == ND_RETURN) {
        gen(node->lhs);
        printf("  pop rax\n");
        epilogue();
        return;
    }

    if (node->kind == ND_IF) {
        int number = label++;
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", number);
        gen(node->rhs);
        printf(".Lend%d:\n", number);
        return;
    }

    if (node->kind == ND_ELSE) {
        int number = label++;
        if (node->lhs->kind != ND_IF) {
            error("if節のないelseです");
        }
        gen(node->lhs->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lelse%d\n", number);
        gen(node->lhs->rhs);
        printf("  jmp  .Lend%d\n", number);
        printf(".Lelse%d:\n", number);
        gen(node->rhs);
        printf(".Lend%d:\n", number);
        return;
    }

    if (node->kind == ND_WHILE) {
        int number = label++;
        printf(".Lbegin%d:\n", number);
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", number);
        gen(node->rhs);
        printf("  jmp  .Lbegin%d\n", number);
        printf(".Lend%d:\n", number);
        return;
    }

    if (node->kind == ND_NUM) {
        printf("  push %d\n", node->val);
        return;
    }

    if (node->kind == ND_LVAR) {
        gen_lval(node);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }

    if (node->kind == ND_ASSIGN) {
        gen_lval(node->lhs);
        gen(node->rhs);

        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
        case ND_ADD:
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            printf("  sub rax, rdi\n");
            break;
        case ND_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case ND_EQ:
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_NE:
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_LT:
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_LE:
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
    }

    printf("  push rax\n");
}
