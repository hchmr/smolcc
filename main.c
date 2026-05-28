#include <stdarg.h>

//=============================================================================
//= unistd

extern int open(const char *pathname, int flags, int mode);
extern int read(int fd, void *buf, int count);
extern int write(int fd, const void *buf, int count);
extern void _exit(int status);
extern void abort();

//=============================================================================
//= io

enum { EOF = -1 };

static void write_str(int fd, const char *s) {
    const char *p = s;
    while (*p++)
        ;
    write(fd, s, p - s - 1);
}

static void write_char(int fd, int c) {
    write(fd, &c, 1);
}

static void write_int(int fd, int n) {
    if (n == -2147483647 - 1) {
        write_str(fd, "-2147483648");
        return;
    }
    if (n < 0) {
        write_char(fd, '-');
        n = -n;
    }
    if (n >= 10) {
        write_int(fd, n / 10);
    }
    write_char(fd, n % 10 + '0');
}

static void vwritef(int fd, const char *fmt, va_list *args) {
    for (; *fmt; fmt++) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                write_int(fd, va_arg(*args, int));
            } else if (*fmt == 'c') {
                write_char(fd, va_arg(*args, int));
            } else if (*fmt == 's') {
                const char *s = va_arg(*args, const char *);
                write_str(fd, s ? s : "(null)");
            } else if (*fmt == '%') {
                write_char(fd, '%');
            } else {
                // undefined behavior
            }
        } else {
            write_char(fd, *fmt);
        }
    }
}

static void writef(int fd, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vwritef(fd, fmt, &args);
    va_end(args);
}

//=============================================================================
//= misc

static int str_eq(const char *a, const char *b) {
    while (*a && *b)
        if (*a++ != *b++)
            return 0;
    return *a == *b;
}

static int align_up(int n, int align) {
    return (n + align - 1) / align * align;
}

static const char *find_chr(const char *s, int c) {
    for (; *s; s++)
        if (c == *s)
            return s;
    return 0;
}

//=============================================================================
//= assertions

static void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vwritef(2, fmt, &args);
    va_end(args);
    abort();
}

static void assert(const char *label, int condition) {
    if (!condition)
        die("%s: assertion failed", label);
}

//=============================================================================
//= arena

enum { Arena_Cap = 8 * 1024 * 1024 };  // 8 MiB
static char arena[Arena_Cap];
static int arena_len;

static void *alloc(int len) {
    arena_len = align_up(arena_len, 8) + len;
    if (arena_len >= Arena_Cap)
        die("alloc: out of memory");
    return arena + arena_len - len;
}

static void *mem_clone(void *s, int len) {
    void *res = alloc(len);
    for (int i = 0; i < len; i++) {
        ((char *)res)[i] = ((char *)s)[i];
    }
    return res;
}

static struct string {
    int label, len;
    char *chars;
    struct string *next;
} *strings;

static struct string *intern(const char *s, int len) {
    for (struct string *str = strings; str; str = str->next)
        if (str_eq(str->chars, s))
            return str;
    struct string *mk_str = alloc(sizeof(struct string));
    mk_str->chars = mem_clone((void *)s, len + 1);
    mk_str->len = len;
    mk_str->next = strings;
    strings = mk_str;
    return mk_str;
}

//=============================================================================
//= diag

struct pos {
    const char *file;
    int line, col;
};

static void err_at(struct pos *pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    writef(2, "%s:%d:%d: error: ", pos->file, pos->line, pos->col);
    vwritef(2, fmt, &args);
    va_end(args);
    _exit(1);
}

//=============================================================================
//= symbols

enum { MAX_FUNC_PARAMS = 8 };

enum {
    Sym_Global = 1,
    Sym_Func,
    Sym_Struct,
    Sym_Const,
    Sym_Local,
    Sym_Field,
};

enum {
    Extern = 1,
    Static = 2,
};

struct sym {
    int kind;
    struct pos last_pos;
    int storage;
    const char *name;
    struct ty *ty;
    int is_defined;

    // for Sym_Const
    int val;
    // for Sym_Func
    struct stmt *body;
    struct sym *last_param;
    int va_offs, va_size;
    // for Sym_Struct and Sym_Func
    struct scope *scope;
    int size, align;
    // for Sym_Local and Sym_Field
    int offs;

    struct sym *next;
};

//=============================================================================
//= types

enum {
    Ty_Void,
    Ty_Char,
    Ty_Int,
    Ty_Ptr,
    Ty_Arr,
    Ty_Func,
    Ty_Struct,
    Ty_VaList,
};

struct ty {
    int kind;
    // for Ty_Ptr and Ty_Arr
    struct ty *ptr_to;
    // for Ty_Arr
    int arr_len;
    // for Ty_Func
    struct ty *ret_ty;
    struct ty *param_tys[MAX_FUNC_PARAMS];
    int n_params, is_va;
    // for Ty_Struct
    struct sym *sym;
    // for type interning
    struct ty *next;
};

static struct ty *tys, *void_ty, *char_ty, *int_ty, *va_list_ty;

static int ty_size(struct ty *t) {
    if (t->kind == Ty_Char) {
        return 1;
    } else if (t->kind == Ty_Int) {
        return 4;
    } else if (t->kind == Ty_Ptr) {
        return 8;
    } else if (t->kind == Ty_Arr) {
        return ty_size(t->ptr_to) * t->arr_len;
    } else if (t->kind == Ty_Struct) {
        return t->sym->is_defined ? t->sym->size : 0;
    } else if (t->kind == Ty_VaList) {
        return 32;
    } else {
        return 0;
    }
}

static int ty_align(struct ty *t) {
    if (t->kind == Ty_Arr) {
        return ty_align(t->ptr_to);
    } else if (t->kind == Ty_Struct) {
        return t->sym->is_defined ? t->sym->align : 0;
    } else if (t->kind == Ty_VaList) {
        return 8;
    } else {
        return ty_size(t);
    }
}

// type predicates

static int ty_eq(struct ty *a, struct ty *b) {
    if (a == b) {
        return 1;
    } else if (a->kind != b->kind) {
        return 0;
    } else if (a->kind == Ty_Ptr) {
        return a->ptr_to == b->ptr_to;
    } else if (a->kind == Ty_Arr) {
        return a->arr_len == b->arr_len && a->ptr_to == b->ptr_to;
    } else if (a->kind == Ty_Func) {
        if (a->ret_ty != b->ret_ty || a->n_params != b->n_params || a->is_va != b->is_va)
            return 0;
        for (int i = 0; i < a->n_params; i++)
            if (a->param_tys[i] != b->param_tys[i])
                return 0;
        return 1;
    } else if (a->kind == Ty_Struct) {
        return a->sym == b->sym;
    } else {
        return 1;
    }
}

static int is_integer_ty(struct ty *t) {
    return t->kind == Ty_Int || t->kind == Ty_Char;
}
static int is_void_ty(struct ty *t) {
    return t->kind == Ty_Void;
}
static int is_ptr_ty(struct ty *t) {
    return t->kind == Ty_Ptr;
}
static int is_scalar(struct ty *t) {
    return is_integer_ty(t) || is_ptr_ty(t);
}
static int is_func_ty(struct ty *t) {
    return t->kind == Ty_Func;
}
static int is_void_ptr(struct ty *t) {
    return is_ptr_ty(t) && t->ptr_to->kind == Ty_Void;
}
static int is_arr_ty(struct ty *t) {
    return t->kind == Ty_Arr;
}
static int is_object_ty(struct ty *t) {
    return ty_size(t) > 0;
}

static struct ty *intern_ty(struct ty *t) {
    for (struct ty *u = tys; u; u = u->next)
        if (ty_eq(t, u))
            return u;
    struct ty *u = mem_clone(t, sizeof(struct ty));
    u->next = tys;
    tys = u;
    return u;
}

static struct ty *mk_ptr_ty(struct ty *base) {
    struct ty t;
    t.kind = Ty_Ptr;
    t.ptr_to = base;
    return intern_ty(&t);
}

static struct ty *mk_arr_ty(struct ty *base, int len) {
    struct ty t;
    t.kind = Ty_Arr;
    t.ptr_to = base;
    t.arr_len = len;
    return intern_ty(&t);
}

static struct ty *mk_struct_ty(struct sym *sym) {
    struct ty t;
    t.kind = Ty_Struct;
    t.sym = sym;
    return intern_ty(&t);
}

static struct ty *mk_func_ty(struct ty *ret_ty, struct ty **param_tys, int n_params, int is_va) {
    struct ty t;
    t.kind = Ty_Func;
    t.ret_ty = ret_ty;
    for (int i = 0; i < n_params; i++) {
        t.param_tys[i] = param_tys[i];
    }
    t.n_params = n_params;
    t.is_va = is_va;
    return intern_ty(&t);
}

static struct ty *uac_ty(struct ty *t1, struct ty *t2) {
    assert("uac_ty", is_integer_ty(t1) && is_integer_ty(t2));
    return t1->kind < t2->kind ? t2 : t1;
}

static struct ty *get_common_ptr_ty(struct ty *t1, struct ty *t2) {
    assert("get_common_ptr_ty", is_ptr_ty(t1) && is_ptr_ty(t2));
    if (is_void_ptr(t1) || is_void_ptr(t2))
        return mk_ptr_ty(void_ty);
    return t1->ptr_to == t2->ptr_to ? t1 : 0;
}

static void init_tys() {
    struct ty tmp;
    tmp.kind = Ty_Void;
    void_ty = intern_ty(&tmp);
    tmp.kind = Ty_Char;
    char_ty = intern_ty(&tmp);
    tmp.kind = Ty_Int;
    int_ty = intern_ty(&tmp);
    tmp.kind = Ty_VaList;
    va_list_ty = intern_ty(&tmp);
}

