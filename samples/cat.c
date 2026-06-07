//= A simple implementation of the Unix `cat` command.

//------------------------------------------------------------------------------
//- deps

// stdio

enum { EOF = -1 };
extern struct file *stdin, *stdout, *stderr;

extern struct file *fopen(const char *filename, const char *mode);
extern int fclose(struct file *stream);
extern int ferror(struct file *stream);
extern long fread(void *ptr, unsigned long size, unsigned long count, struct file *stream);
extern long fwrite(const void *ptr, unsigned long size, unsigned long count, struct file *stream);
extern int fprintf(struct file *stream, const char *format, ...);

// errno

extern void perror(const char *msg);

//------------------------------------------------------------------------------
//- impl

enum { BUF_SIZE = 1024 };

static char rdbuf[BUF_SIZE];

static char *progname;

static void file_error(const char *filename) {
    fprintf(stderr, "%s: ", progname);
    perror(filename);
}

static void cat(const char *filename, struct file *src) {
    long n;
    while ((n = fread(rdbuf, 1, BUF_SIZE, src)) > 0) {
        if (fwrite(rdbuf, 1, n, stdout) < n) {
            file_error(filename);
            break;
        }
    }
    if (ferror(src)) {
        file_error(filename);
    }
}

int main(int argc, char **argv) {
    progname = argv[0];
    if (argc == 1) {
        cat("stdin", stdin);
    } else {
        for (int i = 1; i < argc; i++) {
            struct file *f = fopen(argv[i], "r");
            if (!f) {
                file_error(argv[i]);
                continue;
            }
            cat(argv[i], f);
            fclose(f);
        }
    }
    return 0;
}
