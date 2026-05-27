//# exit: 1
//# stderr: tests/bad/varargs/va-arg-non-scalar-type.c:12:5: error: va_arg second operand must be scalar
#include <stdarg.h>

struct Pair {
    int value;
};

int read_pair(int first, ...) {
    va_list args;
    va_start(args, first);
    va_arg(args, struct Pair);
    va_end(args);
    return 0;
}
