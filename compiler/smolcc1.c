#include <stdarg.h>

//=============================================================================
//= unistd

enum { stdin = 0, stdout = 1, stderr = 2 };
extern long read(long fd, void *buf, unsigned long count);
extern long write(long fd, const void *buf, unsigned long count);
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
    char ch = c;
    write(fd, &ch, 1);
}

static void write_uint(int fd, unsigned long n) {
    if (n >= 10) {
        write_uint(fd, n / 10);
    }
    write_char(fd, n % 10 + '0');
}

static void write_int(int fd, long n) {
    if (n < 0) {
        write_char(fd, '-');
        write_uint(fd, (unsigned long)(-(n + 1)) + 1);  // avoid overflow
    } else {
        write_uint(fd, n);
    }
}

static void vwritef(int fd, const char *fmt, va_list *args) {
    for (; *fmt; fmt++) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                write_int(fd, va_arg(*args, int));
            } else if (*fmt == 'l' && fmt[1] == 'd') {
                fmt++;
                write_int(fd, va_arg(*args, long));
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
    vwritef(stderr, fmt, &args);
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
        if (len == str->len && str_eq(str->chars, s))
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
    int line, col;
};

static void err_at(struct pos *pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    writef(stderr, "%s:%d:%d: error: ", "<stdin>", pos->line, pos->col);
    vwritef(stderr, fmt, &args);
    write_char(stderr, '\n');
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
    long val;
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
    Ty_Integer,
    Ty_Ptr,
    Ty_Arr,
    Ty_Func,
    Ty_Struct,
    Ty_VaList,
};