//=============================================================================
//= symbol table

static struct scope {
    struct sym *head, *tail;
    struct scope *parent;
} *curr_scope;

static struct scope *push_scope() {
    struct scope *scope = alloc(sizeof(struct scope));
    scope->parent = curr_scope;
    return curr_scope = scope;
}

static void pop_scope() {
    curr_scope = curr_scope->parent;
}

static struct sym *add_sym(struct pos *pos, struct scope *scope, int kind, const char *name) {
    struct sym *sym = alloc(sizeof(struct sym));
    sym->kind = kind;
    sym->name = name;
    sym->last_pos = *pos;
    if (scope->head)
        return scope->tail->next = sym, scope->tail = sym;
    else
        return scope->head = scope->tail = sym;
}

static struct sym *lookup_in(struct scope *scope, int is_struct, const char *name) {
    if (!name)
        return 0;
    for (struct sym *sym = scope->head; sym; sym = sym->next)
        if ((sym->kind == Sym_Struct) == is_struct && sym->name == name)
            return sym;
    return 0;
}

static struct sym *lookup(int ns, const char *name) {
    struct sym *sym;
    for (struct scope *s = curr_scope; s; s = s->parent)
        if ((sym = lookup_in(s, ns, name)))
            return sym;
    return 0;
}

static void decl_conflict(struct pos *pos, struct sym *sym, const char *msg) {
    struct pos other = sym->last_pos;
    err_at(pos, "'%s' %s. Previous declaration at %s:%d:%d", sym->name, msg, other.file, other.line, other.col);
}

static struct sym *declare(struct pos *pos, struct sym *parent_sym, int kind, int storage, const char *name,
                           struct ty *ty, int is_def) {
    struct scope *scope = curr_scope;
    if (parent_sym && parent_sym->kind == Sym_Struct) {
        scope = parent_sym->scope;
    }

    struct sym *sym = lookup_in(scope, kind == Sym_Struct, name);
    if (sym) {
        if (sym->kind != kind)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        if (kind != Sym_Struct && sym->ty != ty)
            decl_conflict(pos, sym, "already declared with a different type");
        if (is_def && sym->is_defined)
            decl_conflict(pos, sym, "already defined");
        if (kind == Sym_Global && sym->storage == Static && storage == 0)
            decl_conflict(pos, sym, "already declared as static");
        if ((kind == Sym_Global || kind == Sym_Func) && sym->storage != Static && storage == Static)
            decl_conflict(pos, sym, "already declared as non-static");
        if (kind != Sym_Struct) {
            sym->is_defined = sym->is_defined | is_def;
        }
        return sym;
    }
    sym = add_sym(pos, scope, kind, name);
    sym->ty = kind == Sym_Struct ? mk_struct_ty(sym) : ty;
    sym->is_defined = is_def;
    sym->storage = storage;
    if (parent_sym && (kind == Sym_Local || kind == Sym_Field)) {
        int size = ty_size(ty), align = ty_align(ty);
        if (kind == Sym_Local) {
            sym->offs = -align_up(parent_sym->size + size, align);
            parent_sym->size = -sym->offs;
        } else {
            sym->offs = align_up(parent_sym->size, align);
            parent_sym->size = sym->offs + size;
        }

        if (parent_sym->align < align) {
            parent_sym->align = align;
        }
    }

    return sym;
}

//=============================================================================
//= ast

enum {
    Expr_Comma = 1,
    Expr_Assign,
    Expr_Cond,
    Expr_Or,
    Expr_And,
    Expr_BitOr,
    Expr_BitXor,
    Expr_BitAnd,
    Expr_Eq,
    Expr_Ne,
    Expr_Lt,
    Expr_Le,
    Expr_Gt,
    Expr_Ge,
    Expr_Shl,
    Expr_Shr,
    Expr_Add,
    Expr_Sub,
    Expr_Mul,
    Expr_Div,
    Expr_Mod,
    Expr_Cast,
    Expr_Neg,
    Expr_BitNot,
    Expr_Not,
    Expr_Addr,
    Expr_Deref,
    Expr_PtrAdd,
    Expr_PtrSub,
    Expr_PtrDiff,
    Expr_Var,
    Expr_Func,
    Expr_Call,
    Expr_Member,
    Expr_PostInc,
    Expr_PostDec,
    Expr_Num,
    Expr_Chr,
    Expr_Str,
    Expr_VaStart,
    Expr_VaEnd,
    Expr_VaArg,
};

struct expr {
    int kind;
    struct pos pos;
    struct ty *ty;

    int int_val;
    struct string *str_val;
    const char *fld_name;
    struct expr **subs;
    int n_subs;

    struct sym *sym;  // resolved symbol
};

enum {
    Stmt_Block,
    Stmt_Decl,
    Stmt_If,
    Stmt_Loop,
    Stmt_Return,
    Stmt_Break,
    Stmt_Continue,
    Stmt_Expr,
    Stmt_Empty,
};

struct stmt {
    int kind;
    struct pos pos;

    struct stmt *sub;
    struct sym *sym;
    struct expr *expr;
    struct ty *ty;

    int loop_id;

    struct stmt *next;
};

static struct expr *mk_expr(struct pos *pos, int kind, int n_args) {
    struct expr *e = alloc(sizeof(struct expr));
    e->pos = *pos;
    e->kind = kind;
    e->n_subs = n_args;
    e->subs = alloc(sizeof(struct expr *) * n_args);
    return e;
}

static struct expr *mk_unary_expr(struct pos *pos, int kind, struct expr *sub) {
    struct expr *e = mk_expr(pos, kind, 1);
    e->subs[0] = sub;
    return e;
}

static struct expr *mk_bin_expr(int kind, struct expr *e1, struct expr *e2) {
    struct expr *e = mk_expr(&e1->pos, kind, 2);
    e->subs[0] = e1;
    e->subs[1] = e2;
    return e;
}

static struct stmt *mk_stmt(struct pos *pos, int kind) {
    struct stmt *s = alloc(sizeof(struct stmt));
    s->pos = *pos;
    s->kind = kind;
    return s;
}

static int is_lvalue(struct expr *e) {
    return e->kind == Expr_Var || e->kind == Expr_Deref || e->kind == Expr_Member;
}
static int is_addressable(struct expr *e) {
    return is_lvalue(e) || e->kind == Expr_Func || e->kind == Expr_Str;
}
static int is_assignable(struct expr *e) {
    return is_lvalue(e) && is_object_ty(e->ty);
}
static int is_null_ptr(struct expr *e) {
    return e->kind == Expr_Num && e->int_val == 0
        || e->kind == Expr_Cast && is_void_ptr(e->ty) && is_null_ptr(e->subs[0]);
}

//=============================================================================
//= eval

static int const_cast(int v, struct ty *t) {
    if (t->kind == Ty_Char)
        return v = v & 255, v >= 128 ? v - 256 : v;
    else if (t->kind == Ty_Int)
        return (int)v;
    else
        die("const_cast: unreachable: %d", t->kind);
}

static int eval(struct expr *e) {
    if (e->kind == Expr_Num) {
        return e->int_val;
    } else if (e->kind == Expr_Neg) {
        return -eval(e->subs[0]);
    } else if (e->kind == Expr_Add) {
        return eval(e->subs[0]) + eval(e->subs[1]);
    } else if (e->kind == Expr_Sub) {
        return eval(e->subs[0]) - eval(e->subs[1]);
    } else if (e->kind == Expr_Mul) {
        return eval(e->subs[0]) * eval(e->subs[1]);
    } else if (e->kind == Expr_Shl) {
        return eval(e->subs[0]) << eval(e->subs[1]);
    } else if (e->kind == Expr_Shr) {
        return eval(e->subs[0]) >> eval(e->subs[1]);
    } else if (e->kind == Expr_Cast) {
        if (is_ptr_ty(e->ty) && is_null_ptr(e->subs[0])) {
            return 0;
        } else if (is_integer_ty(e->ty) && is_integer_ty(e->subs[0]->ty)) {
            return const_cast(eval(e->subs[0]), e->ty);
        } else {
            err_at(&e->pos, "bad cast in constant expression");
        }
    } else {
        err_at(&e->pos, "expression cannot be evaluated at compile time");
    }
}

//=============================================================================
//= elab

static struct sym *curr_func;
static struct stmt *curr_loop;

static struct expr *wrap_with(int kind, struct ty *ty, struct expr *orig) {
    struct expr *wrapper = mk_expr(&orig->pos, kind, 1);
    wrapper->ty = ty;
    wrapper->subs[0] = orig;
    return wrapper;
}

// coercion
static struct expr *cast_to(struct ty *t, struct expr *e) {
    assert("cast_to", is_scalar(t) && is_scalar(e->ty));
    if (e->ty == t)
        return e;
    return wrap_with(Expr_Cast, t, e);
}

static void apply_ua_conv(struct expr **args) {
    assert("apply_ua_conv", is_integer_ty(args[0]->ty) && is_integer_ty(args[1]->ty));
    struct ty *target_ty = uac_ty(args[0]->ty, args[1]->ty);
    args[0] = cast_to(target_ty, args[0]);
    args[1] = cast_to(target_ty, args[1]);
}

static void apply_null_ptr_conv(struct expr **args) {
    if (is_null_ptr(args[0]) && is_ptr_ty(args[1]->ty)) {
        args[0] = cast_to(args[1]->ty, args[0]);
    } else if (is_null_ptr(args[1]) && is_ptr_ty(args[0]->ty)) {
        args[1] = cast_to(args[0]->ty, args[1]);
    }
}

