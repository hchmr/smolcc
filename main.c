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

static void write_char(int fd, int c) {
    write(fd, &c, 1);
}

static int read_char(int fd) {
    char c;
    int n = read(fd, &c, 1);
    if (n <= 0)
        return EOF;
    return c;
}

static void write_str(int fd, const char *s) {
    while (*s) {
        write_char(fd, *s++);
    }
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

static void write_f_(int fd, const char *fmt, const char **args) {
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                write_int(fd, **(int **)(args++));
            } else if (*fmt == 'c') {
                write_char(fd, **(char **)(args++));
            } else if (*fmt == 's') {
                const char *s = *(char **)(args++);
                if (s) {
                    write_str(fd, s);
                } else {
                    write_str(fd, "(null)");
                }
            } else {
                write_char(fd, '%');
            }
        } else {
            write_char(fd, *fmt);
        }
        fmt++;
    }
}

static void write_f4(int fd, const char *fmt, const void *x1, const void *x2, const void *x3, const void *x4) {
    const char *args[4];
    args[0] = x1;
    args[1] = x2;
    args[2] = x3;
    args[3] = x4;
    write_f_(fd, fmt, args);
}

static void write_f3(int fd, const char *fmt, const void *x1, const void *x2, const void *x3) {
    write_f4(fd, fmt, x1, x2, x3, 0);
}

static void write_f2(int fd, const char *fmt, const void *x1, const void *x2) {
    write_f4(fd, fmt, x1, x2, 0, 0);
}

static void write_f1(int fd, const char *fmt, const void *x1) {
    write_f4(fd, fmt, x1, 0, 0, 0);
}

static void write_ln(int fd, const char *s) {
    write_str(fd, s);
    write_char(fd, '\n');
}

//=============================================================================
//= str

static int str_len(const char *s) {
    int len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

static int str_eq(const char *a, const char *b) {
    if (a == b)
        return 1;
    while (*a && *b)
        if (*a++ != *b++)
            return 0;
    return *a == *b;
}

//=============================================================================
//= misc

static int align_up(int n, int align) {
    return (n + align - 1) / align * align;
}

static const char *find_chr(const char *s, int c) {
    while (*s) {
        if (c == *s)
            return s;
        s++;
    }
    return 0;
}

static void ptr_swap(void **p1, void **p2) {
    void *tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;
}

//=============================================================================
//= assertions

static void die(const char *label, const char *msg) {
    write_f2(2, "%s: %s\n", label, msg);
    abort();
}

static void assert(const char *label, int condition) {
    if (!condition)
        die(label, "assertion failed");
}

static void unreachable_case(const char *label, int value) {
    write_f2(2, "%s: case not handled: %d\n", label, &value);
    abort();
}

//=============================================================================
//= arena

enum { Arena_Cap = 8 * 1024 * 1024 };  // 8 MiB
static char arena[Arena_Cap];
static int arena_len;

static void *alloc(int len) {
    arena_len = align_up(arena_len, 8);
    if (arena_len + len >= Arena_Cap)
        die("alloc", "out of memory");
    char *res = arena + arena_len;
    arena_len = arena_len + len;
    return res;
}

static void *mem_clone(void *s, int len) {
    void *res = alloc(len);
    int i = 0;
    while (i++ < len)
        ((char *)res)[i - 1] = ((char *)s)[i - 1];
    return res;
}

static char *strings[10240];
static int strings_len;

static const char *intern(const char *s, int len) {
    int i = 0;
    while (i < strings_len) {
        if (str_eq(strings[i], s))
            return strings[i];
        i++;
    }
    if (strings_len >= 10240)
        die("intern", "out of memory for strings");
    char *res = mem_clone((void *)s, len);
    strings[strings_len++] = res;
    return res;
}

//=============================================================================
//= diag

struct pos {
    const char *file;
    int line, col;
};

static void diag_at(struct pos *pos, const char *kind) {
    write_f4(2, "%s:%d:%d: %s: ", pos->file, &pos->line, &pos->col, kind);
}

static void error_at(struct pos *pos, const char *msg) {
    diag_at(pos, "error");
    write_ln(2, msg);
    _exit(1);
}

//=============================================================================
//= symbols

enum {
    MAX_SYMS = 1024,
    MAX_STRUCT_FIELDS = 32,
    MAX_FUNC_PARAMS = 8,
    MAX_LOCALS = 32,
};

enum {
    Sym_Global,
    Sym_Func,
    Sym_Struct,
    Sym_Const,
    Sym_Local,
};

enum {
    Ns_Struct = 1,
};

struct field {
    const char *name;
    struct type *type;
    int offset;
};

struct func_param {
    struct type *type;
    struct pos name_pos;
    const char *name;
};

enum {
    External = 1,
    Internal = 2,
};

static struct sym {
    int kind;
    int linkage;
    const char *name;
    struct type *type;
    int is_defined;

    struct pos last_pos;

    // case Sym_Const:
    int val;
    // case Sym_Func:
    struct func_param params[MAX_FUNC_PARAMS];
    int n_params;
    struct stmt *body;
    struct sym *locals[MAX_LOCALS];
    int n_locals;
    // case Sym_Struct:
    struct field fields[MAX_STRUCT_FIELDS];
    int n_fields;
    // case Sym_Local:
    int slot_idx;
    int offset;

    struct sym *next;
} syms[MAX_SYMS];
static int n_syms;

static int sym_ns(struct sym *sym) {
    if (sym->kind == Sym_Struct)
        return Ns_Struct;
    return 0;
}

static struct sym *alloc_sym() {
    if (n_syms >= MAX_SYMS) {
        write_f1(2, "error: too many symbols\n", 0);
        _exit(1);
    }
    return &syms[n_syms++];
}

static struct field *lookup_field(struct sym *sym, const char *name) {
    int i = 0;
    while (i < sym->n_fields) {
        struct field *field = &sym->fields[i];
        if (field->name == name)
            return field;
        i++;
    }
    return 0;
}

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
};

