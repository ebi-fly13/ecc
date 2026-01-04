#include "ecc.h"

int label = 100;
int depth = 0;

static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static FILE *output_file;

void prologue(int stack_size) {
    // 変数の領域を確保する
    fprintf(output_file, "  push rbp\n");
    fprintf(output_file, "  mov rbp, rsp\n");
    fprintf(output_file, "  sub rsp, %d\n", stack_size);
}

void epilogue() {
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    fprintf(output_file, "  mov rsp, rbp\n");
    fprintf(output_file, "  pop rbp\n");
    fprintf(output_file, "  ret\n");
}

void push() {
    fprintf(output_file, "  push rax\n");
    depth++;
}

void pop(char *reg) {
    fprintf(output_file, "  pop %s\n", reg);
    depth--;
}

void load(struct Type *ty) {
    if (ty->ty == TY_ARRAY || ty->ty == TY_STRUCT || ty->ty == TY_UNION ||
        ty->ty == TY_FUNC) {
        return;
    }

    if (ty->ty == TY_FLOAT) {
        fprintf(output_file, "  xorps xmm0, xmm0\n");
        fprintf(output_file, "  movss xmm0, dword ptr [rax]\n");
        return;
    } else if (ty->ty == TY_DOUBLE) {
        fprintf(output_file, "  xorpd xmm0, xmm0\n");
        fprintf(output_file, "  movsd xmm0, qword ptr [rax]\n");
        return;
    }

    char *instruction = ty->is_unsigned ? "movz" : "movs";

    if (ty->size == 1)
        fprintf(output_file, "  %sx rax, byte ptr [rax]\n", instruction);
    else if (ty->size == 2)
        fprintf(output_file, "  %sx rax, word ptr [rax]\n", instruction);
    else if (ty->size == 4) {
        if (ty->is_unsigned) {
            fprintf(output_file, "  mov rax, [rax]\n");
        } else {
            fprintf(output_file, "  movsxd rax, dword ptr [rax]\n");
        }
    } else
        fprintf(output_file, "  mov rax, [rax]\n");
    return;
}

void store(struct Type *ty) {
    pop("rdi");

    if (ty->ty == TY_STRUCT || ty->ty == TY_UNION) {
        for (int i = 0; i < ty->size; i++) {
            fprintf(output_file, "  mov r8b, [rax + %d]\n", i);
            fprintf(output_file, "  mov [rdi + %d], r8b\n", i);
        }
        return;
    }

    if (ty->ty == TY_FLOAT) {
        fprintf(output_file, "  movss dword ptr [rdi], xmm0\n");
        return;
    }

    if (ty->ty == TY_DOUBLE) {
        fprintf(output_file, "  movsd qword ptr [rdi], xmm0\n");
        return;
    }

    if (ty->ty == TY_BOOL) {
        fprintf(output_file, "  cmp eax, 0\n");
        fprintf(output_file, "  setne al\n");
        fprintf(output_file, "  mov [rdi], al\n");
        return;
    }

    if (ty->size == 1)
        fprintf(output_file, "  mov [rdi], al\n");
    else if (ty->size == 2)
        fprintf(output_file, "  mov [rdi], ax\n");
    else if (ty->size == 4)
        fprintf(output_file, "  mov [rdi], eax\n");
    else
        fprintf(output_file, "  mov [rdi], rax\n");
    return;
}

void gen(struct Node *);

void gen_lval(struct Node *node) {
    switch (node->kind) {
    case ND_DEREF:
        gen(node->lhs);
        return;
    case ND_VAR:
        if (node->obj->is_global_variable) {
            if (node->ty->ty == TY_FUNC) {
                if (node->obj->is_definition) {
                    fprintf(output_file, "  lea rax, [rip + %s]\n",
                            node->obj->name);
                } else {
                    fprintf(output_file, "  mov rax, [rip + %s@GOTPCREL]\n",
                            node->obj->name);
                }
            } else {
                fprintf(output_file, "  lea rax, [rip + %s]\n",
                        node->obj->name);
            }
        } else {
            fprintf(output_file, "  lea rax, [rbp - %d]\n", node->obj->offset);
        }
        return;
    case ND_MEMBER:
        gen_lval(node->lhs);
        fprintf(output_file, "  add rax, %d\n", node->member->offset);
        return;
    case ND_COMMA:
        gen(node->lhs);
        gen_lval(node->rhs);
        return;
    }
    error("代入の左辺値が変数でありません");
}

