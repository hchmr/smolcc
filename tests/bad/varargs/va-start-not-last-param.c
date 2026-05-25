//# exit: 1
//# stderr: tests/bad/varargs/va-start-not-last-param.c:7:20: error: second operand of va_start must be a parameter name
#include <stdarg.h>

int begin(int first, int second, ...) {
    va_list args;
    va_start(args, first);
    return 0;
}
