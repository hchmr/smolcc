//# stderr: tests/bad/varargs/va-start-outside-variadic.c:6:5: error: va_start outside variadic function
#include <stdarg.h>

int main() {
    va_list args;
    va_start(args, args);
    return 0;
}
