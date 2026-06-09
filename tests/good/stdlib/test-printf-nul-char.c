//# description: Verifies %c writes a literal NUL byte, and counts it as a single character
//# mode: run
//# stdout: "\u0000"

extern int printf(const char *fmt, ...);

int main() {
    return printf("%c", 0) != 1;
}