enum {
    I8,
    I16,
    I32,
    I64,
    UI8,
    UI16,
    UI32,
    UI64,
    F32,
    F64,
};

int get_type_id(struct Type *ty) {
    assert(ty != NULL);
    switch (ty->ty) {
    case TY_CHAR:
        return ty->is_unsigned ? UI8 : I8;
    case TY_SHORT:
        return ty->is_unsigned ? UI16 : I16;
    case TY_INT:
        return ty->is_unsigned ? UI32 : I32;
    case TY_LONG:
        return ty->is_unsigned ? UI64 : I64;
    case TY_FLOAT:
        return F32;
    case TY_DOUBLE:
        return F64;
    default:
        return UI64;
    }
}

static char i32i8[] = "movsx eax, al";
static char i32i16[] = "movsx eax, ax";
static char i32i64[] = "movsxd rax, eax";
static char i32u8[] = "movzx eax, al";
static char i32u16[] = "movzx eax, ax";
static char i32f32[] = "cvtsi2ss xmm0, eax";
static char i32f64[] = "cvtsi2sd xmm0, eax";

static char u32i64[] = "mov eax, eax";
static char u32f32[] = "mov eax, eax; cvtsi2ss xmm0, rax";
static char u32f64[] = "mov eax, eax; cvtsi2sd xmm0, rax";

static char i64f32[] = "cvtsi2ss xmm0, rax";
static char i64f64[] = "cvtsi2sd xmm0, rax";

static char u64f32[] = "cvtsi2ss xmm0, rax";

static char u64f64[] =
    "test rax, rax; js 1f; "
    "pxor xmm0, xmm0; cvtsi2sd xmm0, rax; jmp 2f; "
    "1: mov rdi, rax; and eax, 1; pxor xmm0, xmm0; shr rdi, 1; "
    "or rdi, rax; cvtsi2sd xmm0, rdi; addsd xmm0, xmm0; 2:";

static char f32i8[] = "cvttss2si eax, xmm0; movsx eax, al";
static char f32u8[] = "cvttss2si eax, xmm0; movzx eax, al";
static char f32i16[] = "cvttss2si eax, xmm0; movsx eax, ax";
static char f32u16[] = "cvttss2si eax, xmm0; movzx eax, ax";
static char f32i32[] = "cvttss2si eax, xmm0";
static char f32u32[] = "cvttss2si rax, xmm0";
static char f32i64[] = "cvttss2si rax, xmm0";
static char f32u64[] = "cvttss2si rax, xmm0";
static char f32f64[] = "cvtss2sd xmm0, xmm0";

static char f64i8[] = "cvttsd2si eax, xmm0; movsx eax, al";
static char f64u8[] = "cvttsd2si eax, xmm0; movzx eax, al";
static char f64i16[] = "cvttsd2si eax, xmm0; movsx eax, ax";
static char f64u16[] = "cvttsd2si eax, xmm0; movzx eax, ax";
static char f64i32[] = "cvttsd2si eax, xmm0";
static char f64u32[] = "cvttsd2si rax, xmm0";
static char f64f32[] = "cvtsd2ss xmm0, xmm0";
static char f64i64[] = "cvttsd2si rax, xmm0";
static char f64u64[] = "cvttsd2si rax, xmm0";

static char *cast_table[][10] = {
    {NULL, NULL, NULL, i32i64, i32u8, i32u16, NULL, i32i64, i32f32,
     i32f64}, // i8
    {i32i8, NULL, NULL, i32i64, i32u8, i32u16, NULL, i32i64, i32f32,
     i32f64}, // i16
    {i32i8, i32i16, NULL, i32i64, i32u8, i32u16, NULL, i32i64, i32f32,
     i32f64}, // i32
    {i32i8, i32i16, NULL, NULL, i32u8, i32u16, NULL, NULL, i64f32,
     i64f64}, // i64
    {i32i8, NULL, NULL, i32i64, NULL, NULL, NULL, i32i64, i32f32, i32f64}, // u8
    {i32i8, i32i16, NULL, i32i64, i32u8, NULL, NULL, i32i64, i32f32,
     i32f64}, // u16
    {i32i8, i32i16, NULL, u32i64, i32u8, i32u16, NULL, u32i64, u32f32,
     u32f64}, // u32
    {i32i8, i32i16, NULL, NULL, i32u8, i32u16, NULL, NULL, u64f32,
     u64f64}, // u64
    {f32i8, f32i16, f32i32, f32i64, f32u8, f32u16, f32u32, f32u64, NULL,
     f32f64}, // f32
    {f64i8, f64i16, f64i32, f64i64, f64u8, f64u16, f64u32, f64u64, f64f32,
     NULL}, // f64
};

