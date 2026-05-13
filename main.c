//============================================================================
//= unistd

extern int open(const char *pathname, int flags, int mode);
extern int read(int fd, void *buf, int count);
extern int write(int fd, const void *buf, int count);
extern void _exit(int status);

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

static const char *find_chr(const char *s, int c) {
    while (*s)
        if (c == *s++)
            return s;
    return 0;
}

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
            if (*fmt == 'd') {
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
//= dynamic memory allocation

enum { Data_Size = 1048576 };  // 1 MiB
static char data[Data_Size];
static int data_len;

static void *alloc(int len) {
    if (data_len + len >= Data_Size) {
        die("alloc", "out of memory");
    }
    char *res = data + data_len;
    data_len = data_len + len;
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
    Sym_Var,
    Sym_Func,
    Sym_Struct,
    Sym_Const,
};

enum {
    Ns_Struct = 1,
};

struct field {
    const char *name;
    struct type *type;
    int offset;
};

struct func_params {
    const char *names[MAX_FUNC_PARAMS];
    struct type *types[MAX_FUNC_PARAMS];
    int count;
};

static struct sym {
    int kind;
    int ns;
    const char *name;
    struct type *type;
    int is_defined;

    struct pos *last_pos;

    // case Sym_Const:
    int val;
    // case Sym_Func:
    struct tree *body;
    // case Sym_Struct:
    int nsyms;
    struct field field_names[MAX_STRUCT_FIELDS];
} syms[MAX_SYMS];
static int nsyms;

static int sym_ns(struct sym *sym) {
    if (sym->kind == Sym_Struct)
        return Ns_Struct;
    return 0;
}

static struct sym *alloc_sym() {
    if (nsyms >= MAX_SYMS) {
        write_f1(2, "error: too many symbols\n", 0);
        _exit(1);
    }
    return &syms[nsyms++];
}

static void add_struct_field(struct sym *sym, const char *name, struct type *type) {
    if (sym->nsyms >= 16) {
        write_f1(2, "error: too many fields in struct\n", 0);
        _exit(1);
    }
    struct field *field = &sym->field_names[sym->nsyms++];
    field->name = name;
    field->type = type;
}

//=============================================================================
//= types

enum {
    Type_Int,
    Type_Char,
    Type_Void,
    Type_Long,
    Type_Ptr,
    Type_Array,
    Type_Func,
    Type_Struct,
};

enum {
    MAX_TYPES = 256,
};

static struct type {
    int kind;
    // for pointers
    struct type *ptr_to;
    // for arrays
    int array_len;
    // for functions
    struct type *ret_type;
    struct type *param_types[8];
    int nparams;
    // for structs
    struct sym *sym;
} types[MAX_TYPES];
static int ntypes;

static struct type *void_type;
static struct type *char_type;
static struct type *int_type;
static struct type *long_type;
static struct type *ptr_to_void;
static struct type *ptr_to_char;
static struct type *ptr_to_int;

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
        if (a->nparams != b->nparams)
            return 0;
        int i = 0;
        while (i < a->nparams) {
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

static struct type *intern_type(struct type ty) {
    int i = 0;
    while (i < ntypes) {
        if (type_eq(&ty, &types[i]))
            return &types[i];
        i++;
    }
    if (ntypes >= MAX_TYPES) {
        write_f1(2, "error: too many types\n", 0);
        _exit(1);
    }
    types[ntypes] = ty;
    return &types[ntypes++];
}

static struct type *new_ptr_type(struct type *base) {
    struct type ty;
    ty.kind = Type_Ptr;
    ty.ptr_to = base;
    return intern_type(ty);
}

static struct type *new_array_type(struct type *base, int len) {
    struct type ty;
    ty.kind = Type_Array;
    ty.ptr_to = base;
    ty.array_len = len;
    return intern_type(ty);
}

static struct type *new_struct_type(struct sym *sym) {
    struct type ty;
    ty.kind = Type_Struct;
    ty.sym = sym;
    return intern_type(ty);
}

static struct type *new_func_type(struct type *ret_type, struct type **param_types, int nparams) {
    struct type ty;
    ty.kind = Type_Func;
    ty.ret_type = ret_type;
    int i = 0;
    while (i < nparams) {
        ty.param_types[i] = param_types[i];
        i++;
    }
    ty.nparams = nparams;
    return intern_type(ty);
}

static int align_up(int n, int align) {
    return (n + align - 1) / align * align;
}
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
        int offset = 0;
        int max_align = 0;
        int i = 0;
        while (i < sym->nsyms) {
            struct field *field = &sym->field_names[i];
            int field_size, field_align;
            type_layout(field->type, &field_size, &field_align);
            offset = align_up(offset, field_align);
            field->offset = offset;
            offset = offset + field_size;
            if (field_align > max_align)
                max_align = field_align;
            i++;
        }
        *size = align_up(offset, max_align);
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

static void type_init() {
    void_type = &types[ntypes++];
    void_type->kind = Type_Void;
    char_type = &types[ntypes++];
    char_type->kind = Type_Char;
    int_type = &types[ntypes++];
    int_type->kind = Type_Int;
    long_type = &types[ntypes++];
    long_type->kind = Type_Long;
    ptr_to_void = new_ptr_type(void_type);
    ptr_to_char = new_ptr_type(char_type);
    ptr_to_int = new_ptr_type(int_type);
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
    int nsyms;
} scopes[MAX_SCOPES];
static struct scope *scope = scopes;

static void enter_scope(struct pos *pos) {
    if (scope - scopes >= MAX_SCOPES) {
        error_at(pos, "too many nested scopes");
    }
    scope++;
    scope->nsyms = 0;
}

static void leave_scope() {
    if (scope <= scopes) {
        die("leave_scope", "scope underflow");
    }
    scope--;
}

static struct sym *add_sym(struct pos *pos, int kind, const char *name) {
    struct sym *sym = alloc_sym();
    sym->kind = kind;
    sym->name = name;
    sym->last_pos = pos;
    if (scope->nsyms >= MAX_SYMS_PER_SCOPE) {
        error_at(pos, "symbol limit reached in this scope");
    }
    scope->syms[scope->nsyms++] = sym;
    return sym;
}

static struct sym *lookup_in(struct scope *scope, int ns, const char *name) {
    if (!name)
        return 0;
    int i = 0;
    while (i < scope->nsyms) {
        struct sym *sym = scope->syms[i];
        if (sym_ns(sym) == ns && sym->name == name)
            return sym;
        i++;
    }
    return 0;
}

static struct sym *lookup(int ns, const char *name) {
    struct scope *s = scope;
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
    write_f3(2, " Previous declaration at %s:%d:%d\n", sym->last_pos->file, &sym->last_pos->line, &sym->last_pos->col);
    _exit(1);
}

static struct sym *declare_struct(struct pos *pos, const char *name, int is_def) {
    struct sym *sym = lookup_in(scope, Ns_Struct, name);
    if (sym) {
        if (sym->kind != Sym_Struct) {
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        }
        if (is_def && sym->is_defined) {
            decl_conflict(pos, sym, "already defined");
        }
        sym->is_defined = sym->is_defined | is_def;
        return sym;
    }
    sym = add_sym(pos, Sym_Struct, name);
    sym->type = new_struct_type(sym);
    sym->is_defined = is_def;
    return sym;
}

static struct sym *define_const(struct pos *pos, const char *name, int val) {
    struct sym *sym = lookup_in(scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Const) {
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        }
        decl_conflict(pos, sym, "already defined");
    }
    sym = add_sym(pos, Sym_Const, name);
    sym->type = int_type;
    sym->val = val;
    return sym;
}

static struct sym *declare_var(struct pos *pos, const char *name, struct type *type, int is_def) {
    struct sym *sym = lookup_in(scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Var) {
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        }
        if (is_def && sym->is_defined) {
            decl_conflict(pos, sym, "already defined");
        }
        decl_conflict(pos, sym, "already declared");
    }
    sym = add_sym(pos, Sym_Var, name);
    sym->type = type;
    sym->is_defined = is_def;
    return sym;
}

