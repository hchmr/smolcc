//# description: Verifies that zero-length fread calls return immediately
//# mode: run

extern struct file *stdin;
extern unsigned long fread(void *ptr, unsigned long size, unsigned long count, struct file *stream);

int main() {
    char buf[4];

    if (fread(buf, 0, 3, stdin) != 0)
        return 1;
    if (fread(buf, 1, 0, stdin) != 0)
        return 2;

    return 0;
}