enum {
    MAX_TYPES = 512,
};

static struct type {
    int kind;
    // for pointers
    struct type *ptr_to;
    // for arrays
    int array_len;
    // for functions
    struct type *ret_type;
    struct type *param_types[MAX_FUNC_PARAMS];
    int n_params;
    // for structs
    struct sym *sym;
} types[MAX_TYPES];
static int n_types;

static struct type *void_type;
static struct type *char_type;
static struct type *int_type;

static void type_layout(struct type *ty, int *size, int *align) {
    if (ty->kind == Type_Void) {
        *size = 0;
        *align = 0;
    } else if (ty->kind == Type_Char) {
        *size = 1;
        *align = 1;
    } else if (ty->kind == Type_Int) {
        *size = 4;
        *align = 4;
    } else if (ty->kind == Type_Ptr) {
        *size = 8;
        *align = 8;
    } else if (ty->kind == Type_Array) {
        int elem_size, elem_align;
        type_layout(ty->ptr_to, &elem_size, &elem_align);
        *size = align_up(elem_size, elem_align) * ty->array_len;
        *align = elem_align;
    } else if (ty->kind == Type_Struct) {
        struct sym *sym = ty->sym;
        *size = 0;
        int max_align = 0;
        int i = 0;
        while (i < sym->n_fields) {
            struct field *field = &sym->fields[i];
            int field_size, field_align;
            type_layout(field->type, &field_size, &field_align);
            *size = align_up(field->offset, field_align) + field_size;
            if (field_align > max_align) {
                max_align = field_align;
            }
            i++;
        }
        if (max_align > 0)
            *size = align_up(*size, max_align);
        *align = max_align;
    } else if (ty->kind == Type_Func) {
        *size = 0;
        *align = 0;
    } else {
        unreachable_case("type_layout", ty->kind);
    }
}

static int type_size(struct type *ty) {
    int size, align;
    type_layout(ty, &size, &align);
    return size;
}

static int type_align(struct type *ty) {
    int size, align;
    type_layout(ty, &size, &align);
    return align;
}

// type predicates

static int type_eq(struct type *a, struct type *b) {
    if (a == b)
        return 1;
    if (a->kind != b->kind)
        return 0;
    if (a->kind == Type_Ptr)
        return a->ptr_to == b->ptr_to;
    if (a->kind == Type_Array)
        return a->array_len == b->array_len && a->ptr_to == b->ptr_to;
    if (a->kind == Type_Func) {
        if (a->ret_type != b->ret_type)
            return 0;
        if (a->n_params != b->n_params)
            return 0;
        int i = 0;
        while (i < a->n_params) {
            if (a->param_types[i] != b->param_types[i])
                return 0;
            i++;
        }
        return 1;
    }
    if (a->kind == Type_Struct)
        return a->sym == b->sym;
    return 1;
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
    int i = 0;
    while (i < n_types) {
        if (type_eq(&types[i], ty))
            return &types[i];
        i++;
    }
    if (n_types >= MAX_TYPES) {
        write_f1(2, "error: too many types\n", 0);
        _exit(1);
    }
    types[n_types] = *ty;
    return &types[n_types++];
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

static struct type *new_func_type(struct type *ret_type, struct type **param_types, int n_params) {
    struct type ty;
    ty.kind = Type_Func;
    ty.ret_type = ret_type;
    int i = 0;
    while (i < n_params) {
        ty.param_types[i] = param_types[i];
        i++;
    }
    ty.n_params = n_params;
    return intern_type(&ty);
}

static struct type *int_promotion_type(struct type *t1, struct type *t2) {
    assert("int_promotion_type", is_integer_type(t1) && is_integer_type(t2));
    if (t1->kind < t2->kind)
        return t2;
    else
        return t1;
}

static struct type *uac_type(struct type *t1, struct type *t2) {
    assert("uac_type", is_integer_type(t1) && is_integer_type(t2));
    return int_promotion_type(t1, t2);
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

static void type_init() {
    void_type = &types[n_types++];
    void_type->kind = Type_Void;
    char_type = &types[n_types++];
    char_type->kind = Type_Char;
    int_type = &types[n_types++];
    int_type->kind = Type_Int;
}

//=============================================================================
//= symbol table

enum {
    MAX_SCOPES = 256,
};

static struct scope {
    struct sym *syms;
} scopes[MAX_SCOPES];
static struct scope *curr_scope;

static void enter_scope(struct pos *pos) {
    int depth = curr_scope - scopes;
    if (depth + 1 >= MAX_SCOPES)
        error_at(pos, "too many nested scopes");
    curr_scope++;
    curr_scope->syms = 0;
}

static void reenter_scope() {
    int depth = curr_scope - scopes;
    assert("reenter_scope", depth + 1 < MAX_SCOPES);
    curr_scope++;
}

static void leave_scope() {
    if (curr_scope <= scopes)
        die("leave_scope", "scope underflow");
    curr_scope--;
}

static struct sym *add_sym(struct pos *pos, int kind, const char *name) {
    struct sym *sym = alloc_sym();
    sym->kind = kind;
    sym->name = name;
    sym->last_pos = *pos;
    sym->next = curr_scope->syms;
    curr_scope->syms = sym;
    return sym;
}

static struct sym *lookup_in(struct scope *scope, int ns, const char *name) {
    if (!name)
        return 0;
    struct sym *sym = scope->syms;
    while (sym) {
        if (sym_ns(sym) == ns && sym->name == name)
            return sym;
        sym = sym->next;
    }
    return 0;
}

static struct sym *lookup(int ns, const char *name) {
    struct scope *s = curr_scope;
    while (s >= scopes) {
        struct sym *sym = lookup_in(s, ns, name);
        if (sym)
            return sym;
        s--;
    }
    return 0;
}

static void decl_conflict(struct pos *pos, struct sym *sym, const char *msg) {
    diag_at(pos, "error");
    write_f2(2, "'%s' %s.", sym->name, msg);
    write_f3(2, " Previous declaration at %s:%d:%d\n", sym->last_pos.file, &sym->last_pos.line, &sym->last_pos.col);
    _exit(1);
}

static struct sym *declare_struct(struct pos *pos, const char *name, int is_def) {
    struct sym *sym = lookup_in(curr_scope, Ns_Struct, name);
    if (sym) {
        if (sym->kind != Sym_Struct)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        if (is_def && sym->is_defined)
            decl_conflict(pos, sym, "already defined");
        sym->is_defined = sym->is_defined | is_def;
        return sym;
    }
    sym = add_sym(pos, Sym_Struct, name);
    sym->type = new_struct_type(sym);
    sym->is_defined = is_def;
    return sym;
}

static void declare_field(struct pos *pos, struct sym *sym, const char *name, struct type *type) {
    if (lookup_field(sym, name))
        error_at(pos, "duplicate field name in struct");
    if (sym->n_fields >= MAX_STRUCT_FIELDS)
        error_at(pos, "too many fields in struct");
    int offset = 0;
    if (sym->n_fields > 0) {
        struct field *prev = &sym->fields[sym->n_fields - 1];
        offset = prev->offset + type_size(prev->type);
    }
    struct field *field = &sym->fields[sym->n_fields++];
    field->name = name;
    field->type = type;
    field->offset = align_up(offset, type_align(type));
}

static struct sym *define_const(struct pos *pos, const char *name, int val) {
    struct sym *sym = lookup_in(curr_scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Const)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        decl_conflict(pos, sym, "already defined");
    }
    sym = add_sym(pos, Sym_Const, name);
    sym->type = int_type;
    sym->val = val;
    return sym;
}

static struct sym *define_local(struct pos *pos, struct sym *func, const char *name, struct type *type) {
    struct sym *sym = lookup_in(curr_scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Local)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        decl_conflict(pos, sym, "already defined");
    }
    if (func->n_locals >= MAX_LOCALS)
        error_at(pos, "local variable limit reached for function");
    sym = add_sym(pos, Sym_Local, name);
    sym->type = type;
    sym->is_defined = 1;
    sym->slot_idx = func->n_locals;
    func->locals[func->n_locals++] = sym;
    return sym;
}

