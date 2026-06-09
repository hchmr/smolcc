//# mode: run
//# stderr: fopen: No such file or directory
//# stderr: fopen: Invalid argument

struct file;
extern struct file *fopen(const char *filename, const char *mode);
extern int fclose(struct file *stream);

extern int errno;
extern void perror(const char *s);

int main() {
    errno = 0;
    fopen("foo.txt", "r");
    perror("fopen");

    errno = 0;
    fopen("README.md", "");  // invalid mode
    perror("fopen");

    return 0;
}
