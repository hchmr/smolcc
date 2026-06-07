#include <stdarg.h>

//==============================================================================
//= deps

enum { EOF = -1 };
extern struct file *stdout;
extern int fputc(int c, struct file *stream);
extern unsigned long fwrite(const void *ptr, unsigned long size, unsigned long count, struct file *stream);

extern unsigned long strlen(const char *s);
extern const char *strchr(const char *s, int c);
extern void *memset(void *s, long c, unsigned long n);

extern int isdigit(int c);

//==============================================================================
//= utils

enum {
    // declen(-2^63) = len(-9223372036854775808) = 20
    // octlen(2^64) = len(1000000000000000000000) = 22
    // hexlen(2^64) = len(ffffffffffffffff) = 16
    MAX_INT_LEN = 22
};

static void prepend(char **dst, char c) {
    *dst = *dst - 1;
    **dst = c;
}

static char *uint_to_dec(unsigned long n, char *buf, int buf_len) {
    char *p = buf + buf_len;

    while (1) {
        int d = n % 10;
        n = n / 10;
        prepend(&p, '0' + d);
        if (n == 0)
            break;
    }

    return p;
}

static char *int_to_dec(long n, char *buf, int buf_len) {
    if (n < 0) {
        char *num_str = uint_to_dec((unsigned long)(-(n + 1)) + 1, buf, buf_len);
        prepend(&num_str, '-');
        return num_str;
    } else {
        return uint_to_dec((unsigned long)n, buf, buf_len);
    }
}

static char *int_to_hex(unsigned long n, char *buf, int buf_len, int uppercase) {
    const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char *p = buf + buf_len;

    while (p > buf) {
        int d = (int)(n & 15);  // lower 4 bits
        n = n >> 4;
        prepend(&p, alphabet[d]);
        if (n == 0)
            break;
    }
    return p;
}

static char *int_to_oct(unsigned long n, char *buf, int buf_len) {
    char *p = buf + buf_len;

    while (p > buf) {
        int d = (int)(n & 7);  // lower 3 bits
        n = n >> 3;
        prepend(&p, (char)('0' + d));
        if (n == 0)
            break;
    }
    return p;
}

static const char *sscan_int(const char *s, unsigned int *out) {
    unsigned int n = 0;
    while (isdigit(*s)) {
        n = n * 10 + (*s - '0');
        s++;
    }
    *out = n;
    return s;
}

static unsigned long trunc(int size, unsigned long n) {
    unsigned long mask = ~0UL >> (64 - size * 8);
    return n & mask;
}

//==============================================================================
//= formatting

enum {
    FMT_INT = 1,
    FMT_UINT,
    FMT_LONG,
    FMT_ULONG,
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
    unsigned int min_width;

    // for FMT_LIT
    const char *lit;
    unsigned int lit_len;
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

        int int_fmt = FMT_INT;
        if (*fmt == 'l' && strchr("diouxXu", *(fmt + 1))) {
            fmt++;
            int_fmt = FMT_LONG;
        }

        if (*fmt == 'd' || *fmt == 'i') {
            spec->type = int_fmt;
            spec->base = 10;
        } else if (*fmt == 'u' || *fmt == 'U') {
            spec->type = int_fmt == FMT_LONG ? FMT_ULONG : FMT_UINT;
            spec->base = 10;
        } else if (*fmt == 'o') {
            spec->type = int_fmt;
            spec->base = 8;
        } else if (*fmt == 'x' || *fmt == 'X') {
            spec->type = int_fmt;
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
        unsigned int len = 0;
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

static int fmt_int(struct file *stream, struct fmt_spec *spec, unsigned long n) {
    const char *prefix = "";
    unsigned int prefix_len = 0;
    int is_signed = spec->type != FMT_UINT && spec->type != FMT_ULONG;
    if (spec->base == 10 && is_signed) {
        if ((long)n < 0) {
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

    int int_size = spec->type == FMT_INT ? sizeof(int) : sizeof(long);

    char num_buf[MAX_INT_LEN];
    char *num_str_end = num_buf + MAX_INT_LEN;
    char *num_str;
    if (spec->base == 10 && is_signed) {
        num_str = int_to_dec((long)n, num_buf, MAX_INT_LEN);
    } else if (spec->base == 10) {
        num_str = uint_to_dec(n, num_buf, MAX_INT_LEN);
    } else if (spec->base == 16) {
        num_str = int_to_hex(trunc(int_size, n), num_buf, MAX_INT_LEN, spec->flags & FMT_FLAGS_UPPER_HEX);
    } else if (spec->base == 8) {
        num_str = int_to_oct(trunc(int_size, n), num_buf, MAX_INT_LEN);
    } else {
        num_str = num_str_end;  // empty string
    }
    if (*num_str == '-') {
        num_str++;  // remove '-'
    }
    unsigned int num_len = num_str_end - num_str;

    unsigned int width = prefix_len + num_len;

    unsigned int pad_width = spec->min_width > width ? spec->min_width - width : 0;
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

static int fmt_lit(struct file *stream, const char *s, unsigned long len) {
    if (fwrite(s, 1, len, stream) < len)
        return -1;
    return len;
}

static int fmt_str(struct file *stream, struct fmt_spec *spec, const char *s) {
    if (s == 0) {
        s = "(null)";
    }
    unsigned long len = strlen(s);

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
    int pad_width = spec->min_width > 1 ? spec->min_width - 1 : 0;
    int pad_right = spec->flags & FMT_FLAGS_PAD_RIGHT;

    if (!pad_right) {
        if (fill(stream, ' ', pad_width) == -1)
            return -1;
    }
    if (fputc(c, stream) == EOF)
        return -1;
    if (pad_right) {
        if (fill(stream, ' ', pad_width) == -1)
            return -1;
    }

    return 1 + pad_width;
}

static int fmt_ptr(struct file *stream, void *p) {
    enum { PTR_SIZE = sizeof(void *), N_DIGITS = PTR_SIZE * 2, PTR_STR_LEN = 2 + N_DIGITS };

    if (p == 0) {
        return fmt_lit(stream, "(nil)", 5);
    }

    char buf[PTR_STR_LEN];
    char *num_str = int_to_hex((long)p, buf, PTR_STR_LEN, 0);
    if (num_str == 0) {
        return -1;
    }
    prepend(&num_str, 'x');
    prepend(&num_str, '0');
    return fmt_lit(stream, num_str, buf + PTR_STR_LEN - num_str);
}

// must use va_list* because va_list is an aggregate
int _vfprintf(struct file *stream, const char *fmt, va_list *ap) {
    int nw = 0;
    struct fmt_spec spec;
    while ((fmt = p_fmt(fmt, &spec)) != 0) {
        int nwp = 0;  // nw for this part of the format string
        if (spec.type == FMT_INT) {
            nwp = fmt_int(stream, &spec, (unsigned long)va_arg(*ap, int));
        } else if (spec.type == FMT_LONG) {
            nwp = fmt_int(stream, &spec, (unsigned long)va_arg(*ap, long));
        } else if (spec.type == FMT_UINT) {
            nwp = fmt_int(stream, &spec, va_arg(*ap, unsigned long));
        } else if (spec.type == FMT_ULONG) {
            nwp = fmt_int(stream, &spec, va_arg(*ap, unsigned long));
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