static struct sym *declare_global(struct pos *pos, int linkage, const char *name, struct type *type, int is_def) {
    struct sym *sym = lookup_in(curr_scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Global)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        if (sym->type != type)
            decl_conflict(pos, sym, "already declared with a different type");
        if (sym->linkage == Internal && linkage == 0)
            decl_conflict(pos, sym, "already declared as static");
        if (sym->linkage != Internal && linkage == Internal)
            decl_conflict(pos, sym, "already declared as non-static");
        if (is_def && sym->is_defined)
            decl_conflict(pos, sym, "already defined");
        sym->is_defined = sym->is_defined | is_def;
        return sym;
    }
    sym = add_sym(pos, Sym_Global, name);
    sym->type = type;
    sym->is_defined = is_def;
    sym->linkage = linkage;
    return sym;
}

static struct type *prototype_to_func_type(struct type *ret_type, struct func_param *params, int n_params) {
    struct type *param_types[MAX_FUNC_PARAMS];
    int i = 0;
    while (i < n_params) {
        param_types[i] = params[i].type;
        i++;
    }
    return new_func_type(ret_type, param_types, n_params);
}

static void set_func_params(struct sym *sym, struct func_param *params, int n_params, int is_def) {
    sym->n_params = n_params;
    int i = 0;
    while (i < n_params) {
        sym->params[i].type = params[i].type;
        if (is_def) {
            sym->params[i].name = params[i].name;
        }
        i++;
    }
}

static struct sym *declare_func(struct pos *pos, int linkage, const char *name, struct type *ret_type,
                                struct func_param *params, int n_params, int is_def) {
    if (!is_scalar(ret_type) && !is_void_type(ret_type))
        error_at(pos, "bad function return type");
    struct type *type = prototype_to_func_type(ret_type, params, n_params);
    struct sym *sym = lookup_in(curr_scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Func)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        if (sym->type != type)
            decl_conflict(pos, sym, "already declared with a different signature");
        if (sym->linkage != Internal && linkage == Internal)
            decl_conflict(pos, sym, "already declared as non-static");
        if (is_def && sym->is_defined)
            decl_conflict(pos, sym, "already defined");
        set_func_params(sym, params, n_params, is_def);
        sym->is_defined = sym->is_defined | is_def;
        return sym;
    }
    sym = add_sym(pos, Sym_Func, name);
    sym->type = type;
    set_func_params(sym, params, n_params, is_def);
    sym->is_defined = is_def;
    sym->linkage = linkage;
    return sym;
}

//=============================================================================
//= ast

enum {
    Expr_Assign = 1,
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
};

struct expr {
    int kind;
    struct pos pos;
    struct type *type;

    int int_val;
    const char *str_val;
    struct expr **subs;
    int n_subs;

    struct sym *sym;  // resolved symbol
    struct field *field;  // resolved struct field
};

enum {
    Stmt_Block,
    Stmt_Decl,
    Stmt_If,
    Stmt_While,
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
    int k = expr->kind;
    return is_var_expr(expr) || k == Expr_Deref || k == Expr_Member;
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
            int value = eval(expr->subs[0]);
            return const_cast(value, expr->type);
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

static int can_assign_ptr_type(struct type *target_type, struct type *rhs_type) {
    assert("can_assign_ptr_type", is_ptr_type(target_type) && is_ptr_type(rhs_type));
    if (target_type == rhs_type)
        return 1;
    return is_void_ptr(target_type) || is_void_ptr(rhs_type);
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
    } else if (is_ptr_type(t)) {
        if (!is_null_ptr(rhs) && (!is_ptr_type(rhs->type) || !can_assign_ptr_type(t, rhs->type)))
            error_at(&rhs->pos, "target type mismatch. Pointer types are incompatible.");
        return cast_to(t, rhs);
    } else {
        error_at(&rhs->pos, "target type mismatch");
    }
}

