#include <stdarg.h>

//==============================================================================
//= deps

enum { EOF = -1 };
extern struct file *stdout;
extern int fputc(int c, struct file *stream);
extern int fwrite(const void *ptr, int size, int count, struct file *stream);

extern int strlen(const char *s);
extern void *memset(void *s, int c, int n);

extern int isdigit(int c);

//==============================================================================
//= utils

enum {
    // declen(-2^31) = len(-2147483648) = 11
    // octlen(2^32) = len(4000000000) = 10
    // hexlen(2^32) = len(ffffffff) = 8
    MAX_INT_LEN = 12
};

static void prepend(char **dst, char c) {
    *dst = *dst - 1;
    **dst = c;
}

static char *int_to_dec(int n, char buf[MAX_INT_LEN]) {
    char *p = buf + MAX_INT_LEN;

    int neg, d;
    neg = n < 0;
    if (neg) {
        if (n == -2147483647 - 1) {
            d = -(n % -10);
            n = n / -10;
            prepend(&p, '0' + d);
        } else {
            n = -n;
        }
    }

    while (1) {
        d = n % 10;
        n = n / 10;
        prepend(&p, '0' + d);
        if (n == 0)
            break;
    }

    if (neg) {
        prepend(&p, '-');
    }
    return p;
}

static char *int_to_hex(int n, char buf[MAX_INT_LEN], int uppercase) {
    const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char *p = buf + MAX_INT_LEN;

    for (int i = 0; i < 8; i++) {
        int d = n & 15;
        n = (n >> 4) & 268435455; // logical right shift by 4
        prepend(&p, alphabet[d]);
        if (n == 0)
            break;
    }
    return p;
}

static char *int_to_oct(int n, char buf[MAX_INT_LEN]) {
    char *p = buf + MAX_INT_LEN;

    for (int i = 0; i < 11; i++) {
        int d = n & 7;
        n = (n >> 3) & 536870911; // logical right shift by 3
        prepend(&p, '0' + d);
        if (n == 0)
            break;
    }
    return p;
}

static const char *sscan_int(const char *s, int *out) {
    int n = 0;
    while (isdigit(*s)) {
        n = n * 10 + (*s - '0');
        s++;
    }
    *out = n;
    return s;
}

//==============================================================================
//= formatting

enum {
    FMT_INT = 1,
    FMT_STR,
    FMT_CHR,
    FMT_LIT,
    FMT_PTR,
    FMT_N,
};

enum {
    FMT_FLAGS_UPPER_HEX = 1,
    FMT_FLAGS_SPACE_SIGN = 2,
    FMT_FLAGS_PLUS_SIGN = 4,
    FMT_FLAGS_ALT_FORM = 8,
    FMT_FLAGS_ZEROPAD = 16,
    FMT_FLAGS_PAD_RIGHT = 32,
};

struct fmt_spec {
    int type;
    int flags;
    int base;
    int min_width;

    // for FMT_LIT
    const char *lit;
    int lit_len;
};

static const char *p_fmt(const char *fmt, struct fmt_spec *spec) {
    if (!*fmt)
        return 0;

    memset(spec, 0, sizeof(struct fmt_spec));
    if (*fmt == '%') {
        fmt++;
        // flags
        while (1) {
            if (*fmt == '-') {
                spec->flags = spec->flags | FMT_FLAGS_PAD_RIGHT;
            } else if (*fmt == '0') {
                spec->flags = spec->flags | FMT_FLAGS_ZEROPAD;
            } else if (*fmt == ' ') {
                spec->flags = spec->flags | FMT_FLAGS_SPACE_SIGN;
            } else if (*fmt == '+') {
                spec->flags = spec->flags | FMT_FLAGS_PLUS_SIGN;
            } else if (*fmt == '#') {
                spec->flags = spec->flags | FMT_FLAGS_ALT_FORM;
            } else {
                break;
            }
            fmt++;
        }
        if (isdigit(*fmt)) {
            fmt = sscan_int(fmt, &spec->min_width);
        }
        if (*fmt == 'd' || *fmt == 'i') {
            spec->type = FMT_INT;
            spec->base = 10;
        } else if (*fmt == 'o') {
            spec->type = FMT_INT;
            spec->base = 8;
        } else if (*fmt == 'x' || *fmt == 'X') {
            spec->type = FMT_INT;
            spec->base = 16;
            if (*fmt == 'X') {
                spec->flags = spec->flags | FMT_FLAGS_UPPER_HEX;
            }
        } else if (*fmt == 's') {
            spec->type = FMT_STR;
        } else if (*fmt == 'c') {
            spec->type = FMT_CHR;
        } else if (*fmt == 'p') {
            spec->type = FMT_PTR;
        } else if (*fmt == 'n') {
            spec->type = FMT_N;
        } else if (*fmt == '%') {
            spec->type = FMT_LIT;
            spec->lit = fmt;
            spec->lit_len = 1;
        } else {
            // undefined behavior
        }
        fmt++;
    } else {
        int len = 0;
        for (len = 0; fmt[len] && fmt[len] != '%'; len++)
            ;
        spec->type = FMT_LIT;
        spec->lit = fmt;
        spec->lit_len = len;
        fmt = fmt + len;
    }

    return fmt;
}

static int fill(struct file *stream, char c, int count) {
    for (int i = 0; i < count; i++) {
        if (fputc(c, stream) == EOF)
            return -1;
    }
    return count;
}