struct sym *declare_func(struct pos *pos, const char *name, struct type *ret_type, struct func_params *params,
                         int is_def) {
    struct type *type = new_func_type(ret_type, &*params->types, params->count);
    struct sym *sym = lookup_in(scope, 0, name);
    if (sym) {
        if (sym->kind != Sym_Func) {
            decl_conflict(pos, sym, "already declared as a different kind of symbol");
        }
        if (!type_eq(sym->type, type)) {
            decl_conflict(pos, sym, "already declared with a different signature");
        }
        if (is_def && sym->is_defined) {
            decl_conflict(pos, sym, "already defined");
        }
        sym->is_defined = sym->is_defined | is_def;
        return sym;
    }
    sym = add_sym(pos, Sym_Func, name);
    sym->type = type;
    sym->is_defined = is_def;
    return sym;
}

//=============================================================================
//= ast

struct tree {
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
        Stmt_Expr,
        Stmt_If,
        Stmt_While,
        Stmt_Return,
        Stmt_Break,
        Stmt_Continue,
        Stmt_Block,
        Stmt_Empty,
        Stmt_Decl,
    } kind;
    struct pos pos;
    struct type *type;

    int int_val;
    const char *str_val;
    struct tree *sub;

    struct tree *next;
};

static struct tree *new_expr(struct pos *pos, int kind) {
    struct tree *expr = alloc(sizeof(struct tree));
    expr->pos = *pos;
    expr->kind = kind;
    return expr;
}

