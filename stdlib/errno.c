//===============================================================================
//= deps

extern struct file *stderr;
extern int fprintf(struct file *stream, const char *format, ...);

//===============================================================================

int errno;

struct error {
    const char *dscr;
    const char *name;
    int num;
};

extern struct error *_errlist;
extern int _errlist_len;

struct error *_errlist_find(int errnum) {
    int lo = 0, hi = _errlist_len;
    while (lo < hi) {
        int mid = (hi - lo) / 2 + lo;
        if (_errlist[mid].num < errnum) {
            lo = mid + 1;
        } else if (_errlist[mid].num > errnum) {
            hi = mid;
        } else {
            return &_errlist[mid];
        }
    }
    return 0;
}

const char *strerror(int errnum) {
    struct error *err = _errlist_find(errnum);
    return err ? err->dscr : "Unknown error";
}

void perror(const char *dscr) {
    struct error *err = _errlist_find(errno);
    fprintf(stderr, "%s: %s\n", dscr, err ? err->dscr : "Unknown error");
}
