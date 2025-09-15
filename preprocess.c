#include "ecc.h"

static void print_token2(struct Token *token) {
    FILE *out = stdout;
    fprintf(out, "print_token2\n");
    for (bool start = true; token != NULL && token->kind != TK_EOF;
         token = token->next) {
        if (!start && token->is_begin) {
            fprintf(out, "\n");
        }
        if (!token->is_begin && token->has_space) {
            fprintf(out, " ");
        }
        fprintf(out, "%.*s", token->len, token->loc);
        start = false;
    }
    fprintf(out, "\n");
}

typedef enum Cond { If, Elif, Else } Cond;

struct CondIncl {
    struct CondIncl *outer;
    struct Token *token;
    Cond cond;
    bool in;
} *cond;

struct MacroParam {
    struct MacroParam *next;
    char *name;
};

struct MacroArgument {
    struct MacroArgument *next;
    char *name;
    struct Token *body;
};

struct Macro {
    struct Macro *prev;
    struct Macro *next;
    char *name;
    struct Token *body;
    struct MacroParam *params;
    bool is_objlike;
    bool is_erased;
} *macros;

static bool is_hash(struct Token *token) {
    return equal(token, "#") && token->is_begin;
}

static bool is_hash_and_keyword(struct Token *token, char *keyword) {
    return is_hash(token) && equal(token->next, keyword);
}

static struct Hideset *new_hideset(char *name) {
    struct Hideset *hideset = calloc(1, sizeof(struct Hideset));
    hideset->name = name;
    return hideset;
}

static struct Token *new_eof_token() {
    struct Token *token = calloc(1, sizeof(struct Token));
    token->kind = TK_EOF;
    return token;
}

static struct Token *copy_token(struct Token *src) {
    struct Token *dst = calloc(1, sizeof(struct Token));
    *dst = *src;
    dst->next = NULL;
    return dst;
}

static struct Token *copy_line(struct Token **rest, struct Token *src) {
    struct Token head = {};
    struct Token *cur = &head;
    for (; !src->is_begin; src = src->next) {
        cur = cur->next = copy_token(src);
    }
    cur->next = new_eof_token();
    *rest = src;
    return head.next;
}

static long eval_const_expr(struct Token **rest, struct Token *token) {
    struct Token *line = copy_line(rest, token);
    line = preprocess(line);
    return const_expr(&line, line);
}

static struct Token *skip_line(struct Token *token, bool warning_option) {
    if (token->is_begin) {
        return token;
    }
    if (warning_option) {
        warning_token(token, "extra token");
    }
    while (!token->is_begin) {
        token = token->next;
    }
    return token;
}

static struct Token *skip_cond_incl(struct Token *token) {
    while (token->kind != TK_EOF) {
        if (is_hash_and_keyword(token, "if") ||
            is_hash_and_keyword(token, "ifdef") ||
            is_hash_and_keyword(token, "ifndef")) {
            token = skip_line(token->next, false);
            token = skip_cond_incl(token);
            while (is_hash_and_keyword(token, "elif")) {
                token = skip_line(token->next, false);
                token = skip_cond_incl(token);
            }
            if (is_hash_and_keyword(token, "else")) {
                token = skip_line(token->next, false);
                token = skip_cond_incl(token);
            }
            if (!is_hash_and_keyword(token, "endif")) {
                error_token(token, "expected endif");
            }
            token = skip_line(token->next->next, false);
            continue;
        }
        if (is_hash_and_keyword(token, "elif") ||
            is_hash_and_keyword(token, "else") ||
            is_hash_and_keyword(token, "endif")) {
            break;
        }
        token = token->next;
    }
    return token;
}

static struct Hideset *hideset_union(struct Hideset *first,
                                     struct Hideset *second) {
    struct Hideset head = {};
    struct Hideset *cur = &head;
    for (struct Hideset *itr = first; itr != NULL; itr = itr->next) {
        cur = cur->next = new_hideset(itr->name);
    }
    cur->next = second;
    return head.next;
}

static bool hideset_contains(struct Hideset *hideset, char *name) {
    for (struct Hideset *itr = hideset; itr != NULL; itr = itr->next) {
        if (!strcmp(itr->name, name)) {
            return true;
        }
    }
    return false;
}

static struct Hideset *hideset_intersection(struct Hideset *first,
                                            struct Hideset *second) {
    struct Hideset head = {};
    struct Hideset *cur = &head;
    for (struct Hideset *itr = first; itr != NULL; itr = itr->next) {
        if (hideset_contains(second, itr->name)) {
            cur = cur->next = new_hideset(itr->name);
        }
    }
    return head.next;
}

