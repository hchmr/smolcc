//= A simple hexdump

//------------------------------------------------------------------------------
//- deps

// stdlib

extern void exit(int status);
extern long strtol(const char *nptr, char **endptr, int base);

// stdio

struct file;
extern struct file *stdin, *stderr;
extern struct file *fopen(const char *filename, const char *mode);
extern int fclose(struct file *stream);
extern int fprintf(struct file *stream, const char *format, ...);
extern int printf(const char *format, ...);
extern int fread(void *ptr, unsigned long size, unsigned long nmemb, struct file *stream);
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

const char *progname;
int canonical = 0;
int nfiles;
const char *files[MAX_FILES];
long limit = -1;

void print_error(const char *filename) {
    fprintf(stderr, "%s: %s", progname, filename);
    perror("");
}

int hd(struct file *f) {
    unsigned char buf[16 + 1];
    unsigned long offset = 0;
    int at_eof = 0;
    while (!at_eof) {
        unsigned long want = 16;
        if (limit >= 0 && offset + want > (unsigned long)limit) {
            want = limit - offset;
            at_eof = 1;
        }
        if (want == 0) {
            break;
        }
        unsigned long n = fread(buf, 1, want, f);
        if (n != want) {
            if (ferror(f)) {
                print_error("read");
                return -1;
            }
            at_eof = 1;
        }

        printf("%08lx ", offset);
        for (unsigned long i = 0; i < 16; i++) {
            if (i == 8)
                printf(" ");
            if (i < n)
                printf(" %02x", buf[i]);
            else
                printf("   ");
        }
        if (canonical) {
            for (unsigned long i = 0; i < n; i++) {
                if (!isprint(buf[i]))
                    buf[i] = '.';
            }
            buf[n] = '\0';
            printf("  |%s|", buf);
        }
        printf("\n");
        offset = offset + n;
    }

    printf("%08lx\n", offset);

    return 0;
}

void usage() {
    fprintf(stderr, "usage: %s [-cCn] [file...]\n", progname);
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
            char *endptr;
            errno = 0;
            limit = strtol(optarg, &endptr, 10);
            if (errno || *endptr || limit < 0) {
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

int main(int argc, char **argv) {
    argparse(argc, argv);

    int res = 0;
    if (nfiles == 0) {
        res = hd(stdin);
    } else {
        for (int i = 0; i < nfiles; i++) {
            struct file *f = fopen(files[i], "rb");
            if (!f) {
                print_error(files[i]);
                res = -1;
                continue;
            }
            res = res | hd(f);
            if (fclose(f) != 0) {
                print_error(files[i]);
                res = -1;
            }
        }
    }

    return -res;
}
