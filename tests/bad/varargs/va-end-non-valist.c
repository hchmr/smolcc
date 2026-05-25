//# exit: 1
//# stderr: tests/bad/varargs/va-end-non-valist.c:7:5: error: va_end operand must be of type va_list
#include <stdarg.h>

int finish(int first, ...) {
    int args;
    va_end(args);
    return 0;
}
