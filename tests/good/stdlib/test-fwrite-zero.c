//# description: Verifies that zero-length fwrite calls return immediately
//# mode: run

extern struct file *stdout;
extern unsigned long fwrite(const void *ptr, unsigned long size, unsigned long count, struct file *stream);

int main() {
    if (fwrite("abc", 0, 3, stdout) != 0)
        return 1;
    if (fwrite("abc", 1, 0, stdout) != 0)
        return 2;

    return 0;
}
