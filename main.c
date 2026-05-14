//============================================================================
//= unistd

extern int open(const char *pathname, int flags, int mode);
extern int read(int fd, void *buf, int count);
extern int write(int fd, const void *buf, int count);
extern void _exit(int status);

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

static void write_int(int fd, long n) {
    if (n < 0) {
        write_char(fd, '-');
        n = -n;
    }
    if (n >= 10)
        write_int(fd, n / 10);
    write_char(fd, n % 10 + '0');
}

static void write_hex(int fd, long n) {
    if (n >= 16)
        write_hex(fd, n / 16);
    int digit = n % 16;
    if (digit < 10)
        write_char(fd, digit + '0');
    else
        write_char(fd, digit - 10 + 'a');
}

// internal printf implementation supporting %d, %c, %s, and %x
static void write_f_(int fd, const char *fmt, const char **args) {
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'd') {
                    write_int(fd, **(long **)(args++));
                } else if (*fmt == 'x') {
                    write_str(fd, "0x");
                    write_hex(fd, **(long **)(args++));
                } else {
                    write_char(fd, '%');
                    write_char(fd, 'l');
                }
            } else if (*fmt == 'd') {
                write_int(fd, **(int **)(args++));
            } else if (*fmt == 'x') {
                write_hex(fd, **(int **)(args++));
            } else if (*fmt == 'c') {
                write_char(fd, **(char **)(args++));
            } else if (*fmt == 's') {
                const char *s = *(char **)(args++);
                if (s)
                    write_str(fd, s);
                else
                    write_str(fd, "(null)");
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
    while (s[len])
        len++;
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

//=============================================================================
//= assertions

static void die(const char *label, const char *msg) {
    write_f2(2, "%s: %s\n", label, msg);
    _exit(1);
}

static void assert(const char *label, int condition) {
    if (!condition) {
        die(label, "assertion failed");
    }
}

static void unreachable_case(const char *label, int value) {
    write_f2(2, "%s: case not handled: %d\n", label, &value);
    _exit(1);
}

//=============================================================================
//= arena

enum { Arena_Cap = 1 * 1024 * 1024 };  // 1 MiB
static char arena[Arena_Cap];
static int arena_len;

static void *alloc(int len) {
    if (arena_len + len >= Arena_Cap) {
        die("alloc", "out of memory");
    }
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
    if (strings_len >= 10240) {
        die("intern", "out of memory for strings");
    }
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
    MAX_STRUCT_FIELDS = 16,
    MAX_FUNC_PARAMS = 8,
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
    const char *name;
    struct type *type;
};

enum {
    External = 1,
    Internal = 2,
};

static struct sym {
    int kind;
    int ns;
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
    // case Sym_Struct:
    struct field fields[MAX_STRUCT_FIELDS];
    int n_fields;
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
    Type_Long,
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
static struct type *long_type;
static struct type *ptr_to_void;
static struct type *ptr_to_char;
static struct type *ptr_to_int;
static struct type *ptrdiff_type;

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
    } else if (ty->kind == Type_Long) {
        *size = 8;
        *align = 8;
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
            if (field_align > max_align)
                max_align = field_align;
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
        return type_eq(a->ptr_to, b->ptr_to);
    if (a->kind == Type_Array)
        return a->array_len == b->array_len && type_eq(a->ptr_to, b->ptr_to);
    if (a->kind == Type_Func) {
        if (!type_eq(a->ret_type, b->ret_type))
            return 0;
        if (a->n_params != b->n_params)
            return 0;
        int i = 0;
        while (i < a->n_params) {
            if (!type_eq(a->param_types[i], b->param_types[i]))
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
    return ty->kind == Type_Int || ty->kind == Type_Char || ty->kind == Type_Long;
}
static int is_void_type(struct type *ty) {
    return ty->kind == Type_Void;
}
static int is_arithmetic(struct type *ty) {
    return is_integer_type(ty);
}
static int is_ptr_type(struct type *ty) {
    return ty->kind == Type_Ptr;
}
static int is_scalar(struct type *ty) {
    return is_arithmetic(ty) || is_ptr_type(ty);
}
static int is_func_type(struct type *ty) {
    return ty->kind == Type_Func;
}
static int is_void_ptr(struct type *ty) {
    return is_ptr_type(ty) && ty->ptr_to->kind == Type_Void;
}
static int is_int_type(struct type *ty) {
    return ty->kind == Type_Int;
}
static int is_array_type(struct type *ty) {
    return ty->kind == Type_Array;
}
static int is_struct_type(struct type *ty) {
    return ty->kind == Type_Struct;
}
static int is_object_type(struct type *ty) {
    return type_size(ty) > 0;
}
static int is_incompletete_type(struct type *ty) {
    return !is_object_type(ty) && !is_func_type(ty);
}
static int is_allowed_func_ret_type(struct type *ty) {
    return is_void_type(ty) || is_scalar(ty);
}
static int is_allowed_func_param_type(struct type *ty) {
    return is_scalar(ty);
}

static struct type *intern_type(struct type *ty) {
    int i = 0;
    while (i < n_types) {
        if (type_eq(ty, &types[i]))
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
    assert("uac_type", is_arithmetic(t1) && is_arithmetic(t2));
    return int_promotion_type(t1, t2);
}

static struct type *get_common_ptr_type(struct type *t1, struct type *t2) {
    assert("get_common_ptr_type", is_ptr_type(t1) && is_ptr_type(t2));
    if (type_eq(t1->ptr_to, t2->ptr_to))
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
    long_type = &types[n_types++];
    long_type->kind = Type_Long;
    ptr_to_void = new_ptr_type(void_type);
    ptr_to_char = new_ptr_type(char_type);
    ptr_to_int = new_ptr_type(int_type);
    ptrdiff_type = long_type;
}

static const char *type_str(struct type *ty) {
    if (ty->kind == Type_Void)
        return "void";
    if (ty->kind == Type_Char)
        return "char";
    if (ty->kind == Type_Int)
        return "int";
    if (ty->kind == Type_Long)
        return "long";
    if (ty->kind == Type_Ptr)
        return "pointer";
    if (ty->kind == Type_Array)
        return "array";
    if (ty->kind == Type_Struct)
        return "struct";
    if (ty->kind == Type_Func)
        return "function";
    return "unknown";
}

//=============================================================================
//= symbol table

enum {
    MAX_SYMS_PER_SCOPE = 256,
    MAX_SCOPES = 256,
};

static struct scope {
    struct sym *syms[MAX_SYMS_PER_SCOPE];
    int n_syms;
} scopes[MAX_SCOPES];
static struct scope *curr_scope = scopes;

static void enter_scope(struct pos *pos) {
    int depth = curr_scope - scopes;
    if (depth + 1 >= MAX_SCOPES)
        error_at(pos, "too many nested scopes");
    curr_scope++;
    curr_scope->n_syms = 0;
}

static void reenter_scope() {
    int depth = curr_scope - scopes;
    assert("reenter_scope", depth + 1 < MAX_SCOPES);
    curr_scope++;
}

static void leave_scope() {
    if (curr_scope <= scopes) {
        die("leave_scope", "scope underflow");
    }
    curr_scope--;
}

static struct sym *add_sym(struct pos *pos, int kind, const char *name) {
    struct sym *sym = alloc_sym();
    sym->kind = kind;
    sym->name = name;
    sym->last_pos = *pos;
    if (curr_scope->n_syms >= MAX_SYMS_PER_SCOPE)
        error_at(pos, "symbol limit reached in this scope");
    curr_scope->syms[curr_scope->n_syms++] = sym;

    return sym;
}

static struct sym *lookup_in(struct scope *scope, int ns, const char *name) {
    if (!name)
        return 0;
    int i = 0;
    while (i < scope->n_syms) {
        struct sym *sym = scope->syms[i];
        if (sym_ns(sym) == ns && sym->name == name)
            return sym;
        i++;
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

static struct sym *define_var(struct pos *pos, const char *name, struct type *type) {
    struct sym *sym = lookup_in(curr_scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Local)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        decl_conflict(pos, sym, "already defined");
    }
    sym = add_sym(pos, Sym_Local, name);
    sym->type = type;
    sym->is_defined = 1;
    return sym;
}

static struct sym *declare_global(struct pos *pos, int linkage, const char *name, struct type *type, int is_def) {
    struct sym *sym = lookup_in(curr_scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Global)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        if (!type_eq(sym->type, type))
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
        if (is_def)
            sym->params[i].name = params[i].name;
        i++;
    }
}

static struct sym *declare_func(struct pos *pos, int linkage, const char *name, struct type *ret_type,
                                struct func_param *params, int n_params, int is_def) {
    if (!is_allowed_func_ret_type(ret_type))
        error_at(pos, "bad function return type");
    struct type *type = prototype_to_func_type(ret_type, params, n_params);
    struct sym *sym = lookup_in(curr_scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Func)
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        if (!type_eq(sym->type, type))
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
    Expr_Not,
    Expr_Addr,
    Expr_Deref,
    Expr_Ident,
    Expr_Call,
    Expr_Subscript,
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
    if (type_eq(target_type, rhs_type))
        return 1;
    return is_void_ptr(target_type) || is_void_ptr(rhs_type);
}

// expression predicates
static int is_var_expr(struct expr *expr) {
    return expr->kind == Expr_Ident && (expr->sym->kind == Sym_Local || expr->sym->kind == Sym_Global);
}
static int is_func_expr(struct expr *expr) {
    return expr->kind == Expr_Ident && expr->sym->kind == Sym_Func;
}
static int is_lvalue(struct expr *expr) {
    int k = expr->kind;
    return is_var_expr(expr) || k == Expr_Deref || k == Expr_Subscript || k == Expr_Member;
}
static int is_addressable(struct expr *expr) {
    return is_lvalue(expr) || is_func_expr(expr);
}
static int is_assignable(struct expr *expr) {
    return is_lvalue(expr) && is_object_type(expr->type);
}
static int is_null_ptr(struct expr *expr) {
    return expr->kind == Expr_Num && expr->int_val == 0
        || expr->kind == Expr_Cast && is_void_ptr(expr->type) && is_null_ptr(expr->subs[0]);
}

// coercion
static struct expr *cast_to(struct type *t, struct expr *e) {
    assert("cast_to", is_scalar(t) && is_scalar(e->type));
    if (type_eq(e->type, t))
        return e;
    return wrap_with(Expr_Cast, t, e);
}

static void apply_uac(struct expr **args) {
    assert("apply_uac", is_arithmetic(args[0]->type) && is_arithmetic(args[1]->type));
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
    if (type_eq(rhs->type, t)) {
        return rhs;
    } else if (is_arithmetic(rhs->type) && is_arithmetic(t)) {
        return cast_to(t, rhs);
    } else if (is_ptr_type(t)) {
        if (!is_null_ptr(rhs) && (!is_ptr_type(rhs->type) || !can_assign_ptr_type(t, rhs->type)))
            error_at(&rhs->pos, "target type mismatch. Pointer types are incompatible.");
        return cast_to(t, rhs);
    } else {
        error_at(&rhs->pos, "target type mismatch");
        return rhs;
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

static struct type *as_callable_type(struct type *t) {
    if (is_func_type(t))
        return t;
    if (is_ptr_type(t) && is_func_type(t->ptr_to))
        return t->ptr_to;
    return 0;
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
    } else if (k == Expr_PostInc || k == Expr_PostDec) {
        if (!is_assignable(e->subs[0]))
            error_at(&e->pos, "operand must be assignable");
        if (!is_arithmetic(e->subs[0]->type) && !is_ptr_type(e->subs[0]->type))
            error_at(&e->pos, "cannot increment/decrement operand of this type");
        e->type = e->subs[0]->type;
    } else if (k == Expr_Call) {
        struct expr *callee = e->subs[0];
        struct type *func = as_callable_type(callee->type);
        if (!func)
            error_at(&e->pos, "called object is not a function");

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
    } else if (k == Expr_Subscript) {
        e->kind = Expr_Add;
        return elab_expr(new_unary_expr(&e->pos, Expr_Deref, e));
    } else if (k == Expr_Member) {
        if (e->subs[0]->type->kind != Type_Struct)
            error_at(&e->pos, "member access on non-struct type");
        struct sym *sym = e->subs[0]->type->sym;
        struct field *field = lookup_field(sym, e->str_val);
        if (!field)
            error_at(&e->pos, "member not found in struct");
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
        if (!is_arithmetic(e->subs[0]->type))
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
        if (k == Expr_Add && is_ptr_type(e->subs[0]->type) && is_integer_type(e->subs[1]->type)) {
            e->type = e->subs[0]->type;
        } else if (k == Expr_Add && is_integer_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            e->type = e->subs[1]->type;
        } else if (k == Expr_Sub && is_ptr_type(e->subs[0]->type) && is_integer_type(e->subs[1]->type)) {
            e->type = e->subs[0]->type;
        } else if (k == Expr_Sub && is_ptr_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            if (!type_eq(e->subs[0]->type, e->subs[1]->type))
                error_at(&e->pos, "pointer types must match");
            e->type = ptrdiff_type;
        } else if (is_arithmetic(e->subs[0]->type) && is_arithmetic(e->subs[1]->type)) {
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
        if (is_arithmetic(e->subs[0]->type) && is_arithmetic(e->subs[1]->type)) {
            apply_uac(e->subs);
        } else if (is_ptr_type(e->subs[0]->type) && is_ptr_type(e->subs[1]->type)) {
            unify_ptr_operands(e->subs);
        } else {
            error_at(&e->pos, "operands of equality operators must have compatible types");
        }
        e->type = int_type;
    } else if (k == Expr_Lt || k == Expr_Le || k == Expr_Gt || k == Expr_Ge) {
        if (is_arithmetic(e->subs[0]->type) && is_arithmetic(e->subs[1]->type)) {
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
        } else if (is_arithmetic(e->subs[1]->type) && is_arithmetic(e->subs[2]->type)) {
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

static struct expr *chk_expr(struct expr *expr, struct type *expected) {
    expr = elab_rvalue_expr(expr);
    return apply_assignment_conversion(expr, expected);
}

static struct expr *chk_cond_expr(struct expr *expr) {
    expr = elab_rvalue_expr(expr);
    if (!is_scalar(expr->type))
        error_at(&expr->pos, "condition must have scalar type");
    return expr;
}

//=============================================================================
//= eval

static int eval(struct expr *expr);

static int eval_(struct expr *expr) {
    if (expr->kind == Expr_Num) {
        return expr->int_val;
    } else if (expr->kind == Expr_Ident) {
        if (expr->sym->kind != Sym_Const) {
            error_at(&expr->pos, "not a constant");
        }
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
    } else {
        error_at(&expr->pos, "expression cannot be evaluated at compile time");
        return 0;
    }
}

static int eval(struct expr *expr) {
    return eval_(elab_expr(expr));
}

//=============================================================================
//= lex

enum {
    TokNum,
    TokWrd,
    TokStr,
    TokChr,
    TokSym,
};

enum {
    MAX_TOK_LEN = 256,
};

static int inp;

static struct pos chr_pos;
static int chr;

static int tok;
static struct pos tok_pos;
static char tok_str[MAX_TOK_LEN];
static int tok_len;

static void n_ext_chr() {
    if (chr == '\n') {
        chr_pos.line++;
        chr_pos.col = 1;
    } else {
        chr_pos.col++;
    }
    if (tok_len + 1 < MAX_TOK_LEN) {
        tok_str[tok_len++] = chr;
    }
    chr = read_char(inp);
}

static void lex_init(const char *file) {
    chr_pos.file = file;
    chr_pos.line = 1;
    chr_pos.col = 1;
    n_ext_chr();
}

static void lex() {
    while (1) {
        tok_len = 0;
        tok_pos = chr_pos;
        if (chr == EOF) {
            tok = EOF;
        } else if (chr == ' ' || chr == '\t' || chr == '\n') {
            n_ext_chr();
            continue;
        } else if (chr >= '0' && chr <= '9') {
            while (chr >= '0' && chr <= '9')
                n_ext_chr();
            tok = TokNum;
        } else if (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_') {
            while (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_' || chr >= '0' && chr <= '9')
                n_ext_chr();
            tok = TokWrd;
        } else if (chr == '\'' || chr == '"') {
            int delim = chr;
            n_ext_chr();
            while (1) {
                if (chr == delim)
                    break;
                if (chr == EOF)
                    error_at(&chr_pos, "unterminated string/char literal");
                if (delim == '\'' && tok_len != 1)
                    error_at(&chr_pos, "too many characters in char literal");
                int decoded = chr;
                if (chr == '\\') {
                    n_ext_chr();
                    const char *escapes = "abfnrtv\\'\"?", *unescapes = "\a\b\f\n\r\t\v\\\'\"\?";
                    if (find_chr(escapes, chr))
                        decoded = unescapes[find_chr(escapes, chr) - escapes];
                }
                n_ext_chr();
                tok_str[tok_len - 1] = decoded;
            }
            if (delim == '\'' && tok_len == 0)
                error_at(&chr_pos, "empty char literal");
            n_ext_chr();
            tok = delim == '"' ? TokStr : TokChr;
        } else {
            tok = TokSym;
            int prev_chr = chr;
            n_ext_chr();
            if (prev_chr == '#' || prev_chr == '/' && chr == '/') {
                while (chr != '\n' && chr != EOF)
                    n_ext_chr();
                continue;
            } else if (find_chr("<>!=", prev_chr) && chr == '=')
                n_ext_chr();
            else if (find_chr("&|<>+-", prev_chr) && chr == prev_chr)
                n_ext_chr();
            else if (prev_chr == '-' && chr == '>')
                n_ext_chr();
        }
        break;
    }

    tok_str[tok_len] = 0;
    // diag_at(&tok_pos, "info");
    // write_f2(2, "lex: '%s' (tok=%d)\n", tok_str, &tok);
}

//=============================================================================
//= parse.dsl

static void parse_init() {
    lex();
}

static int str_to_int(const char *s) {
    int n = 0;
    while (*s)
        n = n * 10 + (*s++ - '0');
    return n;
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
    if (tok != TokWrd) {
        unexpected_expected("identifier");
    }
    const char *res = intern(tok_str, tok_len + 1);
    lex();
    return res;
}

static int p_num() {
    if (tok != TokNum) {
        unexpected_expected("number");
    }
    int res = str_to_int(tok_str);
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
    return at("void") || at("char") || at("int") || at("long") || at("struct") || at("enum") || at("const");
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
    } else if (tok == TokNum) {
        acc = new_expr(&pos, Expr_Num, 0);
        acc->int_val = p_num();
    } else if (tok == TokStr) {
        acc = new_expr(&pos, Expr_Str, 0);
        acc->str_val = intern(tok_str, tok_len + 1);
        lex();
    } else if (tok == TokChr) {
        acc = new_expr(&pos, Expr_Chr, 0);
        acc->int_val = tok_str[0];
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
    } else if (tok == TokWrd) {
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
            acc = p_bin_expr(acc, Expr_Subscript, 0);
            expect("]");
        } else if (rbp < Prec_Postfix && eat("(")) {
            struct expr *tmp = new_expr(&pos, Expr_Call, MAX_FUNC_PARAMS + 1);
            tmp->subs[0] = acc;
            int n_args = 0;
            struct expr **args = &tmp->subs[1];
            while (!eat(")")) {
                if (n_args >= MAX_FUNC_PARAMS)
                    error_at(&pos, "too many arguments in function call");
                if (n_args > 0)
                    expect(",");
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
    return eval(p_expr(Prec_Cond - 1));
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
        if (!at(";"))
            stmt->expr = p_expr(0);
        expect(";");

        if (is_void_type(curr_func->type->ret_type)) {
            if (stmt->expr)
                error_at(&pos, "returning a value from a void function");
        } else {
            if (!stmt->expr)
                error_at(&pos, "missing return value");
            stmt->expr = chk_expr(stmt->expr, curr_func->type->ret_type);
        }
        return stmt;
    } else if (eat("if")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_If);

        expect("(");
        stmt->expr = p_expr(0);
        stmt->expr = chk_cond_expr(stmt->expr);
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
        stmt->expr = chk_cond_expr(stmt->expr);
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
        expect(";");
        return stmt;
    } else if (eat("continue")) {
        if (!curr_loop)
            error_at(&pos, "continue statement outside loop");
        struct stmt *stmt = new_stmt(&pos, Stmt_Continue);
        expect(";");
        return stmt;
    } else if (eat(";")) {
        struct stmt *stmt = new_stmt(&pos, Stmt_Empty);
        // empty statement
        return stmt;
    } else if (at_decl()) {
        struct stmt *stmt = 0;
        p_decl(Decl_Local, &stmt);
        if (!stmt)
            stmt = new_stmt(&pos, Stmt_Empty);
        return stmt;
    } else {
        struct stmt *stmt = new_stmt(&pos, Stmt_Expr);
        stmt->expr = p_expr(0);
        elab_expr(stmt->expr);
        expect(";");
        return stmt;
    }
}

static struct type *p_struct() {
    struct pos name_pos = tok_pos;
    const char *name = 0;
    if (tok == TokWrd) {
        name = p_ident();
    }
    int is_def = at("{");

    struct sym *sym = 0;
    if (!is_def)
        sym = lookup(Ns_Struct, name);

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
        if (eat("="))
            val = p_const_expr();
        if (!at("}"))
            expect(",");
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
        } else if (!base_type && eat("long")) {
            base_type = long_type;
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
        if (scope != Decl_TypeName && tok == TokWrd) {
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
                if (n_params > 0)
                    expect(",");
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
            if (!is_allowed_func_param_type(type))
                error_at(&name_pos, "bad parameter type");
            param->type = type;
            param->name = name;
            define_var(&name_pos, name, type);
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
                curr_func = sym;
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
                if (init)
                    chk_expr(init, type);
                struct stmt *local = new_stmt(&name_pos, Stmt_Decl);
                local->sym = define_var(&name_pos, name, type);
                local->expr = init;
                if (!prev_local) {
                    prev_local = *(struct stmt **)ctx = local;
                } else {
                    prev_local = prev_local->sub = local;
                }
            } else if (scope == Decl_Global) {
                int linkage = storage_class ? (storage_class[0] == 's' ? Internal : External) : 0;
                if (init)
                    chk_expr(init, type);
                declare_global(&name_pos, linkage, name, type, !!init);
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

        // diag_at(&name_pos, "info");
        // write_f3(2, "declared %s '%s'\n", &scope, name ? name : "<anon>",
        // &type->kind);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        write_f1(2, "usage: %s <source-file>\n", argv[0]);
        return 1;
    }
    inp = open(argv[1], 0, 0);
    if (inp < 0) {
        write_f1(2, "error: cannot open file '%s'\n", argv[1]);
        return 1;
    }
    const char *file_name = argv[1];
    type_init();
    lex_init(file_name);
    parse_init();
    while (tok != EOF) {
        p_decl(Decl_Global, 0);
    }

    // list objects and functions
    struct scope *scope = &scopes[0];
    int i = 0;
    int n_funcs = 0, n_objs = 0;
    while (i < scope->n_syms) {
        struct sym *sym = scope->syms[i++];
        const char *linkage_str = sym->linkage == Internal ? "internal" : "external";
        if (sym->kind == Sym_Func) {
            if (sym->linkage != Internal && !sym->is_defined)
                continue;  // declaration, external linkage
            if (sym->linkage == Internal && !sym->is_defined)
                continue;  // declaration with internal linkage, missing definition
            write_f3(2, "function(name=%s, linkage=%s, n_params=%d)\n", sym->name, linkage_str, &sym->n_params);
            n_funcs++;
        } else if (sym->kind == Sym_Global) {
            if (sym->linkage == External && !sym->is_defined)
                continue;  // declaration, external linkage
            if (!sym->is_defined)
                ;  // tentative definition
            write_f3(2, "object(name=%s, linkage=%s, type=%s)\n", sym->name, linkage_str, type_str(sym->type));
            n_objs++;
        }
    }
    write_f2(2, "defined %d functions and %d objects\n", &n_funcs, &n_objs);

    write_f1(2, "allocated %d bytes from arena\n", &arena_len);
    write_f1(2, "allocated %d types\n", &n_types);
    write_f1(2, "allocated %d symbols\n", &n_syms);
    write_f1(2, "interned %d strings\n", &strings_len);

    return 0;
}