static struct Token *add_hideset(struct Token *token, struct Hideset *hideset) {
    struct Token head = {};
    struct Token *cur = &head;
    for (struct Token *itr = token; itr != NULL; itr = itr->next) {
        cur = cur->next = copy_token(itr);
        cur->hideset = hideset_union(cur->hideset, hideset);
    }
    return head.next;
}

static struct Token *concat(struct Token *first, struct Token *second) {
    struct Token head = {};
    struct Token *cur = &head;
    while (first != NULL && first->kind != TK_EOF) {
        cur = cur->next = copy_token(first);
        first = first->next;
    }
    cur->next = second;
    return head.next;
}

static void push_cond_incl(struct Token *token, int val) {
    struct CondIncl *next = calloc(1, sizeof(struct CondIncl));
    next->outer = cond;
    next->token = token;
    next->cond = If;
    next->in = val != 0;
    cond = next;
}

static void pop_cond_incl() {
    assert(cond != NULL);
    cond = cond->outer;
}

static struct Macro *find_macro(struct Token *token) {
    if (token->kind != TK_IDENT) {
        return NULL;
    }
    for (struct Macro *cur = macros; cur != NULL; cur = cur->next) {
        if (strlen(cur->name) == token->len &&
            !strncmp(cur->name, token->loc, token->len)) {
            return cur;
        }
    }
    return NULL;
}

static struct MacroArgument *
find_macro_argument(struct MacroArgument *arguments, struct Token *token) {
    if (token->kind != TK_IDENT) {
        return NULL;
    }
    for (struct MacroArgument *cur = arguments; cur != NULL; cur = cur->next) {
        if (strlen(cur->name) == token->len &&
            !strncmp(cur->name, token->loc, token->len)) {
            return cur;
        }
    }
    return NULL;
}

static struct MacroArgument *read_one_macro_argument(struct Token **rest,
                                                     struct Token *token) {
    struct MacroArgument *arg = calloc(1, sizeof(struct MacroArgument));
    struct Token head = {};
    struct Token *cur = &head;
    int depth = 0;
    while (depth > 0 || (!equal(token, ")") && !equal(token, ","))) {
        if (equal(token, "("))
            depth++;
        if (equal(token, ")"))
            depth--;
        cur = cur->next = copy_token(token);
        token = token->next;
    }
    *rest = token;
    cur->next = new_eof_token();
    arg->body = head.next;
    return arg;
}

static struct MacroArgument *raed_macro_argument(struct Token **rest,
                                                 struct Token *token,
                                                 struct MacroParam *params) {
    struct MacroArgument head = {};
    struct MacroArgument *cur = &head;
    for (struct MacroParam *p = params; p != NULL; p = p->next) {
        if (cur != &head) {
            token = skip(token, ",");
        }
        cur = cur->next = read_one_macro_argument(&token, token);
        cur->name = p->name;
    }
    assert(equal(token, ")"));
    *rest = token;
    return head.next;
}

static struct MacroParam *read_macro_params(struct Token **rest,
                                            struct Token *token) {
    struct MacroParam head = {};
    struct MacroParam *cur = &head;
    while (!equal(token, ")")) {
        if (cur != &head) {
            token = skip(token, ",");
        }
        struct MacroParam *param = calloc(1, sizeof(struct MacroParam));
        assert(token->kind == TK_IDENT);
        param->name = strndup(token->loc, token->len);
        cur = cur->next = param;
        token = token->next;
    }
    *rest = skip(token, ")");
    return head.next;
}

int a = 0;

static struct Token *expand_funclike_macro(struct Token *token,
                                           struct MacroArgument *arguments) {
    struct Token head = {};
    struct Token *cur = &head;
    for (; token->kind != TK_EOF; token = token->next) {
        struct MacroArgument *arg = find_macro_argument(arguments, token);
        if (arg != NULL) {
            a = 1;
            struct Token *body = preprocess(arg->body);
            for (; body->kind != TK_EOF; body = body->next) {
                cur = cur->next = copy_token(body);
            }
        } else {
            cur = cur->next = copy_token(token);
        }
    }
    cur->next = new_eof_token();
    return head.next;
}

static bool expand_macro(struct Token **rest, struct Token *token) {
    if (token->kind != TK_IDENT)
        return false;
    if (hideset_contains(token->hideset, strndup(token->loc, token->len))) {
        return false;
    }
    struct Macro *m = find_macro(token);
    if (m == NULL || m->is_erased) {
        return false;
    } else if (m->is_objlike) {
        struct Token *body = add_hideset(
            m->body, hideset_union(token->hideset, new_hideset(m->name)));
        *rest = concat(body, token->next);
        return true;
    } else {
        struct Token *macro_token = token;
        token = token->next;
        if (!equal(token, "(")) {
            return false;
        }
        struct MacroArgument *args =
            raed_macro_argument(&token, token->next, m->params);
        struct Token *rparen = token;
        token = skip(token, ")");
        struct Hideset *hideset =
            hideset_intersection(token->hideset, rparen->hideset);
        hideset = hideset_union(hideset, new_hideset(m->name));
        *rest = concat(
            add_hideset(expand_funclike_macro(m->body, args), hideset), token);
        return true;
    }
}

