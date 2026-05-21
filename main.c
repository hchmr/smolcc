#include <stdarg.h>

//============================================================================
//= unistd

extern int open(const char *pathname, int flags, int mode);
extern int read(int fd, void *buf, int count);
extern int write(int fd, const void *buf, int count);
extern void _exit(int status);
extern void abort();

//=============================================================================
//= io

enum { EOF = -1 };

static char rdbuf[2];
static int rdbuf_len, rdbuf_pos;

static int peek_char(int fd) {
    if (rdbuf_pos < rdbuf_len)
        return rdbuf[rdbuf_pos];
    rdbuf_pos = 0, rdbuf_len = read(fd, rdbuf, 2);
    if (rdbuf_len <= 0)
        return EOF;
    return rdbuf[rdbuf_pos];
}

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
            } else {
                write_char(fd, '%');
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
    if (a == b)
        return 1;
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

static void die(const char *label, const char *msg) {
    writef(2, "%s: %s\n", label, msg);
    abort();
}

static void assert(const char *label, int condition) {
    if (!condition)
        die(label, "assertion failed");
}

static void unreachable_case(const char *label, int value) {
    writef(2, "%s: case not handled: %d\n", label, value);
    abort();
}

//=============================================================================
//= arena

enum { Arena_Cap = 8 * 1024 * 1024 };  // 8 MiB
static char arena[Arena_Cap];
static int arena_len;

static void *alloc(int len) {
    arena_len = align_up(arena_len, 8) + len;
    if (arena_len >= Arena_Cap)
        die("alloc", "out of memory");
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

struct string *intern(const char *s, int len) {
    for (struct string *str = strings; str; str = str->next)
        if (str_eq(str->chars, s))
            return str;
    struct string *new_str = alloc(sizeof(struct string));
    new_str->chars = mem_clone((void *)s, len + 1);
    new_str->len = len;
    new_str->next = strings;
    strings = new_str;
    return new_str;
}

//=============================================================================
//= diag

struct pos {
    const char *file;
    int line, col;
};

static void error_at(struct pos *pos, const char *fmt, ...) {
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
    int storage_class;
    const char *name;
    struct type *type;
    int is_defined;

    // for Sym_Const
    int val;
    // for Sym_Func
    struct stmt *body;
    struct sym *last_param;
    int va_area_offset, va_area_size;
    // for Sym_Struct and Sym_Func
    struct scope *scope;
    int size, align;
    // for Sym_Local and Sym_Field
    int offset;

    struct sym *next;
};

//=============================================================================
//= types

enum {
    Type_Void,
    Type_Char,
    Type_Int,
    Type_Ptr,
    Type_Array,
    Type_Func,
    Type_Struct,
    Type_VaList,
};

struct type {
    int kind;
    // for Type_Ptr and Type_Array
    struct type *ptr_to;
    // for Type_Array
    int array_len;
    // for Type_Func
    struct type *ret_type;
    struct type *param_types[MAX_FUNC_PARAMS];
    int n_params, is_va;
    // for Type_Struct
    struct sym *sym;
    // for type interning
    struct type *next;
};

static struct type *types, *void_type, *char_type, *int_type, *va_list_type;

static int type_size(struct type *ty) {
    if (ty->kind == Type_Char) {
        return 1;
    } else if (ty->kind == Type_Int) {
        return 4;
    } else if (ty->kind == Type_Ptr) {
        return 8;
    } else if (ty->kind == Type_Array) {
        return type_size(ty->ptr_to) * ty->array_len;
    } else if (ty->kind == Type_Struct) {
        return ty->sym->is_defined ? ty->sym->size : 0;
    } else if (ty->kind == Type_VaList) {
        return 32;
    } else {
        return 0;
    }
}

static int type_align(struct type *ty) {
    if (ty->kind == Type_Array) {
        return type_align(ty->ptr_to);
    } else if (ty->kind == Type_Struct) {
        return ty->sym->is_defined ? ty->sym->align : 0;
    } else if (ty->kind == Type_VaList) {
        return 8;
    } else {
        return type_size(ty);
    }
}

// type predicates

static int type_eq(struct type *a, struct type *b) {
    if (a == b) {
        return 1;
    } else if (a->kind != b->kind) {
        return 0;
    } else if (a->kind == Type_Ptr) {
        return a->ptr_to == b->ptr_to;
    } else if (a->kind == Type_Array) {
        return a->array_len == b->array_len && a->ptr_to == b->ptr_to;
    } else if (a->kind == Type_Func) {
        if (a->ret_type != b->ret_type || a->n_params != b->n_params || a->is_va != b->is_va)
            return 0;
        for (int i = 0; i < a->n_params; i++)
            if (a->param_types[i] != b->param_types[i])
                return 0;
        return 1;
    } else if (a->kind == Type_Struct) {
        return a->sym == b->sym;
    } else {
        return 1;
    }
}

static int is_integer_type(struct type *ty) {
    return ty->kind == Type_Int || ty->kind == Type_Char;
}
static int is_void_type(struct type *ty) {
    return ty->kind == Type_Void;
}
static int is_ptr_type(struct type *ty) {
    return ty->kind == Type_Ptr;
}
static int is_scalar(struct type *ty) {
    return is_integer_type(ty) || is_ptr_type(ty);
}
static int is_func_type(struct type *ty) {
    return ty->kind == Type_Func;
}
static int is_void_ptr(struct type *ty) {
    return is_ptr_type(ty) && ty->ptr_to->kind == Type_Void;
}
static int is_array_type(struct type *ty) {
    return ty->kind == Type_Array;
}
static int is_object_type(struct type *ty) {
    return type_size(ty) > 0;
}

static struct type *intern_type(struct type *ty) {
    for (struct type *t = types; t; t = t->next)
        if (type_eq(t, ty))
            return t;
    struct type *t = mem_clone(ty, sizeof(struct type));
    t->next = types;
    types = t;
    return t;
}

static struct type *new_ptr_type(struct type *base) {
    struct type ty;
    ty.kind = Type_Ptr;
    ty.ptr_to = base;
    return intern_type(&ty);
}

static struct type *new_array_type(struct type *base, int len) {
    struct type ty;
    ty.kind = Type_Array;
    ty.ptr_to = base;
    ty.array_len = len;
    return intern_type(&ty);
}

static struct type *new_struct_type(struct sym *sym) {
    struct type ty;
    ty.kind = Type_Struct;
    ty.sym = sym;
    return intern_type(&ty);
}

static struct type *new_func_type(struct type *ret_type, struct type **param_types, int n_params, int is_va) {
    struct type ty;
    ty.kind = Type_Func;
    ty.ret_type = ret_type;
    for (int i = 0; i < n_params; i++) {
        ty.param_types[i] = param_types[i];
    }
    ty.n_params = n_params;
    ty.is_va = is_va;
    return intern_type(&ty);
}

static struct type *uac_type(struct type *t1, struct type *t2) {
    assert("uac_type", is_integer_type(t1) && is_integer_type(t2));
    return t1->kind < t2->kind ? t2 : t1;
}

static struct type *get_common_ptr_type(struct type *t1, struct type *t2) {
    assert("get_common_ptr_type", is_ptr_type(t1) && is_ptr_type(t2));
    if (t1->ptr_to == t2->ptr_to)
        return t1;
    if (is_void_ptr(t1))
        return t2;
    if (is_void_ptr(t2))
        return t1;
    return 0;
}

static void init_types() {
    struct type tmp;
    tmp.kind = Type_Void;
    void_type = intern_type(&tmp);
    tmp.kind = Type_Char;
    char_type = intern_type(&tmp);
    tmp.kind = Type_Int;
    int_type = intern_type(&tmp);
    tmp.kind = Type_VaList;
    va_list_type = intern_type(&tmp);
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
    if (scope->head) {
        return scope->tail->next = sym, scope->tail = sym;
    } else {
        return scope->head = scope->tail = sym;
    }
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
    error_at(pos, "'%s' %s. Previous declaration at %s:%d:%d", sym->name, msg, other.file, other.line, other.col);
}

static struct sym *declare(struct pos *pos, struct sym *parent_sym, int kind, int storage_class, const char *name,
                           struct type *type, int is_def) {
    struct scope *scope = curr_scope;
    if (parent_sym && parent_sym->kind == Sym_Struct) {
        scope = parent_sym->scope;
    }

    struct sym *sym = lookup_in(scope, kind == Sym_Struct, name);
    if (sym) {
        if (sym->kind != kind)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        if (kind != Sym_Struct && sym->type != type)
            decl_conflict(pos, sym, "already declared with a different type");
        if (is_def && sym->is_defined)
            decl_conflict(pos, sym, "already defined");
        if (kind == Sym_Global && sym->storage_class == Static && storage_class == 0)
            decl_conflict(pos, sym, "already declared as static");
        if ((kind == Sym_Global || kind == Sym_Func) && sym->storage_class != Static && storage_class == Static)
            decl_conflict(pos, sym, "already declared as non-static");
        if (kind != Sym_Struct) {
            sym->is_defined = sym->is_defined | is_def;
        }
        return sym;
    }
    sym = add_sym(pos, scope, kind, name);
    sym->type = kind == Sym_Struct ? new_struct_type(sym) : type;
    sym->is_defined = is_def;
    sym->storage_class = storage_class;
    if (parent_sym && (kind == Sym_Local || kind == Sym_Field)) {
        int size = type_size(type), align = type_align(type);
        if (kind == Sym_Local) {
            sym->offset = -align_up(parent_sym->size + size, align);
            parent_sym->size = -sym->offset;
        } else {
            sym->offset = align_up(parent_sym->size, align);
            parent_sym->size = sym->offset + size;
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
    Expr_Ident,
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
    struct type *type;

    int int_val;
    struct string *str_val;
    const char *name;
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
    struct type *type;

    int loop_id;

    struct stmt *next;
};

static struct expr *new_expr(struct pos *pos, int kind, int n_args) {
    struct expr *expr = alloc(sizeof(struct expr));
    expr->pos = *pos;
    expr->kind = kind;
    expr->n_subs = n_args;
    expr->subs = alloc(sizeof(struct expr *) * n_args);
    return expr;
}

static struct expr *new_unary_expr(struct pos *pos, int kind, struct expr *sub) {
    struct expr *expr = new_expr(pos, kind, 1);
    expr->subs[0] = sub;
    return expr;
}

static struct expr *new_bin_expr(int kind, struct expr *lhs, struct expr *rhs) {
    struct expr *expr = new_expr(&lhs->pos, kind, 2);
    expr->subs[0] = lhs;
    expr->subs[1] = rhs;
    return expr;
}

static struct stmt *new_stmt(struct pos *pos, int kind) {
    struct stmt *stmt = alloc(sizeof(struct stmt));
    stmt->pos = *pos;
    stmt->kind = kind;
    return stmt;
}

static int is_var_expr(struct expr *expr) {
    return expr->kind == Expr_Ident && (expr->sym->kind == Sym_Local || expr->sym->kind == Sym_Global);
}
static int is_func_expr(struct expr *expr) {
    return expr->kind == Expr_Ident && expr->sym->kind == Sym_Func;
}
static int is_lvalue(struct expr *expr) {
    return is_var_expr(expr) || expr->kind == Expr_Deref || expr->kind == Expr_Member;
}
static int is_addressable(struct expr *expr) {
    return is_lvalue(expr) || is_func_expr(expr) || expr->kind == Expr_Str;
}
static int is_assignable(struct expr *expr) {
    return is_lvalue(expr) && is_object_type(expr->type);
}
static int is_null_ptr(struct expr *expr) {
    return expr->kind == Expr_Num && expr->int_val == 0
        || expr->kind == Expr_Cast && is_void_ptr(expr->type) && is_null_ptr(expr->subs[0]);
}

//=============================================================================
//= eval

static int const_cast(int value, struct type *type) {
    if (type->kind == Type_Char)
        return (char)value;
    else if (type->kind == Type_Int)
        return (int)value;
    else
        unreachable_case("const_cast", type->kind);
}

static int eval(struct expr *expr) {
    if (expr->kind == Expr_Num) {
        return expr->int_val;
    } else if (expr->kind == Expr_Ident) {
        if (expr->sym->kind != Sym_Const)
            error_at(&expr->pos, "not a constant");
        return expr->sym->val;
    } else if (expr->kind == Expr_Neg) {
        return -eval(expr->subs[0]);
    } else if (expr->kind == Expr_Add) {
        return eval(expr->subs[0]) + eval(expr->subs[1]);
    } else if (expr->kind == Expr_Sub) {
        return eval(expr->subs[0]) - eval(expr->subs[1]);
    } else if (expr->kind == Expr_Mul) {
        return eval(expr->subs[0]) * eval(expr->subs[1]);
    } else if (expr->kind == Expr_Shl) {
        return eval(expr->subs[0]) << eval(expr->subs[1]);
    } else if (expr->kind == Expr_Shr) {
        return eval(expr->subs[0]) >> eval(expr->subs[1]);
    } else if (expr->kind == Expr_Cast) {
        if (is_ptr_type(expr->type) && is_null_ptr(expr->subs[0])) {
            return 0;
        } else if (is_integer_type(expr->type) && is_integer_type(expr->subs[0]->type)) {
            return const_cast(eval(expr->subs[0]), expr->type);
        } else {
            error_at(&expr->pos, "bad cast in constant expression");
        }
    } else {
        error_at(&expr->pos, "expression cannot be evaluated at compile time");
    }
}

//=============================================================================
//= elab

static struct sym *curr_func;
static struct stmt *curr_loop;

static struct expr *wrap_with(int kind, struct type *type, struct expr *orig) {
    struct expr *wrapper = new_expr(&orig->pos, kind, 1);
    wrapper->type = type;
    wrapper->subs[0] = orig;
    return wrapper;
}

// coercion
static struct expr *cast_to(struct type *t, struct expr *e) {
    assert("cast_to", is_scalar(t) && is_scalar(e->type));
    if (e->type == t)
        return e;
    return wrap_with(Expr_Cast, t, e);
}

static void apply_uac(struct expr **args) {
    assert("apply_uac", is_integer_type(args[0]->type) && is_integer_type(args[1]->type));
    struct type *target_type = uac_type(args[0]->type, args[1]->type);
    args[0] = cast_to(target_type, args[0]);
    args[1] = cast_to(target_type, args[1]);
}

static void apply_null_ptr_conversion(struct expr **args) {
    if (is_null_ptr(args[0]) && is_ptr_type(args[1]->type)) {
        args[0] = cast_to(args[1]->type, args[0]);
    } else if (is_null_ptr(args[1]) && is_ptr_type(args[0]->type)) {
        args[1] = cast_to(args[0]->type, args[1]);
    }
}

static void unify_ptr_operands(struct expr **args) {
    assert("unify_ptr_operands", is_ptr_type(args[0]->type) && is_ptr_type(args[1]->type));
    struct type *ptr_type = get_common_ptr_type(args[0]->type, args[1]->type);
    if (!ptr_type)
        error_at(&args[0]->pos, "incompatible pointer operands");
    args[0] = cast_to(ptr_type, args[0]);
    args[1] = cast_to(ptr_type, args[1]);
}

static struct expr *apply_assignment_conversion(struct expr *rhs, struct type *t) {
    if (rhs->type == t) {
        return rhs;
    } else if (is_integer_type(rhs->type) && is_integer_type(t)) {
        return cast_to(t, rhs);
    } else if (is_ptr_type(t) && is_null_ptr(rhs)) {
        return cast_to(t, rhs);
    } else if (is_ptr_type(t) && is_ptr_type(rhs->type)) {
        if (!(t->ptr_to == rhs->type->ptr_to || is_void_ptr(t) || is_void_ptr(rhs->type)))
            error_at(&rhs->pos, "target type mismatch. Pointer types are incompatible.");
        return cast_to(t, rhs);
    } else {
        error_at(&rhs->pos, "target type mismatch");
    }
}

static struct expr *ptr_decay(struct expr *e) {
    if (is_array_type(e->type)) {
        return wrap_with(Expr_Cast, new_ptr_type(e->type->ptr_to), wrap_with(Expr_Addr, new_ptr_type(e->type), e));
    } else if (is_func_type(e->type)) {
        return wrap_with(Expr_Addr, new_ptr_type(e->type), e);
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
    int k = e->kind;

    // recursively elaborate subexpressions
    elab_subexprs(e);

    // Primary expressions
    if (k == Expr_Num) {
        e->type = int_type;
    } else if (k == Expr_Chr) {
        e->kind = Expr_Num;
        e->type = char_type;
    } else if (k == Expr_Str) {
        e->type = new_array_type(char_type, e->str_val->len + 1);
    } else if (k == Expr_Ident) {
        struct sym *sym = lookup(0, e->name);
        if (!sym)
            error_at(&e->pos, "undefined symbol");
        if (sym->kind != Sym_Const && sym->kind != Sym_Local && sym->kind != Sym_Global && sym->kind != Sym_Func)
            error_at(&e->pos, "symbol is not a variable, function, or constant");
        e->sym = sym;
        e->type = sym->type;
        if (sym->kind == Sym_Const) {
            e->kind = Expr_Num;
            e->int_val = sym->val;
        }
    } else if (k == Expr_PostInc || k == Expr_PostDec) {
        if (!is_assignable(e->subs[0]))
            error_at(&e->pos, "operand must be assignable");
        if (!is_integer_type(e->subs[0]->type) && !is_ptr_type(e->subs[0]->type))
            error_at(&e->pos, "cannot increment/decrement operand of this type");
        e->type = e->subs[0]->type;
    } else if (k == Expr_Call) {
        struct expr *callee = e->subs[0];
        if (!is_ptr_type(callee->type) || callee->type->ptr_to->kind != Type_Func)
            error_at(&callee->pos, "called object is not a function");
        if (callee->kind != Expr_Addr || callee->subs[0]->kind != Expr_Ident || callee->subs[0]->sym->kind != Sym_Func)
            error_at(&callee->pos, "only direct function calls are supported");
        struct type *func = callee->subs[0]->sym->type;
        struct expr **args = &e->subs[1];
        int n_args = e->n_subs - 1;
        if (n_args > func->n_params && !func->is_va)
            error_at(&e->pos, "too many arguments in function call");
        if (n_args < func->n_params)
            error_at(&e->pos, "too few arguments in function call");
        for (int i = 0; i < n_args; i++) {
            if (i < func->n_params) {
                args[i] = apply_assignment_conversion(args[i], func->param_types[i]);
            } else {
                if (!is_scalar(args[i]->type))
                    error_at(&args[i]->pos, "variadic arguments must have scalar types");
            }
        }
        e->type = func->ret_type;
    } else if (k == Expr_Member) {
        if (e->subs[0]->type->kind != Type_Struct)
            error_at(&e->pos, "member access on non-struct type");
        struct sym *sym = e->subs[0]->type->sym;
        if (!sym->is_defined)
            error_at(&e->pos, "member access on incomplete struct type");
        struct sym *field = lookup_in(sym->scope, 0, e->name);
        if (!field)
            error_at(&e->pos, "member not found in struct");
        e->sym = field;
        e->type = field->type;
    } else if (k == Expr_Addr) {
        if (!is_addressable(e->subs[0]))
            error_at(&e->pos, "operand must be addressable");
        e->type = new_ptr_type(e->subs[0]->type);
    } else if (k == Expr_Deref) {
        if (!is_ptr_type(e->subs[0]->type))
            error_at(&e->pos, "operand must be a pointer");
        e->type = e->subs[0]->type->ptr_to;
    } else if (k == Expr_Neg) {
        if (!is_integer_type(e->subs[0]->type))
            error_at(&e->pos, "operand must be arithmetic");
        e->type = e->subs[0]->type;
    } else if (k == Expr_BitNot) {
        if (!is_integer_type(e->subs[0]->type))
            error_at(&e->pos, "operand must be integer");
        e->type = e->subs[0]->type;
    } else if (k == Expr_Not) {
        if (!is_scalar(e->subs[0]->type))
            error_at(&e->pos, "operand must be scalar");
        e->type = int_type;
    } else if (k == Expr_Cast) {
        if (!is_scalar(e->type) || !is_scalar(e->subs[0]->type))
            error_at(&e->pos, "cast requires scalar types");
    } else if (k == Expr_Mul || k == Expr_Div || k == Expr_Mod || k == Expr_Add || k == Expr_Sub) {
        if (k == Expr_Add && is_integer_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            void *tmp = e->subs[0];
            e->subs[0] = e->subs[1], e->subs[1] = tmp;
        }
        if (k == Expr_Add && is_ptr_type(e->subs[0]->type) && is_integer_type(e->subs[1]->type)) {
            e->kind = Expr_PtrAdd;
            e->type = e->subs[0]->type;
        } else if (k == Expr_Sub && is_ptr_type(e->subs[0]->type) && is_integer_type(e->subs[1]->type)) {
            e->kind = Expr_PtrSub;
            e->type = e->subs[0]->type;
        } else if (k == Expr_Sub && is_ptr_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            if (e->subs[0]->type != e->subs[1]->type)
                error_at(&e->pos, "pointer types must match");
            e->kind = Expr_PtrDiff;
            e->type = int_type;  // standard doesn't mandate pointer-sized ptrdiff_t, so int is sufficient
        } else if (is_integer_type(e->subs[0]->type) && is_integer_type(e->subs[1]->type)) {
            apply_uac(e->subs);
            e->type = e->subs[0]->type;
        } else {
            error_at(&e->pos, "operands must have arithmetic types");
        }
    } else if (k == Expr_Shl || k == Expr_Shr) {
        if (!is_integer_type(e->subs[0]->type) || !is_integer_type(e->subs[1]->type))
            error_at(&e->pos, "operands must have integer types");
        e->type = e->subs[0]->type;  // lhs determines type
    } else if (k == Expr_Eq || k == Expr_Ne) {
        apply_null_ptr_conversion(e->subs);
        if (is_integer_type(e->subs[0]->type) && is_integer_type(e->subs[1]->type)) {
            apply_uac(e->subs);
        } else if (is_ptr_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            unify_ptr_operands(e->subs);
        } else {
            error_at(&e->pos, "operands of equality operators must have compatible types");
        }
        e->type = int_type;
    } else if (k == Expr_Lt || k == Expr_Le || k == Expr_Gt || k == Expr_Ge) {
        if (is_integer_type(e->subs[0]->type) && is_integer_type(e->subs[1]->type)) {
            apply_uac(e->subs);
        } else if (is_ptr_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            unify_ptr_operands(e->subs);
        } else {
            error_at(&e->pos, "operands must have arithmetic types");
        }
        e->type = int_type;
    } else if (k == Expr_BitAnd || k == Expr_BitXor || k == Expr_BitOr) {
        if (!is_integer_type(e->subs[0]->type) || !is_integer_type(e->subs[1]->type))
            error_at(&e->pos, "operands must have integer types");
        apply_uac(e->subs);
        e->type = e->subs[0]->type;
    } else if (k == Expr_And || k == Expr_Or) {
        if (!is_scalar(e->subs[0]->type) || !is_scalar(e->subs[1]->type))
            error_at(&e->pos, "operands must have scalar types");
        e->type = int_type;
    } else if (k == Expr_Cond) {
        apply_null_ptr_conversion(e->subs + 1);
        if (is_void_type(e->subs[1]->type) && is_void_type(e->subs[2]->type)) {
            // pass
        } else if (is_integer_type(e->subs[1]->type) && is_integer_type(e->subs[2]->type)) {
            apply_uac(e->subs + 1);
        } else if (is_ptr_type(e->subs[1]->type) && is_ptr_type(e->subs[2]->type)) {
            unify_ptr_operands(e->subs + 1);
        } else {
            error_at(&e->pos, "operands of conditional operator must have compatible types");
        }
        e->type = e->subs[1]->type;
    } else if (k == Expr_Assign) {
        if (!is_assignable(e->subs[0]))
            error_at(&e->pos, "operand must be assignable");
        e->subs[1] = apply_assignment_conversion(e->subs[1], e->subs[0]->type);
        e->type = e->subs[0]->type;
    } else if (k == Expr_Comma) {
        e->type = e->subs[1]->type;
    } else if (k == Expr_VaStart) {
        if (curr_func->type->kind != Type_Func || !curr_func->type->is_va)
            error_at(&e->pos, "va_start used outside of a variadic function");
        if (e->subs[0]->type != va_list_type)
            error_at(&e->pos, "va_start operand must be of type va_list");
        if (e->subs[1]->kind != Expr_Ident || e->subs[1]->sym != curr_func->last_param)
            error_at(&e->pos, "second operand of va_start must be a parameter name");
        e->type = void_type;
    } else if (k == Expr_VaEnd) {
        if (e->subs[0]->type != va_list_type)
            error_at(&e->pos, "va_end operand must be of type va_list");
        e->type = void_type;
    } else if (k == Expr_VaArg) {
        if (e->subs[0]->type != va_list_type)
            error_at(&e->pos, "va_arg first operand must be of type va_list");
        if (!is_scalar(e->type))
            error_at(&e->pos, "va_arg second operand must have scalar type");
    } else {
        unreachable_case("elab_expr", k);
    }
    return e;
}

static struct expr *elab_rvalue_expr(struct expr *expr) {
    return ptr_decay(elab_expr(expr));
}

static struct expr *elab_expr_expect(struct expr *expr, struct type *expected) {
    return apply_assignment_conversion(elab_rvalue_expr(expr), expected);
}

static struct expr *elab_cond_expr(struct expr *expr) {
    expr = elab_rvalue_expr(expr);
    if (!is_scalar(expr->type))
        error_at(&expr->pos, "condition must have scalar type");
    return expr;
}

static int elab_init(struct expr *expr, struct type *target_type) {
    expr = elab_rvalue_expr(expr);
    if (is_ptr_type(target_type) && is_null_ptr(expr)) {
        return 0;
    } else if (is_integer_type(target_type) && is_integer_type(expr->type)) {
        return const_cast(eval(expr), target_type);
    } else {
        error_at(&expr->pos, "target type mismatch");
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
                    error_at(&chr_pos, "unterminated string/char literal");
                if (delim == '\'' && len != 0)
                    error_at(&chr_pos, "too many characters in char literal");
                if (len == MAX_TOK_LEN)
                    error_at(&chr_pos, "string/char literal too long");
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
                error_at(&chr_pos, "empty char literal");
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
        error_at(&tok_pos, "expected '%s'", t);
}

static void unexpected_expected(const char *d) {
    error_at(&tok_pos, "expected %s, got '%s'", d, tok_str);
}

static const char *p_ident() {
    if (tok != Tok_Wrd)
        unexpected_expected("identifier");
    struct string *res = intern(tok_str, tok_len + 1);
    lex();
    return res->chars;
}

static int p_num() {
    if (tok != Tok_Num)
        unexpected_expected("number");
    int res = tok_val.n;
    lex();
    return res;
}

//=============================================================================
//= parse

enum {
    Decl_Global,
    Decl_Local,
    Decl_Param,
    Decl_Struct,
    Decl_TypeName,
};

static void p_decl(int scope, void *ctx);

static int at_storage_class() {
    return at("static") || at("extern");
}

static int at_typename() {
    return at("void") || at("char") || at("int") || at("va_list") || at("struct") || at("enum") || at("const");
}

static int at_decl() {
    return at_storage_class() || at_typename();
}

static struct type *p_typename() {
    struct type *type;
    p_decl(Decl_TypeName, &type);
    return type;
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
static int p_const_expr();

static struct expr *p_unary_expr(struct pos *pos, int kind) {
    struct expr *res = p_expr(Prec_Unary);
    return new_unary_expr(pos, kind, res);
}

static struct expr *p_bin_expr(struct expr *lhs, int kind, int rbp) {
    struct expr *rhs = p_expr(rbp);
    return new_bin_expr(kind, lhs, rhs);
}

static struct expr *p_expr(int rbp) {
    struct pos pos = tok_pos;
    struct expr *acc;
    if (eat("(")) {
        if (at_decl()) {
            struct type *type = p_typename();
            expect(")");
            struct expr *tmp = p_expr(Prec_Unary);
            acc = new_unary_expr(&pos, Expr_Cast, tmp);
            acc->type = type;
        } else {
            acc = p_expr(0);
            expect(")");
        }
    } else if (eat("-")) {
        acc = p_unary_expr(&pos, Expr_Neg);
    } else if (eat("~")) {
        acc = p_unary_expr(&pos, Expr_BitNot);
    } else if (eat("!")) {
        acc = p_unary_expr(&pos, Expr_Not);
    } else if (tok == Tok_Num) {
        acc = new_expr(&pos, Expr_Num, 0);
        acc->int_val = p_num();
    } else if (tok == Tok_Str) {
        acc = new_expr(&pos, Expr_Str, 0);
        acc->str_val = intern(tok_val.str, tok_val.n + 1);
        lex();
    } else if (tok == Tok_Chr) {
        acc = new_expr(&pos, Expr_Chr, 0);
        acc->int_val = tok_val.str[0];
        lex();
    } else if (eat("&")) {
        acc = p_unary_expr(&pos, Expr_Addr);
    } else if (eat("*")) {
        acc = p_unary_expr(&pos, Expr_Deref);
    } else if (eat("sizeof")) {
        expect("(");
        struct type *type = p_typename();
        expect(")");
        acc = new_expr(&pos, Expr_Num, 0);
        acc->int_val = type_size(type);
    } else if (eat("va_start")) {
        expect("(");
        struct expr *arg = p_expr(Prec_Comma);
        expect(",");
        struct expr *other = p_expr(Prec_Comma);
        expect(")");
        struct sym *last_param_sym = other->kind == Expr_Ident ? lookup(0, other->name) : 0;
        if (!last_param_sym || last_param_sym != curr_func->last_param)
            error_at(&other->pos, "second operand of va_start must be a parameter name");
        acc = new_expr(&pos, Expr_VaStart, 2);
        acc->subs[0] = arg;
        acc->subs[1] = other;
    } else if (eat("va_arg")) {
        expect("(");
        struct expr *arg = p_expr(Prec_Comma);
        expect(",");
        struct type *type = p_typename();
        expect(")");
        acc = new_expr(&pos, Expr_VaArg, 1);
        acc->subs[0] = arg;
        acc->type = type;
    } else if (eat("va_end")) {
        expect("(");
        struct expr *arg = p_expr(Prec_Comma);
        expect(")");
        acc = new_expr(&pos, Expr_VaEnd, 1);
        acc->subs[0] = arg;
    } else if (tok == Tok_Wrd) {
        acc = new_expr(&pos, Expr_Ident, 0);
        acc->name = p_ident();
        struct sym *sym = lookup(0, acc->name);
        if (!sym)
            error_at(&pos, "undefined symbol");
    } else {
        unexpected_expected("expression");
    }

    // led
    while (1) {
        if (rbp < Prec_Comma && eat(",")) {
            acc = p_bin_expr(acc, Expr_Comma, 0);
        } else if (rbp < Prec_Assign && eat("=")) {
            acc = p_bin_expr(acc, Expr_Assign, Prec_Assign - 1);
        } else if (rbp < Prec_Cond && eat("?")) {
            struct expr *mid = p_expr(0);
            expect(":");
            struct expr *rhs = p_expr(Prec_Cond - 1);
            struct expr *tmp = new_expr(&pos, Expr_Cond, 3);
            tmp->subs[0] = acc;
            tmp->subs[1] = mid;
            tmp->subs[2] = rhs;
            acc = tmp;
        } else if (rbp < Prec_Or && eat("||")) {
            acc = p_bin_expr(acc, Expr_Or, Prec_Or);
        } else if (rbp < Prec_And && eat("&&")) {
            acc = p_bin_expr(acc, Expr_And, Prec_And);
        } else if (rbp < Prec_BitOr && eat("|")) {
            acc = p_bin_expr(acc, Expr_BitOr, Prec_BitOr);
        } else if (rbp < Prec_BitXor && eat("^")) {
            acc = p_bin_expr(acc, Expr_BitXor, Prec_BitXor);
        } else if (rbp < Prec_BitAnd && eat("&")) {
            acc = p_bin_expr(acc, Expr_BitAnd, Prec_BitAnd);
        } else if (rbp < Prec_Eq && eat("==")) {
            acc = p_bin_expr(acc, Expr_Eq, Prec_Eq);
        } else if (rbp < Prec_Eq && eat("!=")) {
            acc = p_bin_expr(acc, Expr_Ne, Prec_Eq);
        } else if (rbp < Prec_Rel && eat("<")) {
            acc = p_bin_expr(acc, Expr_Lt, Prec_Rel);
        } else if (rbp < Prec_Rel && eat(">")) {
            acc = p_bin_expr(acc, Expr_Gt, Prec_Rel);
        } else if (rbp < Prec_Rel && eat("<=")) {
            acc = p_bin_expr(acc, Expr_Le, Prec_Rel);
        } else if (rbp < Prec_Rel && eat(">=")) {
            acc = p_bin_expr(acc, Expr_Ge, Prec_Rel);
        } else if (rbp < Prec_Shift && eat("<<")) {
            acc = p_bin_expr(acc, Expr_Shl, Prec_Shift);
        } else if (rbp < Prec_Shift && eat(">>")) {
            acc = p_bin_expr(acc, Expr_Shr, Prec_Shift);
        } else if (rbp < Prec_Add && eat("+")) {
            acc = p_bin_expr(acc, Expr_Add, Prec_Add);
        } else if (rbp < Prec_Add && eat("-")) {
            acc = p_bin_expr(acc, Expr_Sub, Prec_Add);
        } else if (rbp < Prec_Mul && eat("*")) {
            acc = p_bin_expr(acc, Expr_Mul, Prec_Mul);
        } else if (rbp < Prec_Mul && eat("/")) {
            acc = p_bin_expr(acc, Expr_Div, Prec_Mul);
        } else if (rbp < Prec_Mul && eat("%")) {
            acc = p_bin_expr(acc, Expr_Mod, Prec_Mul);
        } else if (rbp < Prec_Postfix && eat("[")) {
            acc = p_bin_expr(acc, Expr_Add, 0);
            acc = new_unary_expr(&pos, Expr_Deref, acc);
            expect("]");
        } else if (rbp < Prec_Postfix && eat("(")) {
            struct expr *tmp = new_expr(&pos, Expr_Call, MAX_FUNC_PARAMS + 1);
            tmp->subs[0] = acc;
            int n_args = 0;
            struct expr **args = &tmp->subs[1];
            while (!eat(")")) {
                if (n_args >= MAX_FUNC_PARAMS)
                    error_at(&pos, "too many arguments in function call");
                if (n_args > 0) {
                    expect(",");
                }
                args[n_args++] = p_expr(Prec_Comma);
            }
            tmp->n_subs = n_args + 1;
            acc = tmp;
        } else if (rbp < Prec_Postfix && eat(".")) {
            acc = new_unary_expr(&pos, Expr_Member, acc);
            acc->name = p_ident();
        } else if (rbp < Prec_Postfix && eat("->")) {
            acc = new_unary_expr(&pos, Expr_Deref, acc);
            acc = new_unary_expr(&pos, Expr_Member, acc);
            acc->name = p_ident();
        } else if (rbp < Prec_Postfix && eat("++")) {
            acc = new_unary_expr(&pos, Expr_PostInc, acc);
        } else if (rbp < Prec_Postfix && eat("--")) {
            acc = new_unary_expr(&pos, Expr_PostDec, acc);
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
    if (eat("{")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_Block);
        if (eat("}"))
            return stmt;
        push_scope();
        stmt->sub = p_stmt();
        struct stmt *tail = stmt->sub;
        while (!eat("}")) {
            tail->next = p_stmt(), tail = tail->next;
        }
        pop_scope();
        return stmt;
    } else if (eat("return")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_Return);
        if (!at(";")) {
            stmt->expr = p_expr(0);
        }
        expect(";");

        if (is_void_type(curr_func->type->ret_type)) {
            if (stmt->expr)
                error_at(&pos, "returning a value from a void function");
        } else {
            if (!stmt->expr)
                error_at(&pos, "missing return value");
            stmt->expr = elab_expr_expect(stmt->expr, curr_func->type->ret_type);
        }
        return stmt;
    } else if (eat("if")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_If);

        expect("(");
        stmt->expr = p_expr(0);
        stmt->expr = elab_cond_expr(stmt->expr);
        expect(")");

        push_scope();
        stmt->sub = p_stmt();
        pop_scope();

        if (eat("else")) {
            push_scope();
            stmt->sub->next = p_stmt();
            pop_scope();
        }

        return stmt;
    } else if (at("for") || at("while")) {
        int is_while = at("while");
        lex();
        struct stmt *stmt = new_stmt(&pos, Stmt_Loop);

        push_scope();
        expect("(");
        if (is_while) {
            stmt->expr = elab_cond_expr(p_expr(0));
            stmt->sub = new_stmt(&pos, Stmt_Empty);
            stmt->sub->next = new_stmt(&pos, Stmt_Empty);
        } else {
            if (eat(";")) {
                stmt->sub = new_stmt(&pos, Stmt_Empty);
            } else if (at_decl()) {
                stmt->sub = p_stmt();
            } else {
                stmt->sub = new_stmt(&pos, Stmt_Expr);
                stmt->sub->expr = elab_rvalue_expr(p_expr(0));
                expect(";");
            }

            if (!at(";")) {
                stmt->expr = elab_cond_expr(p_expr(0));
            }
            expect(";");

            if (!at(")")) {
                stmt->sub->next = new_stmt(&pos, Stmt_Expr);
                stmt->sub->next->expr = elab_rvalue_expr(p_expr(0));
            } else {
                stmt->sub->next = new_stmt(&pos, Stmt_Empty);
            }
        }
        expect(")");

        struct stmt *outer_loop = curr_loop;
        curr_loop = stmt;
        push_scope();
        struct stmt *body = p_stmt();
        pop_scope();
        pop_scope();
        curr_loop = outer_loop;

        stmt->sub->next->next = body;
        return stmt;
    } else if (at("break") || at("continue")) {
        if (!curr_loop)
            error_at(&pos, "break/continue statement outside loop");
        int kind = at("break") ? Stmt_Break : Stmt_Continue;
        lex();
        struct stmt *stmt = new_stmt(&pos, kind);
        stmt->sub = curr_loop;
        expect(";");
        return stmt;
    } else if (eat(";")) {
        return new_stmt(&pos, Stmt_Empty);
    } else if (at_decl()) {
        struct stmt *stmt = 0;
        p_decl(Decl_Local, &stmt);
        return stmt ? stmt : new_stmt(&pos, Stmt_Empty);
    } else {
        struct stmt *stmt = new_stmt(&pos, Stmt_Expr);
        stmt->expr = elab_rvalue_expr(p_expr(0));
        expect(";");
        return stmt;
    }
}

static struct type *p_struct() {
    struct pos name_pos = tok_pos;
    const char *name = 0;
    if (tok == Tok_Wrd) {
        name = p_ident();
    }
    int is_def = at("{");

    struct sym *sym = 0;
    if (!is_def) {
        sym = lookup(1, name);
    }

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
        error_at(&name_pos, "declaration of anonymous struct must be a definition");

    return sym->type;
}

static struct type *p_enum() {
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
        declare(&name_pos, 0, Sym_Const, 0, name, int_type, 1)->val = val;
    }
    return int_type;
}

extern void p_decl(int scope, void *ctx) {
    struct pos pos = tok_pos;
    int storage_class = 0;
    struct type *base_type = 0;
    while (1) {
        if (eat("const")) {
            // ignored
        } else if (!storage_class && eat("static")) {
            storage_class = Static;
        } else if (!storage_class && eat("extern")) {
            storage_class = Extern;
        } else if (!base_type && eat("void")) {
            base_type = void_type;
        } else if (!base_type && eat("int")) {
            base_type = int_type;
        } else if (!base_type && eat("char")) {
            base_type = char_type;
        } else if (!base_type && eat("va_list")) {
            base_type = va_list_type;
        } else if (!base_type && eat("struct")) {
            base_type = p_struct();
        } else if (!base_type && eat("enum")) {
            base_type = p_enum();
        } else {
            break;
        }
    }
    if (!base_type)
        unexpected_expected("type specifier");
    if (storage_class && scope != Decl_Global)
        error_at(&pos, "storage class specifier is not allowed here");

    for (int n_declarators = 0;; n_declarators++) {
        struct type *type = base_type;
        const char *name = 0;
        int has_params = 0;
        struct sym *params[MAX_FUNC_PARAMS];
        int n_params = 0;
        struct expr *init = 0;

        while (eat("*")) {
            type = new_ptr_type(type);
        }

        struct pos name_pos = tok_pos;
        if (scope != Decl_TypeName && tok == Tok_Wrd) {
            name = p_ident();
        }

        // simplified grammar: either a function, array or object definition
        if (eat("(")) {
            struct type *param_types[MAX_FUNC_PARAMS];
            has_params = 1;
            int is_va = 0;
            push_scope();
            for (n_params = 0; !at(")"); n_params++) {
                if (n_params >= MAX_FUNC_PARAMS)
                    error_at(&name_pos, "too many parameters in function declaration");
                if (n_params > 0) {
                    expect(",");
                    if (eat("...")) {
                        is_va = 1;
                        break;
                    }
                }
                p_decl(Decl_Param, &params[n_params]);
                param_types[n_params] = params[n_params]->type;
            }
            expect(")");
            if (!is_scalar(type) && !is_void_type(type))
                error_at(&name_pos, "bad function return type");
            type = new_func_type(type, param_types, n_params, is_va);
            pop_scope();
        } else if (at("[")) {
            while (eat("[")) {
                int len = p_const_expr();
                expect("]");
                type = new_array_type(type, len);
            }
        } else if (eat("=") && (scope == Decl_Global || scope == Decl_Local)) {
            init = p_expr(Prec_Comma);
        }

        if (scope == Decl_TypeName) {
            *(struct type **)ctx = type;
            return;  // max one type per abstract declaration
        } else if (scope == Decl_Param) {
            if (is_array_type(type)) {
                type = new_ptr_type(type->ptr_to);
            } else if (is_func_type(type)) {
                type = new_ptr_type(type);
            }

            if (!is_scalar(type))
                error_at(&name_pos, "bad parameter type");
            *(struct sym **)ctx = declare(&name_pos, 0, Sym_Local, 0, name, type, 1);
            return;  // max one parameter per declaration
        } else if (!name) {
            // declaration does not declare a function or object
        } else if (has_params) {
            // function declaration
            if (scope != Decl_Global && scope != Decl_Local)
                error_at(&name_pos, "function declaration is not allowed here");
            int has_func_body = scope == Decl_Global && n_declarators == 0 && at("{");
            struct sym *sym = declare(&name_pos, 0, Sym_Func, storage_class, name, type, has_func_body);
            if (has_func_body) {
                sym->scope = push_scope();
                for (int i = 0; i < n_params; i++) {
                    struct sym *param =
                        declare(&params[i]->last_pos, sym, Sym_Local, 0, params[i]->name, params[i]->type, 1);
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
            if (!is_object_type(type))
                error_at(&name_pos, "bad object type");
            if (scope == Decl_Local) {
                if (storage_class)
                    error_at(&name_pos, "storage class specifier is not allowed/supported");
                if (init) {
                    init = elab_expr_expect(init, type);
                }
                struct stmt *decl = new_stmt(&pos, Stmt_Decl);
                decl->sym = declare(&name_pos, curr_func, Sym_Local, 0, name, type, 1);
                decl->expr = init;
                decl->sub = *(struct stmt **)ctx;
                *(struct stmt **)ctx = decl;
            } else if (scope == Decl_Global) {
                struct sym *sym = declare(&name_pos, 0, Sym_Global, storage_class, name, type, !!init);
                if (init) {
                    sym->val = elab_init(init, type);
                }
            } else if (scope == Decl_Struct) {
                struct sym *sym = (struct sym *)ctx;
                declare(&name_pos, sym, Sym_Field, 0, name, type, 1);
            } else {
                unreachable_case("p_decl (object declaration)", scope);
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

int next_loop_id, next_cond_id, next_str_id;

static const char *get_str_op(struct type *type) {
    if (type->kind == Type_Char)
        return "strb w";
    else if (type->kind == Type_Int)
        return "str w";
    else if (type->kind == Type_Ptr)
        return "str x";
    else
        unreachable_case("get_str_op", type->kind);
}

static const char *get_ldr_op(struct type *type) {
    if (type->kind == Type_Char)
        return "ldrsb x";
    else if (type->kind == Type_Int)
        return "ldrsw x";
    else if (type->kind == Type_Ptr)
        return "ldr x";
    else
        unreachable_case("get_ldr_op", type->kind);
}

static void emit_scalar_data(int size, int val) {
    if (size == 1) {
        writef(1, ".byte %d\n", val);
    } else if (size == 4) {
        writef(1, ".long %d\n", val);
    } else if (size == 8) {
        writef(1, ".quad %d\n", val);
    } else {
        unreachable_case("emit_data_scalar", size);
    }
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
    int default_chunk_value = val < 0 ? chunk_mask : 0;
    for (int i = 0; i < 4; i++, val = val >> 16) {
        int chunk = val & chunk_mask;
        if (i == 0) {
            const char *mov_op = val < 0 ? "movn" : "movz";
            chunk = val < 0 ? ~chunk & chunk_mask : chunk;
            writef(1, "%s x%d, #%d // %d\n", mov_op, reg, chunk, val);
        } else if (chunk != default_chunk_value) {
            int shift = i * 16;
            writef(1, "movk x%d, #%d, lsl #%d\n", reg, chunk, shift);
        }
    }
}

static void emit_obj(struct sym *sym) {
    int size = type_size(sym->type), align = type_align(sym->type);
    if (is_scalar(sym->type)) {
        write_str(1, ".section .data\n");
        if (sym->storage_class == Extern) {
            writef(1, ".globl %s\n", sym->name);
        }
        writef(1, ".balign %d\n", align);
        writef(1, "%s:\n", sym->name);
        if (sym->is_defined) {
            emit_scalar_data(size, sym->val);
        } else {
            emit_scalar_data(size, 0);
        }
    } else {
        if (sym->storage_class == Extern) {
            writef(1, ".globl %s\n", sym->name);
        }
        write_str(1, ".section .bss\n");
        writef(1, ".balign %d\n", align);
        writef(1, "%s:\n", sym->name);
        writef(1, ".space %d\n", size);
    }
}

static void emit_frame_offset(int offset, int reg) {
    emit_int_load(-offset, reg);
    writef(1, "sub x%d, x29, x%d\n", reg, reg);
}

static void emit_slot_write(struct sym *local, int reg) {
    const char *op = get_str_op(local->type);
    emit_int_load(local->offset, 9);
    writef(1, "%s%d, [x29, x9]\n", op, reg);
}

static void emit_slot_addr(struct sym *local, int reg) {
    emit_int_load(-local->offset, 9);
    writef(1, "sub x%d, x29, x9\n", reg);
}

static void emit_push(int reg) {
    writef(1, "str x%d, [sp, #-16]!\n", reg);
}

static void emit_pop(int reg) {
    writef(1, "ldr x%d, [sp], #16\n", reg);
}

static void emit_load(struct type *type, int dst, int src) {
    const char *op = get_ldr_op(type);
    writef(1, "%s%d, [x%d]\n", op, dst, src);
}

static void emit_store(struct type *type, int src, int dst) {
    const char *op = get_str_op(type);
    writef(1, "%s%d, [x%d]\n", op, src, dst);
}

static void emit_sext(struct type *type, int reg) {
    if (type->kind == Type_Char) {
        writef(1, "sxtb x%d, w%d\n", reg, reg);
    } else if (type->kind == Type_Int) {
        writef(1, "sxtw x%d, w%d\n", reg, reg);
    }
}

static void emit_scalar_expr(struct expr *expr);
static void emit_place_expr(struct expr *expr);

static void emit_binary_operands(struct expr *expr) {
    emit_scalar_expr(expr->subs[0]);
    emit_push(0);
    emit_scalar_expr(expr->subs[1]);
    write_str(1, "mov x1, x0\n");
    emit_pop(0);
}

static void emit_short_circuit_expr(struct expr *expr, const char *cond) {
    int cond_id = next_cond_id++;
    emit_scalar_expr(expr->subs[0]);
    writef(1, "cb%s x0, .L.cond.%d.short\n", cond, cond_id);
    emit_scalar_expr(expr->subs[1]);
    writef(1, ".L.cond.%d.short:\n", cond_id);
    write_str(1, "cmp x0, #0\n");
    write_str(1, "cset x0, ne\n");
    writef(1, ".L.cond.%d.end:\n", cond_id);
}

static void emit_cmp_expr(struct expr *expr, const char *cond, const char *ucond) {
    emit_binary_operands(expr);
    write_str(1, "cmp x0, x1\n");
    if (is_ptr_type(expr->subs[0]->type)) {
        writef(1, "cset x0, %s\n", ucond);
    } else {
        writef(1, "cset x0, %s\n", cond);
    }
}

static void emit_arithmetic_expr(struct expr *expr, const char *op) {
    emit_binary_operands(expr);
    writef(1, "%s x0, x0, x1\n", op);
}

static void emit_assign_to_addr(struct type *dst_type, struct expr *rhs) {
    assert("emit_assign_to_addr", is_object_type(dst_type));
    if (is_scalar(dst_type)) {
        emit_push(0);
        emit_scalar_expr(rhs);
        write_str(1, "mov x1, x0\n");
        emit_pop(0);
        emit_store(dst_type, 1, 0);
        write_str(1, "mov x0, x1\n");
    } else {
        emit_push(0);
        if (rhs->kind == Expr_Assign) {
            emit_place_expr(rhs->subs[0]);
            emit_assign_to_addr(rhs->type, rhs->subs[1]);
        } else {
            if (!is_addressable(rhs))
                unreachable_case("emit_assign_to_addr", rhs->kind);
            emit_place_expr(rhs);
        }
        write_str(1, "mov x1, x0\n");
        emit_pop(0);
        emit_int_load(type_size(dst_type), 2);
        write_str(1, "bl _memcpy\n");
    }
}

static void emit_effect_expr(struct expr *expr) {
    if (expr->kind == Expr_Assign) {
        emit_place_expr(expr->subs[0]);
        emit_assign_to_addr(expr->type, expr->subs[1]);
    } else if (expr->kind == Expr_Comma) {
        emit_effect_expr(expr->subs[0]);
        emit_effect_expr(expr->subs[1]);
    } else if (expr->kind == Expr_VaStart) {
        emit_place_expr(expr->subs[0]);
        write_str(1, "add x1, x29, #16  // top of frame\n");
        write_str(1, "str x1, [x0]  // stack\n");
        emit_frame_offset(curr_func->va_area_offset + curr_func->va_area_size, 1);
        write_str(1, "str x1, [x0, #8]  // gr_top\n");
        write_str(1, "str xzr, [x0, #16]  // vr_top\n");
        int gr_offs = -curr_func->va_area_size;
        writef(1, "mov x1, #%d\n", gr_offs);
        write_str(1, "str w1, [x0, #24]  // gr_offs\n");
        write_str(1, "str wzr, [x0, #28]  // vr_offs\n");
    } else if (expr->kind == Expr_VaEnd) {
        // no-op
    } else if (is_scalar(expr->type) || is_void_type(expr->type)) {
        emit_scalar_expr(expr);
    } else if (is_lvalue(expr)) {
        emit_place_expr(expr);
    } else {
        unreachable_case("emit_effect_expr", expr->kind);
    }
}

static void emit_place_expr(struct expr *expr) {
    int k = expr->kind;
    if (k == Expr_Ident) {
        struct sym *sym = expr->sym;
        if (sym->kind == Sym_Global || sym->kind == Sym_Func) {
            if (sym->storage_class == Extern && !sym->is_defined) {
                writef(1, "adrp x0, :got:%s\n", sym->name);
                writef(1, "ldr x0, [x0, :got_lo12:%s]\n", sym->name);
            } else {
                writef(1, "adrp x0, %s\n", sym->name);
                writef(1, "add x0, x0, :lo12:%s\n", sym->name);
            }
        } else if (sym->kind == Sym_Local) {
            emit_slot_addr(sym, 0);
        } else {
            unreachable_case("emit_place_expr (ident)", sym->kind);
        }
    } else if (k == Expr_Member) {
        emit_place_expr(expr->subs[0]);
        emit_int_load(expr->sym->offset, 1);
        write_str(1, "add x0, x0, x1\n");
    } else if (k == Expr_Deref) {
        emit_scalar_expr(expr->subs[0]);
    } else if (k == Expr_Str) {
        emit_str_load(expr->str_val);
    } else {
        unreachable_case("emit_place_expr", k);
    }
}

static void emit_scalar_expr(struct expr *expr) {
    int k = expr->kind;
    if (is_lvalue(expr)) {
        emit_place_expr(expr);
        emit_load(expr->type, 0, 0);
    } else if (k == Expr_Num) {
        emit_int_load(expr->int_val, 0);
    } else if (k == Expr_Str) {
        emit_str_load(expr->str_val);
    } else if (k == Expr_PostDec || k == Expr_PostInc) {
        const char *op = k == Expr_PostInc ? "add" : "sub";
        int step = is_ptr_type(expr->type) ? type_size(expr->type->ptr_to) : 1;
        emit_place_expr(expr->subs[0]);
        write_str(1, "mov x2, x0\n");
        emit_load(expr->type, 0, 0);
        emit_int_load(step, 1);
        writef(1, "%s x1, x0, x1\n", op);
        emit_store(expr->type, 1, 2);
    } else if (k == Expr_Deref) {
        emit_scalar_expr(expr->subs[0]);
        emit_load(expr->type, 0, 0);
    } else if (k == Expr_Addr) {
        emit_place_expr(expr->subs[0]);
    } else if (k == Expr_BitNot) {
        emit_scalar_expr(expr->subs[0]);
        write_str(1, "mvn x0, x0\n");
    } else if (k == Expr_Not) {
        emit_scalar_expr(expr->subs[0]);
        write_str(1, "cmp x0, #0\n");
        write_str(1, "cset x0, eq\n");
    } else if (k == Expr_Neg) {
        emit_scalar_expr(expr->subs[0]);
        write_str(1, "neg x0, x0\n");
    } else if (k == Expr_Cast) {
        // values in registers are always full width, so
        // narrowing casts can simply truncate the value.
        emit_scalar_expr(expr->subs[0]);
        if (type_size(expr->type) < 8) {
            emit_sext(expr->type, 0);
        }
    } else if (k == Expr_Mod) {
        emit_binary_operands(expr);
        write_str(1, "sdiv x3, x0, x1\n");
        write_str(1, "msub x0, x3, x1, x0\n");
    } else if (k == Expr_Div) {
        emit_arithmetic_expr(expr, "sdiv");
    } else if (k == Expr_Mul) {
        emit_arithmetic_expr(expr, "mul");
    } else if (k == Expr_Sub) {
        emit_arithmetic_expr(expr, "sub");
    } else if (k == Expr_Add) {
        emit_arithmetic_expr(expr, "add");
    } else if (k == Expr_Shr) {
        emit_arithmetic_expr(expr, "asr");
    } else if (k == Expr_Shl) {
        emit_arithmetic_expr(expr, "lsl");
    } else if (k == Expr_Ge) {
        emit_cmp_expr(expr, "ge", "hs");
    } else if (k == Expr_Gt) {
        emit_cmp_expr(expr, "gt", "hi");
    } else if (k == Expr_Le) {
        emit_cmp_expr(expr, "le", "ls");
    } else if (k == Expr_Lt) {
        emit_cmp_expr(expr, "lt", "lo");
    } else if (k == Expr_Ne) {
        emit_cmp_expr(expr, "ne", "ne");
    } else if (k == Expr_Eq) {
        emit_cmp_expr(expr, "eq", "eq");
    } else if (k == Expr_BitAnd) {
        emit_arithmetic_expr(expr, "and");
    } else if (k == Expr_BitXor) {
        emit_arithmetic_expr(expr, "eor");
    } else if (k == Expr_BitOr) {
        emit_arithmetic_expr(expr, "orr");
    } else if (k == Expr_And) {
        emit_short_circuit_expr(expr, "z");
    } else if (k == Expr_Or) {
        emit_short_circuit_expr(expr, "nz");
    } else if (k == Expr_PtrAdd || k == Expr_PtrSub) {
        emit_binary_operands(expr);
        int size = type_size(expr->subs[0]->type->ptr_to);
        const char *op = k == Expr_PtrAdd ? "add" : "sub";
        if (size != 1) {
            emit_int_load(size, 2);
            write_str(1, "mul x1, x1, x2\n");
        }
        writef(1, "%s x0, x0, x1\n", op);
    } else if (k == Expr_PtrDiff) {
        emit_binary_operands(expr);
        int size = type_size(expr->subs[0]->type->ptr_to);
        write_str(1, "sub x0, x0, x1\n");
        if (size != 1) {
            emit_int_load(size, 1);
            write_str(1, "sdiv x0, x0, x1\n");
        }
    } else if (k == Expr_Cond) {
        int cond_id = next_cond_id++;
        emit_scalar_expr(expr->subs[0]);
        writef(1, "cbz x0, .L.cond.%d.else\n", cond_id);
        emit_scalar_expr(expr->subs[1]);
        writef(1, "b .L.cond.%d.end\n", cond_id);
        writef(1, ".L.cond.%d.else:\n", cond_id);
        emit_scalar_expr(expr->subs[2]);
        writef(1, ".L.cond.%d.end:\n", cond_id);
    } else if (k == Expr_Assign) {
        emit_place_expr(expr->subs[0]);
        emit_assign_to_addr(expr->type, expr->subs[1]);
    } else if (k == Expr_Call) {
        struct expr *fn = expr->subs[0], **args = &expr->subs[1];
        int argc = expr->n_subs - 1;

        assert("emit_scalar_expr: func_sym",
               fn->kind == Expr_Addr && fn->subs[0]->kind == Expr_Ident && fn->subs[0]->sym->kind == Sym_Func);
        struct sym *func_sym = fn->subs[0]->sym;

        for (int i = 0; i < argc; i++) {
            emit_scalar_expr(args[i]);
            emit_push(0);
        }
        for (int i = argc; i-- > 0;) {
            emit_pop(i);
        }
        writef(1, "bl %s\n", func_sym->name);
        // if calling a foreign function, we can't be sure
        // if it will sign-extend or zero the return value.
        if (is_scalar(expr->type) && type_size(expr->type) < 8) {
            emit_sext(expr->type, 0);
        }
    } else if (k == Expr_Comma) {
        emit_effect_expr(expr->subs[0]);
        emit_scalar_expr(expr->subs[1]);
    } else if (k == Expr_VaArg) {
        emit_place_expr(expr->subs[0]);
        write_str(1, "bl _va_arg\n");
    } else {
        unreachable_case("emit_scalar_expr", k);
    }
}

static void emit_init_decls(struct stmt *stmt) {
    if (!stmt)
        return;
    emit_init_decls(stmt->sub);
    if (stmt->expr) {
        emit_slot_addr(stmt->sym, 0);
        emit_assign_to_addr(stmt->sym->type, stmt->expr);
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
            emit_scalar_expr(stmt->expr);
        }
        writef(1, "b .L.return.%s\n", curr_func->name);
    } else if (k == Stmt_If) {
        int cond_id = next_cond_id++;
        emit_scalar_expr(stmt->expr);
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
            emit_scalar_expr(stmt->expr);
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
        emit_effect_expr(stmt->expr);
    } else if (k != Stmt_Empty) {
        unreachable_case("emit_stmt", k);
    }
}

static void emit_func(struct sym *func) {
    curr_func = func;
    int n_va_args = func->type->is_va ? 8 - func->type->n_params : 0;
    func->va_area_offset = -align_up(curr_func->size + n_va_args * 8, 8);
    func->va_area_size = n_va_args * 8;
    curr_func->size = align_up(-func->va_area_offset, 16);

    write_str(1, ".text\n");
    if (func->storage_class != Static) {
        writef(1, ".globl %s\n", func->name);
    }
    writef(1, "%s:\n", func->name);
    // prologue
    write_str(1, "stp x29, x30, [sp, #-16]!\n");
    write_str(1, "mov x29, sp\n");
    writef(1, "sub sp, sp, #%d\n", curr_func->size);
    struct sym *sym = func->scope->head;
    for (int i = 0; i < func->type->n_params; i++, sym = sym->next) {
        emit_slot_write(sym, i);
    }
    for (int i = func->type->n_params; i < 8 && func->type->is_va; i++) {
        int offset = func->va_area_offset + (i - func->type->n_params) * 8;
        emit_int_load(offset, 9);
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

static void emit_runtime_helpers() {
    write_str(1, ".section .text\n");
    write_str(1, ".globl _memcpy\n");
    write_str(1, "_memcpy:\n"); // void *_memcpy(void *d, const void *s, int n)
    write_str(1, "mov x3, x0\n");
    write_str(1, "cbz x2, .L.memcpy.end\n");
    write_str(1, ".L.memcpy.body:\n");
    write_str(1, "ldrb w4, [x1], #1\n");
    write_str(1, "strb w4, [x3], #1\n");
    write_str(1, "subs x2, x2, #1\n");
    write_str(1, "cbnz x2, .L.memcpy.body\n");
    write_str(1, ".L.memcpy.end:\n");
    write_str(1, "ret\n");  // x0 still holds dest

    write_str(1, ".section .text\n");
    write_str(1, "_va_arg:\n"); // void *_va_arg(va_list *ap)
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
        const char *chars = str->chars;
        writef(1, ".L.str.%d:\n", str->label);
        for (int c; (c = *chars++);) {
            writef(1, ".byte %d\n", c);
        }
        write_str(1, ".byte 0\n");
    }
}

//=============================================================================
//= main

int main(int argc, char **argv) {
    if (argc != 2) {
        write_str(2, "usage: minicc <file>\n");
        return 1;
    }
    const char *file_name = argv[1];
    if (inp = open(file_name, 0, 0), inp < 0) {
        writef(2, "error: cannot open file '%s'\n", file_name);
        return 1;
    }

    init_types();

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
            if (sym->storage_class != Static && !sym->is_defined)
                continue;  // declaration, external linkage
            if (sym->storage_class == Static && !sym->is_defined)
                continue;  // declaration with internal linkage, missing definition
            emit_func(sym);
        } else if (sym->kind == Sym_Global) {
            if (sym->storage_class == Extern && !sym->is_defined)
                continue;  // declaration, external linkage
            if (!sym->is_defined)
                ;  // tentative definition
            emit_obj(sym);
        }
    }
    emit_runtime_helpers();
    emit_str_literals();

    return 0;
}
