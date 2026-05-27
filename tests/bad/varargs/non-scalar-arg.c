//# exit: 1
//# stderr: tests/bad/varargs/non-scalar-arg.c:13:20: error: variadic arguments must be scalar
#include <stdarg.h>

int sink(int first, ...);

struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return sink(0, pair);
}
