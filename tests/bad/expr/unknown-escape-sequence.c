//# mode: compile-only
//# stderr: tests/bad/expr/unknown-escape-sequence.c:8:14: error: unknown escape sequence

int printf(const char *format, ...);

int main() {
    // hexadecimal escape sequences are not supported yet
    printf("\x1b[31mred\x1b[0m\n");
}
