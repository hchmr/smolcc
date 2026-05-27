//# exit: 1
//# stderr: tests/bad/varargs/va-arg-non-valist.c:7:12: error: va_arg first operand must be va_list
#include <stdarg.h>

int read_value(int first, ...) {
    int args;
    return va_arg(args, int);
}