static void unify_ptr_subs(struct expr **args) {
    assert("unify_ptr_subs", is_ptr_ty(args[0]->ty) && is_ptr_ty(args[1]->ty));
    struct ty *ptr_ty = get_common_ptr_ty(args[0]->ty, args[1]->ty);
    if (!ptr_ty)
        err_at(&args[0]->pos, "incompatible pointer operands");
    args[0] = cast_to(ptr_ty, args[0]);
    args[1] = cast_to(ptr_ty, args[1]);
}

static struct expr *apply_assign_conv(struct expr *rhs, struct ty *t) {
    if (rhs->ty == t) {
        return rhs;
    } else if (is_integer_ty(rhs->ty) && is_integer_ty(t)) {
        return cast_to(t, rhs);
    } else if (is_ptr_ty(t) && is_null_ptr(rhs)) {
        return cast_to(t, rhs);
    } else if (is_ptr_ty(t) && is_ptr_ty(rhs->ty)) {
        if (!(t->ptr_to == rhs->ty->ptr_to || is_void_ptr(t) || is_void_ptr(rhs->ty)))
            err_at(&rhs->pos, "target type mismatch");
        return cast_to(t, rhs);
    } else {
        err_at(&rhs->pos, "target type mismatch");
    }
}

static struct expr *ptr_decay(struct expr *e) {
    if (is_arr_ty(e->ty)) {
        return wrap_with(Expr_Cast, mk_ptr_ty(e->ty->ptr_to), wrap_with(Expr_Addr, mk_ptr_ty(e->ty), e));
    } else if (is_func_ty(e->ty)) {
        return wrap_with(Expr_Addr, mk_ptr_ty(e->ty), e);
    } else {
        return e;
    }
}

static struct expr *elab_expr(struct expr *e);

static void elab_subexprs(struct expr *e) {
    for (int i = 0; i < e->n_subs; i++) {
        e->subs[i] = elab_expr(e->subs[i]);
        if (e->kind != Expr_Addr) {
            e->subs[i] = ptr_decay(e->subs[i]);
        }
    }
}

static struct expr *elab_expr(struct expr *e) {
    elab_subexprs(e);

    if (e->kind == Expr_Num) {
        e->ty = int_ty;
    } else if (e->kind == Expr_Chr) {
        e->kind = Expr_Num;
        e->ty = char_ty;
    } else if (e->kind == Expr_Str) {
        e->ty = mk_arr_ty(char_ty, e->str_val->len + 1);
    } else if (e->kind == Expr_Var || e->kind == Expr_Func) {
        e->ty = e->sym->ty;
    } else if (e->kind == Expr_PostInc || e->kind == Expr_PostDec) {
        if (!is_assignable(e->subs[0]))
            err_at(&e->pos, "operand not assignable");
        if (!is_integer_ty(e->subs[0]->ty) && !is_ptr_ty(e->subs[0]->ty))
            err_at(&e->pos, "operand cannot be incremented/decremented");
        e->ty = e->subs[0]->ty;
    } else if (e->kind == Expr_Call) {
        struct expr *callee = e->subs[0];
        if (!is_ptr_ty(callee->ty) || callee->ty->ptr_to->kind != Ty_Func)
            err_at(&callee->pos, "called object is not a function");
        if (callee->kind != Expr_Addr || callee->subs[0]->kind != Expr_Func)
            err_at(&callee->pos, "indirect calls are not supported");
        struct ty *func = callee->subs[0]->sym->ty;
        struct expr **args = &e->subs[1];
        int n_args = e->n_subs - 1;
        if (n_args > func->n_params && !func->is_va)
            err_at(&e->pos, "too many arguments");
        if (n_args < func->n_params)
            err_at(&e->pos, "too few arguments");
        for (int i = 0; i < n_args; i++) {
            if (i < func->n_params) {
                args[i] = apply_assign_conv(args[i], func->param_tys[i]);
            } else {
                if (!is_scalar(args[i]->ty))
                    err_at(&args[i]->pos, "variadic arguments must be scalar");
            }
        }
        e->ty = func->ret_ty;
    } else if (e->kind == Expr_Member) {
        if (e->subs[0]->ty->kind != Ty_Struct)
            err_at(&e->pos, "member access on non-struct");
        struct sym *sym = e->subs[0]->ty->sym;
        if (!sym->is_defined)
            err_at(&e->pos, "member access on incomplete struct");
        struct sym *fld = lookup_in(sym->scope, 0, e->fld_name);
        if (!fld)
            err_at(&e->pos, "no such member");
        e->sym = fld;
        e->ty = fld->ty;
    } else if (e->kind == Expr_Addr) {
        if (!is_addressable(e->subs[0]))
            err_at(&e->pos, "operand not addressable");
        e->ty = mk_ptr_ty(e->subs[0]->ty);
    } else if (e->kind == Expr_Deref) {
        if (!is_ptr_ty(e->subs[0]->ty))
            err_at(&e->pos, "operand not a pointer");
        e->ty = e->subs[0]->ty->ptr_to;
    } else if (e->kind == Expr_Neg) {
        if (!is_integer_ty(e->subs[0]->ty))
            err_at(&e->pos, "operand must be integer");
        e->ty = e->subs[0]->ty;
    } else if (e->kind == Expr_BitNot) {
        if (!is_integer_ty(e->subs[0]->ty))
            err_at(&e->pos, "operand must be integer");
        e->ty = e->subs[0]->ty;
    } else if (e->kind == Expr_Not) {
        if (!is_scalar(e->subs[0]->ty))
            err_at(&e->pos, "operand must be scalar");
        e->ty = int_ty;
    } else if (e->kind == Expr_Cast) {
        if (!is_scalar(e->ty) || !is_scalar(e->subs[0]->ty))
            err_at(&e->pos, "cast requires scalar");
    } else if (e->kind == Expr_Mul || e->kind == Expr_Div || e->kind == Expr_Mod || e->kind == Expr_Add
               || e->kind == Expr_Sub) {
        if (e->kind == Expr_Add && is_integer_ty(e->subs[0]->ty) && is_ptr_ty(e->subs[1]->ty)) {
            struct expr *tmp = e->subs[0];
            e->subs[0] = e->subs[1], e->subs[1] = tmp;
        }
        if (e->kind == Expr_Add && is_ptr_ty(e->subs[0]->ty) && is_integer_ty(e->subs[1]->ty)) {
            e->kind = Expr_PtrAdd;
            e->ty = e->subs[0]->ty;
        } else if (e->kind == Expr_Sub && is_ptr_ty(e->subs[0]->ty) && is_integer_ty(e->subs[1]->ty)) {
            e->kind = Expr_PtrSub;
            e->ty = e->subs[0]->ty;
        } else if (e->kind == Expr_Sub && is_ptr_ty(e->subs[0]->ty) && is_ptr_ty(e->subs[1]->ty)) {
            if (e->subs[0]->ty != e->subs[1]->ty)
                err_at(&e->pos, "pointer types must match");
            e->kind = Expr_PtrDiff;
            e->ty = int_ty;  // standard doesn't mandate pointer-sized ptrdiff_t, so int is sufficient
        } else if (is_integer_ty(e->subs[0]->ty) && is_integer_ty(e->subs[1]->ty)) {
            apply_ua_conv(e->subs);
            e->ty = e->subs[0]->ty;
        } else {
            err_at(&e->pos, "operands must be arithmetic");
        }
    } else if (e->kind == Expr_Shl || e->kind == Expr_Shr) {
        if (!is_integer_ty(e->subs[0]->ty) || !is_integer_ty(e->subs[1]->ty))
            err_at(&e->pos, "operands must be integers");
        e->ty = e->subs[0]->ty;  // lhs determines type
    } else if (e->kind == Expr_Eq || e->kind == Expr_Ne) {
        apply_null_ptr_conv(e->subs);
        if (is_integer_ty(e->subs[0]->ty) && is_integer_ty(e->subs[1]->ty)) {
            apply_ua_conv(e->subs);
        } else if (is_ptr_ty(e->subs[0]->ty) && is_ptr_ty(e->subs[1]->ty)) {
            unify_ptr_subs(e->subs);
        } else {
            err_at(&e->pos, "operands must have compatible types");
        }
        e->ty = int_ty;
    } else if (e->kind == Expr_Lt || e->kind == Expr_Le || e->kind == Expr_Gt || e->kind == Expr_Ge) {
        if (is_integer_ty(e->subs[0]->ty) && is_integer_ty(e->subs[1]->ty)) {
            apply_ua_conv(e->subs);
        } else if (is_ptr_ty(e->subs[0]->ty) && is_ptr_ty(e->subs[1]->ty)) {
            if (!ty_eq(e->subs[0]->ty, e->subs[1]->ty))
                err_at(&e->pos, "pointer types must match");
        } else {
            err_at(&e->pos, "operands must both be integers or pointers");
        }
        e->ty = int_ty;
    } else if (e->kind == Expr_BitAnd || e->kind == Expr_BitXor || e->kind == Expr_BitOr) {
        if (!is_integer_ty(e->subs[0]->ty) || !is_integer_ty(e->subs[1]->ty))
            err_at(&e->pos, "operands must have integer types");
        apply_ua_conv(e->subs);
        e->ty = e->subs[0]->ty;
    } else if (e->kind == Expr_And || e->kind == Expr_Or) {
        if (!is_scalar(e->subs[0]->ty) || !is_scalar(e->subs[1]->ty))
            err_at(&e->pos, "operands must be scalar");
        e->ty = int_ty;
    } else if (e->kind == Expr_Cond) {
        apply_null_ptr_conv(e->subs + 1);
        if (is_void_ty(e->subs[1]->ty) && is_void_ty(e->subs[2]->ty)) {
            // pass
        } else if (is_integer_ty(e->subs[1]->ty) && is_integer_ty(e->subs[2]->ty)) {
            apply_ua_conv(e->subs + 1);
        } else if (is_ptr_ty(e->subs[1]->ty) && is_ptr_ty(e->subs[2]->ty)) {
            unify_ptr_subs(e->subs + 1);
        } else {
            err_at(&e->pos, "operands must have compatible types");
        }
        e->ty = e->subs[1]->ty;
    } else if (e->kind == Expr_Assign) {
        if (!is_assignable(e->subs[0]))
            err_at(&e->pos, "operand not assignable");
        e->subs[1] = apply_assign_conv(e->subs[1], e->subs[0]->ty);
        e->ty = e->subs[0]->ty;
    } else if (e->kind == Expr_Comma) {
        e->ty = e->subs[1]->ty;
    } else if (e->kind == Expr_VaStart) {
        if (!curr_func || curr_func->ty->kind != Ty_Func || !curr_func->ty->is_va)
            err_at(&e->pos, "va_start outside variadic function");
        if (e->subs[0]->ty != va_list_ty)
            err_at(&e->pos, "va_start operand must be va_list");
        if (e->subs[1]->kind != Expr_Var || e->subs[1]->sym != curr_func->last_param)
            err_at(&e->subs[1]->pos, "va_start second operand must be parameter name");
        e->ty = void_ty;
    } else if (e->kind == Expr_VaEnd) {
        if (e->subs[0]->ty != va_list_ty)
            err_at(&e->pos, "va_end operand must be va_list");
        e->ty = void_ty;
    } else if (e->kind == Expr_VaArg) {
        if (e->subs[0]->ty != va_list_ty)
            err_at(&e->pos, "va_arg first operand must be va_list");
        if (!is_scalar(e->ty))
            err_at(&e->pos, "va_arg second operand must be scalar");
    } else {
        die("elab_expr: unreachable: %d", e->kind);
    }
    return e;
}

