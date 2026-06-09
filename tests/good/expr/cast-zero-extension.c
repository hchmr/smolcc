//# mode: run
//# stdout: 000000ff
//# stdout: 00000000fffffc00

extern int printf(const char *fmt, ...);

int main() {
    char c = (char)-1;
    int i = -1024;

    unsigned int c_to_uint = (unsigned int)(unsigned char)c;
    unsigned long i_to_ulong = (unsigned long)(unsigned int)i;

    printf("%08x\n", c_to_uint);
    printf("%016lx\n", i_to_ulong);

    return 0;
}
