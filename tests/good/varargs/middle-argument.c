//# description: variadic integer arguments are read as int and the middle one is kept in a char local.
//# mode: run
//# exit: 2

#include <stdarg.h>

int pick_middle(int count, ...) {
    va_list args;
    va_start(args, count);
    char middle = 0;
    for (int i = 1; i * 2 <= count; i++) {
        middle = va_arg(args, int);
    }
    va_end(args);
    return middle;
}

int main() {
    return pick_middle(5, 1, 2, 3, 4, 5);
}
