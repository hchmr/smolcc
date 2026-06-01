//# mode: run
//# exit: 0
//# stdout: visible

int puts(const char *);

int main() {
    puts("visible\n\0invisible\n");
    return 0;
}
