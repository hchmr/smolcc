//# stderr: tests/bad/varargs/va-end-non-valist.c:6:5: error: va_end operand must be va_list
#include <stdarg.h>

int finish(int first, ...) {
    int args;
    va_end(args);
    return 0;
}