static struct tree *new_unary_expr(struct pos *pos, int kind, struct tree *sub) {
    struct tree *expr = new_expr(pos, kind);
    expr->sub = sub;
    return expr;
}

static struct tree *new_bin_expr(int kind, struct tree *lhs, struct tree *rhs) {
    struct tree *expr = new_expr(&lhs->pos, kind);
    expr->sub = lhs;
    lhs->next = rhs;
    return expr;
}

//=============================================================================
//= eval

static int eval(struct tree *expr) {
    if (expr->kind == Expr_Num) {
        return expr->int_val;
    } else if (expr->kind == Expr_Ident) {
        struct sym *sym = lookup(0, expr->str_val);
        if (!sym)
            error_at(&expr->pos, "undefined symbol");
        if (sym->kind != Sym_Const)
            error_at(&expr->pos, "symbol is not a constant");
        return sym->val;
    } else if (expr->kind == Expr_Neg) {
        return -eval(expr->sub);
    } else if (expr->kind == Expr_Add) {
        return eval(expr->sub) + eval(expr->sub->next);
    } else if (expr->kind == Expr_Sub) {
        return eval(expr->sub) - eval(expr->sub->next);
    } else if (expr->kind == Expr_Mul) {
        return eval(expr->sub) * eval(expr->sub->next);
    } else if (expr->kind == Expr_Shl) {
        return eval(expr->sub) << eval(expr->sub->next);
    } else if (expr->kind == Expr_Shr) {
        return eval(expr->sub) >> eval(expr->sub->next);
    } else {
        error_at(&expr->pos, "expression cannot be evaluated at compile time");
        return 0;
    }
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

static void next_chr() {
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
            while (chr >= '0' && chr <= '9')
                next_chr();
            tok = TokNum;
        } else if (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_') {
            while (chr >= 'a' && chr <= 'z' || chr >= 'A' && chr <= 'Z' || chr == '_' || chr >= '0' && chr <= '9')
                next_chr();
            tok = TokWrd;
        } else if (chr == '\'' || chr == '"') {
            int delim = chr;
            next_chr();
            while (1) {
                if (chr == delim)
                    break;
                if (chr == EOF)
                    error_at(&chr_pos, "unterminated string/char literal");
                if (delim == '\'' && tok_len != 1)
                    error_at(&chr_pos, "too many characters in char literal");
                if (chr == '\\') {
                    next_chr();
                    const char *escapes = "abfnrtv\\'\"?";
                    const char *unescapes = "\a\b\f\n\r\t\v\\\'\"\?";
                    if (find_chr(escapes, chr))
                        chr = unescapes[find_chr(escapes, chr) - escapes];
                }
                next_chr();
            }
            if (delim == '\'' && tok_len == 0)
                error_at(&chr_pos, "empty char literal");
            next_chr();
            tok = delim == '"' ? TokStr : TokChr;
        } else {
            tok = TokSym;
            int prev_chr = chr;
            next_chr();
            if (prev_chr == '#' || prev_chr == '/' && chr == '/') {
                while (chr != '\n' && chr != EOF)
                    next_chr();
                continue;
            } else if (find_chr("<>!=", prev_chr) && chr == '=')
                next_chr();
            else if (find_chr("&|<>+-", prev_chr) && chr == prev_chr)
                next_chr();
            else if (prev_chr == '-' && chr == '>')
                next_chr();
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

static struct tree *p_expr(int rbp);
static int p_const_expr();

static struct tree *p_unary_expr(struct pos *pos, int kind) {
    struct tree *res = p_expr(Prec_Unary);
    res = new_unary_expr(pos, kind, res);
    return res;
}

static struct tree *p_bin_expr(struct tree *lhs, int kind, int rbp) {
    struct tree *rhs = p_expr(rbp);
    return new_bin_expr(kind, lhs, rhs);
}

static struct tree *p_expr(int rbp) {
    struct pos pos = tok_pos;
    struct tree *acc;
    if (eat("(")) {
        if (at_decl()) {
            struct type *type = p_typename();
            expect(")");
            struct tree *tmp = p_expr(Prec_Unary);
            acc = new_expr(&pos, Expr_Cast);
            acc->type = type;
            acc->sub = tmp;
        } else {
            acc = p_expr(0);
            expect(")");
        }
    } else if (eat("-")) {
        acc = p_unary_expr(&pos, Expr_Neg);
    } else if (eat("!")) {
        acc = p_unary_expr(&pos, Expr_Not);
    } else if (tok == TokNum) {
        acc = new_expr(&pos, Expr_Num);
        acc->int_val = p_num();
    } else if (tok == TokStr) {
        acc = new_expr(&pos, Expr_Str);
        acc->str_val = intern(tok_str, tok_len + 1);
        lex();
    } else if (tok == TokChr) {
        acc = new_expr(&pos, Expr_Chr);
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
        acc = new_expr(&pos, Expr_Num);
        acc->int_val = type_size(type);
    } else if (tok == TokWrd) {
        acc = new_expr(&pos, Expr_Ident);
        acc->str_val = p_ident();
        struct sym *sym = lookup(0, acc->str_val);
        if (!sym) {
            error_at(&pos, "undefined symbol");
        }
    } else {
        unexpected_expected("expression");
    }

    // led
    while (1) {
        if (rbp < Prec_Assign && eat("=")) {
            acc = p_bin_expr(acc, Expr_Assign, Prec_Assign - 1);
        } else if (rbp < Prec_Cond && eat("?")) {
            struct tree *sub = acc;
            sub->next = p_expr(0);
            expect(":");
            sub->next->next = p_expr(Prec_Cond - 1);
            acc = new_expr(&pos, Expr_Cond);
            acc->sub = sub;
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
            struct tree *head = acc;
            int argc = 0;
            while (!eat(")")) {
                if (argc > 0)
                    expect(",");
                acc = acc->next = p_expr(0);
                argc++;
            }
            acc = new_expr(&pos, Expr_Call);
            acc->sub = head;
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

static struct tree *p_stmt() {
    struct pos pos = tok_pos;
    if (eat("{")) {
        struct tree *stmt = new_expr(&pos, Stmt_Block);
        if (eat("}"))
            return stmt;
        enter_scope(&pos);
        stmt->sub = p_stmt();
        struct tree *tail = stmt->sub;
        while (!eat("}")) {
            tail->next = p_stmt();
            tail = tail->next;
        }
        leave_scope();
        return stmt;
    } else if (eat("return")) {
        struct tree *stmt = new_expr(&pos, Stmt_Return);
        if (!at(";"))
            stmt->sub = p_expr(0);
        expect(";");
        return stmt;
    } else if (eat("if")) {
        struct tree *stmt = new_expr(&pos, Stmt_If);
        expect("(");
        stmt->sub = p_expr(0);
        expect(")");
        stmt->next = p_stmt();
        if (eat("else"))
            stmt->next->next = p_stmt();
        return stmt;
    } else if (eat("while")) {
        struct tree *stmt = new_expr(&pos, Stmt_While);
        expect("(");
        stmt->sub = p_expr(0);
        expect(")");
        stmt->next = p_stmt();
        return stmt;
    } else if (eat("break")) {
        struct tree *stmt = new_expr(&pos, Stmt_Break);
        expect(";");
        return stmt;
    } else if (eat("continue")) {
        struct tree *stmt = new_expr(&pos, Stmt_Continue);
        expect(";");
        return stmt;
    } else if (eat(";")) {
        struct tree *stmt = new_expr(&pos, Stmt_Empty);
        // empty statement
        return stmt;
    } else if (at_decl()) {
        struct tree *stmt = new_expr(&pos, Stmt_Decl);
        p_decl(Decl_Local, stmt);
        return stmt;
    } else {
        struct tree *expr = p_expr(0);
        expect(";");
        return expr;
    }
}

static void p_decl(int scope, void *ctx) {
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
            struct pos name_pos = tok_pos;
            const char *name = 0;
            if (tok == TokWrd) {
                name = p_ident();
            }
            int is_def = at("{");
            struct sym *sym = declare_struct(&name_pos, name, is_def);
            if (eat("{")) {
                while (!eat("}")) {
                    p_decl(Decl_Struct, sym);
                }
            }
            base_type = new_struct_type(sym);
        } else if (!base_type && eat("enum")) {
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
            base_type = int_type;
        } else {
            break;
        }
    }
    if (!base_type)
        unexpected_expected("type specifier");
    if (storage_class && scope != Decl_Global && scope != Decl_Local)
        error_at(&pos, "storage class specifier is not allowed here");

    int n_declarators = 0;
    while (1) {
        struct type *type = base_type;
        const char *name = 0;
        int has_params = 0;
        struct func_params params;
        int has_func_body = 0;
        struct tree *init = 0;

        while (eat("*")) {
            type = new_ptr_type(type);
        }

        struct pos name_pos = tok_pos;
        if (scope != Decl_TypeName && tok == TokWrd) {
            name = p_ident();
        }

        // simplified grammar: either a function, array or object definition
        if (eat("(") && (scope == Decl_Global || scope == Decl_Local)) {
            has_params = 1;
            params.count = 0;
            while (!eat(")")) {
                if (params.count >= 8) {
                    error_at(&tok_pos, "too many parameters in function declaration");
                }
                if (params.count > 0)
                    expect(",");
                p_decl(Decl_Param, &params);
            }
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
            struct func_params *params = (struct func_params *)ctx;
            if (params->count >= MAX_FUNC_PARAMS) {
                error_at(&name_pos, "too many parameters in function declaration");
            }
            params->names[params->count] = name;
            params->types[params->count] = type;
            params->count++;
            return;  // max one parameter per declaration
        } else if (!name) {
            // declaration does not declare a function or object
        } else if (has_params) {
            // function declaration
            if (scope != Decl_Global && scope != Decl_Local) {
                error_at(&name_pos, "function declaration is not allowed here");
            }
            has_func_body = scope == Decl_Global && n_declarators == 0 && at("{");
            struct sym *sym = declare_func(&name_pos, name, type, &params, has_func_body);
            if (has_func_body) {
                enter_scope(&tok_pos);
                int i = 0;
                while (i < params.count) {
                    declare_var(&name_pos, params.names[i], params.types[i], 1);
                    i++;
                }
                sym->body = p_stmt();
                leave_scope();
                return;  // max one function definition per declaration
            }
        } else {
            // object declaration
            if (scope == Decl_Local) {
                declare_var(&name_pos, name, type, 1);
                struct tree *decl = (struct tree *)ctx;
                decl->type = type;
                decl->str_val = name;
                decl->sub = init;
            } else if (scope == Decl_Global) {
                declare_var(&name_pos, name, type, 1);
            } else if (scope == Decl_Struct) {
                struct sym *sym = (struct sym *)ctx;
                if (sym->nsyms >= MAX_STRUCT_FIELDS) {
                    error_at(&name_pos, "too many fields in struct");
                }
                struct field *field = &sym->field_names[sym->nsyms++];
                field->name = name;
                field->type = type;
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
        // write_f3(2, "declared %s '%s'\n", &scope, name ? name : "<anon>", &type->kind);
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

    int i = 0;
    while (i < nsyms) {
        struct sym *sym = &syms[i];
        write_f4(2, "sym %d: kind=%d name='%s' type_kind=%d\n", &i, &sym->kind, sym->name, &sym->type->kind);
        i++;
    }

    write_f1(2, "success\n", 0);
    write_f1(2, "allocated %d bytes from data arena\n", &data_len);
    write_f1(2, "allocated %d types\n", &ntypes);
    write_f1(2, "allocated %d symbols\n", &nsyms);
    write_f1(2, "interned %d strings\n", &strings_len);

    return 0;
}
