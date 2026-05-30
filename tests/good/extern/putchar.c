//# mode: run
//# exit: 0
//# stdout: ABC

extern int putchar(int c);

int main() {
    putchar('A');
    putchar('B');
    putchar('C');
    putchar('\n');
    return 0;
}