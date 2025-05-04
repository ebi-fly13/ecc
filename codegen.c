#include "ecc.h"

int label = 100;
int depth = 0;

static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void prologue(int stack_size) {
    // 変数の領域を確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", stack_size);
}

void epilogue() {
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void push() {
    printf("  push rax\n");
    depth++;
}

void pop(char *reg) {
    printf("  pop %s\n", reg);
    depth--;
}

void load(struct Type *ty) {
    if (ty->ty == TY_ARRAY) {
        return;
    }
    if (ty->size == 1)
        printf("  movsbq rax, [rax]\n");
    else if (ty->size == 2)
        printf("  movswq rax, [rax]\n");
    else if (ty->size == 4)
        printf("  movsxd rax, [rax]\n");
    else
        printf("  mov rax, [rax]\n");
    return;
}

void store(struct Type *ty) {
    if (ty->size == 1)
        printf("  mov [rdi], al\n");
    else if (ty->size == 2)
        printf("  mov [rdi], ax\n");
    else if (ty->size == 4)
        printf("  mov [rdi], eax\n");
    else
        printf("  mov [rdi], rax\n");
    return;
}

void gen(struct Node *);

void gen_lval(struct Node *node) {
    switch (node->kind) {
        case ND_DEREF:
            gen(node->lhs);
            return;
        case ND_LVAR:
            printf("  mov rax, rbp\n");
            printf("  sub rax, %d\n", node->obj->offset);
            return;
        case ND_GVAR:
            printf("  lea rax, [rip + %s]\n", node->obj->name);
            return;
        case ND_MEMBER:
            gen_lval(node->lhs);
            printf("  add rax, %d\n", node->member->offset);
            return;
    }
    error("代入の左辺値が変数でありません");
}

void gen(struct Node *node) {
    if (node == NULL) {
        return;
    }

    if (node->kind == ND_RETURN) {
        gen(node->lhs);
        epilogue();
        return;
    }

    if (node->kind == ND_IF) {
        int number = label++;
        gen(node->cond);
        printf("  cmp rax, 0\n");
        printf("  je  .Lelse%d\n", number);
        gen(node->then);
        printf("  jmp .Lend%d\n", number);
        printf(".Lelse%d:\n", number);
        if (node->els) gen(node->els);
        printf(".Lend%d:\n", number);
        return;
    }

    if (node->kind == ND_WHILE) {
        int number = label++;
        printf(".Lbegin%d:\n", number);
        gen(node->cond);
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", number);
        gen(node->then);
        printf("  jmp  .Lbegin%d\n", number);
        printf(".Lend%d:\n", number);
        return;
    }

    if (node->kind == ND_FOR) {
        int number = label++;
        gen(node->init);
        printf(".Lbegin%d:\n", number);
        gen(node->cond);
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", number);
        gen(node->then);
        gen(node->inc);
        printf("  jmp  .Lbegin%d\n", number);
        printf(".Lend%d:\n", number);
        return;
    }

    if(node->kind == ND_COMMA) {
        gen(node->lhs);
        gen(node->rhs);
        return;
    }

    if (node->kind == ND_BLOCK || node->kind == ND_STMT_EXPR) {
        for (struct Node *block = node->body; block; block = block->next) {
            gen(block);
        }
        return;
    }

    if (node->kind == ND_FUNCALL) {
        int nargs = 0;
        for (struct Node *arg = node->args; arg; arg = arg->next) {
            gen(arg);
            push();
            nargs++;
        }

        for (int i = nargs - 1; i >= 0; i--) {
            pop(argreg64[i]);
        }

        printf("  mov rax, 0\n");

        // 16-byte align rsp
        printf("  push rsp\n");
        printf("  push [rsp]\n");
        printf("  and rsp, -0x10\n");

        printf("  call %s\n", node->obj->name);

        // get original rsp
        printf("  add rsp, 8\n");
        printf("  mov rsp, [rsp]\n");

        return;
    }

    if (node->kind == ND_MEMBER) {
        gen_lval(node);
        load(node->member->ty);
        return;
    }

    if (node->kind == ND_NUM) {
        printf("  mov rax, %ld\n", node->val);
        return;
    }

    if (node->kind == ND_LVAR || node->kind == ND_GVAR) {
        gen_lval(node);
        load(node->ty);
        return;
    }

    if (node->kind == ND_ASSIGN) {
        gen_lval(node->lhs);
        push();
        gen(node->rhs);
        pop("rdi");

        store(node->ty);
        return;
    }

    if (node->kind == ND_ADDR) {
        gen_lval(node->lhs);
        return;
    }

    if (node->kind == ND_DEREF) {
        add_type(node);
        gen(node->lhs);
        load(node->ty);
        return;
    }

    gen(node->lhs);
    push();
    gen(node->rhs);
    push();

    pop("rdi");
    pop("rax");

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
}

void codegen() {
    printf(".intel_syntax noprefix\n");

    for (struct Object *obj = globals; obj; obj = obj->next) {
        assert(obj->is_global_variable);
        printf("  .data\n");
        printf("  .globl %s\n", obj->name);
        printf("%s:\n", obj->name);
        if (obj->init_data) {
            for (int i = 0; i < obj->ty->size; i++) {
                printf("  .byte %d\n", obj->init_data[i]);
            }
        } else {
            printf("  .zero %ld\n", obj->ty->size);
        }
    }

    for (struct Object *obj = functions; obj; obj = obj->next) {
        if (obj->body == NULL) continue;
        assert(obj->is_function);
        printf("  .text\n");
        if (obj->is_static) {
            printf("  .local %s\n", obj->name);
        } else {
            printf("  .globl %s\n", obj->name);
        }
        printf("%s:\n", obj->name);

        prologue(obj->stack_size);

        int i = 0;
        for (struct Node *arg = obj->args; arg; arg = arg->next) {
            gen_lval(arg);
            if (arg->ty->size == 1)
                printf("  mov [rax], %s\n", argreg8[i++]);
            else if (arg->ty->size == 2)
                printf("  mov [rax], %s\n", argreg16[i++]);
            else if (arg->ty->size == 4)
                printf("  mov [rax], %s\n", argreg32[i++]);
            else
                printf("  mov [rax], %s\n", argreg64[i++]);
        }

        gen(obj->body);

        epilogue();
    }
    if (depth != 0) {
        printf("%d\n", depth);
    }
    assert(depth == 0);
    return;
}