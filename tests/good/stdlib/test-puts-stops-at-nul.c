//# mode: run
//# stdout: visible

int puts(const char *);

int main() {
    puts("visible\0invisible");
    return 0;
}
