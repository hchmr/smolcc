//# exit: 1
//# stderr: tests/bad/varargs/va-start-non-valist.c:7:5: error: va_start operand must be of type va_list
#include <stdarg.h>

int begin(int first, ...) {
    int args;
    va_start(args, first);
    return 0;
}
