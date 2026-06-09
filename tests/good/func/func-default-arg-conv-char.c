#include <stdarg.h>

unsigned long get_va_arg_bits(int ignored, ...) {
    va_list args;
    va_start(args, ignored);
    unsigned long result = va_arg(args, unsigned long);
    va_end(args);
    return result;
}

int main() {
    unsigned long X00000000ffffffff = 4294967295UL;

    char c = (char)-1;
    unsigned long bits = get_va_arg_bits(0, c);

    // Only succeeds if the char value is promoted to an int before being passed in X1
    return bits == X00000000ffffffff ? 0 : 1;
}
