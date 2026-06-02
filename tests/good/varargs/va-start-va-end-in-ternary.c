//# mode: run
//# exit: 6

#include <stdarg.h>

static int sum_args(int flag, int n, ...) {
    va_list ap;
    flag ? va_start(ap, n) : va_start(ap, n);
    int a = va_arg(ap, int);
    int b = va_arg(ap, int);
    int c = va_arg(ap, int);
    flag ? va_end(ap) : va_end(ap);
    return a + b + c;
}

int main() {
    return sum_args(1, 0, 1, 2, 3);
}
