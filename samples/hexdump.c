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
extern void *memset(void *s, long c, unsigned long n);

// unistd

extern int optopt, optind, opterr;
extern const char *optarg;
int getopt(int argc, char **argv, const char *optstring);

//------------------------------------------------------------------------------
//- impl

enum { CHUNK_SIZE = 16 };

// args
const char *progname;

// error count
int nerr;

struct conf {
    int canonical;
    unsigned long limit;
    int nfiles;
    const char **filenames;
};

void report_err(const char *filename) {
    nerr++;
    fprintf(stderr, "%s: %s", progname, filename);
    perror("");
}

//------------------------------------------------------------------------------
//- file sequence handling

struct file_seq {
    int nfiles;
    const char **filenames;

    int fileno;
    struct file *file;
};

void file_seq_init(struct file_seq *self, const char **filenames, int nfiles) {
    memset(self, 0, sizeof(struct file_seq));
    self->filenames = filenames;
    self->nfiles = nfiles;
    if (nfiles == 0) {
        self->file = stdin;
    }
}

const char *curr_filename(struct file_seq *self) {
    if (self->nfiles == 0)
        return "stdin";
    return self->filenames[self->fileno];
}

int open_next(struct file_seq *self) {
    while (self->fileno < self->nfiles) {
        const char *filename = self->filenames[self->fileno];
        self->file = fopen(filename, "rb");
        if (self->file)
            break;
        report_err(filename);
        self->fileno++;
    }

    return self->file != 0;
}

int next_file(struct file_seq *self) {
    if (self->nfiles == 0)
        return 0;
    if (self->file) {
        if (fclose(self->file) != 0) {
            report_err(curr_filename(self));
        }
        self->file = 0;
        self->fileno++;
    }
    return open_next(self);
}

int read_chunk(struct file_seq *self, unsigned long offset, unsigned long limit, unsigned char *buf,
               unsigned long bufsize) {
    int want = bufsize;
    if (offset + want > limit) {
        want = (int)(limit - offset);
    }

    if (!self->file) {
        if (!open_next(self))
            return 0;
    }

    int pos = 0;
    while (self->file && want) {
        int n = (int)fread(buf + pos, 1, want, self->file);
        if (n < want) {
            if (ferror(self->file)) {
                report_err(curr_filename(self));
            }
            if (!next_file(self))
                break;
        }
        want = want - n;
        pos = pos + n;
    }
    return pos;
}

//------------------------------------------------------------------------------
//- hexdump

int format_chunk(struct conf *conf, unsigned long offset, unsigned char *buf, int n) {
    printf("%08lx ", offset);

    for (int i = 0; i < CHUNK_SIZE; i++) {
        if (i == 8)
            printf(" ");
        if (i < n)
            printf(" %02x", buf[i]);
        else
            printf("   ");
    }

    if (conf->canonical) {
        for (int i = 0; i < n; i++) {
            if (!isprint(buf[i])) {
                buf[i] = '.';
            }
        }
        printf("  |%.*s|", n, buf);
    }

    printf("\n");
    return n;
}

void hexdump(struct conf *conf, struct file_seq *seq) {
    unsigned char buf[CHUNK_SIZE];
    unsigned long offset = 0;

    while (offset < conf->limit) {
        int n = read_chunk(seq, offset, conf->limit, buf, CHUNK_SIZE);
        if (n == 0)
            break;
        format_chunk(conf, offset, buf, n);
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

void argparse(int argc, char **argv, struct conf *conf) {
    progname = argv[0];

    int canonical = 0;
    unsigned long limit = ~0UL;

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

    conf->limit = limit;
    conf->canonical = canonical;
    conf->nfiles = argc - optind;
    conf->filenames = (const char **)argv + optind;
}

//------------------------------------------------------------------------------
//- main

int main(int argc, char **argv) {
    struct conf conf;
    argparse(argc, argv, &conf);

    struct file_seq seq;
    file_seq_init(&seq, conf.filenames, conf.nfiles);

    hexdump(&conf, &seq);

    return !!nerr;
}