struct ty {
    int kind;
    // for Ty_Integer
    int is_signed, width;
    // for Ty_Ptr and Ty_Arr
    struct ty *base;
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

static struct ty *tys, *void_ty, *char_ty, *uchar_ty, *int_ty, *uint_ty, *long_ty, *ulong_ty, *va_list_ty;

static int ty_size(struct ty *t) {
    if (t->kind == Ty_Integer) {
        return t->width;
    } else if (t->kind == Ty_Ptr) {
        return 8;
    } else if (t->kind == Ty_Arr) {
        return ty_size(t->base) * t->arr_len;
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
        return ty_align(t->base);
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
    } else if (a->kind == Ty_Integer) {
        return a->is_signed == b->is_signed && a->width == b->width;
    } else if (a->kind == Ty_Ptr) {
        return a->base == b->base;
    } else if (a->kind == Ty_Arr) {
        return a->arr_len == b->arr_len && a->base == b->base;
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
    return t->kind == Ty_Integer;
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
    return is_ptr_ty(t) && t->base->kind == Ty_Void;
}
static int is_arr_ty(struct ty *t) {
    return t->kind == Ty_Arr;
}
static int is_obj_ty(struct ty *t) {
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
    t.base = base;
    return intern_ty(&t);
}

static struct ty *mk_arr_ty(struct ty *base, int len) {
    struct ty t;
    t.kind = Ty_Arr;
    t.base = base;
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

static struct ty *mk_integer_ty(int is_signed, int width) {
    struct ty t;
    t.kind = Ty_Integer;
    t.is_signed = is_signed;
    t.width = width;
    return intern_ty(&t);
}

static struct ty *get_common_ptr_ty(struct ty *t1, struct ty *t2) {
    assert("get_common_ptr_ty", is_ptr_ty(t1) && is_ptr_ty(t2));
    if (is_void_ptr(t1) || is_void_ptr(t2))
        return mk_ptr_ty(void_ty);
    return t1->base == t2->base ? t1 : 0;
}

static void init_tys() {
    struct ty tmp;
    tmp.kind = Ty_Void;
    void_ty = intern_ty(&tmp);
    tmp.kind = Ty_VaList;
    va_list_ty = intern_ty(&tmp);
    char_ty = mk_integer_ty(1, 1);
    uchar_ty = mk_integer_ty(0, 1);
    int_ty = mk_integer_ty(1, 4);
    uint_ty = mk_integer_ty(0, 4);
    long_ty = mk_integer_ty(1, 8);
    ulong_ty = mk_integer_ty(0, 8);
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
    err_at(pos, "'%s' %s. Previous declaration at %s:%d:%d", sym->name, msg, "<stdin>", other.line, other.col);
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

    long int_val;
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
    return is_lvalue(e) && is_obj_ty(e->ty);
}
static int is_null_ptr(struct expr *e) {
    return e->kind == Expr_Num && e->int_val == 0
        || e->kind == Expr_Cast && is_void_ptr(e->ty) && is_null_ptr(e->subs[0]);
}

//=============================================================================
//= eval

static unsigned long const_cast(struct ty *target, unsigned long a) {
    a = a & ~0UL >> (64 - target->width * 8);  // truncate
    if (target->is_signed && target->width < 8) {
        int shift = 64 - target->width * 8;
        a = (long)(a << shift) >> shift;  // sign-extend
    }
    return a;
}

static unsigned long eval(struct expr *e) {
    if (e->kind == Expr_Num) {
        return e->int_val;
    } else if (e->kind == Expr_Neg) {
        return const_cast(e->ty, -eval(e->subs[0]));
    } else if (e->kind == Expr_BitNot) {
        return const_cast(e->ty, ~eval(e->subs[0]));
    } else if (e->kind == Expr_Add) {
        return const_cast(e->ty, eval(e->subs[0]) + eval(e->subs[1]));
    } else if (e->kind == Expr_Sub) {
        return const_cast(e->ty, eval(e->subs[0]) - eval(e->subs[1]));
    } else if (e->kind == Expr_Mul) {
        return const_cast(e->ty, eval(e->subs[0]) * eval(e->subs[1]));
    } else if (e->kind == Expr_Shl) {
        return const_cast(e->ty, eval(e->subs[0]) << eval(e->subs[1]));
    } else if (e->kind == Expr_Shr && e->ty->is_signed) {
        return const_cast(e->ty, (long)eval(e->subs[0]) >> eval(e->subs[1]));
    } else if (e->kind == Expr_Shr) {
        return const_cast(e->ty, eval(e->subs[0]) >> eval(e->subs[1]));
    } else if (e->kind == Expr_BitAnd) {
        return const_cast(e->ty, eval(e->subs[0]) & eval(e->subs[1]));
    } else if (e->kind == Expr_BitXor) {
        return const_cast(e->ty, eval(e->subs[0]) ^ eval(e->subs[1]));
    } else if (e->kind == Expr_BitOr) {
        return const_cast(e->ty, eval(e->subs[0]) | eval(e->subs[1]));
    } else if (e->kind == Expr_Cast) {
        if (is_ptr_ty(e->ty) && is_null_ptr(e->subs[0])) {
            return 0;
        } else if (is_integer_ty(e->ty) && is_integer_ty(e->subs[0]->ty)) {
            return const_cast(e->ty, eval(e->subs[0]));
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
    return e->ty != t ? wrap_with(Expr_Cast, t, e) : e;
}

static struct ty *integer_promo_ty(struct ty *t1, struct ty *t2) {
    assert("integer_promo_ty", is_integer_ty(t1) && is_integer_ty(t2));
    int rank1 = t1->width + !t1->is_signed;
    int rank2 = t2->width + !t2->is_signed;
    if (rank1 < 4 || rank2 < 4)
        return int_ty;
    return rank1 > rank2 ? t1 : t2;
}

static void apply_ua_conv(struct expr **args) {
    struct ty *target_ty = integer_promo_ty(args[0]->ty, args[1]->ty);
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

static struct expr *apply_assign_conv(struct expr *e, struct ty *t) {
    if (e->ty == t) {
        return e;
    } else if (is_integer_ty(e->ty) && is_integer_ty(t)) {
        return cast_to(t, e);
    } else if (is_ptr_ty(t) && is_null_ptr(e)) {
        return cast_to(t, e);
    } else if (is_ptr_ty(t) && is_ptr_ty(e->ty)) {
        if (!(t->base == e->ty->base || is_void_ptr(t) || is_void_ptr(e->ty)))
            err_at(&e->pos, "target type mismatch");
        return cast_to(t, e);
    } else {
        err_at(&e->pos, "target type mismatch");
    }
}

static struct expr *ptr_decay(struct expr *e) {
    if (is_arr_ty(e->ty)) {
        return wrap_with(Expr_Cast, mk_ptr_ty(e->ty->base), wrap_with(Expr_Addr, mk_ptr_ty(e->ty), e));
    } else if (is_func_ty(e->ty)) {
        return wrap_with(Expr_Addr, mk_ptr_ty(e->ty), e);
    } else {
        return e;
    }
}

static struct expr *elab_expr(struct expr *e) {
    for (int i = 0; i < e->n_subs; i++) {
        e->subs[i] = elab_expr(e->subs[i]);
        if (e->kind != Expr_Addr) {
            e->subs[i] = ptr_decay(e->subs[i]);
        }
    }

    if (e->kind == Expr_Num) {
        e->ty = e->ty ? e->ty : int_ty;
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
        if (!is_ptr_ty(callee->ty) || callee->ty->base->kind != Ty_Func)
            err_at(&callee->pos, "called object is not a function");
        if (callee->kind != Expr_Addr || callee->subs[0]->kind != Expr_Func)
            err_at(&callee->pos, "indirect calls are not supported");
        struct ty *func = callee->subs[0]->sym->ty;
        if (e->n_subs - 1 > func->n_params && !func->is_va || e->n_subs - 1 < func->n_params)
            err_at(&e->pos, "wrong number of arguments");
        for (int i = 0; i < e->n_subs - 1; i++) {
            if (i < func->n_params) {
                e->subs[i + 1] = apply_assign_conv(e->subs[i + 1], func->param_tys[i]);
            } else if (!is_scalar(e->subs[i + 1]->ty)) {
                err_at(&e->subs[i + 1]->pos, "variadic arguments must be scalar");
            }
        }
        e->ty = func->ret_ty;
    } else if (e->kind == Expr_Member) {
        if (e->subs[0]->ty->kind != Ty_Struct)
            err_at(&e->pos, "member access on non-struct");
        struct sym *sym = e->subs[0]->ty->sym;
        if (!sym->is_defined)
            err_at(&e->pos, "member access on incomplete struct");
        e->sym = lookup_in(sym->scope, 0, e->fld_name);
        if (!e->sym)
            err_at(&e->pos, "no such member");
        e->ty = e->sym->ty;
    } else if (e->kind == Expr_Addr) {
        if (!is_addressable(e->subs[0]))
            err_at(&e->pos, "operand not addressable");
        e->ty = mk_ptr_ty(e->subs[0]->ty);
    } else if (e->kind == Expr_Deref) {
        if (!is_ptr_ty(e->subs[0]->ty))
            err_at(&e->pos, "operand not a pointer");
        e->ty = e->subs[0]->ty->base;
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
            e->ty = long_ty;
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
    } else if (e->kind == Expr_Lt || e->kind == Expr_Le) {
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

static long elab_init(struct expr *e, struct ty *target_ty) {
    e = elab_rvalue_expr(e);
    if (is_ptr_ty(target_ty) && is_null_ptr(e)) {
        return 0;
    } else if (is_integer_ty(target_ty) && is_integer_ty(e->ty)) {
        return const_cast(target_ty, eval(e));
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

enum { RDBUF_CAP = 256 };
static char rdbuf[RDBUF_CAP];
static long rdbuf_len, rdbuf_pos;
static struct pos chr_pos;
static int chr;

static int tok;
static struct pos tok_pos;
static char tok_str[MAX_TOK_LEN + 1];
static int tok_len;
static struct {
    unsigned long n;
    int u, l;  // suffixes: unsigned, long
    char str[MAX_TOK_LEN + 1];
} tok_val;

static int peek_char() {
    if (rdbuf_pos < rdbuf_len)
        return rdbuf[rdbuf_pos];
    rdbuf_pos = 0, rdbuf_len = read(stdin, rdbuf, RDBUF_CAP);
    return (rdbuf_len <= 0) ? EOF : rdbuf[rdbuf_pos];
}

static void skip_chr() {
    if (chr == '\n') {
        chr_pos.line++, chr_pos.col = 1;
    } else {
        chr_pos.col++;
    }
    if (chr = peek_char(), chr == EOF)
        return;
    rdbuf_pos++;
}

static void next_chr() {
    if (tok_len == MAX_TOK_LEN)
        err_at(&chr_pos, "token too long");
    tok_str[tok_len++] = chr;
    skip_chr();
}

static int eat_chr(int c) {
    return chr == c ? (next_chr(), 1) : 0;
}

static void lex() {
    while (1) {
        tok_len = 0, tok_pos = chr_pos;
        if (chr == EOF) {
            tok = EOF;
        } else if (eat_chr(' ') || eat_chr('\t') || eat_chr('\n')) {
            continue;
        } else if (chr >= '0' && chr <= '9') {
            unsigned long n = 0;
            while (chr >= '0' && chr <= '9') {
                if (n > (~0UL - (chr - '0')) / 10)
                    err_at(&chr_pos, "integer literal overflow");
                n = n * 10 + (chr - '0');
                next_chr();
            }
            tok_val.u = eat_chr('u') || eat_chr('U');
            tok_val.l = eat_chr('l') || eat_chr('L');
            tok_val.n = n;
            tok = Tok_Num;
        } else if (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_') {
            while (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_' || chr >= '0' && chr <= '9') {
                next_chr();
            }
            tok = Tok_Wrd;
        } else if (chr == '\'' || chr == '"') {
            int delim = chr, len = 0;
            skip_chr();
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
                if (eat_chr('\\')) {
                    const char *escapes = "abfnrtv\\'\"?0", *unescapes = "\a\b\f\n\r\t\v\\\'\"\?\0";
                    if (!find_chr(escapes, chr))
                        err_at(&chr_pos, "unknown escape sequence");
                    decoded = unescapes[find_chr(escapes, chr) - escapes];
                }
                skip_chr();
                tok_val.str[len++] = decoded;
            }
            if (delim == '\'' && len == 0)
                err_at(&chr_pos, "empty char literal");
            skip_chr();
            tok_val.str[len] = 0;
            tok_val.n = len;
            tok = delim == '"' ? Tok_Str : Tok_Chr;
        } else {
            int prev = chr;
            next_chr();
            if (prev == '#' || prev == '/' && eat_chr('/')) {
                while (chr != '\n' && chr != EOF) {
                    skip_chr();
                }
                continue;
            } else if (find_chr("<>!=", prev) && chr == '=' || find_chr("&|<>+-", prev) && chr == prev
                       || prev == '-' && chr == '>') {
                next_chr();
            } else if (prev == '.' && chr == '.' && peek_char() == '.') {
                next_chr(), next_chr();
            }
            tok = Tok_Sym;
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
    return at("void") || at("char") || at("int") || at("long") || at("va_list") || at("struct") || at("enum")
        || at("unsigned") || at("const");
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
    Prec_Primary
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
        acc->ty = tok_val.u ? (tok_val.l ? ulong_ty : uint_ty) : (tok_val.l ? long_ty : int_ty);
        if ((long)const_cast(acc->ty, acc->int_val) != acc->int_val)
            err_at(&pos, "integer literal overflow");
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
        acc = mk_expr(&pos, Expr_Num, 0);
        expect("(");
        acc->int_val = ty_size(p_tyname());
        expect(")");
        if (acc->int_val <= 0)
            err_at(&pos, "sizeof operand must be object");
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
            acc = mk_bin_expr(Expr_Lt, p_expr(Prec_Rel), acc);
        } else if (rbp < Prec_Rel && eat("<=")) {
            acc = mk_bin_expr(Expr_Le, acc, p_expr(Prec_Rel));
        } else if (rbp < Prec_Rel && eat(">=")) {
            acc = mk_bin_expr(Expr_Le, p_expr(Prec_Rel), acc);
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

static unsigned long p_const_expr() {
    return eval(elab_rvalue_expr(p_expr(Prec_Cond - 1)));
}

static struct stmt *p_stmt() {
    struct pos pos = tok_pos;
    struct stmt *s;
    if (eat("{")) {
        s = mk_stmt(&pos, Stmt_Block);
        push_scope();
        for (struct stmt **lastp = &s->sub; !eat("}"); lastp = &(*lastp)->next) {
            *lastp = p_stmt();
        }
        pop_scope();
    } else if (eat("return")) {
        s = mk_stmt(&pos, Stmt_Return);
        if (!is_void_ty(curr_func->ty->ret_ty)) {
            s->expr = elab_expr_expect(p_expr(0), curr_func->ty->ret_ty);
        }
        expect(";");
    } else if (eat("if")) {
        s = mk_stmt(&pos, Stmt_If);

        expect("(");
        s->expr = elab_cond_expr(p_expr(0));
        expect(")");

        push_scope();
        s->sub = p_stmt();
        pop_scope();

        if (eat("else")) {
            push_scope();
            s->sub->next = p_stmt();
            pop_scope();
        }

    } else if (at("for") || at("while")) {
        struct stmt *outer_loop = curr_loop;
        curr_loop = s = mk_stmt(&pos, Stmt_Loop);

        int is_while = eat("while") || !eat("for");

        push_scope();
        expect("(");
        if (is_while) {
            s->expr = elab_cond_expr(p_expr(0));
            s->sub = mk_stmt(&pos, Stmt_Empty);
            s->sub->next = mk_stmt(&pos, Stmt_Empty);
        } else {
            if (eat(";")) {
                s->sub = mk_stmt(&pos, Stmt_Empty);
            } else if (at_decl()) {
                s->sub = p_stmt();
            } else {
                s->sub = mk_stmt(&pos, Stmt_Expr);
                s->sub->expr = elab_rvalue_expr(p_expr(0));
                expect(";");
            }

            if (!at(";")) {
                s->expr = elab_cond_expr(p_expr(0));
            }
            expect(";");

            if (!at(")")) {
                s->sub->next = mk_stmt(&pos, Stmt_Expr);
                s->sub->next->expr = elab_rvalue_expr(p_expr(0));
            } else {
                s->sub->next = mk_stmt(&pos, Stmt_Empty);
            }
        }
        expect(")");

        push_scope();
        s->sub->next->next = p_stmt();
        pop_scope();
        pop_scope();
        curr_loop = outer_loop;
    } else if (at("break") || at("continue")) {
        if (!curr_loop)
            err_at(&pos, "break/continue outside loop");
        int kind = eat("break") || !eat("continue") ? Stmt_Break : Stmt_Continue;
        s = mk_stmt(&pos, kind);
        s->sub = curr_loop;
        expect(";");
    } else if (eat(";")) {
        s = mk_stmt(&pos, Stmt_Empty);
    } else if (at_decl()) {
        s = 0;
        p_decl(Decl_Local, &s);
        s = s ? s : mk_stmt(&pos, Stmt_Empty);
    } else {
        s = mk_stmt(&pos, Stmt_Expr);
        s->expr = elab_rvalue_expr(p_expr(0));
        expect(";");
    }
    return s;
}

static struct ty *p_struct() {
    struct pos name_pos = tok_pos;
    const char *name = tok == Tok_Wrd ? p_ident() : 0;

    int is_def = at("{");
    struct sym *sym = is_def ? 0 : lookup(1, name);

    if (!name && !is_def)
        err_at(&name_pos, "anonymous struct must be a definition");

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

    return sym->ty;
}

static struct ty *p_enum() {
    expect("{");
    for (int val = 0; !eat("}"); val++) {
        struct pos name_pos = tok_pos;
        const char *name = p_ident();
        if (eat("=")) {
            val = (int)p_const_expr();
        }
        if (!at("}")) {
            expect(",");
        }
        declare(&name_pos, 0, Sym_Const, 0, name, int_ty, 1)->val = val;
    }
    return int_ty;
}

static void p_decl(int scope, void *ctx) {
    struct pos pos = tok_pos;
    int storage = 0;
    struct ty *base_ty = 0;
    int is_unsigned = 0;
    while (1) {
        if (eat("const")) {
            // ignored
        } else if (!base_ty && eat("unsigned")) {
            is_unsigned = 1;
        } else if (!storage && eat("static")) {
            storage = Static;
        } else if (!storage && eat("extern")) {
            storage = Extern;
        } else if (!base_ty && eat("void")) {
            base_ty = void_ty;
        } else if (!base_ty && eat("int")) {
            base_ty = is_unsigned ? uint_ty : int_ty;
        } else if (!base_ty && eat("long")) {
            base_ty = is_unsigned ? ulong_ty : long_ty;
        } else if (!base_ty && eat("char")) {
            base_ty = is_unsigned ? uchar_ty : char_ty;
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
    if (is_unsigned && !is_integer_ty(base_ty))
        err_at(&pos, "unsigned specifier used with non-integer type");

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
                    if ((is_va = eat("..."))) {
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
            ty = mk_arr_ty(ty, p_const_expr());
            expect("]");
        } else if (eat("=") && (scope == Decl_Global || scope == Decl_Local)) {
            init = p_expr(Prec_Comma);
        }

        if (scope == Decl_TyName) {
            *(struct ty **)ctx = ty;
            return;  // max one type per abstract declaration
        } else if (scope == Decl_Param) {
            if (is_arr_ty(ty)) {
                ty = mk_ptr_ty(ty->base);
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
            if (!is_obj_ty(ty))
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

// register class
static char X(struct ty *ty) {
    return ty_size(ty) == 8 ? 'x' : 'w';
}

static void emit_str_load(struct string *str) {
    if (!str->label) {
        next_str_id++;
        str->label = next_str_id;
    }
    writef(stdout, "adrp x0, .L.str.%d\n", str->label);
    writef(stdout, "add x0, x0, :lo12:.L.str.%d\n", str->label);
}

static void emit_int_load(struct ty *ty, long val, int reg) {
    long chunk_mask = ~0U >> 16;
    long default_chunk = val < 0 ? chunk_mask : 0;
    for (int i = 0; i * 2 < ty_size(ty); i++, val = val >> 16) {
        long chunk = val & chunk_mask;
        if (i == 0) {
            chunk = val < 0 ? chunk | ~chunk_mask : chunk;
            writef(stdout, "mov %c%d, #%d // %ld\n", X(ty), reg, chunk, val);
        } else if (chunk != default_chunk) {
            writef(stdout, "movk %c%d, #%d, lsl #%ld\n", X(ty), reg, chunk, i * 16);
        }
    }
}

static void emit_push(int reg) {
    writef(stdout, "str x%d, [sp, #-16]!\n", reg);
}

static void emit_pop(int reg) {
    writef(stdout, "ldr x%d, [sp], #16\n", reg);
}

static void emit_load(struct ty *ty, int dst, int src) {
    const char *op = ty == char_ty && char_ty->is_signed ? "ldrsb" : ty == uchar_ty ? "ldrb" : "ldr";
    writef(stdout, "%s %c%d, [x%d]\n", op, X(ty), dst, src);
}

static void emit_store(struct ty *ty, int src, int dst) {
    const char *op = ty == char_ty || ty == uchar_ty ? "strb" : "str";
    writef(stdout, "%s %c%d, [x%d]\n", op, X(ty), src, dst);
}

static void emit_frame_addr(int offs, int reg) {
    emit_int_load(long_ty, offs, reg);
    writef(stdout, "add x%d, x29, x%d\n", reg, reg);
}

static void emit_frame_store(struct ty *ty, int offs, int reg) {
    emit_frame_addr(offs, 9);
    emit_store(ty, reg, 9);
}

static void emit_expr(struct expr *e);
static void emit_place_expr(struct expr *e);

static void emit_bin_subs(struct expr *e1, struct expr *e2) {
    emit_expr(e1);
    emit_push(0);
    emit_expr(e2);
    writef(stdout, "mov %c1, %c0\n", X(e1->ty), X(e1->ty));
    emit_pop(0);
}

static void emit_cmp_expr(struct expr *e, const char *cond, const char *ucond) {
    struct ty *ty = e->subs[0]->ty;
    emit_bin_subs(e->subs[0], e->subs[1]);
    writef(stdout, "cmp %c0, %c1\n", X(ty), X(ty));
    writef(stdout, "cset %c0, %s\n", X(ty), ty->is_signed ? cond : ucond);
}

static void emit_arith_expr(struct expr *e, const char *op) {
    struct ty *ty = e->subs[0]->ty;
    emit_bin_subs(e->subs[0], e->subs[1]);
    writef(stdout, "%s %c0, %c0, %c1\n", op, X(ty), X(ty), X(ty));
}

static void emit_assign_to_addr(struct ty *dst_ty, struct expr *e) {
    assert("emit_assign_to_addr", is_obj_ty(dst_ty));
    if (is_scalar(dst_ty)) {
        emit_push(0);
        emit_expr(e);
        emit_pop(1);
        emit_store(dst_ty, 0, 1);
    } else {
        emit_push(0);
        if (e->kind == Expr_Assign) {
            emit_place_expr(e->subs[0]);
            emit_assign_to_addr(e->ty, e->subs[1]);
        } else {
            if (!is_addressable(e))
                die("emit_assign_to_addr: unreachable: %d", e->kind);
            emit_place_expr(e);
        }
        writef(stdout, "mov x1, x0\n");
        emit_pop(0);
        emit_int_load(long_ty, ty_size(dst_ty), 2);
        writef(stdout, "bl _memcpy\n");
    }
}

static void emit_place_expr(struct expr *e) {
    if (e->kind == Expr_Var || e->kind == Expr_Func) {
        struct sym *sym = e->sym;
        if (sym->kind == Sym_Global || sym->kind == Sym_Func) {
            if (sym->storage == Extern && !sym->is_defined) {
                writef(stdout, "adrp x0, :got:%s\n", sym->name);
                writef(stdout, "ldr x0, [x0, :got_lo12:%s]\n", sym->name);
            } else {
                writef(stdout, "adrp x0, %s\n", sym->name);
                writef(stdout, "add x0, x0, :lo12:%s\n", sym->name);
            }
        } else if (sym->kind == Sym_Local) {
            emit_frame_addr(sym->offs, 0);
        } else {
            die("emit_place_expr: unreachable: %d (ident)", sym->kind);
        }
    } else if (e->kind == Expr_Member) {
        emit_place_expr(e->subs[0]);
        emit_int_load(long_ty, e->sym->offs, 1);
        writef(stdout, "add x0, x0, x1\n");
    } else if (e->kind == Expr_Assign || e->kind == Expr_Comma) {
        emit_expr(e);
    } else if (e->kind == Expr_Deref) {
        emit_expr(e->subs[0]);
    } else if (e->kind == Expr_Str) {
        emit_str_load(e->str_val);
    } else {
        die("emit_place_expr: unreachable: %d", e->kind);
    }
}

static void emit_expr(struct expr *e) {
    struct ty *ty = e->ty;
    if (is_lvalue(e)) {
        emit_place_expr(e);
        if (is_scalar(ty)) {
            emit_load(ty, 0, 0);
        }
    } else if (e->kind == Expr_Num) {
        emit_int_load(ty, e->int_val, 0);
    } else if (e->kind == Expr_Str) {
        emit_str_load(e->str_val);
    } else if (e->kind == Expr_PostDec || e->kind == Expr_PostInc) {
        const char *op = e->kind == Expr_PostInc ? "add" : "sub";
        int step = is_ptr_ty(ty) ? ty_size(ty->base) : 1;
        emit_place_expr(e->subs[0]);
        writef(stdout, "mov x2, x0\n");
        emit_load(ty, 0, 0);
        emit_int_load(ty, step, 1);
        writef(stdout, "%s %c1, %c0, %c1\n", op, X(ty), X(ty), X(ty));
        emit_store(ty, 1, 2);
    } else if (e->kind == Expr_Addr) {
        emit_place_expr(e->subs[0]);
    } else if (e->kind == Expr_BitNot) {
        emit_expr(e->subs[0]);
        writef(stdout, "mvn %c0, %c0\n", X(ty), X(ty));
    } else if (e->kind == Expr_Not) {
        emit_expr(e->subs[0]);
        writef(stdout, "cmp %c0, #0\n", X(ty));
        writef(stdout, "cset %c0, eq\n", X(ty));
    } else if (e->kind == Expr_Neg) {
        emit_expr(e->subs[0]);
        writef(stdout, "neg %c0, %c0\n", X(ty), X(ty));
    } else if (e->kind == Expr_Cast) {
        struct ty *to = e->ty, *from = e->subs[0]->ty;
        emit_expr(e->subs[0]);
        if (ty_size(to) < ty_size(from) && ty_size(to) == 1) {
            writef(stdout, "%s %c0, %c0\n", to == char_ty ? "sxtb" : "uxtb", X(to), X(to));
        } else if (ty_size(to) > ty_size(from) && ty_size(from) == 1) {
            writef(stdout, "%s %c0, %c0\n", from == char_ty ? "sxtb" : "uxtb", X(to), X(from));
        } else if (ty_size(to) > ty_size(from) && ty_size(from) == 4) {
            writef(stdout, "%s %c0, %c0\n", from == int_ty ? "sxtw" : "uxtw", X(to), X(from));
        }
    } else if (e->kind == Expr_Mod) {
        emit_bin_subs(e->subs[0], e->subs[1]);
        writef(stdout, "%s %c2, %c0, %c1\n", ty->is_signed ? "sdiv" : "udiv", X(ty), X(ty), X(ty));
        writef(stdout, "msub %c0, %c2, %c1, %c0\n", X(ty), X(ty), X(ty), X(ty));
    } else if (e->kind == Expr_Div) {
        emit_arith_expr(e, ty->is_signed ? "sdiv" : "udiv");
    } else if (e->kind == Expr_Mul) {
        emit_arith_expr(e, "mul");
    } else if (e->kind == Expr_Sub) {
        emit_arith_expr(e, "sub");
    } else if (e->kind == Expr_Add) {
        emit_arith_expr(e, "add");
    } else if (e->kind == Expr_Shr) {
        emit_arith_expr(e, ty->is_signed ? "asr" : "lsr");
    } else if (e->kind == Expr_Shl) {
        emit_arith_expr(e, "lsl");
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
    } else if (e->kind == Expr_And || e->kind == Expr_Or) {
        const char *cond = e->kind == Expr_And ? "z" : "nz";
        int cond_id = next_cond_id++;
        emit_expr(e->subs[0]);
        writef(stdout, "cb%s x0, .L.cond.%d.short\n", cond, cond_id);
        emit_expr(e->subs[1]);
        writef(stdout, ".L.cond.%d.short:\n", cond_id);
        writef(stdout, "cmp x0, #0\n");
        writef(stdout, "cset x0, ne\n");
    } else if (e->kind == Expr_PtrAdd || e->kind == Expr_PtrSub) {
        const char *op = e->kind == Expr_PtrAdd ? "add" : "sub";
        int size = ty_size(e->subs[0]->ty->base);
        emit_bin_subs(e->subs[0], e->subs[1]);
        if (size != 1) {
            emit_int_load(long_ty, size, 2);
            writef(stdout, "mul x1, x1, x2\n");
        }
        writef(stdout, "%s x0, x0, x1\n", op);
    } else if (e->kind == Expr_PtrDiff) {
        int size = ty_size(e->subs[0]->ty->base);
        emit_bin_subs(e->subs[0], e->subs[1]);
        writef(stdout, "sub x0, x0, x1\n");
        if (size != 1) {
            emit_int_load(long_ty, size, 1);
            writef(stdout, "sdiv x0, x0, x1\n");
        }
    } else if (e->kind == Expr_Cond) {
        int cond_id = next_cond_id++;
        emit_expr(e->subs[0]);
        writef(stdout, "cbz x0, .L.cond.%d.else\n", cond_id);
        emit_expr(e->subs[1]);
        writef(stdout, "b .L.cond.%d.end\n", cond_id);
        writef(stdout, ".L.cond.%d.else:\n", cond_id);
        emit_expr(e->subs[2]);
        writef(stdout, ".L.cond.%d.end:\n", cond_id);
    } else if (e->kind == Expr_Assign) {
        emit_place_expr(e->subs[0]);
        emit_assign_to_addr(ty, e->subs[1]);
    } else if (e->kind == Expr_Call) {
        struct expr *fn = e->subs[0];
        assert("emit_scalar_expr: func", fn->kind == Expr_Addr && fn->subs[0]->kind == Expr_Func);
        for (int i = 1; i < e->n_subs; i++) {
            emit_expr(e->subs[i]);
            emit_push(0);
        }
        for (int i = e->n_subs; i-- > 1;) {
            emit_pop(i - 1);
        }
        writef(stdout, "bl %s\n", fn->subs[0]->sym->name);
    } else if (e->kind == Expr_Comma) {
        emit_expr(e->subs[0]), emit_expr(e->subs[1]);
    } else if (e->kind == Expr_VaStart) {
        emit_place_expr(e->subs[0]);  // va_list*
        writef(stdout, "add x1, x29, #16  // top of frame\n");
        writef(stdout, "str x1, [x0]  // stack\n");
        emit_frame_addr(curr_func->va_offs + curr_func->va_size, 1);
        writef(stdout, "stp x1, xzr, [x0, #8]  // gr_top, vr_top\n");
        writef(stdout, "mov x1, #%d\n", -curr_func->va_size);
        writef(stdout, "stp w1, wzr, [x0, #24]  // gr_offs, vr_offs\n");
    } else if (e->kind == Expr_VaEnd) {
        // no-op
    } else if (e->kind == Expr_VaArg) {
        emit_place_expr(e->subs[0]);
        writef(stdout, "bl _va_arg\n");
        emit_load(e->ty, 0, 0);
    } else {
        die("emit_scalar_expr: unreachable: %d", e->kind);
    }
}

static void emit_init_decls(struct stmt *s) {
    if (!s)
        return;
    emit_init_decls(s->sub);
    if (s->expr) {
        emit_frame_addr(s->sym->offs, 0);
        emit_assign_to_addr(s->sym->ty, s->expr);
    }
}

static void emit_stmt(struct stmt *s) {
    writef(stdout, "// %s:%d:%d\n", "<stdin>", s->pos.line, s->pos.col);  // debug info
    if (s->kind == Stmt_Block) {
        for (struct stmt *sub = s->sub; sub; sub = sub->next) {
            emit_stmt(sub);
        }
    } else if (s->kind == Stmt_Return) {
        if (s->expr) {
            emit_expr(s->expr);
        }
        writef(stdout, "b .L.return.%s\n", curr_func->name);
    } else if (s->kind == Stmt_If) {
        int cond_id = next_cond_id++;
        emit_expr(s->expr);
        writef(stdout, "cbz x0, .L.if.%d.else\n", cond_id);
        emit_stmt(s->sub);
        writef(stdout, "b .L.if.%d.end\n", cond_id);
        writef(stdout, ".L.if.%d.else:\n", cond_id);
        if (s->sub->next) {
            emit_stmt(s->sub->next);
        }
        writef(stdout, ".L.if.%d.end:\n", cond_id);
    } else if (s->kind == Stmt_Loop) {
        s->loop_id = next_loop_id++;
        emit_stmt(s->sub);
        writef(stdout, "b .L.loop.%d.cond\n", s->loop_id);
        writef(stdout, ".L.loop.%d.body:\n", s->loop_id);
        emit_stmt(s->sub->next->next);
        writef(stdout, ".L.loop.%d.step:\n", s->loop_id);
        emit_stmt(s->sub->next);
        writef(stdout, ".L.loop.%d.cond:\n", s->loop_id);
        if (s->expr) {
            emit_expr(s->expr);
            writef(stdout, "cbnz x0, .L.loop.%d.body\n", s->loop_id);
        } else {
            writef(stdout, "b .L.loop.%d.body\n", s->loop_id);
        }
        writef(stdout, ".L.loop.%d.end:\n", s->loop_id);
    } else if (s->kind == Stmt_Break || s->kind == Stmt_Continue) {
        writef(stdout, "b .L.loop.%d.%s\n", s->sub->loop_id, s->kind == Stmt_Break ? "end" : "step");
    } else if (s->kind == Stmt_Decl) {
        emit_init_decls(s);
    } else if (s->kind == Stmt_Expr) {
        emit_expr(s->expr);
    } else if (s->kind != Stmt_Empty) {
        die("emit_stmt: unreachable: %d", s->kind);
    }
}

static void emit_func(struct sym *func) {
    curr_func = func;
    int n_va_args = func->ty->is_va ? 8 - func->ty->n_params : 0;
    func->va_offs = -align_up(curr_func->size + n_va_args * 8, 8);
    func->va_size = n_va_args * 8;
    curr_func->size = align_up(-func->va_offs, 16);

    writef(stdout, ".section .text\n");
    if (func->storage != Static) {
        writef(stdout, ".globl %s\n", func->name);
    }
    writef(stdout, "%s:\n", func->name);
    // prologue
    writef(stdout, "stp x29, x30, [sp, #-16]!\n");
    writef(stdout, "mov x29, sp\n");
    writef(stdout, "sub sp, sp, #%d\n", curr_func->size);
    struct sym *sym = func->scope->head;
    for (int i = 0; i < func->ty->n_params; i++, sym = sym->next) {
        emit_frame_store(sym->ty, sym->offs, i);
    }
    for (int i = func->ty->n_params; i < 8 && func->ty->is_va; i++) {
        emit_frame_store(mk_ptr_ty(void_ty), func->va_offs + (i - func->ty->n_params) * 8, i);
    }
    // body
    emit_stmt(func->body);
    // epilogue
    writef(stdout, ".L.return.%s:\n", func->name);
    writef(stdout, "mov sp, x29\n");
    writef(stdout, "ldp x29, x30, [sp], #16\n");
    writef(stdout, "ret\n");
    curr_func = 0;
}

static void emit_obj(struct sym *sym) {
    int size = ty_size(sym->ty), align = ty_align(sym->ty);

    if (is_scalar(sym->ty) && sym->is_defined) {
        writef(stdout, ".section .data\n");
    } else {
        writef(stdout, ".section .bss\n");
    }
    if (sym->storage != Static) {
        writef(stdout, ".globl %s\n", sym->name);
    }
    writef(stdout, ".balign %d\n", align);
    writef(stdout, "%s:\n", sym->name);

    if (is_scalar(sym->ty)) {
        if (size == 1) {
            writef(stdout, ".byte %d\n", (int)sym->val);
        } else if (size == 4) {
            writef(stdout, ".long %d\n", (int)sym->val);
        } else if (size == 8) {
            writef(stdout, ".quad %ld\n", sym->val);
        } else {
            die("emit_obj: unreachable: %d", size);
        }
    } else {
        writef(stdout, ".space %d\n", size);
    }
}

static void emit_rt_helpers() {
    writef(stdout, ".section .text\n");

    // void *_memcpy(void *d, const void *s, long n)
    writef(stdout, "_memcpy:\n");
    writef(stdout, "mov x3, x0\n");
    writef(stdout, "cbz x2, .L.memcpy.end\n");
    writef(stdout, ".L.memcpy.body:\n");
    writef(stdout, "ldrb w4, [x1], #1\n");
    writef(stdout, "strb w4, [x3], #1\n");
    writef(stdout, "subs x2, x2, #1\n");
    writef(stdout, "cbnz x2, .L.memcpy.body\n");
    writef(stdout, ".L.memcpy.end:\n");
    writef(stdout, "ret\n");  // x0 still holds dest

    // void *_va_arg(va_list *ap)
    writef(stdout, "_va_arg:\n");
    writef(stdout, "ldrsw x1, [x0, #24]  // gr_offs\n");
    writef(stdout, "ldr x2, [x0, #8]  // gr_top\n");
    writef(stdout, "add x3, x2, x1\n");
    writef(stdout, "add w1, w1, #8\n");
    writef(stdout, "str w1, [x0, #24]\n");
    writef(stdout, "mov x0, x3\n");
    writef(stdout, "ret\n");
}

static void emit_str_literals() {
    writef(stdout, ".section .rodata\n");
    for (struct string *str = strings; str; str = str->next) {
        if (!str->label)
            continue;
        writef(stdout, ".L.str.%d:\n", str->label);
        for (int i = 0; i <= str->len; i++) {
            writef(stdout, ".byte %d\n", str->chars[i]);
        }
    }
}

//=============================================================================
//= main

int main() {
    init_tys();

    // lexer
    chr_pos.line = 1, chr_pos.col = 0;
    skip_chr();

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
