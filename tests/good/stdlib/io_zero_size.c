//# description: Verifies zero-byte fread/fwrite return immediately without triggering division by zero or similar errors.
//# mode: run
//# exit: 0

struct file;

extern struct file *stdin, *stdout;
extern int fread(void *ptr, int size, int count, struct file *stream);
extern int fwrite(const void *ptr, int size, int count, struct file *stream);

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