static struct expr *ptr_decay(struct expr *e) {
    if (is_array_type(e->type)) {
        struct type *array_type = e->type;
        struct type *elem_type = array_type->ptr_to;
        return wrap_with(Expr_Cast, new_ptr_type(elem_type), wrap_with(Expr_Addr, new_ptr_type(array_type), e));
    } else if (is_func_type(e->type)) {
        return wrap_with(Expr_Addr, new_ptr_type(e->type), e);
    } else {
        return e;
    }
}

static struct expr *elab_expr(struct expr *e);

static void elab_subexprs(struct expr *e) {
    int k = e->kind;

    int i = 0;
    while (i < e->n_subs) {
        e->subs[i] = elab_expr(e->subs[i]);
        if (k != Expr_Addr) {
            e->subs[i] = ptr_decay(e->subs[i]);
        }
        i++;
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
        e->type = new_array_type(char_type, str_len(e->str_val) + 1);
    } else if (k == Expr_Ident) {
        struct sym *sym = lookup(0, e->str_val);
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
        if (n_args > func->n_params)
            error_at(&e->pos, "too many arguments in function call");
        if (n_args < func->n_params)
            error_at(&e->pos, "too few arguments in function call");
        int i = 0;
        while (i < n_args) {
            args[i] = apply_assignment_conversion(args[i], func->param_types[i]);
            i++;
        }
        e->type = func->ret_type;
    } else if (k == Expr_Member) {
        if (e->subs[0]->type->kind != Type_Struct)
            error_at(&e->pos, "member access on non-struct type");
        struct sym *sym = e->subs[0]->type->sym;
        struct field *field = lookup_field(sym, e->str_val);
        if (!field)
            error_at(&e->pos, "member not found in struct");
        e->field = field;
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
    } else if (k == Expr_Not) {
        if (!is_scalar(e->subs[0]->type))
            error_at(&e->pos, "operand must be scalar");
        e->type = int_type;
    } else if (k == Expr_Cast) {
        if (!is_scalar(e->type) || !is_scalar(e->subs[0]->type))
            error_at(&e->pos, "cast requires scalar types");
    } else if (k == Expr_Mul || k == Expr_Div || k == Expr_Mod || k == Expr_Add || k == Expr_Sub) {
        if (k == Expr_Add && is_integer_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            ptr_swap((void **)&e->subs[0], (void **)&e->subs[1]);
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
        apply_null_ptr_conversion(e->subs);
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
    } else {
        unreachable_case("elab_expr", k);
    }
    return e;
}

static struct expr *elab_rvalue_expr(struct expr *expr) {
    expr = elab_expr(expr);
    return ptr_decay(expr);
}

static struct expr *elab_expr_expect(struct expr *expr, struct type *expected) {
    expr = elab_rvalue_expr(expr);
    return apply_assignment_conversion(expr, expected);
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

enum {
    MAX_TOK_LEN = 255,
};

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
        chr_pos.line++;
        chr_pos.col = 1;
    } else {
        chr_pos.col++;
    }
    if (tok_len < MAX_TOK_LEN) {
        tok_str[tok_len++] = chr;
    }
    chr = read_char(inp);
}

static void lex_init(const char *file) {
    chr_pos.file = file;
    chr_pos.line = 1;
    chr_pos.col = 1;
    next_chr();
}

static void lex() {
    while (1) {
        tok_len = 0;
        tok_pos = chr_pos;
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
            int delim = chr;
            int len = 0;
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
            }
        }
        break;
    }

    tok_str[tok_len] = 0;
}

//=============================================================================
//= parse.dsl

static void parse_init() {
    lex();
}

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
    if (!eat(t)) {
        diag_at(&tok_pos, "error");
        write_f2(2, "expected '%s', got '%s'\n", t, tok_str);
        _exit(1);
    }
}

static void unexpected_expected(const char *d) {
    diag_at(&tok_pos, "error");
    write_f2(2, "expected %s, got '%s'\n", d, tok_str);
    _exit(1);
}

static const char *p_ident() {
    if (tok != Tok_Wrd)
        unexpected_expected("identifier");
    const char *res = intern(tok_str, tok_len + 1);
    lex();
    return res;
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
    return at("void") || at("char") || at("int") || at("struct") || at("enum") || at("const");
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
    Prec_Assign = 1,
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
    res = new_unary_expr(pos, kind, res);
    return res;
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
    } else if (tok == Tok_Wrd) {
        acc = new_expr(&pos, Expr_Ident, 0);
        acc->str_val = p_ident();
        struct sym *sym = lookup(0, acc->str_val);
        if (!sym)
            error_at(&pos, "undefined symbol");
    } else {
        unexpected_expected("expression");
    }

    // led
    while (1) {
        if (rbp < Prec_Assign && eat("=")) {
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
                args[n_args++] = p_expr(0);
            }
            tmp->n_subs = n_args + 1;
            acc = tmp;
        } else if (rbp < Prec_Postfix && eat(".")) {
            acc = new_unary_expr(&pos, Expr_Member, acc);
            acc->str_val = p_ident();
        } else if (rbp < Prec_Postfix && eat("->")) {
            acc = new_unary_expr(&pos, Expr_Deref, acc);
            acc = new_unary_expr(&pos, Expr_Member, acc);
            acc->str_val = p_ident();
        } else if (rbp < Prec_Postfix && eat("++")) {
            acc = new_unary_expr(&pos, Expr_PostInc, acc);
        } else if (rbp < Prec_Postfix && eat("--")) {
            acc = new_unary_expr(&pos, Expr_PostDec, acc);
        } else {
            break;
        }
    }

    return acc;
}

