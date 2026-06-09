//# mode: run

#include <stdarg.h>

static int variadic(int c, ...) {
    va_list ap;
    c ? va_start(ap, c) : va_start(ap, c);
    int x = c ? va_arg(ap, int) : va_arg(ap, int);
    c ? va_end(ap) : va_end(ap);
    return x;
}

int main() {
    return variadic(1, 0, 1, 2, 3);
}
