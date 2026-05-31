//# mode: run
//# args: -lwcL tests/good/stdlib/wc.c tests/good/stdlib/write.c tests/good/stdlib/printf.c
//# exit: 0
//# stdout:      212     622    5258      91 tests/good/stdlib/wc.c
//# stdout:       12      39     222      53 tests/good/stdlib/write.c
//# stdout:      185     853    6134      66 tests/good/stdlib/printf.c
//# stdout:      409    1514   11614      91 total

//------------------------------------------------------------------------------
//- libc

// stdlib

extern void exit(int status);

// stdio

enum { EOF = -1 };

struct File;

extern struct File *stdin, *stderr, *stdout;

extern int fgetc(struct File *stream);
extern int fprintf(struct File *stream, const char *format, ...);
extern int ferror(struct File *stream);
extern struct File *fopen(const char *file_name, const char *mode);
extern int fclose(struct File *stream);

// errno

extern void perror(const char *msg);

// ctype

extern int isspace(int c);

//------------------------------------------------------------------------------
//- wc

struct Stats {
    int lines;
    int words;
    int bytes;
    int max_len;
};

struct Args {
    int options;
    char **files;
    int files_count;
};

enum {
    Opt_PrintLines = 1,
    Opt_PrintWords = 2,
    Opt_PrintBytes = 4,
    Opt_PrintMaxLen = 8,
};

void stats_zero(struct Stats *stats) {
    stats->lines = 0;
    stats->words = 0;
    stats->bytes = 0;
    stats->max_len = 0;
}

struct File *open_file(const char *file_name) {
    struct File *stream = fopen(file_name, "r");
    if (!stream) {
        perror("Error opening file");
        exit(1);
    }
    return stream;
}

void close_file(struct File *stream) {
    if (fclose(stream) != 0) {
        perror("Error closing file");
        exit(1);
    }
}

int get_char(struct File *stream) {
    int c = fgetc(stream);
    if (c == EOF && ferror(stream) != 0) {
        perror("Error reading file");
        exit(1);
    }
    return c;
}

int int_max(int a, int b) {
    return a < b ? b : a;
}

void stats_accum(struct Stats *total, struct Stats *other) {
    total->lines = total->lines + other->lines;
    total->words = total->words + other->words;
    total->bytes = total->bytes + other->bytes;
    total->max_len = int_max(total->max_len, other->max_len);
}

void get_stats(struct File *stream, struct Stats *stats) {
    stats_zero(stats);

    int prev_char = ' ';
    int curr_len = 0;
    for (;;) {
        int curr_char = get_char(stream);
        if (curr_char == EOF)
            break;

        curr_len = curr_len + 1;

        if (isspace(prev_char) && !isspace(curr_char)) {
            stats->words = stats->words + 1;
        }
        prev_char = curr_char;

        if (curr_char == '\n') {
            stats->bytes = stats->bytes + curr_len;
            stats->lines = stats->lines + 1;
            stats->max_len = int_max(stats->max_len, curr_len - 1);
            curr_len = 0;
        }
    }

    stats->bytes = stats->bytes + curr_len;
    stats->max_len = int_max(stats->max_len, curr_len);
}

void print_row(int options, struct Stats *stats, const char *label) {
    if (options & Opt_PrintLines)
        fprintf(stdout, "%8d", stats->lines);
    if (options & Opt_PrintWords)
        fprintf(stdout, "%8d", stats->words);
    if (options & Opt_PrintBytes)
        fprintf(stdout, "%8d", stats->bytes);
    if (options & Opt_PrintMaxLen)
        fprintf(stdout, "%8d", stats->max_len);
    if (label)
        fprintf(stdout, " %s", label);
    fprintf(stdout, "\n");
}

void print_usage(char **argv) {
    fprintf(stderr, "Usage: %s [-cLlw] [file ...]\n", argv[0]);
}

void arg_parse(int argc, char **argv, struct Args *args) {
    int options = 0;
    int i = 1;

    for (; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] != '-')
            break;

        for (int j = 1; arg[j] != 0; j++) {
            int opt = arg[j];
            if (opt == 'l') {
                options = options | Opt_PrintLines;
            } else if (opt == 'w') {
                options = options | Opt_PrintWords;
            } else if (opt == 'c') {
                options = options | Opt_PrintBytes;
            } else if (opt == 'L') {
                options = options | Opt_PrintMaxLen;
            } else {
                fprintf(stderr, "Unknown option: %c\n", opt);
                print_usage(argv);
                exit(1);
            }
        }
    }

    if (options == 0)
        options = Opt_PrintLines | Opt_PrintWords | Opt_PrintBytes;

    args->options = options;
    args->files = &argv[i];
    args->files_count = argc - i;
}

int main(int argc, char **argv) {
    struct Args args;
    arg_parse(argc, argv, &args);

    struct Stats total;
    stats_zero(&total);

    int i = 0;
    for (; i < args.files_count; i++) {
        char *file_name = args.files[i];
        struct File *stream = open_file(file_name);
        struct Stats stats;
        get_stats(stream, &stats);
        close_file(stream);
        print_row(args.options, &stats, file_name);
        stats_accum(&total, &stats);
    }

    if (i == 0) {
        get_stats(stdin, &total);
        print_row(args.options, &total, 0);
    } else if (i > 1) {
        print_row(args.options, &total, "total");
    }

    return 0;
}