static int p_const_expr() {
    struct expr *expr = p_expr(Prec_Cond - 1);
    return eval(elab_rvalue_expr(expr));
}

static struct stmt *p_stmt() {
    struct pos pos = tok_pos;
    if (eat("{")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_Block);
        if (eat("}"))
            return stmt;
        enter_scope(&pos);
        stmt->sub = p_stmt();
        struct stmt *tail = stmt->sub;
        while (!eat("}")) {
            tail->next = p_stmt();
            tail = tail->next;
        }
        leave_scope();
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

        enter_scope(&pos);
        stmt->sub = p_stmt();
        leave_scope();

        if (eat("else")) {
            enter_scope(&pos);
            stmt->sub->next = p_stmt();
            leave_scope();
        }

        return stmt;
    } else if (eat("while")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_While);

        expect("(");
        stmt->expr = p_expr(0);
        stmt->expr = elab_cond_expr(stmt->expr);
        expect(")");

        struct stmt *outer_loop = curr_loop;
        curr_loop = stmt;
        enter_scope(&pos);
        stmt->sub = p_stmt();
        leave_scope();
        curr_loop = outer_loop;

        return stmt;
    } else if (eat("break")) {
        if (!curr_loop)
            error_at(&pos, "break statement outside loop");
        struct stmt *stmt = new_stmt(&pos, Stmt_Break);
        stmt->sub = curr_loop;
        expect(";");
        return stmt;
    } else if (eat("continue")) {
        if (!curr_loop)
            error_at(&pos, "continue statement outside loop");
        struct stmt *stmt = new_stmt(&pos, Stmt_Continue);
        stmt->sub = curr_loop;
        expect(";");
        return stmt;
    } else if (eat(";")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_Empty);
        // empty statement
        return stmt;
    } else if (at_decl()) {
        struct stmt *stmt = 0;
        p_decl(Decl_Local, &stmt);
        if (!stmt) {
            stmt = new_stmt(&pos, Stmt_Empty);
        }
        return stmt;
    } else {
        struct stmt *stmt = new_stmt(&pos, Stmt_Expr);
        stmt->expr = p_expr(0);
        stmt->expr = elab_rvalue_expr(stmt->expr);
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
        sym = lookup(Ns_Struct, name);
    }

    if (is_def || !sym) {
        sym = declare_struct(&name_pos, name, is_def);
        if (eat("{")) {
            while (!eat("}")) {
                p_decl(Decl_Struct, sym);
            }
        }
    }

    if (!name && !is_def)
        error_at(&name_pos, "declaration of anonymous struct must be a definition");

    return sym->type;
}

static struct type *p_enum() {
    expect("{");
    int val = 0;
    while (!eat("}")) {
        struct pos name_pos = tok_pos;
        const char *name = p_ident();
        if (eat("=")) {
            val = p_const_expr();
        }
        if (!at("}")) {
            expect(",");
        }
        define_const(&name_pos, name, val);
        val++;
    }
    return int_type;
}

