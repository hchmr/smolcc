//# description: Test that a variadic function can receive up to 8 arguments.
//# mode: run
//# exit: 28

#include <stdarg.h>

int argsum(int n, ...) {
    va_list args;
    va_start(args, n);
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum = sum + va_arg(args, long);
    }
    va_end(args);
    return sum;
}

int main() {
    return argsum(7, 1L, 2L, 3L, 4L, 5L, 6L, 7L);
}