static void cast(struct Type *from, struct Type *to) {
    if (to->ty == TY_VOID) {
        return;
    }

    if (to->ty == TY_BOOL) {
        if (is_integer(from) && from->size <= 4) {
            fprintf(output_file, "  cmp eax, 0\n");
        } else {
            fprintf(output_file, "  cmp rax, 0\n");
        }
        fprintf(output_file, "  setne al\n");
        fprintf(output_file, "  movzx eax, al\n");
        return;
    }
    int from_id = get_type_id(from);
    int to_id = get_type_id(to);
    if (cast_table[from_id][to_id]) {
        fprintf(output_file, "  %s\n", cast_table[from_id][to_id]);
    }
}

void gen(struct Node *node) {
    if (node == NULL) {
        return;
    }

    if (node->kind == ND_DUMMY) {
        return;
    }

    if (node->kind == ND_MEMZERO) {
        fprintf(output_file, "  mov rcx, %d\n", node->obj->ty->size);
        fprintf(output_file, "  lea rdi, [rbp - %d]\n", node->obj->offset);
        fprintf(output_file, "  mov al, 0\n");
        fprintf(output_file, "  rep stosb\n");
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
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  je  .Lelse%d\n", number);
        gen(node->then);
        fprintf(output_file, "  jmp .Lend%d\n", number);
        fprintf(output_file, ".Lelse%d:\n", number);
        if (node->els)
            gen(node->els);
        fprintf(output_file, ".Lend%d:\n", number);
        return;
    }

    if (node->kind == ND_WHILE) {
        fprintf(output_file, "%s:\n", node->continue_label);
        gen(node->cond);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  je  %s\n", node->break_label);
        gen(node->then);
        fprintf(output_file, "  jmp %s\n", node->continue_label);
        fprintf(output_file, "%s:\n", node->break_label);
        return;
    }

    if (node->kind == ND_FOR) {
        int number = label++;
        gen(node->init);
        fprintf(output_file, ".Lbegin%d:\n", number);
        gen(node->cond);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  je  %s\n", node->break_label);
        gen(node->then);
        fprintf(output_file, "%s:\n", node->continue_label);
        gen(node->inc);
        fprintf(output_file, "  jmp  .Lbegin%d\n", number);
        fprintf(output_file, "%s:\n", node->break_label);
        return;
    }

    if (node->kind == ND_GOTO) {
        fprintf(output_file, "  jmp %s\n", node->unique_label);
        return;
    }

    if (node->kind == ND_LABEL) {
        fprintf(output_file, "%s:\n", node->unique_label);
        gen(node->body);
        return;
    }

    if (node->kind == ND_BREAK) {
        fprintf(output_file, "  jmp %s\n", node->break_label);
        return;
    }

    if (node->kind == ND_CONTINUE) {
        fprintf(output_file, "  jmp %s\n", node->continue_label);
        return;
    }

    if (node->kind == ND_SWITCH) {
        gen(node->cond);
        for (struct Node *case_node = node->next_case; case_node != NULL;
             case_node = case_node->next_case) {
            char *reg = (node->cond->ty->size == 8) ? "rax" : "eax";
            fprintf(output_file, "  cmp %s, %ld\n", reg, case_node->val);
            fprintf(output_file, "  je %s\n", case_node->label);
        }
        if (node->default_case != NULL) {
            fprintf(output_file, "  jmp %s\n", node->default_case->label);
        }
        fprintf(output_file, "  jmp %s\n", node->break_label);

        gen(node->then);
        fprintf(output_file, "%s:\n", node->break_label);
        return;
    }

    if (node->kind == ND_CASE) {
        fprintf(output_file, "%s:\n", node->label);
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
        assert(node->ty != NULL);
        int nargs = 0;
        for (struct Node *arg = node->args; arg; arg = arg->next) {
            gen(arg);
            push();
            nargs++;
        }

        gen(node->lhs);

        assert(nargs <= 6);
        for (int i = nargs - 1; i >= 0; i--) {
            pop(argreg64[i]);
        }

        // 16-byte align rsp
        fprintf(output_file, "  push rsp\n");
        fprintf(output_file, "  push [rsp]\n");
        fprintf(output_file, "  and rsp, -0x10\n");

        fprintf(output_file, "  call rax\n");

        // get original rsp
        fprintf(output_file, "  add rsp, 8\n");
        fprintf(output_file, "  mov rsp, [rsp]\n");

        switch (node->ty->ty) {
        case TY_BOOL:
        case TY_CHAR:
            if (node->ty->is_unsigned) {
                fprintf(output_file, "  movsx eax, al\n");
            } else {
                fprintf(output_file, "  movzx eax, al\n");
            }
            break;
        case TY_SHORT:
            if (node->ty->is_unsigned) {
                fprintf(output_file, "  movzx eax, ax\n");
            } else {
                fprintf(output_file, "  movsx eax, ax\n");
            }
            break;
        }

        return;
    }

    if (node->kind == ND_MEMBER) {
        gen_lval(node);
        load(node->member->ty);
        return;
    }

    if (node->kind == ND_NUM) {
        union {
            float f32;
            double f64;
            uint32_t u32;
            uint64_t u64;
        } tmp;
        switch (node->ty->ty) {
        case TY_FLOAT:
            tmp.f32 = node->fval;
            fprintf(output_file, "  mov eax, %u # float %f\n", tmp.u32,
                    node->fval);
            fprintf(output_file, "  movq xmm0, rax\n");
            return;
        case TY_DOUBLE:
            tmp.f64 = node->fval;
            fprintf(output_file, "  mov rax, %lu # double %f\n", tmp.u64,
                    node->fval);
            fprintf(output_file, "  movq xmm0, rax\n");
            return;
        default:
            fprintf(output_file, "  mov rax, %ld # integer %ld\n", node->val,
                    node->val);
            return;
        }
    }

    if (node->kind == ND_VAR) {
        gen_lval(node);
        load(node->ty);
        return;
    }

    if (node->kind == ND_ASSIGN) {
        add_type(node);

        gen_lval(node->lhs);
        push();
        gen(node->rhs);

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
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  sete al\n");
        fprintf(output_file, "  movzx rax, al\n");
        return;
    }

    if (node->kind == ND_BITNOT) {
        gen(node->lhs);
        fprintf(output_file, "  not rax\n");
        return;
    }

    if (node->kind == ND_CAST) {
        gen(node->lhs);
        if (node->lhs->ty == NULL || node->ty == NULL) {
            error_node(node, "ND_CAST ty NULL");
        }
        cast(node->lhs->ty, node->ty);
        return;
    }

    if (node->kind == ND_LOGOR) {
        int number = label++;
        gen(node->lhs);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  jne .L.true.%d\n", number);
        gen(node->rhs);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  jne .L.true.%d\n", number);
        fprintf(output_file, "  mov rax, 0\n");
        fprintf(output_file, "  jmp .L.end.%d\n", number);
        fprintf(output_file, ".L.true.%d:\n", number);
        fprintf(output_file, "  mov rax, 1\n");
        fprintf(output_file, ".L.end.%d:\n", number);
        return;
    }
    if (node->kind == ND_LOGAND) {
        int number = label++;
        gen(node->lhs);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  je .L.false.%d\n", number);
        gen(node->rhs);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  je .L.false.%d\n", number);
        fprintf(output_file, "  mov rax, 1\n");
        fprintf(output_file, "  jmp .L.end.%d\n", number);
        fprintf(output_file, ".L.false.%d:\n", number);
        fprintf(output_file, "  mov rax, 0\n");
        fprintf(output_file, ".L.end.%d:\n", number);
        return;
    }

    if (node->kind == ND_COND) {
        int number = label++;
        gen(node->cond);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  je .L.false.%d\n", number);
        gen(node->then);
        fprintf(output_file, "  jmp .L.end.%d\n", number);
        fprintf(output_file, ".L.false.%d:\n", number);
        gen(node->els);
        fprintf(output_file, ".L.end.%d:\n", number);
        return;
    }

    if (node->kind == ND_DO) {
        int number = label++;
        fprintf(output_file, ".L.begin.%d:\n", number);
        gen(node->then);
        fprintf(output_file, "%s:\n", node->continue_label);
        gen(node->cond);
        fprintf(output_file, "  cmp rax, 0\n");
        fprintf(output_file, "  jne .L.begin.%d\n", number);
        fprintf(output_file, "%s:\n", node->break_label);
        return;
    }

    gen(node->lhs);
    push();
    gen(node->rhs);
    push();

    pop("rdi");
    pop("rax");

    add_type(node);

    char *ax, *di, *dx;

    if (node->lhs->ty->ty == TY_LONG || node->lhs->ty->ptr_to != NULL) {
        ax = "rax";
        di = "rdi";
        dx = "rdx";
    } else {
        ax = "eax";
        di = "edi";
        dx = "edx";
    }

    switch (node->kind) {
    case ND_ADD:
        fprintf(output_file, "  add %s, %s\n", ax, di);
        break;
    case ND_SUB:
        fprintf(output_file, "  sub %s, %s\n", ax, di);
        break;
    case ND_MUL:
        fprintf(output_file, "  imul %s, %s\n", ax, di);
        break;
    case ND_DIV:
    case ND_MOD:
        if (node->ty->is_unsigned) {
            fprintf(output_file, "  mov %s, 0\n", dx);
            fprintf(output_file, "  div %s\n", di);
        } else {
            if (node->lhs->ty->size == 8) {
                fprintf(output_file, "  cqo\n");
            } else {
                fprintf(output_file, "  cdq\n");
            }
            fprintf(output_file, "  idiv %s\n", di);
        }

        if (node->kind == ND_MOD) {
            fprintf(output_file, "  mov rax, rdx\n");
        }
        break;
    case ND_EQ:
        fprintf(output_file, "  cmp %s, %s\n", ax, di);
        fprintf(output_file, "  sete al\n");
        fprintf(output_file, "  movzb rax, al\n");
        break;
    case ND_NE:
        fprintf(output_file, "  cmp %s, %s\n", ax, di);
        fprintf(output_file, "  setne al\n");
        fprintf(output_file, "  movzb rax, al\n");
        break;
    case ND_LT:
        fprintf(output_file, "  cmp %s, %s\n", ax, di);
        if (node->lhs->ty->is_unsigned) {
            fprintf(output_file, "  setb al\n");
        } else {
            fprintf(output_file, "  setl al\n");
        }
        fprintf(output_file, "  movzb rax, al\n");
        break;
    case ND_LE:
        fprintf(output_file, "  cmp %s, %s\n", ax, di);
        if (node->lhs->ty->is_unsigned) {
            fprintf(output_file, "  setbe al\n");
        } else {
            fprintf(output_file, "  setle al\n");
        }
        fprintf(output_file, "  movzb rax, al\n");
        break;
    case ND_BITOR:
        fprintf(output_file, "  or rax, rdi\n");
        break;
    case ND_BITXOR:
        fprintf(output_file, "  xor rax, rdi\n");
        break;
    case ND_BITAND:
        fprintf(output_file, "  and rax, rdi\n");
        break;
    case ND_SHL: {
        fprintf(output_file, "  mov rcx, rdi\n");
        fprintf(output_file, "  sal %s, cl\n", ax);
        break;
    }
    case ND_SHR: {
        fprintf(output_file, "  mov rcx, rdi\n");
        if (node->lhs->ty->is_unsigned) {
            fprintf(output_file, "  shr %s, cl\n", ax);
        } else {
            fprintf(output_file, "  sar %s, cl\n", ax);
        }
        break;
    }
    default:
        error("unreached");
    }
}

