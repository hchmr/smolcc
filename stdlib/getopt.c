//-----------------------------------------------------------------------------
//- deps

extern struct file *stderr;
extern int fprintf(struct file *stream, const char *format, ...);

extern const char *strchr(const char *s, int c);

//-----------------------------------------------------------------------------
//- impl

const char *optarg;
int optind, opterr, optopt;

const char *curr_arg;

int getopt(int argc, char **argv, const char *spec) {
    if (!curr_arg) {
        if (optind >= argc)
            return -1;

        const char *arg = argv[optind];
        if (!arg || !arg[0] || arg[0] != '-' || !arg[1])
            return -1;

        if (arg[1] == '-' && !arg[2]) {
            optind++;
            return -1;
        }

        optind++;
        curr_arg = arg + 1;
    }

    optopt = *curr_arg++;
    optarg = 0;

    int retval = optopt;

    const char *opt_spec = strchr(spec, optopt);
    if (!opt_spec) {
        retval = '?';
        if (*spec == ':') {
            // pass
        } else if (opterr) {
            fprintf(stderr, "%s: illegal option -- %c\n", argv[0], optopt);
        }
    } else if (opt_spec[1] == ':') {
        if (*curr_arg) {
            optarg = curr_arg;
            curr_arg = 0;
        } else if (optind < argc) {
            optarg = argv[optind];
            curr_arg = 0;
            optind++;
        } else if (*spec == ':') {
            retval = ':';
        } else {
            if (opterr) {
                fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], optopt);
            }
            retval = '?';
        }
    }

    if (curr_arg && !*curr_arg) {
        curr_arg = 0;
    }

    return retval;
}

void _init_getopt() {
    optind = 1;
    opterr = 1;
}