static int fmt_int(struct file *stream, struct fmt_spec *spec, int n) {
    const char *prefix = "";
    int prefix_len = 0;
    if (spec->base == 10) {
        if (n < 0) {
            prefix = "-";
            prefix_len = 1;
        } else if (spec->flags & FMT_FLAGS_PLUS_SIGN) {
            prefix = "+";
            prefix_len = 1;
        } else if (spec->flags & FMT_FLAGS_SPACE_SIGN) {
            prefix = " ";
            prefix_len = 1;
        }
    } else if (spec->base == 8 && n != 0 && (spec->flags & FMT_FLAGS_ALT_FORM)) {
        prefix = "0";
        prefix_len = 1;
    } else if (spec->base == 16 && n != 0 && (spec->flags & FMT_FLAGS_ALT_FORM)) {
        prefix = spec->flags & FMT_FLAGS_UPPER_HEX ? "0X" : "0x";
        prefix_len = 2;
    }

    char num_buf[MAX_INT_LEN];
    char *num_str_end = num_buf + MAX_INT_LEN;
    char *num_str;
    if (spec->base == 10) {
        num_str = int_to_dec(n, num_buf);
    } else if (spec->base == 16) {
        num_str = int_to_hex(n, num_buf, spec->flags & FMT_FLAGS_UPPER_HEX);
    } else if (spec->base == 8) {
        num_str = int_to_oct(n, num_buf);
    } else {
        num_str = num_str_end;  // empty string
    }
    if (*num_str == '-') {
        num_str++;  // remove '-'
    }
    int num_len = num_str_end - num_str;

    int width = prefix_len + num_len;

    int pad_width = spec->min_width > width ? spec->min_width - width : 0;
    int pad_right = spec->flags & FMT_FLAGS_PAD_RIGHT;
    int pad_chr = !pad_right && spec->flags & FMT_FLAGS_ZEROPAD ? '0' : ' ';

    if (!pad_right && pad_chr == ' ') {
        if (fill(stream, ' ', pad_width) == -1)
            return -1;
    }
    if (prefix_len > 0) {
        if (fwrite(prefix, 1, prefix_len, stream) < prefix_len)
            return -1;
    }
    if (!pad_right && pad_chr == '0') {
        if (fill(stream, '0', pad_width) == -1)
            return -1;
    }
    if (fwrite(num_str, 1, num_len, stream) < num_len)
        return -1;
    if (pad_right && pad_chr == ' ') {
        if (fill(stream, ' ', pad_width) == -1)
            return -1;
    }

    return width + pad_width;
}

static int fmt_lit(struct file *stream, const char *s, int len) {
    if (fwrite(s, 1, len, stream) < len)
        return -1;
    return len;
}

static int fmt_str(struct file *stream, struct fmt_spec *spec, const char *s) {
    if (s == 0) {
        s = "(null)";
    }
    int len = strlen(s);

    int pad_width = spec->min_width > len ? spec->min_width - len : 0;
    int pad_right = spec->flags & FMT_FLAGS_PAD_RIGHT;

    if (!pad_right) {
        if (fill(stream, ' ', pad_width) == -1)
            return -1;
    }
    if (fwrite(s, 1, len, stream) < len)
        return -1;
    if (pad_right) {
        if (fill(stream, ' ', pad_width) == -1)
            return -1;
    }

    return len + pad_width;
}

static int fmt_chr(struct file *stream, struct fmt_spec *spec, char c) {
    char s[2];
    s[0] = c;
    s[1] = 0;
    return fmt_str(stream, spec, s);
}

static int fmt_ptr(struct file *stream, void *p) {
    if (p == 0) {
        return fmt_lit(stream, "(nil)", 5);
    }

    char bytes[8];
    *(void **)bytes = p;

    struct fmt_spec spec;
    memset(&spec, 0, sizeof(struct fmt_spec));
    spec.type = FMT_INT;
    spec.base = 16;
    spec.flags = FMT_FLAGS_ZEROPAD;
    spec.min_width = 2;

    int nw = 0;
    if (fwrite("0x", 1, 2, stream) < 2)
        return -1;
    nw = 2;
    for (int i = 7; i >= 0; i--) {
        if (fmt_int(stream, &spec, bytes[i]) == -1)
            return -1;
        nw = nw + 2;
    }
    return nw;
}

// must use va_list* because va_list is an aggregate
int _vfprintf(struct file *stream, const char *fmt, va_list *ap) {
    int nw = 0;
    struct fmt_spec spec;
    while ((fmt = p_fmt(fmt, &spec)) != 0) {
        int nwp = 0;  // nw for this part of the format string
        if (spec.type == FMT_INT) {
            nwp = fmt_int(stream, &spec, va_arg(*ap, int));
        } else if (spec.type == FMT_STR) {
            nwp = fmt_str(stream, &spec, va_arg(*ap, char *));
        } else if (spec.type == FMT_CHR) {
            nwp = fmt_chr(stream, &spec, (char)va_arg(*ap, int));
        } else if (spec.type == FMT_LIT) {
            nwp = fmt_lit(stream, spec.lit, spec.lit_len);
        } else if (spec.type == FMT_PTR) {
            nwp = fmt_ptr(stream, va_arg(*ap, void *));
        } else if (spec.type == FMT_N) {
            *va_arg(*ap, int *) = nw;
        } else {
            // undefined behavior
        }
        if (nwp < 0) {
            nw = -1;
            break;
        }
        nw = nw + nwp;
    }

    return nw;
}

int fprintf(struct file *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int nw = _vfprintf(stream, fmt, &ap);
    va_end(ap);
    return nw;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int nw = _vfprintf(stdout, fmt, &ap);
    va_end(ap);
    return nw;
}