static void assign_lvar_offsets(struct Object *function) {
    int offset = 0;
    for (struct Object *var = function->locals; var != NULL; var = var->next) {
        offset += var->ty->size;
        offset = align_to(offset, var->align);
        var->offset = offset;
    }
    function->stack_size = align_to(offset, 16);
}

void codegen(FILE *out) {
    output_file = out;
    fprintf(output_file, ".intel_syntax noprefix\n");

    for (struct Object *obj = globals; obj; obj = obj->next) {
        assert(obj->is_global_variable);
        if (!obj->is_definition) {
            continue;
        }
        if (obj->is_static) {
            fprintf(output_file, "  .local %s\n", obj->name);
        } else {
            fprintf(output_file, "  .globl %s\n", obj->name);
        }
        fprintf(output_file, "  .align %d\n", obj->align);
        if (obj->init_data) {
            fprintf(output_file, "  .data\n");
            fprintf(output_file, "%s:\n", obj->name);
            struct Relocation *rel = obj->rel;
            int pos = 0;
            while (pos < obj->ty->size) {
                if (rel != NULL && rel->offset == pos) {
                    fprintf(output_file, "  .quad %s%+ld\n", rel->label,
                            rel->addend);
                    pos += 8;
                    rel = rel->next;
                } else {
                    fprintf(output_file, "  .byte %d\n", obj->init_data[pos++]);
                }
            }
        } else {
            fprintf(output_file, "  .bss\n");
            fprintf(output_file, "%s:\n", obj->name);
            fprintf(output_file, "  .zero %d\n", obj->ty->size);
        }
    }

    for (struct Object *obj = functions; obj; obj = obj->next) {
        if (obj->body == NULL)
            continue;
        assert(obj->is_function);
        assign_lvar_offsets(obj);
        fprintf(output_file, "  .text\n");
        if (obj->is_static) {
            fprintf(output_file, "  .local %s\n", obj->name);
        } else {
            fprintf(output_file, "  .globl %s\n", obj->name);
        }
        fprintf(output_file, "%s:\n", obj->name);

        prologue(obj->stack_size);

        if (obj->va_area) {
            int gp = 0;
            for (struct Node *arg = obj->args; arg; arg = arg->next) {
                gp++;
            }
            int offset = obj->va_area->offset;
            // va_elem
            fprintf(output_file, "  mov dword ptr [rbp - %d], %d\n", offset,
                    gp * 8); // general purpose offset
            fprintf(output_file, "  mov dword ptr [rbp - %d], 0\n",
                    offset - 4); // floating purpose offset
            fprintf(output_file, "  mov qword ptr [rbp - %d], rbp\n",
                    offset - 16);
            fprintf(output_file, "  sub qword ptr [rbp - %d], %d\n",
                    offset - 16, offset - 24);

            // __reg_save_area__
            fprintf(output_file, "  mov qword ptr [rbp - %d], rdi\n",
                    offset - 24);
            fprintf(output_file, "  mov qword ptr [rbp - %d], rsi\n",
                    offset - 32);
            fprintf(output_file, "  mov qword ptr [rbp - %d], rdx\n",
                    offset - 40);
            fprintf(output_file, "  mov qword ptr [rbp - %d], rcx\n",
                    offset - 48);
            fprintf(output_file, "  mov qword ptr [rbp - %d], r8\n",
                    offset - 56);
            fprintf(output_file, "  mov qword ptr [rbp - %d], r9\n",
                    offset - 64);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm0\n",
                    offset - 72);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm1\n",
                    offset - 80);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm2\n",
                    offset - 88);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm3\n",
                    offset - 96);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm4\n",
                    offset - 104);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm5\n",
                    offset - 112);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm6\n",
                    offset - 120);
            fprintf(output_file, "  movsd qword ptr [rbp - %d], xmm7\n",
                    offset - 128);
        }

        int i = 0;
        for (struct Node *arg = obj->args; arg; arg = arg->next) {
            gen_lval(arg);
            if (arg->ty->size == 1)
                fprintf(output_file, "  mov [rax], %s\n", argreg8[i++]);
            else if (arg->ty->size == 2)
                fprintf(output_file, "  mov [rax], %s\n", argreg16[i++]);
            else if (arg->ty->size == 4)
                fprintf(output_file, "  mov [rax], %s\n", argreg32[i++]);
            else
                fprintf(output_file, "  mov [rax], %s\n", argreg64[i++]);
        }

        gen(obj->body);

        epilogue();
    }
    if (depth != 0) {
        fprintf(output_file, "%d\n", depth);
    }
    assert(depth == 0);
    fprintf(output_file, ".section .note.GNU-stack,\"\",@progbits\n");
    return;
}