//# mode: run
//# exit: 0
//# description: Verify that string literals with embedded NUL characters are fully preserved and not truncated at the first NUL

int main() {
    char *lhs = "a\0b";
    char *rhs = "a\0c";
    return lhs[2] == 'b' && rhs[2] == 'c' ? 0 : 1;
}