static struct expr *elab_rvalue_expr(struct expr *e) {
    return ptr_decay(elab_expr(e));
}

static struct expr *elab_expr_expect(struct expr *e, struct ty *expected) {
    return apply_assign_conv(elab_rvalue_expr(e), expected);
}

static struct expr *elab_cond_expr(struct expr *e) {
    e = elab_rvalue_expr(e);
    if (!is_scalar(e->ty))
        err_at(&e->pos, "condition must be scalar");
    return e;
}

static int elab_init(struct expr *e, struct ty *target_ty) {
    e = elab_rvalue_expr(e);
    if (is_ptr_ty(target_ty) && is_null_ptr(e)) {
        return 0;
    } else if (is_integer_ty(target_ty) && is_integer_ty(e->ty)) {
        return const_cast(eval(e), target_ty);
    } else {
        err_at(&e->pos, "target type mismatch");
    }
}

//=============================================================================
//= lex

enum {
    Tok_Num,
    Tok_Wrd,
    Tok_Str,
    Tok_Chr,
    Tok_Sym,
};

enum { MAX_TOK_LEN = 255 };

static int inp;

enum { Rdbuf_Cap = 256 };
static char rdbuf[Rdbuf_Cap];
static int rdbuf_len, rdbuf_pos;

static int peek_char(int fd) {
    if (rdbuf_pos < rdbuf_len)
        return rdbuf[rdbuf_pos];
    rdbuf_pos = 0, rdbuf_len = read(fd, rdbuf, Rdbuf_Cap);
    if (rdbuf_len <= 0)
        return EOF;
    return rdbuf[rdbuf_pos];
}

static struct pos chr_pos;
static int chr;

static int tok;
static struct pos tok_pos;
static char tok_str[MAX_TOK_LEN + 1];
static int tok_len;
static struct {
    int n;
    char str[MAX_TOK_LEN + 1];
} tok_val;

static void next_chr() {
    if (chr == '\n') {
        chr_pos.line++, chr_pos.col = 1;
    } else {
        chr_pos.col++;
    }
    if (tok_len < MAX_TOK_LEN) {
        tok_str[tok_len++] = chr;
    }
    if (chr = peek_char(inp), chr == EOF)
        return;
    rdbuf_pos++;
}

static void lex() {
    while (1) {
        tok_len = 0, tok_pos = chr_pos;
        if (chr == EOF) {
            tok = EOF;
        } else if (chr == ' ' || chr == '\t' || chr == '\n') {
            next_chr();
            continue;
        } else if (chr >= '0' && chr <= '9') {
            int n = 0;
            while (chr >= '0' && chr <= '9') {
                n = n * 10 + (chr - '0');
                next_chr();
            }
            tok_val.n = n;
            tok = Tok_Num;
        } else if (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_') {
            while (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_' || chr >= '0' && chr <= '9') {
                next_chr();
            }
            tok = Tok_Wrd;
        } else if (chr == '\'' || chr == '"') {
            int delim = chr, len = 0;
            next_chr();
            while (1) {
                if (chr == delim)
                    break;
                if (chr == EOF)
                    err_at(&chr_pos, "unterminated string/char literal");
                if (delim == '\'' && len != 0)
                    err_at(&chr_pos, "too many characters in char literal");
                if (len == MAX_TOK_LEN)
                    err_at(&chr_pos, "string/char literal too long");
                int decoded = chr;
                if (chr == '\\') {
                    next_chr();
                    const char *escapes = "abfnrtv\\'\"?", *unescapes = "\a\b\f\n\r\t\v\\\'\"\?";
                    if (find_chr(escapes, chr)) {
                        decoded = unescapes[find_chr(escapes, chr) - escapes];
                    }
                }
                next_chr();
                tok_val.str[len++] = decoded;
            }
            if (delim == '\'' && len == 0)
                err_at(&chr_pos, "empty char literal");
            next_chr();
            tok_val.str[len] = 0;
            tok_val.n = len;
            tok = delim == '"' ? Tok_Str : Tok_Chr;
        } else {
            tok = Tok_Sym;
            int prev_chr = chr;
            next_chr();
            if (prev_chr == '#' || prev_chr == '/' && chr == '/') {
                while (chr != '\n' && chr != EOF) {
                    next_chr();
                }
                continue;
            } else if (find_chr("<>!=", prev_chr) && chr == '=') {
                next_chr();
            } else if (find_chr("&|<>+-", prev_chr) && chr == prev_chr) {
                next_chr();
            } else if (prev_chr == '-' && chr == '>') {
                next_chr();
            } else if (prev_chr == '.' && chr == '.' && peek_char(inp) == '.') {
                next_chr(), next_chr();
            }
        }
        break;
    }

    tok_str[tok_len] = 0;
}

//=============================================================================
//= parse.dsl

static int at(const char *t) {
    return str_eq(tok_str, t);
}

static int eat(const char *t) {
    if (!at(t))
        return 0;
    lex();
    return 1;
}

static void expect(const char *t) {
    if (!eat(t))
        err_at(&tok_pos, "expected '%s'", t);
}

static const char *p_ident() {
    if (tok != Tok_Wrd)
        err_at(&tok_pos, "expected identifier");
    struct string *res = intern(tok_str, tok_len);
    lex();
    return res->chars;
}

//=============================================================================
//= parse

enum {
    Decl_Global,
    Decl_Local,
    Decl_Param,
    Decl_Struct,
    Decl_TyName,
};

static void p_decl(int scope, void *ctx);

static int at_ty() {
    return at("void") || at("char") || at("int") || at("va_list") || at("struct") || at("enum") || at("const");
}

static int at_decl() {
    return at("static") || at("extern") || at_ty();
}

static struct ty *p_tyname() {
    struct ty *ty;
    p_decl(Decl_TyName, &ty);
    return ty;
}

enum {
    Prec_Comma = 1,
    Prec_Assign,
    Prec_Cond,
    Prec_Or,
    Prec_And,
    Prec_BitOr,
    Prec_BitXor,
    Prec_BitAnd,
    Prec_Eq,
    Prec_Rel,
    Prec_Shift,
    Prec_Add,
    Prec_Mul,
    Prec_Unary,
    Prec_Postfix,
    Prec_Primary,
};

static struct expr *p_expr(int rbp);

