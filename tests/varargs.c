//# mode: run
//# stdout: sum3(10, 20, 12) = 42
//# stdout: pick_middle(5, 1, 2, 3, 4, 5) = 2
//# stdout: pick_middle(4, 1, 2, 3, 4) = 2
//# exit: 0

#include <stdarg.h>
extern int printf(const char *format, ...);

int sum3(int first, ...) {
    va_list args;
    va_start(args, first);
    int second = va_arg(args, int);
    int third = va_arg(args, int);
    va_end(args);
    return first + second + third;
}

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
    int x = 10, y = 20, z = 12;
    int sum = sum3(x, y, z);
    printf("sum3(%d, %d, %d) = %d\n", x, y, z, sum);
    printf("pick_middle(%d, %d, %d, %d, %d, %d) = %d\n", 5, 1, 2, 3, 4, 5, pick_middle(5, 1, 2, 3, 4, 5));
    printf("pick_middle(%d, %d, %d, %d, %d) = %d\n", 4, 1, 2, 3, 4, pick_middle(4, 1, 2, 3, 4));
    return 0;
}