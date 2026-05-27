//# exit: 1
//# stderr: tests/bad/varargs/va-start-not-last-param.c:7:20: error: va_start second operand must be parameter name
#include <stdarg.h>

int begin(int first, int second, ...) {
    va_list args;
    va_start(args, first);
    return 0;
}