static struct expr *p_expr(int rbp) {
    struct pos pos = tok_pos;
    struct expr *acc;
    if (eat("(")) {
        if (at_decl()) {
            struct ty *ty = p_tyname();
            expect(")");
            struct expr *tmp = p_expr(Prec_Unary);
            acc = mk_unary_expr(&pos, Expr_Cast, tmp);
            acc->ty = ty;
        } else {
            acc = p_expr(0);
            expect(")");
        }
    } else if (eat("-")) {
        acc = mk_unary_expr(&pos, Expr_Neg, p_expr(Prec_Unary));
    } else if (eat("~")) {
        acc = mk_unary_expr(&pos, Expr_BitNot, p_expr(Prec_Unary));
    } else if (eat("!")) {
        acc = mk_unary_expr(&pos, Expr_Not, p_expr(Prec_Unary));
    } else if (tok == Tok_Num) {
        acc = mk_expr(&pos, Expr_Num, 0);
        acc->int_val = tok_val.n;
        lex();
    } else if (tok == Tok_Str) {
        acc = mk_expr(&pos, Expr_Str, 0);
        acc->str_val = intern(tok_val.str, tok_val.n);
        lex();
    } else if (tok == Tok_Chr) {
        acc = mk_expr(&pos, Expr_Chr, 0);
        acc->int_val = tok_val.str[0];
        lex();
    } else if (eat("&")) {
        acc = mk_unary_expr(&pos, Expr_Addr, p_expr(Prec_Unary));
    } else if (eat("*")) {
        acc = mk_unary_expr(&pos, Expr_Deref, p_expr(Prec_Unary));
    } else if (eat("sizeof")) {
        expect("(");
        struct ty *ty = p_tyname();
        expect(")");
        if (!is_object_ty(ty))
            err_at(&pos, "sizeof operand must be object");
        acc = mk_expr(&pos, Expr_Num, 0);
        acc->int_val = ty_size(ty);
    } else if (eat("va_start")) {
        acc = mk_expr(&pos, Expr_VaStart, 2);
        expect("(");
        acc->subs[0] = p_expr(Prec_Comma);
        expect(",");
        acc->subs[1] = p_expr(Prec_Comma);
        expect(")");
    } else if (eat("va_arg")) {
        acc = mk_expr(&pos, Expr_VaArg, 1);
        expect("(");
        acc->subs[0] = p_expr(Prec_Comma);
        expect(",");
        acc->ty = p_tyname();
        expect(")");
    } else if (eat("va_end")) {
        acc = mk_expr(&pos, Expr_VaEnd, 1);
        expect("(");
        acc->subs[0] = p_expr(Prec_Comma);
        expect(")");
    } else if (tok == Tok_Wrd) {
        struct sym *sym = lookup(0, p_ident());
        if (!sym)
            err_at(&pos, "undefined symbol");
        if (sym->kind == Sym_Const) {
            acc = mk_expr(&pos, Expr_Num, 0);
            acc->int_val = sym->val;
        } else if (sym->kind == Sym_Local || sym->kind == Sym_Global) {
            acc = mk_expr(&pos, Expr_Var, 0);
            acc->sym = sym;
        } else if (sym->kind == Sym_Func) {
            acc = mk_expr(&pos, Expr_Func, 0);
            acc->sym = sym;
        } else {
            err_at(&pos, "not a variable, function, or constant");
        }
    } else {
        err_at(&pos, "expected expression");
    }

    // led
    while (1) {
        if (rbp < Prec_Comma && eat(",")) {
            acc = mk_bin_expr(Expr_Comma, acc, p_expr(0));
        } else if (rbp < Prec_Assign && eat("=")) {
            acc = mk_bin_expr(Expr_Assign, acc, p_expr(Prec_Assign - 1));
        } else if (rbp < Prec_Cond && eat("?")) {
            struct expr *tmp = mk_expr(&pos, Expr_Cond, 3);
            tmp->subs[0] = acc;
            tmp->subs[1] = p_expr(0);
            expect(":");
            tmp->subs[2] = p_expr(Prec_Cond - 1);
            acc = tmp;
        } else if (rbp < Prec_Or && eat("||")) {
            acc = mk_bin_expr(Expr_Or, acc, p_expr(Prec_Or));
        } else if (rbp < Prec_And && eat("&&")) {
            acc = mk_bin_expr(Expr_And, acc, p_expr(Prec_And));
        } else if (rbp < Prec_BitOr && eat("|")) {
            acc = mk_bin_expr(Expr_BitOr, acc, p_expr(Prec_BitOr));
        } else if (rbp < Prec_BitXor && eat("^")) {
            acc = mk_bin_expr(Expr_BitXor, acc, p_expr(Prec_BitXor));
        } else if (rbp < Prec_BitAnd && eat("&")) {
            acc = mk_bin_expr(Expr_BitAnd, acc, p_expr(Prec_BitAnd));
        } else if (rbp < Prec_Eq && eat("==")) {
            acc = mk_bin_expr(Expr_Eq, acc, p_expr(Prec_Eq));
        } else if (rbp < Prec_Eq && eat("!=")) {
            acc = mk_bin_expr(Expr_Ne, acc, p_expr(Prec_Eq));
        } else if (rbp < Prec_Rel && eat("<")) {
            acc = mk_bin_expr(Expr_Lt, acc, p_expr(Prec_Rel));
        } else if (rbp < Prec_Rel && eat(">")) {
            acc = mk_bin_expr(Expr_Gt, acc, p_expr(Prec_Rel));
        } else if (rbp < Prec_Rel && eat("<=")) {
            acc = mk_bin_expr(Expr_Le, acc, p_expr(Prec_Rel));
        } else if (rbp < Prec_Rel && eat(">=")) {
            acc = mk_bin_expr(Expr_Ge, acc, p_expr(Prec_Rel));
        } else if (rbp < Prec_Shift && eat("<<")) {
            acc = mk_bin_expr(Expr_Shl, acc, p_expr(Prec_Shift));
        } else if (rbp < Prec_Shift && eat(">>")) {
            acc = mk_bin_expr(Expr_Shr, acc, p_expr(Prec_Shift));
        } else if (rbp < Prec_Add && eat("+")) {
            acc = mk_bin_expr(Expr_Add, acc, p_expr(Prec_Add));
        } else if (rbp < Prec_Add && eat("-")) {
            acc = mk_bin_expr(Expr_Sub, acc, p_expr(Prec_Add));
        } else if (rbp < Prec_Mul && eat("*")) {
            acc = mk_bin_expr(Expr_Mul, acc, p_expr(Prec_Mul));
        } else if (rbp < Prec_Mul && eat("/")) {
            acc = mk_bin_expr(Expr_Div, acc, p_expr(Prec_Mul));
        } else if (rbp < Prec_Mul && eat("%")) {
            acc = mk_bin_expr(Expr_Mod, acc, p_expr(Prec_Mul));
        } else if (rbp < Prec_Postfix && eat("[")) {
            acc = mk_unary_expr(&pos, Expr_Deref, mk_bin_expr(Expr_Add, acc, p_expr(0)));
            expect("]");
        } else if (rbp < Prec_Postfix && eat("(")) {
            struct expr *tmp = mk_expr(&pos, Expr_Call, MAX_FUNC_PARAMS + 1);
            tmp->subs[0] = acc;
            int n_args = 0;
            struct expr **args = &tmp->subs[1];
            while (!eat(")")) {
                if (n_args >= MAX_FUNC_PARAMS)
                    err_at(&pos, "too many arguments");
                if (n_args > 0) {
                    expect(",");
                }
                args[n_args++] = p_expr(Prec_Comma);
            }
            tmp->n_subs = n_args + 1;
            acc = tmp;
        } else if (rbp < Prec_Postfix && eat(".")) {
            acc = mk_unary_expr(&pos, Expr_Member, acc);
            acc->fld_name = p_ident();
        } else if (rbp < Prec_Postfix && eat("->")) {
            acc = mk_unary_expr(&pos, Expr_Member, mk_unary_expr(&pos, Expr_Deref, acc));
            acc->fld_name = p_ident();
        } else if (rbp < Prec_Postfix && eat("++")) {
            acc = mk_unary_expr(&pos, Expr_PostInc, acc);
        } else if (rbp < Prec_Postfix && eat("--")) {
            acc = mk_unary_expr(&pos, Expr_PostDec, acc);
        } else {
            return acc;
        }
    }
}

static int p_const_expr() {
    return eval(elab_rvalue_expr(p_expr(Prec_Cond - 1)));
}

static struct stmt *p_stmt() {
    struct pos pos = tok_pos;
    struct stmt *stmt;
    if (eat("{")) {
        stmt = mk_stmt(&pos, Stmt_Block);
        push_scope();
        for (struct stmt **lastp = &stmt->sub; !eat("}"); lastp = &(*lastp)->next) {
            *lastp = p_stmt();
        }
        pop_scope();
    } else if (eat("return")) {
        stmt = mk_stmt(&pos, Stmt_Return);
        if (!is_void_ty(curr_func->ty->ret_ty)) {
            stmt->expr = elab_expr_expect(p_expr(0), curr_func->ty->ret_ty);
        }
        expect(";");
    } else if (eat("if")) {
        stmt = mk_stmt(&pos, Stmt_If);

        expect("(");
        stmt->expr = elab_cond_expr(p_expr(0));
        expect(")");

        push_scope();
        stmt->sub = p_stmt();
        pop_scope();

        if (eat("else")) {
            push_scope();
            stmt->sub->next = p_stmt();
            pop_scope();
        }

    } else if (at("for") || at("while")) {
        struct stmt *outer_loop = curr_loop;
        curr_loop = stmt = mk_stmt(&pos, Stmt_Loop);

        int is_while = eat("while") || !eat("for");

        push_scope();
        expect("(");
        if (is_while) {
            stmt->expr = elab_cond_expr(p_expr(0));
            stmt->sub = mk_stmt(&pos, Stmt_Empty);
            stmt->sub->next = mk_stmt(&pos, Stmt_Empty);
        } else {
            if (eat(";")) {
                stmt->sub = mk_stmt(&pos, Stmt_Empty);
            } else if (at_decl()) {
                stmt->sub = p_stmt();
            } else {
                stmt->sub = mk_stmt(&pos, Stmt_Expr);
                stmt->sub->expr = elab_rvalue_expr(p_expr(0));
                expect(";");
            }

            if (!at(";")) {
                stmt->expr = elab_cond_expr(p_expr(0));
            }
            expect(";");

            if (!at(")")) {
                stmt->sub->next = mk_stmt(&pos, Stmt_Expr);
                stmt->sub->next->expr = elab_rvalue_expr(p_expr(0));
            } else {
                stmt->sub->next = mk_stmt(&pos, Stmt_Empty);
            }
        }
        expect(")");

        push_scope();
        stmt->sub->next->next = p_stmt();
        pop_scope();
        pop_scope();
        curr_loop = outer_loop;
    } else if (at("break") || at("continue")) {
        if (!curr_loop)
            err_at(&pos, "break/continue outside loop");
        int kind = eat("break") || !eat("continue") ? Stmt_Break : Stmt_Continue;
        stmt = mk_stmt(&pos, kind);
        stmt->sub = curr_loop;
        expect(";");
    } else if (eat(";")) {
        stmt = mk_stmt(&pos, Stmt_Empty);
    } else if (at_decl()) {
        stmt = 0;
        p_decl(Decl_Local, &stmt);
        stmt = stmt ? stmt : mk_stmt(&pos, Stmt_Empty);
    } else {
        stmt = mk_stmt(&pos, Stmt_Expr);
        stmt->expr = elab_rvalue_expr(p_expr(0));
        expect(";");
    }
    return stmt;
}

