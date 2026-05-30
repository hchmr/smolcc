//===============================================================================
//= deps

extern struct file *stderr;
extern int fprintf(struct file *stream, const char *format, ...);

//===============================================================================

int errno;

void perror(const char *msg) {
    fprintf(stderr, "%s: errno=%d\n", msg, errno);
}