static void add_macro(char *name,
                      bool is_objlike,
                      struct MacroParam *params,
                      struct Token *body) {
    struct Macro *macro = calloc(1, sizeof(struct Macro));
    macro->name = name;
    macro->is_objlike = is_objlike;
    macro->params = params;
    macro->body = body;
    macro->next = macros;
    macros = macro;
}

static void define_macro(struct Token **rest, struct Token *token) {
    assert(token->kind == TK_IDENT);
    char *name = strndup(token->loc, token->len);
    token = token->next;
    if (!token->has_space && equal(token, "(")) {
        struct MacroParam *params = read_macro_params(&token, token->next);
        add_macro(name, false, params, copy_line(rest, token));
    } else {
        add_macro(name, true, NULL, copy_line(rest, token));
    }
}

static void undef_macro(struct Token *token) {
    assert(token->kind == TK_IDENT);
    struct Macro *macro = calloc(1, sizeof(struct Macro));
    macro->name = strndup(token->loc, token->len);
    macro->is_erased = true;
    if (macros == NULL) {
        macros = macro;
    }
    macro->next = macros;
    macros = macro;
}

bool is_defined(struct Token *token) {
    struct Macro *macro = find_macro(token);
    if (macro == NULL) {
        return false;
    }
    return !macro->is_erased;
}

struct Token *preprocess(struct Token *token) {
    struct Token head = {};
    struct Token *cur = &head;
    while (token->kind != TK_EOF) {
        assert(token != NULL);
        struct Token *start = token;

        if (expand_macro(&token, token)) {
            continue;
        }

        if (!is_hash(token)) {
            cur = cur->next = token;
            token = token->next;
            continue;
        }

        token = token->next;

        if (token->is_begin) {
            continue;
        }

        if (equal(token, "include")) {
            token = token->next;
            if (token->kind != TK_STR) {
                error_token(token, "expected a filename");
            }
            char *path =
                format("%s/%s", dirname(strdup(token->file->path)), token->str);
            struct Token *include_token = tokenize_file(path);
            if (include_token == NULL) {
                error_token(start, "%s\n", strerror(errno));
            }
            token = skip_line(token->next, true);
            token = concat(include_token, token);
            continue;
        }

        if (equal(token, "if")) {
            long val = eval_const_expr(&token, token->next);
            push_cond_incl(start, val);
            if (!cond->in) {
                token = token->next = skip_cond_incl(token);
            }
            continue;
        }

        if (equal(token, "ifdef")) {
            token = token->next;
            push_cond_incl(start, is_defined(token));
            if (!cond->in) {
                token = token->next = skip_cond_incl(token->next);
            } else {
                token = skip_line(token->next, true);
            }
            continue;
        }

        if (equal(token, "ifndef")) {
            token = token->next;
            push_cond_incl(start, !is_defined(token));
            if (!cond->in) {
                token = token->next = skip_cond_incl(token->next);
            } else {
                token = skip_line(token->next, true);
            }
            continue;
        }

        if (equal(token, "elif")) {
            if (cond == NULL || cond->cond == Else) {
                error_token(token, "stray #elif");
            }
            cond->cond = Elif;
            if (cond->in) {
                token = token->next = skip_cond_incl(token->next);
            } else {
                long val = eval_const_expr(&token, token->next);
                if (val != 0) {
                    cond->in = true;
                } else {
                    token = token->next = skip_cond_incl(token);
                }
            }
            continue;
        }

        if (equal(token, "else")) {
            if (cond == NULL || cond->cond == Else) {
                error_token(token, "stray #else");
            }
            token = skip_line(token->next, true);
            if (cond->in) {
                token = skip_cond_incl(token);
            }
            cond->cond = Else;
            cond->in = true;
            continue;
        }

        if (equal(token, "endif")) {
            pop_cond_incl();
            token = skip_line(token->next, true);
            continue;
        }

        if (equal(token, "define")) {
            token = token->next;
            if (token->kind != TK_IDENT) {
                error_token(token, "expected ident");
            }
            define_macro(&token, token);
            continue;
        }

        if (equal(token, "undef")) {
            token = token->next;
            if (token->kind != TK_IDENT) {
                error_token(token, "expected ident");
            }
            undef_macro(token);
            token = skip_line(token->next, true);
            continue;
        }

        error_token(token, "invalid preprocessor directive");
    }
    cur->next = token;
    return head.next;
}