static struct ty *p_struct() {
    struct pos name_pos = tok_pos;
    const char *name = tok == Tok_Wrd ? p_ident() : 0;

    int is_def = at("{");
    struct sym *sym = is_def ? 0 : lookup(1, name);

    if (is_def || !sym) {
        sym = declare(&name_pos, 0, Sym_Struct, 0, name, 0, is_def);
        if (eat("{")) {
            sym->scope = push_scope();  // allocate an empty scope
            pop_scope();
            while (!eat("}")) {
                p_decl(Decl_Struct, sym);
            }
            sym->is_defined = 1;
            sym->size = align_up(sym->size, sym->align);
        }
    }

    if (!name && !is_def)
        err_at(&name_pos, "anonymous struct must be a definition");

    return sym->ty;
}

static struct ty *p_enum() {
    expect("{");
    for (int val = 0; !eat("}"); val++) {
        struct pos name_pos = tok_pos;
        const char *name = p_ident();
        if (eat("=")) {
            val = p_const_expr();
        }
        if (!at("}")) {
            expect(",");
        }
        declare(&name_pos, 0, Sym_Const, 0, name, int_ty, 1)->val = val;
    }
    return int_ty;
}

static struct ty *p_decl_arr(struct ty *base_ty) {
    int len = p_const_expr();
    expect("]");
    if (eat("[")) {
        base_ty = p_decl_arr(base_ty);
    }
    return mk_arr_ty(base_ty, len);
}

static void p_decl(int scope, void *ctx) {
    struct pos pos = tok_pos;
    int storage = 0;
    struct ty *base_ty = 0;
    while (1) {
        if (eat("const")) {
            // ignored
        } else if (!storage && eat("static")) {
            storage = Static;
        } else if (!storage && eat("extern")) {
            storage = Extern;
        } else if (!base_ty && eat("void")) {
            base_ty = void_ty;
        } else if (!base_ty && eat("int")) {
            base_ty = int_ty;
        } else if (!base_ty && eat("char")) {
            base_ty = char_ty;
        } else if (!base_ty && eat("va_list")) {
            base_ty = va_list_ty;
        } else if (!base_ty && eat("struct")) {
            base_ty = p_struct();
        } else if (!base_ty && eat("enum")) {
            base_ty = p_enum();
        } else {
            break;
        }
    }
    if (!base_ty)
        err_at(&pos, "expected type specifier");
    if (storage && scope != Decl_Global)
        err_at(&pos, "storage class specifier is not allowed here");

    for (int n_declarators = 0;; n_declarators++) {
        struct ty *ty = base_ty;
        const char *name = 0;
        int has_params = 0;
        struct sym *params[MAX_FUNC_PARAMS];
        int n_params = 0;
        struct expr *init = 0;

        while (eat("*")) {
            ty = mk_ptr_ty(ty);
        }

        struct pos name_pos = tok_pos;
        if (scope != Decl_TyName && tok == Tok_Wrd) {
            name = p_ident();
        }

        // simplified grammar: either a function, array or object definition
        if (eat("(")) {
            struct ty *param_tys[MAX_FUNC_PARAMS];
            has_params = 1;
            int is_va = 0;
            push_scope();
            for (n_params = 0; !at(")"); n_params++) {
                if (n_params >= MAX_FUNC_PARAMS)
                    err_at(&name_pos, "too many parameters");
                if (n_params > 0) {
                    expect(",");
                    if (eat("...")) {
                        is_va = 1;
                        break;
                    }
                }
                p_decl(Decl_Param, &params[n_params]);
                param_tys[n_params] = params[n_params]->ty;
            }
            expect(")");
            if (!is_scalar(ty) && !is_void_ty(ty))
                err_at(&name_pos, "bad return type");
            ty = mk_func_ty(ty, param_tys, n_params, is_va);
            pop_scope();
        } else if (eat("[")) {
            ty = p_decl_arr(ty);
        } else if (eat("=") && (scope == Decl_Global || scope == Decl_Local)) {
            init = p_expr(Prec_Comma);
        }

        if (scope == Decl_TyName) {
            *(struct ty **)ctx = ty;
            return;  // max one type per abstract declaration
        } else if (scope == Decl_Param) {
            if (is_arr_ty(ty)) {
                ty = mk_ptr_ty(ty->ptr_to);
            } else if (is_func_ty(ty)) {
                ty = mk_ptr_ty(ty);
            }

            if (!is_scalar(ty))
                err_at(&name_pos, "bad parameter type");
            *(struct sym **)ctx = declare(&name_pos, 0, Sym_Local, 0, name, ty, 1);
            return;  // max one parameter per declaration
        } else if (!name) {
            // declaration does not declare a function or object
        } else if (has_params) {
            // function declaration
            if (scope != Decl_Global && scope != Decl_Local)
                err_at(&name_pos, "function declaration not allowed here");
            int has_func_body = scope == Decl_Global && n_declarators == 0 && at("{");
            struct sym *sym = declare(&name_pos, 0, Sym_Func, storage, name, ty, has_func_body);
            if (has_func_body) {
                sym->scope = push_scope();
                for (int i = 0; i < n_params; i++) {
                    struct sym *param =
                        declare(&params[i]->last_pos, sym, Sym_Local, 0, params[i]->name, params[i]->ty, 1);
                    sym->last_param = param;
                }
                curr_func = sym;
                sym->body = p_stmt();
                curr_func = 0;
                pop_scope();
                return;  // max one function definition per declaration
            }
        } else {
            // object declaration
            if (!is_object_ty(ty))
                err_at(&name_pos, "bad object type");
            if (scope == Decl_Local) {
                struct stmt *decl = mk_stmt(&pos, Stmt_Decl);
                decl->sym = declare(&name_pos, curr_func, Sym_Local, 0, name, ty, 1);
                decl->sub = *(struct stmt **)ctx;
                *(struct stmt **)ctx = decl;
                if (init) {
                    decl->expr = elab_expr_expect(init, ty);
                }
            } else if (scope == Decl_Global) {
                struct sym *sym = declare(&name_pos, 0, Sym_Global, storage, name, ty, !!init);
                if (init) {
                    sym->val = elab_init(init, ty);
                }
            } else if (scope == Decl_Struct) {
                declare(&name_pos, (struct sym *)ctx, Sym_Field, 0, name, ty, 1);
            } else {
                die("p_decl (object declaration): unreachable: %d", scope);
            }
        }

        if (!name || !eat(",")) {
            expect(";");
            return;
        }
    }
}

//=============================================================================
//= codegen

static int next_loop_id, next_cond_id, next_str_id;

static const char *get_str_op(struct ty *ty) {
    if (ty->kind == Ty_Char)
        return "strb w";
    else if (ty->kind == Ty_Int)
        return "str w";
    else if (ty->kind == Ty_Ptr)
        return "str x";
    else
        die("get_str_op: unreachable: %d", ty->kind);
}

static const char *get_ldr_op(struct ty *ty) {
    if (ty->kind == Ty_Char)
        return "ldrsb x";
    else if (ty->kind == Ty_Int)
        return "ldrsw x";
    else if (ty->kind == Ty_Ptr)
        return "ldr x";
    else
        die("get_ldr_op: unreachable: %d", ty->kind);
}

static void emit_str_load(struct string *str) {
    if (!str->label) {
        next_str_id++;
        str->label = next_str_id;
    }
    writef(1, "adrp x0, .L.str.%d\n", str->label);
    writef(1, "add x0, x0, :lo12:.L.str.%d\n", str->label);
}

static void emit_int_load(int val, int reg) {
    int chunk_mask = (1 << 16) - 1;
    int default_chunk = val < 0 ? chunk_mask : 0;
    for (int i = 0; i < 4; i++, val = val >> 16) {
        int chunk = val & chunk_mask;
        if (i == 0) {
            chunk = val < 0 ? chunk | ~chunk_mask : chunk;
            writef(1, "mov x%d, #%d // %d\n", reg, chunk, val);
        } else if (chunk != default_chunk) {
            writef(1, "movk x%d, #%d, lsl #%d\n", reg, chunk, i * 16);
        }
    }
}

static void emit_slot_addr(struct sym *local, int reg) {
    emit_int_load(local->offs, 9);
    writef(1, "add x%d, x29, x9\n", reg);
}

static void emit_push(int reg) {
    writef(1, "str x%d, [sp, #-16]!\n", reg);
}

static void emit_pop(int reg) {
    writef(1, "ldr x%d, [sp], #16\n", reg);
}

static void emit_load(struct ty *ty, int dst, int src) {
    writef(1, "%s%d, [x%d]\n", get_ldr_op(ty), dst, src);
}