extern void p_decl(int scope, void *ctx) {
    struct pos pos = tok_pos;
    const char *storage_class = 0;
    struct type *base_type = 0;
    while (1) {
        if (eat("const")) {
            // ignored
        } else if (!storage_class && eat("static")) {
            storage_class = "static";
        } else if (!storage_class && eat("extern")) {
            storage_class = "extern";
        } else if (!base_type && eat("void")) {
            base_type = void_type;
        } else if (!base_type && eat("int")) {
            base_type = int_type;
        } else if (!base_type && eat("char")) {
            base_type = char_type;
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

    int n_declarators = 0;
    struct stmt *prev_local = 0;
    while (1) {
        struct type *type = base_type;
        const char *name = 0;
        int has_params = 0;
        struct func_param params[MAX_FUNC_PARAMS];
        int n_params = 0;
        int has_func_body = 0;
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
            has_params = 1;
            n_params = 0;
            enter_scope(&name_pos);
            while (!eat(")")) {
                if (n_params >= MAX_FUNC_PARAMS)
                    error_at(&name_pos, "too many parameters in function declaration");
                if (n_params > 0) {
                    expect(",");
                }
                p_decl(Decl_Param, &params[n_params++]);
            }
            leave_scope();
            type = prototype_to_func_type(type, params, n_params);
        } else if (at("[")) {
            while (eat("[")) {
                int len = p_const_expr();
                expect("]");
                type = new_array_type(type, len);
            }
        } else if (eat("=") && (scope == Decl_Global || scope == Decl_Local)) {
            init = p_expr(0);
        }

        if (scope == Decl_TypeName) {
            *(struct type **)ctx = type;
            return;  // max one type per abstract declaration
        } else if (scope == Decl_Param) {
            struct func_param *param = (struct func_param *)ctx;
            if (is_array_type(type)) {
                type = new_ptr_type(type->ptr_to);
            } else if (is_func_type(type)) {
                type = new_ptr_type(type);
            }

            if (!is_scalar(type))
                error_at(&name_pos, "bad parameter type");
            param->type = type;
            param->name_pos = name_pos;
            param->name = name;
            return;  // max one parameter per declaration
        } else if (!name) {
            // declaration does not declare a function or object
        } else if (has_params) {
            // function declaration
            if (scope != Decl_Global && scope != Decl_Local)
                error_at(&name_pos, "function declaration is not allowed here");
            int linkage = storage_class ? (storage_class[0] == 's' ? Internal : External) : 0;
            has_func_body = scope == Decl_Global && n_declarators == 0 && at("{");
            struct sym *sym = declare_func(&name_pos, linkage, name, type->ret_type, params, n_params, has_func_body);
            if (has_func_body) {
                reenter_scope();
                int i = 0;
                curr_func = sym;
                while (i < n_params) {
                    define_local(&params[i].name_pos, curr_func, params[i].name, params[i].type);
                    i++;
                }
                sym->body = p_stmt();
                curr_func = 0;
                leave_scope();
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
                struct stmt *local = new_stmt(&pos, Stmt_Decl);
                local->sym = define_local(&name_pos, curr_func, name, type);
                local->expr = init;
                if (!prev_local) {
                    prev_local = *(struct stmt **)ctx = local;
                } else {
                    prev_local = prev_local->sub = local;
                }
            } else if (scope == Decl_Global) {
                int linkage = storage_class ? (storage_class[0] == 's' ? Internal : External) : 0;
                struct sym *sym = declare_global(&name_pos, linkage, name, type, !!init);
                if (init) {
                    sym->val = elab_init(init, type);
                }
            } else if (scope == Decl_Struct) {
                struct sym *sym = (struct sym *)ctx;
                declare_field(&name_pos, sym, name, type);
            } else {
                unreachable_case("p_decl (object declaration)", scope);
            }
        }

        if (!name || !eat(",")) {
            expect(";");
            return;
        }

        n_declarators++;
    }
}

//=============================================================================
//= codegen

int next_loop_id;
int next_cond_id;
int next_str_id;

struct emitted_str {
    const char *str;
    int label;
    struct emitted_str *next;
} *output_strs;

int add_literal(const char *str) {
    struct emitted_str *node = output_strs, *head = output_strs;
    while (node) {
        if (node->str == str)
            return node->label;
        node = node->next;
    }
    output_strs = alloc(sizeof(struct emitted_str));
    output_strs->str = str;
    output_strs->label = next_str_id++;
    output_strs->next = head;
    return output_strs->label;
}

static const char *get_str_op(struct type *type) {
    if (type->kind == Type_Char)
        return "strb w";
    else if (type->kind == Type_Int)
        return "str w";
    else if (type->kind == Type_Ptr)
        return "str x";
    else
        unreachable_case("get_str_op", type->kind);
    return 0;
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
    return 0;
}

static void emit_scalar_data(int size, int val) {
    if (size == 1) {
        write_f1(1, ".byte %d\n", &val);
    } else if (size == 4) {
        write_f1(1, ".long %d\n", &val);
    } else if (size == 8) {
        write_f1(1, ".quad %d\n", &val);
    } else
        unreachable_case("emit_data_scalar", size);
}

static void emit_str_load(const char *str) {
    int label_id = add_literal(str);
    write_f1(1, "adrp x0, .L.str.%d\n", &label_id);
    write_f1(1, "add x0, x0, :lo12:.L.str.%d\n", &label_id);
}

static void emit_int_load(int val, int reg) {
    int chunk_mask = (1 << 16) - 1;
    int default_chunk_value = val < 0 ? chunk_mask : 0;
    int i = 0;
    while (i < 4) {
        int chunk = val & chunk_mask;
        if (i == 0) {
            const char *mov_op = val < 0 ? "movn" : "movz";
            chunk = val < 0 ? (chunk ^ chunk_mask) & chunk_mask : chunk;
            write_f3(1, "%s x%d, #%d\n", mov_op, &reg, &chunk);
        } else if (chunk != default_chunk_value) {
            int shift = i * 16;
            write_f3(1, "movk x%d, #%d, lsl #%d\n", &reg, &chunk, &shift);
        }
        i++;
        val = val >> 16;
    }
}

static void emit_obj(struct sym *sym) {
    int size, align;
    type_layout(sym->type, &size, &align);

    if (size <= 8) {
        write_str(1, ".section .data\n");
        if (sym->linkage == External) {
            write_f1(1, ".globl %s\n", sym->name);
        }
        write_f1(1, ".balign %d\n", &align);
        write_f1(1, "%s:\n", sym->name);
        if (sym->is_defined) {
            emit_scalar_data(size, sym->val);
        } else {
            emit_scalar_data(size, 0);
        }
    } else {
        if (sym->linkage == External) {
            write_f1(1, ".globl %s\n", sym->name);
        }
        write_str(1, ".section .bss\n");
        write_f1(1, ".balign %d\n", &align);
        write_f1(1, "%s:\n", sym->name);
        write_f1(1, ".space %d\n", &size);
    }
}

int layout_func(struct sym *sym) {
    int slots_size = 0;
    int i = 0;
    while (i < sym->n_locals) {
        struct sym *local = sym->locals[i];
        int size, align;
        type_layout(local->type, &size, &align);
        local->offset = -align_up(slots_size + size, align);
        slots_size = -local->offset;
        i++;
    }

    slots_size = align_up(slots_size, 16);
    return slots_size;
}

static void emit_slot_write(int i, int reg) {
    struct sym *local = curr_func->locals[i];
    const char *op = get_str_op(local->type);
    emit_int_load(local->offset, 9);
    write_f2(1, "%s%d, [x29, x9]\n", op, &reg);
}

static void emit_slot_addr(int i, int reg) {
    struct sym *local = curr_func->locals[i];
    emit_int_load(local->offset, 9);
    write_f1(1, "add x%d, x29, x9\n", &reg);
}

static void emit_push(int reg) {
    write_f1(1, "str x%d, [sp, #-16]!\n", &reg);
}

static void emit_pop(int reg) {
    write_f1(1, "ldr x%d, [sp], #16\n", &reg);
}

static void emit_load(struct type *type, int dst, int src) {
    const char *op = get_ldr_op(type);
    write_f3(1, "%s%d, [x%d]\n", op, &dst, &src);
}

static void emit_store(struct type *type, int src, int dst) {
    const char *op = get_str_op(type);
    write_f3(1, "%s%d, [x%d]\n", op, &src, &dst);
}

static void emit_sext(struct type *type, int reg) {
    if (type->kind == Type_Char) {
        write_f2(1, "sxtb x%d, w%d\n", &reg, &reg);
    } else if (type->kind == Type_Int) {
        write_f2(1, "sxtw x%d, w%d\n", &reg, &reg);
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
    write_f1(1, ".L.cond.%d:\n", &cond_id);
    emit_scalar_expr(expr->subs[0]);
    write_f2(1, "cb%s x0, .L.cond.%d.short\n", cond, &cond_id);
    emit_scalar_expr(expr->subs[1]);
    write_f1(1, ".L.cond.%d.short:\n", &cond_id);
    write_str(1, "cmp x0, #0\n");
    write_str(1, "cset x0, ne\n");
    write_f1(1, ".L.cond.%d.end:\n", &cond_id);
}

static void emit_cmp_expr(struct expr *expr, const char *cond, const char *ucond) {
    emit_binary_operands(expr);
    write_str(1, "cmp x0, x1\n");
    if (is_ptr_type(expr->subs[0]->type)) {
        write_f1(1, "cset x0, %s\n", ucond);
    } else {
        write_f1(1, "cset x0, %s\n", cond);
    }
}

static void emit_arithmetic_expr(struct expr *expr, const char *op) {
    emit_binary_operands(expr);
    write_f1(1, "%s x0, x0, x1\n", op);
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
        write_str(1, "bl memcpy\n");
    }
}

static void emit_place_expr(struct expr *expr) {
    int k = expr->kind;
    if (k == Expr_Ident) {
        struct sym *sym = expr->sym;
        if (sym->kind == Sym_Global || sym->kind == Sym_Func) {
            if (sym->linkage == External && !sym->is_defined) {
                write_f1(1, "adrp x0, :got:%s\n", sym->name);
                write_f1(1, "ldr x0, [x0, :got_lo12:%s]\n", sym->name);
            } else {
                write_f1(1, "adrp x0, %s\n", sym->name);
                write_f1(1, "add x0, x0, :lo12:%s\n", sym->name);
            }
        } else if (sym->kind == Sym_Local) {
            emit_slot_addr(sym->slot_idx, 0);
        } else {
            unreachable_case("emit_place_expr (ident)", sym->kind);
        }
    } else if (k == Expr_Member) {
        emit_place_expr(expr->subs[0]);
        emit_int_load(expr->field->offset, 1);
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
        int step = 1;
        if (is_ptr_type(expr->type)) {
            step = type_size(expr->type->ptr_to);
        }
        emit_place_expr(expr->subs[0]);
        write_str(1, "mov x2, x0\n");
        emit_load(expr->type, 0, 0);
        emit_int_load(step, 1);
        write_f1(1, "%s x1, x0, x1\n", op);
        emit_store(expr->type, 1, 2);
    } else if (k == Expr_Deref) {
        emit_scalar_expr(expr->subs[0]);
        emit_load(expr->type, 0, 0);
    } else if (k == Expr_Addr) {
        emit_place_expr(expr->subs[0]);
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
        int target_size = type_size(expr->type);
        if (target_size < 8) {
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
        write_f1(1, "%s x0, x0, x1\n", op);
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
        write_f1(1, ".L.cond.%d:\n", &cond_id);
        emit_scalar_expr(expr->subs[0]);
        write_f1(1, "cbz x0, .L.cond.%d.else\n", &cond_id);
        emit_scalar_expr(expr->subs[1]);
        write_f1(1, "b .L.cond.%d.end\n", &cond_id);
        write_f1(1, ".L.cond.%d.else:\n", &cond_id);
        emit_scalar_expr(expr->subs[2]);
        write_f1(1, ".L.cond.%d.end:\n", &cond_id);
    } else if (k == Expr_Assign) {
        emit_place_expr(expr->subs[0]);
        emit_assign_to_addr(expr->type, expr->subs[1]);
    } else if (k == Expr_Call) {
        struct expr *fn = expr->subs[0];
        struct expr **args = &expr->subs[1];
        int argc = expr->n_subs - 1;

        struct sym *func_sym = 0;
        if (fn->kind == Expr_Addr && fn->subs[0]->kind == Expr_Ident && fn->subs[0]->sym->kind == Sym_Func) {
            func_sym = fn->subs[0]->sym;
        }

        int i = 0;
        while (i < argc) {
            emit_scalar_expr(args[i]);
            emit_push(0);
            i++;
        }
        while (i > 0) {
            i--;
            emit_pop(i);
        }
        write_f1(1, "bl %s\n", func_sym->name);
        // if calling a foreign function, we can't be sure
        // if it will sign-extend or zero the return value.
        if (is_scalar(expr->type) && type_size(expr->type) < 8) {
            emit_sext(expr->type, 0);
        }
    } else {
        unreachable_case("emit_scalar_expr", k);
    }
}

static void emit_effect_expr(struct expr *expr) {
    if (expr->kind == Expr_Assign) {
        emit_place_expr(expr->subs[0]);
        emit_assign_to_addr(expr->type, expr->subs[1]);
    } else if (is_scalar(expr->type) || is_void_type(expr->type)) {
        emit_scalar_expr(expr);
    } else if (is_lvalue(expr)) {
        emit_place_expr(expr);
    } else {
        unreachable_case("emit_effect_expr", expr->kind);
    }
}

static void emit_stmt(struct stmt *stmt) {
    write_f3(1, "// %s:%d:%d\n", stmt->pos.file, &stmt->pos.line, &stmt->pos.col);  // debug info
    int k = stmt->kind;
    if (k == Stmt_Block) {
        struct stmt *sub = stmt->sub;
        while (sub) {
            emit_stmt(sub);
            sub = sub->next;
        }
    } else if (k == Stmt_Return) {
        if (stmt->expr) {
            emit_scalar_expr(stmt->expr);
        }
        write_f1(1, "b .L.return.%s\n", curr_func->name);
    } else if (k == Stmt_If) {
        int cond_id = next_cond_id++;
        emit_scalar_expr(stmt->expr);
        write_f1(1, "cbz x0, .L.if.%d.else\n", &cond_id);
        emit_stmt(stmt->sub);
        write_f1(1, "b .L.if.%d.end\n", &cond_id);
        write_f1(1, ".L.if.%d.else:\n", &cond_id);
        if (stmt->sub->next) {
            emit_stmt(stmt->sub->next);
        }
        write_f1(1, ".L.if.%d.end:\n", &cond_id);
    } else if (k == Stmt_While) {
        stmt->loop_id = next_loop_id++;
        write_f1(1, "b .L.loop.%d.cond\n", &stmt->loop_id);
        write_f1(1, ".L.loop.%d.body:\n", &stmt->loop_id);
        emit_stmt(stmt->sub);
        write_f1(1, ".L.loop.%d.cond:\n", &stmt->loop_id);
        emit_scalar_expr(stmt->expr);
        write_f1(1, "cbnz x0, .L.loop.%d.body\n", &stmt->loop_id);
        write_f1(1, ".L.loop.%d.end:\n", &stmt->loop_id);
    } else if (k == Stmt_Break) {
        write_f1(1, "b .L.loop.%d.end\n", &stmt->sub->loop_id);
    } else if (k == Stmt_Continue) {
        write_f1(1, "b .L.loop.%d.cond\n", &stmt->sub->loop_id);
    } else if (k == Stmt_Empty) {
        // pass
    } else if (k == Stmt_Decl) {
        struct stmt *decl = stmt;
        while (decl) {
            if (decl->expr) {
                emit_slot_addr(decl->sym->slot_idx, 0);
                if (decl->expr) {
                    emit_assign_to_addr(decl->sym->type, decl->expr);
                }
            }
            decl = decl->sub;
        }
    } else if (k == Stmt_Expr) {
        emit_effect_expr(stmt->expr);
    } else {
        unreachable_case("emit_stmt", k);
    }
}

static void emit_func(struct sym *func) {
    curr_func = func;
    int slots_size = layout_func(func);

    write_str(1, ".text\n");
    if (func->linkage != Internal) {
        write_f1(1, ".globl %s\n", func->name);
    }
    write_f1(1, "%s:\n", func->name);
    // prologue
    write_str(1, "stp x29, x30, [sp, #-16]!\n");
    write_str(1, "mov x29, sp\n");
    write_f1(1, "sub sp, sp, #%d\n", &slots_size);
    int i = 0;
    while (i < func->type->n_params) {
        emit_slot_write(i, i);
        i++;
    }
    // body
    emit_stmt(func->body);
    // epilogue
    write_f1(1, ".L.return.%s:\n", func->name);
    write_str(1, "mov sp, x29\n");
    write_str(1, "ldp x29, x30, [sp], #16\n");
    write_str(1, "ret\n");
    curr_func = 0;
}

static void emit_memcpy() {
    write_str(1, ".section .text\n");
    write_str(1, ".globl memcpy\n");
    write_str(1, "memcpy:\n");
    write_str(1, "mov x3, x0\n");
    write_str(1, "cbz x2, .L.memcpy.end\n");
    write_str(1, ".L.memcpy.body:\n");
    write_str(1, "ldrb w4, [x1], #1\n");
    write_str(1, "strb w4, [x3], #1\n");
    write_str(1, "subs x2, x2, #1\n");
    write_str(1, "cbnz x2, .L.memcpy.body\n");
    write_str(1, ".L.memcpy.end:\n");
    write_str(1, "ret\n");  // x0 still holds dest
}

static void emit_str_literals() {
    write_str(1, ".section .rodata\n");
    struct emitted_str *node = output_strs;
    while (node) {
        const char *str = node->str;
        write_f1(1, ".L.str.%d:\n", &node->label);
        int c;
        while ((c = *str++)) {
            write_f1(1, ".byte %d\n", &c);
        }
        write_str(1, ".byte 0\n");
        node = node->next;
    }
}

//=============================================================================
//= main

int main(int argc, char **argv) {
    int quiet = 1;
    const char *file_name = 0;
    int i = 1;
    while (i < argc) {
        if (str_eq(argv[i], "-v")) {
            quiet = 0;
        } else if (!file_name) {
            file_name = argv[i];
        } else {
            write_f1(2, "error: unrecognized argument '%s'\n", argv[i]);
            return 1;
        }
        i++;
    }
    if (!file_name) {
        write_str(2, "input file not specified\n");
        return 1;
    }
    if ((inp = open(file_name, 0, 0)) < 0) {
        write_f1(2, "error: cannot open file '%s'\n", file_name);
        return 1;
    }

    type_init();
    lex_init(file_name);
    parse_init();
    curr_scope = scopes;
    while (tok != EOF) {
        p_decl(Decl_Global, 0);
    }

    int n_funcs = 0, n_objs = 0;
    struct sym *syms = scopes[0].syms;
    while (syms) {
        struct sym *sym = syms;
        syms = syms->next;
        if (sym->kind == Sym_Func) {
            if (sym->linkage != Internal && !sym->is_defined)
                continue;  // declaration, external linkage
            if (sym->linkage == Internal && !sym->is_defined)
                continue;  // declaration with internal linkage, missing definition
            emit_func(sym);
            n_funcs++;
        } else if (sym->kind == Sym_Global) {
            if (sym->linkage == External && !sym->is_defined)
                continue;  // declaration, external linkage
            if (!sym->is_defined)
                ;  // tentative definition
            emit_obj(sym);
            n_objs++;
        }
    }
    emit_memcpy();
    emit_str_literals();

    if (!quiet) {
        write_f2(2, "generated %d functions and %d objects\n", &n_funcs, &n_objs);
        write_f1(2, "allocated %d bytes from arena\n", &arena_len);
        write_f1(2, "allocated %d types\n", &n_types);
        write_f1(2, "allocated %d symbols\n", &n_syms);
        write_f1(2, "interned %d strings\n", &strings_len);
    }

    return 0;
}
