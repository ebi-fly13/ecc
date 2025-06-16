#include "ecc.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct Initializer {
    struct Initializer *next;
    struct Type *ty;

    struct Node *expr;

    bool is_flexible;

    struct Initializer **children;
};

struct InitDesg {
    struct InitDesg *next;
    int index;
    struct Object *var;
    struct Member *member;
};

struct Object *locals = NULL;
struct Object *globals = NULL;
struct Object *functions = NULL;

struct Scope *scope = &(struct Scope){};
struct Node *gotos = NULL;
struct Node *labels = NULL;
struct Node *switch_node = NULL;
char *continue_label = NULL;
char *break_label = NULL;

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

struct Node *new_node_cast(struct Node *cur, struct Type *ty) {
    add_type(cur);
    struct Node *node = new_node_unary(ND_CAST, cur);
    node->ty = ty;
    return node;
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

void push_typedef_scope(struct NameTag *tag) {
    struct VarScope *vs = calloc(1, sizeof(struct VarScope));
    vs->name = tag->name;
    vs->type_def = tag->ty;
    vs->next = scope->vars;
    scope->vars = vs;
    return;
}

void push_var_scope(struct Object *var) {
    struct VarScope *vs = calloc(1, sizeof(struct VarScope));
    vs->name = var->name;
    vs->var = var;
    vs->next = scope->vars;
    scope->vars = vs;
    return;
}

struct VarScope *push_scope(char *name) {
    struct VarScope *sc = calloc(1, sizeof(struct VarScope));
    sc->name = name;
    sc->next = scope->vars;
    scope->vars = sc;
    return sc;
}

void push_tag_scope(struct Type *ty) {
    struct TagScope *tag = calloc(1, sizeof(struct TagScope));
    tag->ty = ty;
    tag->next = scope->tags;
    scope->tags = tag;
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

struct Type *find_typedef(char *name) {
    for (struct Scope *scp = scope; scp != NULL; scp = scp->next) {
        for (struct VarScope *var = scp->vars; var != NULL; var = var->next) {
            if (!strcmp(var->name, name)) {
                return var->type_def;
            }
        }
    }
    return NULL;
}

struct Type *find_typedef_from_scope(char *name) {
    for (struct VarScope *var = scope->vars; var != NULL; var = var->next) {
        if (!strcmp(var->name, name)) {
            return var->type_def;
        }
    }
    return NULL;
}

struct VarScope *find_variable(char *name) {
    for (struct Scope *scp = scope; scp != NULL; scp = scp->next) {
        for (struct VarScope *var = scp->vars; var != NULL; var = var->next) {
            if (!strcmp(var->name, name)) {
                return var;
            }
        }
    }
    return NULL;
}

struct VarScope *find_variable_from_scope(char *name) {
    for (struct VarScope *var = scope->vars; var != NULL; var = var->next) {
        if (!strcmp(var->name, name)) {
            return var;
        }
    }
    return NULL;
}

struct Type *find_tag(char *name) {
    for (struct Scope *scp = scope; scp != NULL; scp = scp->next) {
        for (struct TagScope *tag = scp->tags; tag != NULL; tag = tag->next) {
            if (!strcmp(tag->ty->name, name)) {
                return tag->ty;
            }
        }
    }
    return NULL;
}

struct TagScope *find_tag_from_scope(char *name) {
    for (struct TagScope *tag = scope->tags; tag != NULL; tag = tag->next) {
        if (!strcmp(tag->ty->name, name)) {
            return tag;
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

struct Object *new_var(struct NameTag *tag) {
    struct Object *var = calloc(1, sizeof(struct Object));
    var->name = tag->name;
    var->ty = tag->ty;
    push_var_scope(var);
    return var;
}

struct Object *new_local_var(struct NameTag *tag) {
    if (is_void(tag->ty)) {
        error("%sはvoidで宣言されています", tag->name);
    } else {
        struct Object *lvar = new_var(tag);
        lvar->next = locals;
        lvar->len = strlen(tag->name);
        if (locals == NULL) {
            lvar->offset = (int)tag->ty->size;
        } else {
            lvar->offset = locals->offset + (int)tag->ty->size;
        }
        locals = lvar;
        return lvar;
    }
}

struct Object *new_global_var(struct NameTag *tag) {
    struct Object *gvar = find_object(globals, tag->name);
    if (gvar) {
        error("グローバル変数%sは既に定義されています", tag->name);
    } else if (is_void(tag->ty)) {
        error("%sはvoidで宣言されています", tag->name);
    } else {
        gvar = new_var(tag);
        gvar->next = globals;
        gvar->len = strlen(tag->name);
        gvar->is_global_variable = true;
        globals = gvar;
        return gvar;
    }
}

struct Object *new_anon_gvar(struct Type *ty) {
    struct NameTag *tag = calloc(1, sizeof(1, sizeof(struct NameTag)));
    tag->name = new_unique_name();
    tag->ty = ty;
    return new_global_var(tag);
}

struct Object *new_string_literal(struct Token *token) {
    struct Object *obj = new_anon_gvar(token->ty);
    obj->len = strlen(obj->name);
    obj->is_global_variable = true;
    obj->init_data = token->str;
    return obj;
}

struct Object *new_func(struct NameTag *tag) {
    struct Object *fn = find_object(globals, tag->name);
    if (fn) {
        error("関数%sは既に定義されています", tag->name);
    } else {
        fn = new_var(tag);
        fn->next = globals;
        fn->len = strlen(tag->name);
        fn->is_function_definition = true;
        fn->next = functions;
        functions = fn;
    }
    return fn;
}

struct Node *expand_func_params(struct Type *ty) {
    struct Node head = {};
    struct Node *cur = &head;
    for (struct NameTag *p = ty->params; p != NULL; p = p->next) {
        cur = cur->next = new_node_var(new_local_var(p));
    }
    return head.next;
}

struct Member *get_struct_member(struct Type *ty, char *name) {
    for (struct Member *member = ty->member; member != NULL;
         member = member->next) {
        if (strcmp(member->name, name) == 0) return member;
    }
    error("struct %sにメンバ変数%sは存在しません", ty->name, name);
    return NULL;
}

struct Node *struct_union_ref(struct Node *node, char *name) {
    add_type(node);
    assert(node->ty->ty == TY_STRUCT || node->ty->ty == TY_UNION);
    struct Member *member = get_struct_member(node->ty, name);
    struct Node *res = new_node_unary(ND_MEMBER, node);
    res->member = member;
    res->ty = member->ty;
    return res;
}

bool is_typename(struct Token *token) {
    if (equal_keyword(token, TK_MOLD) || equal(token, "struct") ||
        equal(token, "union") || equal(token, "typedef") ||
        equal(token, "static"))
        return true;
    struct Type *ty = find_typedef(strndup(token->loc, token->len));
    return ty != NULL;
}

static long eval(struct Node *);
static long internal_eval(struct Node *, char **);

static long eval_rval(struct Node *node, char **label) {
    switch (node->kind) {
        case ND_GVAR:
            *label = node->obj->name;
            return 0;
        case ND_DEREF:
            return internal_eval(node->lhs, label);
        case ND_MEMBER:
            return eval_rval(node->lhs, label) + node->member->offset;
        default:
            error("invalid initializer");
    }
}

static long internal_eval(struct Node *node, char **label) {
    add_type(node);
    switch (node->kind) {
        case ND_ADD:
            return internal_eval(node->lhs, label) + eval(node->rhs);
        case ND_SUB:
            return internal_eval(node->lhs, label) - eval(node->rhs);
        case ND_MUL:
            return eval(node->lhs) * eval(node->rhs);
        case ND_DIV:
            return eval(node->lhs) / eval(node->rhs);
        case ND_BITAND:
            return eval(node->lhs) & eval(node->rhs);
        case ND_BITOR:
            return eval(node->lhs) | eval(node->rhs);
        case ND_BITXOR:
            return eval(node->lhs) ^ eval(node->rhs);
        case ND_SHL:
            return eval(node->lhs) << eval(node->rhs);
        case ND_SHR:
            return eval(node->lhs) >> eval(node->rhs);
        case ND_EQ:
            return eval(node->lhs) == eval(node->rhs);
        case ND_LT:
            return eval(node->lhs) < eval(node->rhs);
        case ND_LE:
            return eval(node->lhs) <= eval(node->rhs);
        case ND_NE:
            return eval(node->lhs) != eval(node->rhs);
        case ND_MOD:
            return eval(node->lhs) % eval(node->rhs);
        case ND_LOGAND:
            return eval(node->lhs) && eval(node->rhs);
        case ND_LOGOR:
            return eval(node->lhs) || eval(node->rhs);
        case ND_COND:
            return eval(node->cond) ? internal_eval(node->then, label)
                                    : internal_eval(node->els, label);
        case ND_BITNOT:
            return ~eval(node->lhs);
        case ND_COMMA:
            return internal_eval(node->rhs, label);
        case ND_NOT:
            return !eval(node->lhs);
        case ND_NUM:
            return node->val;
        case ND_CAST:
            long val = internal_eval(node->lhs, label);
            if (is_integer(node->ty)) {
                switch (node->ty->size) {
                    case 1:
                        return (__uint8_t)val;
                    case 2:
                        return (__uint16_t)val;
                    case 4:
                        return (__uint32_t)val;
                    case 8:
                        return val;
                }
            }
            return val;
        case ND_ADDR:
            return eval_rval(node->lhs, label);
        case ND_MEMBER:
            if (!label) {
                error("not a compile-time constant");
            }
            if (node->ty->ty != TY_ARRAY) {
                error("invalid initializer");
            }
            return eval_rval(node->lhs, label) + node->member->offset;
        case ND_GVAR:
            if (!label) {
                error("not a compile-time constant");
            }
            if (node->obj->ty->ty != TY_ARRAY && node->obj->ty->ty != TY_FUNC) {
                error("invalid initializer");
            }
            *label = node->obj->name;
            return 0;
    }
    error("コンパイル時定数ではありません");
}

static long eval(struct Node *node) {
    return internal_eval(node, NULL);
}

static void write_buffer(char *buf, uint64_t val, int sz) {
    if (sz == 1) {
        *buf = val;
    } else if (sz == 2) {
        *(uint16_t *)buf = val;
    } else if (sz == 4) {
        *(uint32_t *)buf = val;
    } else if (sz == 8) {
        *(uint64_t *)buf = val;
    } else {
        error("バッファに書き込めないサイズです");
    }
}

static struct Relocation *write_gvar_data(struct Relocation *cur,
                                          struct Initializer *init,
                                          struct Type *ty, char *buf,
                                          int offset) {
    if (ty->ty == TY_ARRAY) {
        int sz = ty->ptr_to->size;
        for (int i = 0; i < ty->array_size; i++) {
            cur = write_gvar_data(cur, init->children[i], ty->ptr_to, buf,
                                  offset + sz * i);
        }
        return cur;
    }

    if (ty->ty == TY_STRUCT) {
        for (struct Member *member = ty->member; member != NULL;
             member = member->next) {
            cur = write_gvar_data(cur, init->children[member->index],
                                  member->ty, buf, offset + member->offset);
        }
        return cur;
    }

    if (ty->ty == TY_UNION) {
        return write_gvar_data(cur, init->children[0], ty->member->ty, buf,
                               offset);
    }

    if (init->expr == NULL) {
        return cur;
    }

    char *label = NULL;
    long val = internal_eval(init->expr, &label);

    if (!label) {
        write_buffer(buf + offset, val, ty->size);
        return cur;
    }

    struct Relocation *rel = calloc(1, sizeof(struct Relocation));
    rel->offset = offset;
    rel->label = label;
    rel->addend = val;
    cur->next = rel;
    return cur->next;
}

struct VarAttr {
    bool is_typedef;
    bool is_static;
};

struct Object *program(struct Token *);
struct Token *function(struct Token *, struct Type *, struct VarAttr *attr);
struct Token *global_variable(struct Token *, struct Type *);
struct Type *declspec(struct Token **, struct Token *, struct VarAttr *attr);
struct Type *struct_decl(struct Token **, struct Token *);
struct Type *union_decl(struct Token **, struct Token *);
struct Type *enum_specifier(struct Token **, struct Token *);
struct NameTag *declarator(struct Token **, struct Token *, struct Type *);
struct Type *type_suffix(struct Token **, struct Token *, struct Type *);
struct Type *array_dimensions(struct Token **, struct Token *, struct Type *);
struct NameTag *func_params(struct Token **, struct Token *, struct NameTag *);
struct Member *struct_union_members(struct Token **, struct Token *);
struct NameTag *param(struct Token **, struct Token *);
struct Node *stmt(struct Token **, struct Token *);
struct Node *compound_stmt(struct Token **, struct Token *);
void typedef_decl(struct Token **, struct Token *, struct Type *);
struct Node *declaration(struct Token **, struct Token *, struct Type *base_ty);
struct Node *lvar_initializer(struct Token **, struct Token *, struct Object *);
static void gvar_initializer(struct Token **, struct Token *, struct Object *);
struct Node *expr_stmt(struct Token **, struct Token *);
long const_expr(struct Token **, struct Token *);
struct Node *expr(struct Token **, struct Token *);
struct Node *assign(struct Token **, struct Token *);
struct Node *conditional(struct Token **, struct Token *);
struct Node *logor(struct Token **, struct Token *);
struct Node *logand(struct Token **, struct Token *);
struct Node * bitor (struct Token **, struct Token *);
struct Node *bitxor(struct Token **, struct Token *);
struct Node *bitand(struct Token **, struct Token *);
struct Node *equality(struct Token **, struct Token *);
struct Node *relation(struct Token **, struct Token *);
struct Node *shift(struct Token **, struct Token *);
struct Node *add(struct Token **, struct Token *);
struct Node *mul(struct Token **, struct Token *);
struct Node *unary(struct Token **, struct Token *);
static struct Node *cast(struct Token **, struct Token *);
struct Node *postfix(struct Token **, struct Token *);
struct Node *funcall(struct Token **, struct Token *);
struct Node *primary(struct Token **, struct Token *);

bool is_function(struct Token *token) {
    if (equal(token, ";")) return false;
    struct Type dummy = {};
    declarator(&token, token, &dummy);
    return equal(token, "(");
}

void resolve_goto_labels() {
    for (struct Node *goto_node = gotos; goto_node != NULL;
         goto_node = goto_node->goto_next) {
        for (struct Node *label = labels; label != NULL;
             label = label = label->goto_next) {
            if (!strcmp(goto_node->label, label->label)) {
                goto_node->unique_label = label->unique_label;
                break;
            }
        }
        if (goto_node->unique_label == NULL) {
            error("定義されていないラベルを使用しています");
        }
    }
    gotos = NULL;
    labels = NULL;
}

/*
program = (function | global_variable)*
*/
struct Object *program(struct Token *token) {
    globals = NULL;
    while (token->kind != TK_EOF) {
        struct VarAttr attr = {};
        struct Type *ty = declspec(&token, token, &attr);
        if (attr.is_typedef) {
            typedef_decl(&token, token, ty);
            continue;
        }

        if (is_function(token)) {
            token = function(token, ty, &attr);
        } else {
            token = global_variable(token, ty);
        }
    }
    return globals;
}

/*
function = declarator "(" func_params "{" compound_stmt
*/
struct Token *function(struct Token *token, struct Type *ty,
                       struct VarAttr *attr) {
    struct NameTag *tag = declarator(&token, token, ty);
    token = skip(token, "(");
    tag = func_params(&token, token, tag);
    struct Object *fn = find_object(functions, tag->name);
    if (fn == NULL) {
        fn = new_func(tag);
    } else {
        assert(is_same_type(fn->ty, tag->ty));
    }
    locals = NULL;
    fn->is_function = true;
    fn->is_function_definition = !equal(token, ";");
    fn->is_static = attr->is_static;

    if (equal(token, "{")) {
        assert(fn->is_function_definition);

        enter_scope();
        fn->args = expand_func_params(tag->ty);
        token = skip(token, "{");
        fn->body = compound_stmt(&token, token);
        if (locals)
            fn->stack_size = locals->offset;
        else
            fn->stack_size = 0;
        fn->stack_size = align_to(fn->stack_size + 8, 16);
        leave_scope();
        resolve_goto_labels();
    } else {
        token = skip(token, ";");
    }
    return token;
}

/*
global_variable = (declarator ("," declarator)* )? ";"
*/
struct Token *global_variable(struct Token *token, struct Type *ty) {
    bool is_first = true;
    while (!equal(token, ";")) {
        if (!is_first)
            token = skip(token, ",");
        else
            is_first = false;
        struct NameTag *gvar_nametag = declarator(&token, token, ty);
        struct Object *gvar = new_global_var(gvar_nametag);

        if (equal(token, "=")) {
            token = skip(token, "=");
            gvar_initializer(&token, token, gvar);
        }
    }
    return token->next;
}

/*
declspec = ("long" | "int" | "short" | "char" | "typedef" | typedef_type )+ |
struct_decl | union_decl | enum_specifier
*/
struct Type *declspec(struct Token **rest, struct Token *token,
                      struct VarAttr *attr) {
    enum {
        VOID = 1 << 0,
        CHAR = 1 << 2,
        SHORT = 1 << 4,
        INT = 1 << 6,
        LONG = 1 << 8,
        OTHER = 1 << 10,
    };
    int counter = 0;
    while (is_typename(token)) {
        if (equal(token, "typedef") || equal(token, "static")) {
            if (attr == NULL) {
                error("storage class specifier is not allowed in this context");
            }

            if (equal(token, "typedef")) {
                attr->is_typedef = true;
                token = skip(token, "typedef");
            } else {
                attr->is_static = true;
                token = skip(token, "static");
            }
            if (attr->is_typedef && attr->is_static) {
                error("typedefとstaticは同時に使えません");
            }
            continue;
        } else if (equal(token, "long")) {
            token = skip(token, "long");
            if ((counter & (LONG + LONG))) {
                error("longが3重になっています");
            }
            counter += LONG;
        } else if (equal(token, "int")) {
            token = skip(token, "int");
            if (counter & INT) {
                error("intが2重になっています");
            }
            counter += INT;
        } else if (equal(token, "short")) {
            token = skip(token, "short");
            if (counter & SHORT) {
                error("shortが2重になっています");
            }
            counter += SHORT;
        } else if (equal(token, "char")) {
            token = skip(token, "char");
            if (counter & CHAR) {
                error("charが2重になっています");
            }
            counter += CHAR;
        } else if (equal(token, "void")) {
            token = skip(token, "void");
            if (counter & VOID) {
                error("voidが2重になっています");
            }
            counter += VOID;
        } else if (equal(token, "struct")) {
            assert(counter == 0);
            struct Type *ty = struct_decl(rest, token->next);
            return ty;
        } else if (equal(token, "union")) {
            assert(counter == 0);
            struct Type *ty = union_decl(rest, token->next);
            return ty;
        } else if (equal(token, "enum")) {
            assert(counter == 0);
            struct Type *ty = enum_specifier(rest, token->next);
            return ty;
        } else if (equal_keyword(token, TK_IDENT)) {
            struct Type *ty = find_typedef(strndup(token->loc, token->len));
            assert(ty != NULL);
            *rest = token->next;
            return ty;
        } else {
            error("既定の型でありません");
        }
    }
    struct Type *ty = NULL;
    switch (counter) {
        case VOID:
            ty = ty_void;
            break;
        case CHAR:
            ty = ty_char;
            break;
        case SHORT:
        case SHORT + INT:
            ty = ty_short;
            break;
        case INT:
            ty = ty_int;
            break;
        case LONG:
        case LONG + INT:
        case LONG + LONG:
        case LONG + LONG + INT:
            ty = ty_long;
            break;
        default:
            if (attr != NULL && attr->is_typedef) {
                ty = ty_int;
                break;
            }
            error("invalid type");
    }
    *rest = token;
    return ty;
}

// abstract-declarator = "*"* ("(" abstract-declarator ")")? type_suffix
struct Type *abstract_declarator(struct Token **rest, struct Token *token,
                                 struct Type *ty) {
    while (equal(token, "*")) {
        ty = pointer_to(ty);
        token = skip(token, "*");
    }
    if (equal(token, "(")) {
        struct Token *start = skip(token, "(");
        struct Type dummy;
        abstract_declarator(&token, start, &dummy);
        token = skip(token, ")");
        ty = type_suffix(rest, token, ty);
        return abstract_declarator(&token, start, ty);
    }
    return type_suffix(rest, token, ty);
}

// typename = declspec abstruct-declarator
struct Type *typename(struct Token **rest, struct Token *token) {
    struct Type *ty = declspec(&token, token, NULL);
    return abstract_declarator(rest, token, ty);
}

/*
struct_decl = "struct" "{" struct_union_members
*/
struct Type *struct_decl(struct Token **rest, struct Token *token) {
    char *name = NULL;
    if (token->kind == TK_IDENT) {
        name = strndup(token->loc, token->len);
        token = skip_keyword(token, TK_IDENT);
    }
    if (name != NULL && !equal(token, "{")) {
        *rest = token;

        struct Type *ty = find_tag(name);
        if (ty) {
            return ty;
        }
        ty = struct_type();
        ty->name = name;
        ty->size = -1;
        push_tag_scope(ty);
        return ty;
    }
    token = skip(token, "{");
    struct Type *ty = struct_type();
    ty->member = struct_union_members(rest, token);
    int offset = 0;
    for (struct Member *member = ty->member; member != NULL;
         member = member->next) {
        member->offset = offset;
        offset += member->ty->size;
    }
    ty->size = offset;
    ty->name = name;
    if (name != NULL) {
        struct TagScope *tag = find_tag_from_scope(ty->name);
        if (tag != NULL) {
            *tag->ty = *ty;
            return tag->ty;
        }
        push_tag_scope(ty);
    }
    return ty;
}

/*
struct union_decl = "union" "{" struct_union_members
*/
struct Type *union_decl(struct Token **rest, struct Token *token) {
    char *name = NULL;
    if (token->kind == TK_IDENT) {
        name = strndup(token->loc, token->len);
        token = skip_keyword(token, TK_IDENT);
    }
    if (name != NULL && !equal(token, "{")) {
        struct Type *ty = find_tag(name);
        if (ty == NULL) {
            error("共用体%sは存在しません", name);
        }
        *rest = token;
        return ty;
    }
    token = skip(token, "{");
    struct Type *ty = union_type();
    ty->member = struct_union_members(rest, token);
    int offset = 0;
    for (struct Member *member = ty->member; member != NULL;
         member = member->next) {
        member->offset = 0;
        if (offset < member->ty->size) {
            offset = member->ty->size;
        }
    }
    ty->size = offset;
    ty->name = name;
    if (name != NULL) {
        if (find_tag_from_scope(ty->name)) {
            error("共用体%sは既に定義されています", ty->name);
        }
        push_tag_scope(ty);
    }
    return ty;
}

/*
struct_union_members = (declspec declarator (","  declarator)* ";")* "}"
*/
struct Member *struct_union_members(struct Token **rest, struct Token *token) {
    struct Member head = {};
    struct Member *cur = &head;
    int index = 0;
    while (!equal(token, "}")) {
        struct Type *base_ty = declspec(&token, token, NULL);
        bool is_first = true;
        while (!equal(token, ";")) {
            if (!is_first) token = skip(token, ",");
            is_first = false;
            struct NameTag *tag = declarator(&token, token, base_ty);
            struct Member *member = calloc(1, sizeof(struct Member));
            member->name = tag->name;
            member->ty = tag->ty;
            member->index = index++;
            cur = cur->next = member;
        }
        token = skip(token, ";");
    }
    if(cur != head.next && cur->ty->ty == TY_ARRAY && cur->ty->array_size < 0) {
        cur->ty = array_to(cur->ty->ptr_to, 0);
    }

    *rest = token->next;
    return head.next;
}

/*
enum_specifier = ident? "{" enum_list "}" | ident ("{" enum_list "}")?
*/
struct Type *enum_specifier(struct Token **rest, struct Token *token) {
    char *name = NULL;
    if (token->kind == TK_IDENT) {
        name = strndup(token->loc, token->len);
        token = skip_keyword(token, TK_IDENT);
    }

    if (name != NULL && !equal(token, "{")) {
        struct Type *ty = find_tag(name);
        if (ty == NULL) {
            error("enum %sは存在しません", name);
        }
        if (ty->ty != TY_ENUM) {
            error("%s はenumではありません", name);
        }
        *rest = token;
        return ty;
    }

    token = skip(token, "{");

    struct Type *ty = enum_type();
    ty->name = name;

    int count = 0;
    int val = 0;

    while (!is_end(token)) {
        if (count > 0) {
            token = skip(token, ",");
        }
        count++;
        assert(token->kind == TK_IDENT);
        char *ident_name = strndup(token->loc, token->len);
        token = skip_keyword(token, TK_IDENT);
        if (equal(token, "=")) {
            token = skip(token, "=");
            val = const_expr(&token, token);
        }
        struct VarScope *sc = push_scope(ident_name);
        sc->enum_ty = ty;
        sc->enum_val = val++;
    }
    token = skip_end(token);
    *rest = token;

    if (name != NULL) {
        if (find_tag_from_scope(name)) {
            error("列挙型%sは既に定義されています", name);
        }
        push_tag_scope(ty);
    }
    return ty;
}

/*
declarator = "*"* ( "(" declarator ")" | ident)? type_suffix
*/
struct NameTag *declarator(struct Token **rest, struct Token *token,
                           struct Type *ty) {
    while (equal(token, "*")) {
        token = skip(token, "*");
        ty = pointer_to(ty);
    }

    if (equal(token, "(")) {
        token = skip(token, "(");
        struct Token *start = token;
        struct Type dummy = {};
        struct NameTag *tag = declarator(&token, token, &dummy);
        token = skip(token, ")");
        ty = type_suffix(&token, token, ty);
        *rest = token;
        return declarator(&start, start, ty);
    }

    struct NameTag *tag = calloc(1, sizeof(struct NameTag));
    if (equal_keyword(token, TK_IDENT)) {
        tag->name = strndup(token->loc, token->len);
        token = token->next;
    }
    tag->ty = type_suffix(rest, token, ty);
    return tag;
}

/*
type_suffix = "[" array_dimensions | ""
*/
struct Type *type_suffix(struct Token **rest, struct Token *token,
                         struct Type *ty) {
    if (equal(token, "[")) {
        return array_dimensions(rest, token->next, ty);
    }
    *rest = token;
    return ty;
}

/*
array_dimensions = const_expr? "]" type_suffix
*/
struct Type *array_dimensions(struct Token **rest, struct Token *token,
                              struct Type *ty) {
    if (equal(token, "]")) {
        return array_to(type_suffix(rest, token->next, ty), -1);
    }
    int sz = const_expr(&token, token);
    token = skip(token, "]");
    return array_to(type_suffix(rest, token, ty), sz);
}

/*
func_params = (param ("," param )? )? ")"
*/
struct NameTag *func_params(struct Token **rest, struct Token *token,
                            struct NameTag *return_tag) {
    struct NameTag head = {};
    struct NameTag *cur = &head;
    while (!equal(token, ")")) {
        if (cur != &head) {
            token = skip(token, ",");
        }
        struct NameTag *tag = param(&token, token);
        cur->next = tag;
        cur = tag;
    }
    *rest = skip(token, ")");
    struct NameTag *fn = calloc(1, sizeof(1, sizeof(struct NameTag)));
    fn->ty = func_to(return_tag->ty, head.next);
    fn->name = return_tag->name;
    return fn;
}

/*
param = declspec declarator
*/
struct NameTag *param(struct Token **rest, struct Token *token) {
    struct Type *ty = declspec(&token, token, NULL);
    struct NameTag *tag = declarator(&token, token, ty);
    *rest = token;

    if (tag->ty->ty == TY_ARRAY) {
        tag->ty = pointer_to(tag->ty->ptr_to);
    }
    return tag;
}

/*
stmt = "return" expr? ";" | "if" "(" expr ")" stmt ("else" stmt)? | "while" "("
expr ")" stmt | "for" "(" expr? ";" expr? ";" expr ")" |  "{" compound_stmt |
"goto" ident ";"  | ident ":" stmt | "break" ";" | "switch" "(" expr ")" stmt |
"case" const_expr ":" stmt | "default" ":" stmt | expr-stmt
*/
struct Node *stmt(struct Token **rest, struct Token *token) {
    struct Node *node = NULL;
    if (equal_keyword(token, TK_RETURN)) {
        token = skip_keyword(token, TK_RETURN);
        node = new_node(ND_RETURN);
        if (!equal(token, ";")) {
            node->lhs = expr(&token, token);
        }
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

        char *current_continue_label = continue_label;
        continue_label = node->continue_label = new_unique_name();
        char *curren_break_label = break_label;
        break_label = node->break_label = new_unique_name();

        node->then = stmt(&token, token);
        continue_label = current_continue_label;
        break_label = curren_break_label;
    } else if (equal_keyword(token, TK_FOR)) {
        enter_scope();
        token = skip_keyword(token, TK_FOR);
        node = new_node(ND_FOR);
        token = skip(token, "(");
        if (is_typename(token)) {
            struct Type *type = declspec(&token, token, NULL);
            node->init = declaration(&token, token, type);
        } else {
            node->init = expr_stmt(&token, token);
        }
        node->cond = expr_stmt(&token, token);
        if (!equal(token, ")")) {
            node->inc = expr(&token, token);
        }
        token = skip(token, ")");

        char *current_continue_label = continue_label;
        continue_label = node->continue_label = new_unique_name();
        char *current_break_label = break_label;
        break_label = node->break_label = new_unique_name();

        node->then = stmt(&token, token);
        continue_label = current_continue_label;
        break_label = current_break_label;
        leave_scope();
    } else if (equal_keyword(token, TK_GOTO)) {
        token = skip_keyword(token, TK_GOTO);
        node = new_node(ND_GOTO);
        node->label = strndup(token->loc, token->len);
        node->goto_next = gotos;
        gotos = node;
        token = skip_keyword(token, TK_IDENT);
        token = skip(token, ";");
    } else if (equal_keyword(token, TK_IDENT) && equal(token->next, ":")) {
        node = new_node(ND_LABEL);
        node->label = strndup(token->loc, token->len);
        node->unique_label = new_unique_name();
        node->goto_next = labels;
        labels = node;
        token = skip_keyword(token, TK_IDENT);
        token = skip(token, ":");
        node->body = stmt(&token, token);
    } else if (equal_keyword(token, TK_BREAK)) {
        node = new_node(ND_BREAK);
        node->break_label = break_label;
        token = skip_keyword(token, TK_BREAK);
        token = skip(token, ";");
    } else if (equal_keyword(token, TK_CONTINUE)) {
        node = new_node(ND_CONTINUE);
        node->continue_label = continue_label;
        token = skip_keyword(token, TK_CONTINUE);
        token = skip(token, ";");
    } else if (equal_keyword(token, TK_SWITCH)) {
        struct Node *current_switch_node = switch_node;

        switch_node = new_node(ND_SWITCH);
        token = skip_keyword(token, TK_SWITCH);
        token = skip(token, "(");
        switch_node->cond = expr(&token, token);
        token = skip(token, ")");

        char *current_break_label = break_label;
        break_label = switch_node->break_label = new_unique_name();

        switch_node->then = stmt(&token, token);

        node = switch_node;

        switch_node = current_switch_node;
        break_label = current_break_label;
    } else if (equal_keyword(token, TK_CASE)) {
        if (switch_node == NULL) {
            error("switch文ではないのにcaseがあります");
        }
        node = new_node(ND_CASE);
        token = skip_keyword(token, TK_CASE);

        node->val = const_expr(&token, token);
        token = skip(token, ":");
        node->label = new_unique_name();
        node->lhs = stmt(&token, token);

        node->next_case = switch_node->next_case;
        switch_node->next_case = node;
    } else if (equal_keyword(token, TK_DEFAULT)) {
        if (switch_node == NULL) {
            error("switch文ではないのにdefaultがあります");
        }
        node = new_node(ND_CASE);
        token = skip_keyword(token, TK_DEFAULT);
        token = skip(token, ":");
        node->label = new_unique_name();
        node->lhs = stmt(&token, token);

        switch_node->default_case = node;
    } else if (equal(token, "{")) {
        node = compound_stmt(&token, token->next);
    } else {
        node = expr_stmt(&token, token);
    }
    *rest = token;
    return node;
}

/*
compound_stmt = (declaration | typedef_decl | stmt)* "}"
*/
struct Node *compound_stmt(struct Token **rest, struct Token *token) {
    struct Node *node = new_node(ND_BLOCK);
    struct Node head = {};
    struct Node *cur = &head;

    enter_scope();
    while (!equal(token, "}")) {
        if (is_typename(token) && !equal(token->next, ":")) {
            struct VarAttr attr = {};
            struct Type *base_ty = declspec(&token, token, &attr);
            if (attr.is_typedef) {
                typedef_decl(&token, token, base_ty);
            } else {
                cur->next = declaration(&token, token, base_ty);
            }
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
declaration = (declarator ("=" lvar_initializer) ("," declarator "="
lvar_initializer)* )? ";"
*/
struct Node *declaration(struct Token **rest, struct Token *token,
                         struct Type *base_ty) {
    struct Node head = {};
    struct Node *cur = &head;
    bool is_first = true;
    while (!equal(token, ";")) {
        if (!is_first) token = skip(token, ",");
        is_first = false;
        struct NameTag *tag = declarator(&token, token, base_ty);
        struct Object *var = new_local_var(tag);

        if (equal(token, "=")) {
            token = skip(token, "=");
            struct Node *exp = lvar_initializer(&token, token, var);
            cur = cur->next = new_node_unary(ND_ASSIGN_EXPR, exp);
            add_type(exp);
        }

        if (var->ty->size < 0) {
            error("変数が不完全な型です");
        }
        if (var->ty->ty == TY_VOID) {
            error("void 型の変数が宣言されています");
        }
    }
    *rest = skip(token, ";");
    struct Node *node = new_node(ND_BLOCK);
    node->body = head.next;
    return node;
}

struct Initializer *new_initializer(struct Type *ty, bool is_flexible) {
    struct Initializer *init = calloc(1, sizeof(struct Initializer));
    init->ty = ty;

    if (ty->ty == TY_ARRAY) {
        if (is_flexible && ty->array_size < 0) {
            init->is_flexible = true;
            return init;
        }

        if (ty->array_size < 0) {
            error("配列サイズが必要です");
        }

        init->children = calloc(ty->array_size, sizeof(struct Initializer));
        for (int i = 0; i < ty->array_size; i++) {
            init->children[i] = new_initializer(ty->ptr_to, false);
        }
    }

    if (ty->ty == TY_STRUCT || ty->ty == TY_UNION) {
        int len = 0;
        for (struct Member *member = init->ty->member; member != NULL;
             member = member->next) {
            len++;
        }

        init->children = calloc(len, sizeof(struct Initializer));
        for (struct Member *member = ty->member; member != NULL;
             member = member->next) {
            init->children[member->index] = new_initializer(member->ty, false);
        }
    }

    return init;
}

struct Token *skip_excess_elements(struct Token *token) {
    if (equal(token, "{")) {
        token = skip(token, "{");
        for (int i = 0; !equal(token, "}"); i++) {
            if (i > 0) {
                token = skip(token, ",");
            }
            token = skip_excess_elements(token);
        }
        token = skip(token, "}");
    } else {
        assign(&token, token);
    }
    return token;
}

struct Token *string_initializer(struct Token *token,
                                 struct Initializer *init) {
    assert(equal_keyword(token, TK_STR));
    assert(token != NULL && token->ty->ty == TY_ARRAY &&
           token->ty->ptr_to == ty_char);
    if (init->is_flexible) {
        *init =
            *new_initializer(array_to(init->ty->ptr_to, token->len - 1), false);
    }
    int len = MIN(init->ty->array_size, token->ty->array_size);
    for (int i = 0; i < len; i++) {
        init->children[i]->expr = new_node_num(token->str[i]);
    }
    return skip_keyword(token, TK_STR);
}

void internal_initializer(struct Token **, struct Token *,
                          struct Initializer *);

void internal_initializer2(struct Token **, struct Token *,
                           struct Initializer *);

struct Token *struct_initializer(struct Token *token,
                                 struct Initializer *init) {
    struct Member *member = init->ty->member;
    while (!is_end(token)) {
        if (member != init->ty->member) {
            token = skip(token, ",");
        }
        if (member != NULL) {
            internal_initializer2(&token, token, init->children[member->index]);
            member = member->next;
        } else {
            token = skip_excess_elements(token);
        }
    }
    return token;
}

struct Token *struct_initializer2(struct Token *token,
                                  struct Initializer *init) {
    struct Member *member = init->ty->member;
    while (member != NULL && !is_end(token)) {
        if (member != init->ty->member) {
            token = skip(token, ",");
        }
        internal_initializer2(&token, token, init->children[member->index]);
        member = member->next;
    }
    return token;
}

struct Token *union_initializer(struct Token *token, struct Initializer *init) {
    if (equal(token, "{")) {
        token = skip(token, "{");
        internal_initializer2(&token, token, init->children[0]);
        token = skip_end(token);
    } else {
        internal_initializer(&token, token, init->children[0]);
    }
    return token;
}

static int count_array_elements(struct Token *token, struct Type *ty) {
    struct Initializer *dummy = new_initializer(ty->ptr_to, false);
    int i = 0;
    for (; !is_end(token); i++) {
        if (i > 0) {
            token = skip(token, ",");
        }
        internal_initializer2(&token, token, dummy);
    }
    return i;
}

struct Token *array_initializer(struct Token *token, struct Initializer *init) {
    if (init->is_flexible) {
        int len = count_array_elements(token, init->ty);
        *init = *new_initializer(array_to(init->ty->ptr_to, len), false);
    }

    for (int i = 0; !is_end(token); i++) {
        if (i > 0) {
            token = skip(token, ",");
        }
        if (i < init->ty->array_size) {
            internal_initializer2(&token, token, init->children[i]);
        } else {
            token = skip_excess_elements(token);
        }
    }
    return token;
}

struct Token *array_initializer2(struct Token *token,
                                 struct Initializer *init) {
    if (init->is_flexible) {
        int len = count_array_elements(token, init->ty);
        *init = *new_initializer(array_to(init->ty->ptr_to, len), false);
    }

    for (int i = 0; i < init->ty->array_size && !is_end(token); i++) {
        if (i > 0) {
            token = skip(token, ",");
        }
        internal_initializer2(&token, token, init->children[i]);
    }
    return token;
}

/*
initializer = struct_initializer | string_initializer | array_initializer |
assign
*/
void internal_initializer(struct Token **rest, struct Token *token,
                          struct Initializer *init) {
    if (init->ty->ty == TY_STRUCT) {
        if (!equal(token, "{")) {
            struct Node *node = assign(&token, token);
            add_type(node);
            if (node->ty->ty == TY_STRUCT) {
                init->expr = node;
            } else {
                error("構造体ではありません");
            }
        } else {
            token = skip(token, "{");
            token = struct_initializer(token, init);
            token = skip_end(token);
        }
    } else if (init->ty->ty == TY_UNION) {
        token = union_initializer(token, init);
    } else if (init->ty->ty == TY_ARRAY && equal_keyword(token, TK_STR)) {
        token = string_initializer(token, init);
    } else if (init->ty->ty == TY_ARRAY) {
        token = skip(token, "{");
        token = array_initializer(token, init);
        token = skip_end(token);
    } else if (equal(token, "{")) {
        token = skip(token, "{");
        init->expr = assign(&token, token);
        token = skip_end(token);
    } else {
        init->expr = assign(&token, token);
    }
    *rest = token;
}

void internal_initializer2(struct Token **rest, struct Token *token,
                           struct Initializer *init) {
    if (init->ty->ty == TY_STRUCT) {
        if (equal(token, "{")) {
            token = skip(token, "{");
            token = struct_initializer(token, init);
            token = skip_end(token);
        } else {
            struct Node *node = assign(rest, token);
            add_type(node);
            if (node->ty->ty == TY_STRUCT) {
                init->expr = node;
                return;
            }
            token = struct_initializer2(token, init);
        }
    } else if (init->ty->ty == TY_UNION) {
        token = union_initializer(token, init);
    } else if (init->ty->ty == TY_ARRAY && equal_keyword(token, TK_STR)) {
        token = string_initializer(token, init);
    } else if (init->ty->ty == TY_ARRAY) {
        if (equal(token, "{")) {
            token = skip(token, "{");
            token = array_initializer(token, init);
            token = skip_end(token);
        } else {
            token = array_initializer2(token, init);
        }
    } else if (equal(token, "{")) {
        token = skip(token, "{");
        init->expr = assign(&token, token);
        token = skip_end(token);
    } else {
        init->expr = assign(&token, token);
    }
    *rest = token;
}

struct Initializer *initializer(struct Token **rest, struct Token *token,
                                struct Type *ty, struct Type **new_type) {
    struct Initializer *init = new_initializer(ty, true);
    internal_initializer(rest, token, init);
    *new_type = init->ty;
    return init;
}

struct Node *init_desg_expr(struct InitDesg *desg) {
    if (desg->var) {
        return new_node_var(desg->var);
    }

    if (desg->member) {
        struct Node *node =
            new_node_unary(ND_MEMBER, init_desg_expr(desg->next));
        node->member = desg->member;
        return node;
    }

    struct Node *lhs = init_desg_expr(desg->next);
    struct Node *rhs = new_node_num(desg->index);
    return new_node_unary(ND_DEREF, new_node_add(lhs, rhs));
}

struct Node *create_lvar_init(struct Initializer *init, struct Type *ty,
                              struct InitDesg *desg) {
    if (ty->ty == TY_ARRAY) {
        struct Node *node = NULL;
        for (int i = 0; i < ty->array_size; i++) {
            struct InitDesg desg2 = {desg, i};
            struct Node *rhs =
                create_lvar_init(init->children[i], ty->ptr_to, &desg2);
            node = new_node_binary(ND_COMMA, node, rhs);
        }
        return node;
    }

    if (ty->ty == TY_STRUCT && !init->expr) {
        struct Node *node = new_node(ND_DUMMY);

        for (struct Member *member = ty->member; member != NULL;
             member = member->next) {
            struct InitDesg desg2 = {desg, 0, NULL, member};
            struct Node *rhs = create_lvar_init(init->children[member->index],
                                                member->ty, &desg2);
            node = new_node_binary(ND_COMMA, node, rhs);
        }
        return node;
    }

    if (ty->ty == TY_UNION) {
        struct InitDesg desg2 = {desg, 0, NULL, ty->member};
        return create_lvar_init(init->children[0], ty->member->ty, &desg2);
    }

    if (init->expr == NULL) {
        return new_node(ND_DUMMY);
    }

    struct Node *lhs = init_desg_expr(desg);
    return new_node_binary(ND_ASSIGN, lhs, init->expr);
}

struct Node *lvar_initializer(struct Token **rest, struct Token *token,
                              struct Object *var) {
    struct Initializer *init = initializer(rest, token, var->ty, &var->ty);
    struct InitDesg desg = {NULL, 0, var, NULL};
    struct Node *lhs = new_node(ND_MEMZERO);
    lhs->obj = var;
    struct Node *rhs = create_lvar_init(init, var->ty, &desg);
    return new_node_binary(ND_COMMA, lhs, rhs);
}

static void gvar_initializer(struct Token **rest, struct Token *token,
                             struct Object *gvar) {
    struct Initializer *init = initializer(rest, token, gvar->ty, &gvar->ty);

    struct Relocation head = {};
    char *buf = calloc(1, gvar->ty->size);
    write_gvar_data(&head, init, init->ty, buf, 0);
    gvar->init_data = buf;
    gvar->rel = head.next;
}

/*
typedef_decl = (declarator ("," declarator)* )? ";"
*/
void typedef_decl(struct Token **rest, struct Token *token,
                  struct Type *base_ty) {
    bool first = true;
    while (!equal(token, ";")) {
        if (!first) {
            token = skip(token, ",");
        }
        first = false;
        struct NameTag *tag = declarator(&token, token, base_ty);
        if (find_typedef_from_scope(tag->name) != NULL) {
            error("%sは既にtypedefされています", tag->name);
        }
        push_typedef_scope(tag);
    }
    *rest = skip(token, ";");
    return;
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
const_expr = conditional
*/
long const_expr(struct Token **rest, struct Token *token) {
    return eval(conditional(rest, token));
}

/*
expr = assign (, expr)?
*/
struct Node *expr(struct Token **rest, struct Token *token) {
    struct Node *node = assign(&token, token);
    if (equal(token, ",")) {
        token = skip(token, ",");
        return new_node_binary(ND_COMMA, node, expr(rest, token));
    }
    *rest = token;
    return node;
}

struct Node *to_assign(struct Node *binary) {
    add_type(binary->lhs);
    add_type(binary->rhs);

    struct NameTag tag = {};
    tag.name = "";
    tag.ty = pointer_to(binary->lhs->ty);
    struct Object *var = new_local_var(&tag);

    struct Node *expr1 = new_node_binary(ND_ASSIGN, new_node_var(var),
                                         new_node_unary(ND_ADDR, binary->lhs));
    struct Node *expr2 = new_node_binary(
        ND_ASSIGN, new_node_unary(ND_DEREF, new_node_var(var)),
        new_node_binary(binary->kind,
                        new_node_unary(ND_DEREF, new_node_var(var)),
                        binary->rhs));
    return new_node_binary(ND_COMMA, expr1, expr2);
}

/*
assign = conditional (assign_op assign)?
assign_op = "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "|=" | "^=" | "&=" | "<<="
| ">>="
*/
struct Node *assign(struct Token **rest, struct Token *token) {
    struct Node *node = conditional(&token, token);
    if (equal(token, "=")) {
        node = new_node_binary(ND_ASSIGN, node, assign(&token, token->next));
    } else if (equal(token, "+=")) {
        node = to_assign(new_node_add(node, assign(&token, token->next)));
    } else if (equal(token, "-=")) {
        node = to_assign(new_node_sub(node, assign(&token, token->next)));
    } else if (equal(token, "*=")) {
        node = to_assign(
            new_node_binary(ND_MUL, node, assign(&token, token->next)));
    } else if (equal(token, "/=")) {
        node = to_assign(
            new_node_binary(ND_DIV, node, assign(&token, token->next)));
    } else if (equal(token, "%=")) {
        node = to_assign(
            new_node_binary(ND_MOD, node, assign(&token, token->next)));
    } else if (equal(token, "|=")) {
        node = to_assign(
            new_node_binary(ND_BITOR, node, assign(&token, token->next)));
    } else if (equal(token, "^=")) {
        node = to_assign(
            new_node_binary(ND_BITXOR, node, assign(&token, token->next)));
    } else if (equal(token, "&=")) {
        node = to_assign(
            new_node_binary(ND_BITAND, node, assign(&token, token->next)));
    } else if (equal(token, "<<=")) {
        node = to_assign(
            new_node_binary(ND_SHL, node, assign(&token, token->next)));
    } else if (equal(token, ">>=")) {
        node = to_assign(
            new_node_binary(ND_SHR, node, assign(&token, token->next)));
    }
    *rest = token;
    return node;
}

/*
conditional = logor ("?" expr : conditional)?
*/
struct Node *conditional(struct Token **rest, struct Token *token) {
    struct Node *node = logor(&token, token);
    if (equal(token, "?")) {
        struct Node *cond = new_node(ND_COND);
        cond->cond = node;
        token = skip(token, "?");
        cond->then = expr(&token, token);
        token = skip(token, ":");
        cond->els = conditional(&token, token);
        node = cond;
    }
    *rest = token;
    return node;
}

/*
logor = logand ("||" logand)*
*/
struct Node *logor(struct Token **rest, struct Token *token) {
    struct Node *node = logand(&token, token);
    while (equal(token, "||")) {
        node = new_node_binary(ND_LOGOR, node, logand(&token, token->next));
    }
    *rest = token;
    return node;
}

/*
logand = bitor ("&&" bitor)*
*/
struct Node *logand(struct Token **rest, struct Token *token) {
    struct Node *node = bitor (&token, token);
    while (equal(token, "&&")) {
        node = new_node_binary(ND_LOGAND, node, bitor (&token, token->next));
    }
    *rest = token;
    return node;
}

/*
bitor = bitxor ("|" bitxor)*
*/
struct Node * bitor (struct Token * *rest, struct Token *token) {
    struct Node *node = bitxor(&token, token);
    while (equal(token, "|")) {
        node = new_node_binary(ND_BITOR, node, bitxor(&token, token->next));
    }
    *rest = token;
    return node;
}

/*
bitxor = bitand ("^" bitand)*
*/
struct Node *bitxor(struct Token **rest, struct Token *token) {
    struct Node *node = bitand(&token, token);
    while (equal(token, "^")) {
        node = new_node_binary(ND_BITXOR, node, bitand(&token, token->next));
    }
    *rest = token;
    return node;
}

/*
bitand = equality ("&" equality)*
*/
struct Node *bitand(struct Token **rest, struct Token *token) {
    struct Node *node = equality(&token, token);
    while (equal(token, "&")) {
        node = new_node_binary(ND_BITAND, node, equality(&token, token->next));
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
relation = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
*/
struct Node *relation(struct Token **rest, struct Token *token) {
    struct Node *node = shift(&token, token);

    for (;;) {
        if (equal(token, "<")) {
            node = new_node_binary(ND_LT, node, shift(&token, token->next));
        } else if (equal(token, "<=")) {
            node = new_node_binary(ND_LE, node, shift(&token, token->next));
        } else if (equal(token, ">")) {
            node = new_node_binary(ND_LT, shift(&token, token->next), node);
        } else if (equal(token, ">=")) {
            node = new_node_binary(ND_LE, shift(&token, token->next), node);
        } else {
            *rest = token;
            return node;
        }
    }
}

/*
shift = add ("<<" add | ">>" add)*
*/
struct Node *shift(struct Token **rest, struct Token *token) {
    struct Node *node = add(&token, token);

    for (;;) {
        if (equal(token, "<<")) {
            node = new_node_binary(ND_SHL, node, add(&token, token->next));
        } else if (equal(token, ">>")) {
            node = new_node_binary(ND_SHR, node, add(&token, token->next));
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
mul = cast ("*" cast | "/" cast | "%" cast)*
*/
struct Node *mul(struct Token **rest, struct Token *token) {
    struct Node *node = cast(&token, token);
    for (;;) {
        if (equal(token, "*")) {
            node = new_node_binary(ND_MUL, node, cast(&token, token->next));
        } else if (equal(token, "/")) {
            node = new_node_binary(ND_DIV, node, cast(&token, token->next));
        } else if (equal(token, "%")) {
            node = new_node_binary(ND_MOD, node, cast(&token, token->next));
        } else {
            *rest = token;
            return node;
        }
    }
}

/*
cast = "(" typename ")" cast | unary
*/
struct Node *cast(struct Token **rest, struct Token *token) {
    if (equal(token, "(") && is_typename(token->next)) {
        token = skip(token, "(");
        struct Type *ty = typename(&token, token);
        token = skip(token, ")");
        struct Node *node = new_node_cast(cast(&token, token), ty);
        *rest = token;
        return node;
    } else {
        struct Node *node = unary(rest, token);
        add_type(node);
        return node;
    }
}

/*
unary = ("+" | "-" | "*" | "&" | "!" | "~") cast | ("++" | "--") unary | postfix
*/
struct Node *unary(struct Token **rest, struct Token *token) {
    if (equal(token, "+")) {
        return cast(rest, token->next);
    } else if (equal(token, "-")) {
        return new_node_binary(ND_SUB, new_node_num(0),
                               cast(rest, token->next));
    } else if (equal(token, "*")) {
        return new_node_unary(ND_DEREF, cast(rest, token->next));
    } else if (equal(token, "&")) {
        return new_node_unary(ND_ADDR, cast(rest, token->next));
    } else if (equal(token, "++")) {
        return to_assign(
            new_node_binary(ND_ADD, unary(rest, token->next), new_node_num(1)));
    } else if (equal(token, "--")) {
        return to_assign(
            new_node_binary(ND_SUB, unary(rest, token->next), new_node_num(1)));
    } else if (equal(token, "!")) {
        return new_node_unary(ND_NOT, cast(rest, token->next));
    } else if (equal(token, "~")) {
        return new_node_unary(ND_BITNOT, cast(rest, token->next));
    } else {
        return postfix(rest, token);
    }
}

struct Node *new_node_inc_dec(struct Node *node, struct Token *token,
                              int addend) {
    add_type(node);
    struct Node *node2 =
        new_node_sub(to_assign(new_node_add(node, new_node_num(addend))),
                     new_node_num(addend));
    node2->ty = node->ty;
    return node2;
}

/*
postfix = primary ( "[" expr "]" | "." ident | "->" ident | "++" | "--")*
*/
struct Node *postfix(struct Token **rest, struct Token *token) {
    struct Node *node = primary(&token, token);
    while (true) {
        if (equal(token, "[")) {
            node = new_node_unary(
                ND_DEREF, new_node_add(node, expr(&token, token->next)));
            token = skip(token, "]");
            continue;
        }

        if (equal(token, ".")) {
            token = skip(token, ".");
            assert(token->kind == TK_IDENT);
            node = struct_union_ref(node, strndup(token->loc, token->len));
            token = skip_keyword(token, TK_IDENT);
            continue;
        }

        if (equal(token, "->")) {
            token = skip(token, "->");
            assert(token->kind == TK_IDENT);
            node = new_node_unary(ND_DEREF, node);
            node = struct_union_ref(node, strndup(token->loc, token->len));
            token = skip_keyword(token, TK_IDENT);
            continue;
        }

        if (equal(token, "++")) {
            node = new_node_inc_dec(node, token, 1);
            token = skip(token, "++");
            continue;
        }

        if (equal(token, "--")) {
            node = new_node_inc_dec(node, token, -1);
            token = skip(token, "--");
            continue;
        }
        break;
    }
    *rest = token;
    return node;
}

/*
funcall = ident "(" (assign ("," assign)*)? ")"
*/
struct Node *funccall(struct Token **rest, struct Token *token) {
    assert(token->kind == TK_IDENT);
    char *name = strndup(token->loc, token->len);
    struct Node *node = new_node(ND_FUNCALL);
    struct Object *func = find_object(functions, name);
    if (func == NULL) {
        error("関数 %s は定義されていません", name);
    }
    token = token->next;
    node->ty = func->ty->return_ty;
    node->obj = func;
    struct Node head = {};
    struct Node *cur = &head;
    assert(equal(token, "("));
    token = skip(token, "(");
    while (!equal(token, ")")) {
        if (cur != &head) token = skip(token, ",");
        cur = cur->next = assign(&token, token);
    }
    node->args = head.next;
    *rest = token->next;
    return node;
}

/*
primary =  "(" "{" compound_stmt ")" | "(" expr ")" | "sizeof" "(" typename ")"
| "sizeof" unary | ident funccall? | string | num
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
    } else if (equal(token, "sizeof") && equal(token->next, "(") &&
               is_typename(token->next->next)) {
        token = skip_keyword(token, TK_SIZEOF);
        token = skip(token, "(");
        node = new_node_num(typename(&token, token)->size);
        token = skip(token, ")");
        *rest = token;
    } else if (equal(token, "sizeof")) {
        node = unary(&token, token->next);
        add_type(node);
        node = new_node_num(node->ty->size);
        *rest = token;
    } else if (token->kind == TK_IDENT) {
        if (equal(token->next, "(")) {
            node = funccall(&token, token);
            *rest = token;
        } else {
            char *name = strndup(token->loc, token->len);
            struct VarScope *sc = find_variable(name);
            if (sc == NULL || (sc->var == NULL && sc->enum_ty == NULL)) {
                error("変数%sは定義されていません", name);
            }
            if (sc->var != NULL) {
                node = new_node_var(sc->var);
            } else {
                node = new_node_num(sc->enum_val);
            }
            *rest = token->next;
        }
    } else if (token->kind == TK_STR) {
        node = new_node_var(new_string_literal(token));
        *rest = token->next;
    } else if (token->kind == TK_NUM) {
        node = new_node_num(get_number(token));
        *rest = token->next;
    } else {
        error("parseエラー");
    }
    return node;
}