static void emit_store(struct ty *ty, int src, int dst) {
    writef(1, "%s%d, [x%d]\n", get_str_op(ty), src, dst);
}

static void emit_sext(struct ty *ty, int reg) {
    if (ty->kind == Ty_Char) {
        writef(1, "sxtb x%d, w%d\n", reg, reg);
    } else if (ty->kind == Ty_Int) {
        writef(1, "sxtw x%d, w%d\n", reg, reg);
    }
}

static void emit_expr(struct expr *e);
static void emit_place_expr(struct expr *e);

static void emit_bin_subs(struct expr *e) {
    emit_expr(e->subs[0]);
    emit_push(0);
    emit_expr(e->subs[1]);
    write_str(1, "mov x1, x0\n");
    emit_pop(0);
}

static void emit_short_circuit_expr(struct expr *e, const char *cond) {
    int cond_id = next_cond_id++;
    emit_expr(e->subs[0]);
    writef(1, "cb%s x0, .L.cond.%d.short\n", cond, cond_id);
    emit_expr(e->subs[1]);
    writef(1, ".L.cond.%d.short:\n", cond_id);
    write_str(1, "cmp x0, #0\n");
    write_str(1, "cset x0, ne\n");
}

static void emit_cmp_expr(struct expr *e, const char *cond, const char *ucond) {
    emit_bin_subs(e);
    write_str(1, "cmp x0, x1\n");
    if (is_ptr_ty(e->subs[0]->ty)) {
        writef(1, "cset x0, %s\n", ucond);
    } else {
        writef(1, "cset x0, %s\n", cond);
    }
}

static void emit_arith_expr(struct expr *e, const char *op) {
    emit_bin_subs(e);
    writef(1, "%s x0, x0, x1\n", op);
}

static void emit_assign_to_addr(struct ty *dst_ty, struct expr *rhs) {
    assert("emit_assign_to_addr", is_object_ty(dst_ty));
    if (is_scalar(dst_ty)) {
        emit_push(0);
        emit_expr(rhs);
        write_str(1, "mov x1, x0\n");
        emit_pop(0);
        emit_store(dst_ty, 1, 0);
        write_str(1, "mov x0, x1\n");
    } else {
        emit_push(0);
        if (rhs->kind == Expr_Assign) {
            emit_place_expr(rhs->subs[0]);
            emit_assign_to_addr(rhs->ty, rhs->subs[1]);
        } else {
            if (!is_addressable(rhs))
                die("emit_assign_to_addr: unreachable: %d", rhs->kind);
            emit_place_expr(rhs);
        }
        write_str(1, "mov x1, x0\n");
        emit_pop(0);
        emit_int_load(ty_size(dst_ty), 2);
        write_str(1, "bl _memcpy\n");
    }
}

static void emit_place_expr(struct expr *e) {
    int k = e->kind;
    if (k == Expr_Var || k == Expr_Func) {
        struct sym *sym = e->sym;
        if (sym->kind == Sym_Global || sym->kind == Sym_Func) {
            if (sym->storage == Extern && !sym->is_defined) {
                writef(1, "adrp x0, :got:%s\n", sym->name);
                writef(1, "ldr x0, [x0, :got_lo12:%s]\n", sym->name);
            } else {
                writef(1, "adrp x0, %s\n", sym->name);
                writef(1, "add x0, x0, :lo12:%s\n", sym->name);
            }
        } else if (sym->kind == Sym_Local) {
            emit_slot_addr(sym, 0);
        } else {
            die("emit_place_expr: unreachable: %d (ident)", sym->kind);
        }
    } else if (k == Expr_Member) {
        emit_place_expr(e->subs[0]);
        emit_int_load(e->sym->offs, 1);
        write_str(1, "add x0, x0, x1\n");
    } else if (k == Expr_Deref) {
        emit_expr(e->subs[0]);
    } else if (k == Expr_Str) {
        emit_str_load(e->str_val);
    } else {
        die("emit_place_expr: unreachable: %d", k);
    }
}

static void emit_expr(struct expr *e) {
    if (is_lvalue(e)) {
        emit_place_expr(e);
        if (is_scalar(e->ty)) {
            emit_load(e->ty, 0, 0);
        }
    } else if (e->kind == Expr_Num) {
        emit_int_load(e->int_val, 0);
    } else if (e->kind == Expr_Str) {
        emit_str_load(e->str_val);
    } else if (e->kind == Expr_PostDec || e->kind == Expr_PostInc) {
        const char *op = e->kind == Expr_PostInc ? "add" : "sub";
        int step = is_ptr_ty(e->ty) ? ty_size(e->ty->ptr_to) : 1;
        emit_place_expr(e->subs[0]);
        write_str(1, "mov x2, x0\n");
        emit_load(e->ty, 0, 0);
        emit_int_load(step, 1);
        writef(1, "%s x1, x0, x1\n", op);
        emit_store(e->ty, 1, 2);
    } else if (e->kind == Expr_Addr) {
        emit_place_expr(e->subs[0]);
    } else if (e->kind == Expr_BitNot) {
        emit_expr(e->subs[0]);
        write_str(1, "mvn x0, x0\n");
    } else if (e->kind == Expr_Not) {
        emit_expr(e->subs[0]);
        write_str(1, "cmp x0, #0\n");
        write_str(1, "cset x0, eq\n");
    } else if (e->kind == Expr_Neg) {
        emit_expr(e->subs[0]);
        write_str(1, "neg x0, x0\n");
    } else if (e->kind == Expr_Cast) {
        // values in registers are always full width, so
        // narrowing casts can simply truncate the value.
        emit_expr(e->subs[0]);
        if (ty_size(e->ty) < 8) {
            emit_sext(e->ty, 0);
        }
    } else if (e->kind == Expr_Mod) {
        emit_bin_subs(e);
        write_str(1, "sdiv x3, x0, x1\n");
        write_str(1, "msub x0, x3, x1, x0\n");
    } else if (e->kind == Expr_Div) {
        emit_arith_expr(e, "sdiv");
    } else if (e->kind == Expr_Mul) {
        emit_arith_expr(e, "mul");
    } else if (e->kind == Expr_Sub) {
        emit_arith_expr(e, "sub");
    } else if (e->kind == Expr_Add) {
        emit_arith_expr(e, "add");
    } else if (e->kind == Expr_Shr) {
        emit_arith_expr(e, "asr");
    } else if (e->kind == Expr_Shl) {
        emit_arith_expr(e, "lsl");
    } else if (e->kind == Expr_Ge) {
        emit_cmp_expr(e, "ge", "hs");
    } else if (e->kind == Expr_Gt) {
        emit_cmp_expr(e, "gt", "hi");
    } else if (e->kind == Expr_Le) {
        emit_cmp_expr(e, "le", "ls");
    } else if (e->kind == Expr_Lt) {
        emit_cmp_expr(e, "lt", "lo");
    } else if (e->kind == Expr_Ne) {
        emit_cmp_expr(e, "ne", "ne");
    } else if (e->kind == Expr_Eq) {
        emit_cmp_expr(e, "eq", "eq");
    } else if (e->kind == Expr_BitAnd) {
        emit_arith_expr(e, "and");
    } else if (e->kind == Expr_BitXor) {
        emit_arith_expr(e, "eor");
    } else if (e->kind == Expr_BitOr) {
        emit_arith_expr(e, "orr");
    } else if (e->kind == Expr_And) {
        emit_short_circuit_expr(e, "z");
    } else if (e->kind == Expr_Or) {
        emit_short_circuit_expr(e, "nz");
    } else if (e->kind == Expr_PtrAdd || e->kind == Expr_PtrSub) {
        emit_bin_subs(e);
        int size = ty_size(e->subs[0]->ty->ptr_to);
        const char *op = e->kind == Expr_PtrAdd ? "add" : "sub";
        if (size != 1) {
            emit_int_load(size, 2);
            write_str(1, "mul x1, x1, x2\n");
        }
        writef(1, "%s x0, x0, x1\n", op);
    } else if (e->kind == Expr_PtrDiff) {
        emit_bin_subs(e);
        int size = ty_size(e->subs[0]->ty->ptr_to);
        write_str(1, "sub x0, x0, x1\n");
        if (size != 1) {
            emit_int_load(size, 1);
            write_str(1, "sdiv x0, x0, x1\n");
        }
    } else if (e->kind == Expr_Cond) {
        int cond_id = next_cond_id++;
        emit_expr(e->subs[0]);
        writef(1, "cbz x0, .L.cond.%d.else\n", cond_id);
        emit_expr(e->subs[1]);
        writef(1, "b .L.cond.%d.end\n", cond_id);
        writef(1, ".L.cond.%d.else:\n", cond_id);
        emit_expr(e->subs[2]);
        writef(1, ".L.cond.%d.end:\n", cond_id);
    } else if (e->kind == Expr_Assign) {
        emit_place_expr(e->subs[0]);
        emit_assign_to_addr(e->ty, e->subs[1]);
    } else if (e->kind == Expr_Call) {
        struct expr *fn = e->subs[0], **args = &e->subs[1];
        int argc = e->n_subs - 1;

        assert("emit_scalar_expr: func", fn->kind == Expr_Addr && fn->subs[0]->kind == Expr_Func);

        for (int i = 0; i < argc; i++) {
            emit_expr(args[i]);
            emit_push(0);
        }
        for (int i = argc; i-- > 0;) {
            emit_pop(i);
        }
        writef(1, "bl %s\n", fn->subs[0]->sym->name);
        // Normalize the result by sign-extending in case this
        // is a foreign function returning its result in w0.
        if (is_scalar(e->ty) && ty_size(e->ty) < 8) {
            emit_sext(e->ty, 0);
        }
    } else if (e->kind == Expr_Comma) {
        emit_expr(e->subs[0]);
        emit_expr(e->subs[1]);
    } else if (e->kind == Expr_VaStart) {
        emit_place_expr(e->subs[0]);
        write_str(1, "add x1, x29, #16  // top of frame\n");
        write_str(1, "str x1, [x0]  // stack\n");
        emit_int_load(-curr_func->va_offs - curr_func->va_size, 1);
        writef(1, "sub x%d, x29, x%d\n", 1, 1);
        write_str(1, "str x1, [x0, #8]  // gr_top\n");
        write_str(1, "str xzr, [x0, #16]  // vr_top\n");
        writef(1, "mov x1, #%d\n", -curr_func->va_size);
        write_str(1, "str w1, [x0, #24]  // gr_offs\n");
        write_str(1, "str wzr, [x0, #28]  // vr_offs\n");
    } else if (e->kind == Expr_VaEnd) {
        // no-op
    } else if (e->kind == Expr_VaArg) {
        emit_place_expr(e->subs[0]);
        write_str(1, "bl _va_arg\n");
    } else {
        die("emit_scalar_expr: unreachable: %d", e->kind);
    }
}

