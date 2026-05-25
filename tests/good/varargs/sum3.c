//# mode: run
//# exit: 42

#include <stdarg.h>

int sum3(int first, ...) {
    va_list args;
    va_start(args, first);
    int second = va_arg(args, int);
    int third = va_arg(args, int);
    va_end(args);
    return first + second + third;
}

int main() {
    return sum3(10, 20, 12);
}
