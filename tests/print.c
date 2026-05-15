extern void putchar(int c);

void print_int(int n) {
    if (n < 0) {
        putchar('-');
        n = -n;
    }
    if (n >= 10)
        print_int(n / 10);
    putchar(n % 10 + '0');
}

void print_hex(int n) {
    if (n >= 16)
        print_hex(n / 16);
    putchar("0123456789abcdef"[n % 16]);
}

void print_str(const char *s) {
    while (*s)
        putchar(*s++);
}

// poor man's printf implementation without varargs
void print_fmt(const char *fmt, const void *x1, const void *x2, const void *x3, const void *x4) {
    const void *args[4];
    args[0] = x1;
    args[1] = x2;
    args[2] = x3;
    args[3] = x4;
    const void **it = args;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                print_int(**(int **)(it++));
            } else if (*fmt == 'x') {
                print_hex(**(int **)(it++));
            } else if (*fmt == 'c') {
                putchar(**(char **)(it++));
            } else if (*fmt == 's') {
                const char *s = *(char **)(it++);
                print_str(s ? s : "(null)");
            } else if (*fmt == '%') {
                putchar('%');
            } else {
                putchar('%');
                putchar(*fmt);
            }
        } else {
            putchar(*fmt);
        }
        fmt++;
    }
}

int main() {
    int x = 42;
    int y = 255;
    char c = 'A';
    const char *s = "Hello";
    print_fmt("x = %d, y = 0x%x, c = '%c', s = \"%s\", %% = %%, %? = %?\n", &x, &y, &c, s);
    return 0;
}