static void emit_init_decls(struct stmt *stmt) {
    if (!stmt)
        return;
    emit_init_decls(stmt->sub);
    if (stmt->expr) {
        emit_slot_addr(stmt->sym, 0);
        emit_assign_to_addr(stmt->sym->ty, stmt->expr);
    }
}

static void emit_stmt(struct stmt *stmt) {
    writef(1, "// %s:%d:%d\n", stmt->pos.file, stmt->pos.line, stmt->pos.col);  // debug info
    int k = stmt->kind;
    if (k == Stmt_Block) {
        for (struct stmt *sub = stmt->sub; sub; sub = sub->next) {
            emit_stmt(sub);
        }
    } else if (k == Stmt_Return) {
        if (stmt->expr) {
            emit_expr(stmt->expr);
        }
        writef(1, "b .L.return.%s\n", curr_func->name);
    } else if (k == Stmt_If) {
        int cond_id = next_cond_id++;
        emit_expr(stmt->expr);
        writef(1, "cbz x0, .L.if.%d.else\n", cond_id);
        emit_stmt(stmt->sub);
        writef(1, "b .L.if.%d.end\n", cond_id);
        writef(1, ".L.if.%d.else:\n", cond_id);
        if (stmt->sub->next) {
            emit_stmt(stmt->sub->next);
        }
        writef(1, ".L.if.%d.end:\n", cond_id);
    } else if (k == Stmt_Loop) {
        stmt->loop_id = next_loop_id++;
        emit_stmt(stmt->sub);
        writef(1, "b .L.loop.%d.cond\n", stmt->loop_id);
        writef(1, ".L.loop.%d.body:\n", stmt->loop_id);
        emit_stmt(stmt->sub->next->next);
        writef(1, ".L.loop.%d.step:\n", stmt->loop_id);
        emit_stmt(stmt->sub->next);
        writef(1, ".L.loop.%d.cond:\n", stmt->loop_id);
        if (stmt->expr) {
            emit_expr(stmt->expr);
            writef(1, "cbnz x0, .L.loop.%d.body\n", stmt->loop_id);
        } else {
            writef(1, "b .L.loop.%d.body\n", stmt->loop_id);
        }
        writef(1, ".L.loop.%d.end:\n", stmt->loop_id);
    } else if (k == Stmt_Break) {
        writef(1, "b .L.loop.%d.end\n", stmt->sub->loop_id);
    } else if (k == Stmt_Continue) {
        writef(1, "b .L.loop.%d.step\n", stmt->sub->loop_id);
    } else if (k == Stmt_Decl) {
        emit_init_decls(stmt);
    } else if (k == Stmt_Expr) {
        emit_expr(stmt->expr);
    } else if (k != Stmt_Empty) {
        die("emit_stmt: unreachable: %d", k);
    }
}

static void emit_func(struct sym *func) {
    curr_func = func;
    int n_va_args = func->ty->is_va ? 8 - func->ty->n_params : 0;
    func->va_offs = -align_up(curr_func->size + n_va_args * 8, 8);
    func->va_size = n_va_args * 8;
    curr_func->size = align_up(-func->va_offs, 16);

    write_str(1, ".section .text\n");
    if (func->storage != Static) {
        writef(1, ".globl %s\n", func->name);
    }
    writef(1, "%s:\n", func->name);
    // prologue
    write_str(1, "stp x29, x30, [sp, #-16]!\n");
    write_str(1, "mov x29, sp\n");
    writef(1, "sub sp, sp, #%d\n", curr_func->size);
    struct sym *sym = func->scope->head;
    for (int i = 0; i < func->ty->n_params; i++, sym = sym->next) {
        emit_int_load(sym->offs, 9);
        writef(1, "%s%d, [x29, x9]\n", get_str_op(sym->ty), i);
    }
    for (int i = func->ty->n_params; i < 8 && func->ty->is_va; i++) {
        int offs = func->va_offs + (i - func->ty->n_params) * 8;
        emit_int_load(offs, 9);
        writef(1, "str x%d, [x29, x9]\n", i);
    }
    // body
    emit_stmt(func->body);
    // epilogue
    writef(1, ".L.return.%s:\n", func->name);
    write_str(1, "mov sp, x29\n");
    write_str(1, "ldp x29, x30, [sp], #16\n");
    write_str(1, "ret\n");
    curr_func = 0;
}

static void emit_obj(struct sym *sym) {
    int size = ty_size(sym->ty), align = ty_align(sym->ty);

    if (is_scalar(sym->ty) && sym->is_defined) {
        write_str(1, ".section .data\n");
    } else {
        write_str(1, ".section .bss\n");
    }
    if (sym->storage != Static) {
        writef(1, ".globl %s\n", sym->name);
    }
    writef(1, ".balign %d\n", align);
    writef(1, "%s:\n", sym->name);

    if (is_scalar(sym->ty)) {
        if (size == 1) {
            writef(1, ".byte %d\n", sym->val);
        } else if (size == 4) {
            writef(1, ".long %d\n", sym->val);
        } else if (size == 8) {
            writef(1, ".quad %d\n", sym->val);
        } else {
            die("emit_obj: unreachable: %d", size);
        }
    } else {
        writef(1, ".space %d\n", size);
    }
}

static void emit_rt_helpers() {
    write_str(1, ".section .text\n");

    // void *_memcpy(void *d, const void *s, int n)
    write_str(1, "_memcpy:\n");
    write_str(1, "mov x3, x0\n");
    write_str(1, "cbz x2, .L.memcpy.end\n");
    write_str(1, ".L.memcpy.body:\n");
    write_str(1, "ldrb w4, [x1], #1\n");
    write_str(1, "strb w4, [x3], #1\n");
    write_str(1, "subs x2, x2, #1\n");
    write_str(1, "cbnz x2, .L.memcpy.body\n");
    write_str(1, ".L.memcpy.end:\n");
    write_str(1, "ret\n");  // x0 still holds dest

    // void *_va_arg(va_list *ap)
    write_str(1, "_va_arg:\n");
    write_str(1, "ldrsw x1, [x0, #24]  // gr_offs\n");
    write_str(1, "cmp w1, #0\n");
    write_str(1, "b.ge .L.va_arg.err\n");
    write_str(1, "ldr x2, [x0, #8]  // gr_top\n");
    write_str(1, "ldr x3, [x2, x1]\n");
    write_str(1, "add w1, w1, #8\n");
    write_str(1, "str w1, [x0, #24]\n");
    write_str(1, "mov x0, x3\n");
    write_str(1, "ret\n");
    write_str(1, ".L.va_arg.err:\n");
    write_str(1, "brk #0\n");
}

static void emit_str_literals() {
    write_str(1, ".section .rodata\n");
    for (struct string *str = strings; str; str = str->next) {
        if (!str->label)
            continue;
        writef(1, ".L.str.%d:\n", str->label);
        for (int i = 0; i <= str->len; i++) {
            writef(1, ".byte %d\n", str->chars[i]);
        }
    }
}

//=============================================================================
//= main

int main(int argc, char **argv) {
    if (argc != 2) {
        write_str(2, "usage: smolcc <file>\n");
        return 1;
    }
    const char *file_name = argv[1];
    if (inp = open(file_name, 0, 0), inp < 0) {
        writef(2, "error: cannot open '%s'\n", file_name);
        return 1;
    }

    init_tys();

    // lexer
    chr_pos.file = file_name;
    chr_pos.line = chr_pos.col = 1;
    next_chr();

    // parser
    lex();

    push_scope();

    while (tok != EOF) {
        p_decl(Decl_Global, 0);
    }

    for (struct sym *sym = curr_scope->head; sym; sym = sym->next) {
        if (sym->kind == Sym_Func) {
            if (sym->storage != Static && !sym->is_defined)
                continue;  // declaration, external linkage
            if (sym->storage == Static && !sym->is_defined)
                continue;  // declaration with internal linkage, missing definition
            emit_func(sym);
        } else if (sym->kind == Sym_Global) {
            if (sym->storage == Extern && !sym->is_defined)
                continue;  // declaration, external linkage
            if (!sym->is_defined)
                ;  // tentative definition
            emit_obj(sym);
        }
    }
    emit_rt_helpers();
    emit_str_literals();

    return 0;
}
