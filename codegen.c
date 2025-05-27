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

int get_type_id(struct Type *ty) {
    switch (ty->ty) {
        case TY_CHAR:
            return 0;
        case TY_SHORT:
            return 1;
        case TY_INT:
            return 2;
        case TY_LONG:
            return 3;
        default:
            return 4;
    }
}

static char i32i8[] = "movsbl eax, al";
static char i32i16[] = "movswl eax, ax";
static char i32i64[] = "movsxd rax, eax";

static char *cast_table[][10] = {
    {NULL, NULL, NULL, i32i64},     // i8
    {i32i8, NULL, NULL, i32i64},    // i16
    {i32i8, i32i16, NULL, i32i64},  // i32
    {i32i8, i32i16, NULL, NULL},    // i64
};

static void cast(struct Type *from, struct Type *to) {
    if (to->ty == TY_VOID) {
        return;
    }
    int from_id = get_type_id(from);
    int to_id = get_type_id(to);
    if (cast_table[from_id][to_id]) {
        printf("  %s\n", cast_table[from_id][to_id]);
    }
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

    if (node->kind == ND_ASSIGN_EXPR) {
        gen(node->lhs);
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
        printf("%s:\n", node->continue_label);
        gen(node->cond);
        printf("  cmp rax, 0\n");
        printf("  je  %s\n", node->break_label);
        gen(node->then);
        printf("  jmp %s\n", node->continue_label);
        printf("%s:\n", node->break_label);
        return;
    }

    if (node->kind == ND_FOR) {
        int number = label++;
        gen(node->init);
        printf(".Lbegin%d:\n", number);
        gen(node->cond);
        printf("  cmp rax, 0\n");
        printf("  je  %s\n", node->break_label);
        gen(node->then);
        printf("%s:\n", node->continue_label);
        gen(node->inc);
        printf("  jmp  .Lbegin%d\n", number);
        printf("%s:\n", node->break_label);
        return;
    }

    if (node->kind == ND_GOTO) {
        printf("  jmp %s\n", node->unique_label);
        return;
    }

    if (node->kind == ND_LABEL) {
        printf("%s:\n", node->unique_label);
        gen(node->body);
        return;
    }

    if (node->kind == ND_BREAK) {
        printf("  jmp %s\n", node->break_label);
        return;
    }

    if (node->kind == ND_CONTINUE) {
        printf("  jmp %s\n", node->continue_label);
        return;
    }

    if (node->kind == ND_SWITCH) {
        gen(node->cond);
        for (struct Node *case_node = node->next_case; case_node != NULL;
             case_node = case_node->next_case) {
            char *reg = (node->cond->ty->size == 8) ? "rax" : "eax";
            printf("  cmp %s, %ld\n", reg, case_node->val);
            printf("  je %s\n", case_node->label);
        }
        if (node->default_case != NULL) {
            printf("  jmp %s\n", node->default_case->label);
        }
        printf("  jmp %s\n", node->break_label);

        gen(node->then);
        printf("%s:\n", node->break_label);
        return;
    }

    if (node->kind == ND_CASE) {
        printf("%s:\n", node->label);
        gen(node->lhs);
        return;
    }

    if (node->kind == ND_COMMA) {
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

    if (node->kind == ND_NOT) {
        gen(node->lhs);
        printf("  cmp rax, 0\n");
        printf("  sete al\n");
        printf("  movzx rax, al\n");
        return;
    }

    if (node->kind == ND_BITNOT) {
        gen(node->lhs);
        printf("  not rax\n");
        return;
    }

    if (node->kind == ND_CAST) {
        gen(node->lhs);
        cast(node->lhs->ty, node->ty);
        return;
    }

    if (node->kind == ND_LOGOR) {
        int number = label++;
        gen(node->lhs);
        printf("  cmp rax, 0\n");
        printf("  jne .L.true.%d\n", number);
        gen(node->rhs);
        printf("  cmp rax, 0\n");
        printf("  jne .L.true.%d\n", number);
        printf("  mov rax, 0\n");
        printf("  jmp .L.end.%d\n", number);
        printf(".L.true.%d:\n", number);
        printf("  mov rax, 1\n");
        printf(".L.end.%d:\n", number);
        return;
    }
    if (node->kind == ND_LOGAND) {
        int number = label++;
        gen(node->lhs);
        printf("  cmp rax, 0\n");
        printf("  je .L.false.%d\n", number);
        gen(node->rhs);
        printf("  cmp rax, 0\n");
        printf("  je .L.false.%d\n", number);
        printf("  mov rax, 1\n");
        printf("  jmp .L.end.%d\n", number);
        printf(".L.false.%d:\n", number);
        printf("  mov rax, 0\n");
        printf(".L.end.%d:\n", number);
        return;
    }

    if (node->kind == ND_COND) {
        int number = label++;
        gen(node->cond);
        printf("  cmp rax, 0\n");
        printf("  je .L.false.%d\n", number);
        gen(node->then);
        printf("  jmp .L.end.%d\n", number);
        printf(".L.false.%d:\n", number);
        gen(node->els);
        printf(".L.end.%d:\n", number);
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
        case ND_MOD:
            if (node->lhs->ty->size == 8) {
                printf("  cqo\n");
            } else {
                printf("  cdq\n");
            }
            printf("  cqo\n");
            printf("  idiv rdi\n");

            if (node->kind == ND_MOD) {
                printf("  mov rax, rdx\n");
            }
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
        case ND_BITOR:
            printf("  or rax, rdi\n");
            break;
        case ND_BITXOR:
            printf("  xor rax, rdi\n");
            break;
        case ND_BITAND:
            printf("  and rax, rdi\n");
            break;
        case ND_SHL: {
            printf("  mov rcx, rdi\n");
            printf("  sal rax, cl\n");
            break;
        }
        case ND_SHR: {
            printf("  mov rcx, rdi\n");
            printf("  sar rax, cl\n");
            break;
        }
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
            printf("  .zero %d\n", obj->ty->size);
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