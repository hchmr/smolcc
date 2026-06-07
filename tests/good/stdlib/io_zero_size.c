//# description: Verifies zero-byte fread/fwrite return immediately without triggering division by zero or similar errors.
//# mode: run

struct file;

extern struct file *stdin, *stdout;
extern unsigned long fread(void *ptr, unsigned long size, unsigned long count, struct file *stream);
extern unsigned long fwrite(const void *ptr, unsigned long size, unsigned long count, struct file *stream);

int main() {
    char buf[4];

    if (fwrite("abc", 0, 3, stdout) != 0)
        return 1;
    if (fread(buf, 0, 3, stdin) != 0)
        return 2;
    if (fwrite("abc", 1, 0, stdout) != 0)
        return 3;
    if (fread(buf, 1, 0, stdin) != 0)
        return 4;

    return 0;
}
