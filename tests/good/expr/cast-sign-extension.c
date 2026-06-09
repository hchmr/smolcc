//# mode: run
//# stdout: ffffffff
//# stdout: fffffffffffffc00

extern int printf(const char *fmt, ...);

int main() {
    // NB! char is signed
    char c = (char)-1;  // 0xff
    int i = -1024;  // 0xfffffc00

    int c_to_int = (int)c;
    long i_to_long = (long)i;

    printf("%0x\n", c_to_int);
    printf("%016lx\n", i_to_long);

    return 0;
}
