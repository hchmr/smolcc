//= A simple hexdump

//------------------------------------------------------------------------------
//- deps

// stdlib

extern void exit(int status);
extern unsigned long strtoul(const char *nptr, char **endptr, int base);

// stdio

enum { EOF = -1 };
struct file;
extern struct file *stdin, *stderr;
extern struct file *fopen(const char *filename, const char *mode);
extern int fclose(struct file *stream);
extern int fprintf(struct file *stream, const char *format, ...);
extern int printf(const char *format, ...);
extern unsigned long fread(void *ptr, unsigned long size, unsigned long nmemb, struct file *stream);
extern int ferror(struct file *stream);

// ctype

extern int isprint(int c);

// errno

extern int errno;
extern void perror(const char *s);

// string

extern const char *strchr(const char *s, int c);

// unistd

extern int optopt, optind, opterr;
extern const char *optarg;
int getopt(int argc, char **argv, const char *optstring);

//------------------------------------------------------------------------------
//- impl

enum { MAX_FILES = 64 };

// file iter
int nfiles;
const char *files[MAX_FILES];
int fileno;
const char *filename;
struct file *file;

// args
const char *progname;
int canonical = 0;
unsigned long limit = ~0UL;

// chunk
enum { CHUNK_SIZE = 16 };
unsigned char buf[CHUNK_SIZE + 1];

// error count
int nerr;

void report_err(const char *filename) {
    nerr++;
    fprintf(stderr, "%s: %s", progname, filename);
    perror("");
}

void next_file() {
    if (file) {
        if (fclose(file) != 0) {
            report_err(filename);
        }
        file = 0;
    }
    while (1) {
        if (fileno == nfiles)
            break;
        filename = files[fileno++];
        file = fopen(filename, "rb");
        if (file)
            break;
        report_err(filename);
    }
}

int next_chunk(unsigned long offset) {
    int want = CHUNK_SIZE;
    if (offset + want > limit) {
        want = (int)(limit - offset);
    }

    int pos = 0;
    while (file && want) {
        int n = (int)fread(buf + pos, 1, want, file);
        if (n < want) {
            if (ferror(file)) {
                report_err(filename);
            }
            next_file();
        }
        want = want - n;
        pos = pos + n;
    }
    return pos;
}

int format_chunk(unsigned long offset, int n) {
    printf("%08lx ", offset);

    for (int i = 0; i < CHUNK_SIZE; i++) {
        if (i == 8)
            printf(" ");
        if (i < n)
            printf(" %02x", buf[i]);
        else
            printf("   ");
    }

    if (canonical) {
        for (int i = 0; i < n; i++) {
            if (!isprint(buf[i])) {
                buf[i] = '.';
            }
        }
        buf[n] = '\0';
        printf("  |%s|", buf);
    }

    printf("\n");
}

void hexdump() {
    if (nfiles == 0) {
        filename = "stdin";
        file = stdin;
    } else {
        next_file();
    }

    unsigned long offset = 0;

    while (offset < limit) {
        int n = next_chunk(offset);
        if (n == 0) {
            break;
        }
        format_chunk(offset, n);
        offset = offset + n;
    }

    printf("%08lx\n", offset);
}

//------------------------------------------------------------------------------
//- argparse

void usage() {
    fprintf(stderr, "usage: %s [-C] [-n <length>] [file...]\n", progname);
}

void arg_error() {
    usage();
    exit(1);
}

void argparse(int argc, char **argv) {
    progname = argv[0];

    int opt;
    while ((opt = getopt(argc, argv, "Cn:")) != -1) {
        if (opt == 'C') {
            canonical = 1;
        } else if (opt == 'n') {
            char *trailing;
            errno = 0;
            limit = strtoul(optarg, &trailing, 10);
            if (errno || *trailing) {
                fprintf(stderr, "%s: invalid length limit: %s\n", progname, optarg);
                arg_error();
            }
        } else {
            arg_error();
        }
    }

    for (int i = optind; i < argc; i++) {
        if (nfiles == MAX_FILES) {
            fprintf(stderr, "%s: too many files\n", progname);
            arg_error();
        }
        files[nfiles++] = argv[i];
    }
}

//------------------------------------------------------------------------------
//- main

int main(int argc, char **argv) {
    argparse(argc, argv);

    hexdump();

    return !!nerr;
}
