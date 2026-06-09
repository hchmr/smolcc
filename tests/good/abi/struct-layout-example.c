//# mode: run
//# stdout: offset  size  field/padding
//# stdout:      0     1  x1
//# stdout:      1     3  padding
//# stdout:      4     4  x2
//# stdout:      8     8  x3
//# stdout:     16     2  x4
//# stdout:     18     6  padding
//# stdout:     24     0  end of struct

extern void printf(const char *fmt, ...);

struct S {
    char x1;
    int x2;
    long x3;
    struct {
        char a[2];
    } x4;
};

void print_header() {
    printf("%6s  %4s  %s\n", "offset", "size", "field/padding");
}

void print_row(const char *name, long offset, long size, long padding) {
    printf("%6d  %4d  %s\n", offset, size, name);
    if (padding > 0) {
        printf("%6d  %4d  %s\n", offset + size, padding, "padding");
    }
}

int main() {
    struct S s;

    long sizeof_x1 = sizeof(char);
    long sizeof_x2 = sizeof(int);
    long sizeof_x3 = sizeof(long);
    long sizeof_x4 = sizeof(char[2]);

    long offsetof_x1 = (char *)&s.x1 - (char *)&s;
    long offsetof_x2 = (char *)&s.x2 - (char *)&s;
    long offsetof_x3 = (char *)&s.x3 - (char *)&s;
    long offsetof_x4 = (char *)&s.x4 - (char *)&s;

    long padding_x1 = offsetof_x2 - (offsetof_x1 + sizeof_x1);
    long padding_x2 = offsetof_x3 - (offsetof_x2 + sizeof_x2);
    long padding_x3 = offsetof_x4 - (offsetof_x3 + sizeof_x3);
    long padding_x4 = sizeof(struct S) - (offsetof_x4 + sizeof_x4);

    print_header();
    print_row("x1", offsetof_x1, sizeof_x1, padding_x1);
    print_row("x2", offsetof_x2, sizeof_x2, padding_x2);
    print_row("x3", offsetof_x3, sizeof_x3, padding_x3);
    print_row("x4", offsetof_x4, sizeof_x4, padding_x4);
    print_row("end of struct", sizeof(struct S), 0, 0);

    return 0;
}
