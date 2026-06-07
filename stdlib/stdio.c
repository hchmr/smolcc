//==============================================================================
//= deps

#include <stdarg.h>

enum { NULL = 0 };

enum {
    O_RDONLY = 0,
    O_WRONLY = 1,
    O_RDWR = 2,
    O_CREAT = 64,
    O_TRUNC = 512,
    O_APPEND = 1024,
};

extern int open(const char *pathname, int flags, int mode);
extern int close(int fd);
extern long write(int fd, const void *buf, unsigned long nbytes);
extern long read(int fd, void *buf, unsigned long nbytes);

extern int strcmp(const char *s1, const char *s2);
extern unsigned long strlen(const char *s);
extern void *memset(void *s, int c, unsigned long n);

enum {
    EINVAL = 22,
};
extern int errno;

//==============================================================================
//= impl

enum { FILE_BUF_CAP = 1024 };
enum { MAX_FILES = 32 };

enum {
    FFLG_R = 1,
    FFLG_W = 2,
    FFLG_EOF = 4,
    FFLG_ERR = 8,
};

struct file {
    int fd;
    int flags;
    char buf[FILE_BUF_CAP];
    long buf_pos, buf_len;
};

static struct file streams[MAX_FILES];
static int n_streams = 0;

static void file_free(struct file *stream) {
    memset(stream, 0, sizeof(struct file));
    n_streams--;
}

static struct file *file_alloc(int flags) {
    if (n_streams == MAX_FILES)
        return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        struct file *stream = &streams[i];
        if (stream->flags == 0) {
            n_streams++;
            stream->fd = -1;
            stream->flags = flags;
            return stream;
        }
    }

    return 0;
}

static void set_flag(struct file *stream, int flag) {
    stream->flags = stream->flags | flag;
}

//==============================================================================
//= core

enum { EOF = -1 };

struct file *stdin, *stdout, *stderr;

void clearerr(struct file *stream) {
    stream->flags = stream->flags & ~(FFLG_EOF | FFLG_ERR);
}

int feof(struct file *stream) {
    return stream->flags & FFLG_EOF;
}

int ferror(struct file *stream) {
    return stream->flags & FFLG_ERR;
}

int fflush(struct file *stream) {
    while (stream->buf_pos < stream->buf_len) {
        long nw = write(stream->fd, &stream->buf[stream->buf_pos], stream->buf_len - stream->buf_pos);
        if (nw == -1) {
            set_flag(stream, FFLG_ERR);
            return EOF;
        }
        stream->buf_pos = stream->buf_pos + nw;
    }
    stream->buf_pos = 0;
    stream->buf_len = 0;
    return 0;
}

static long ffill(struct file *stream) {
    long nr = read(stream->fd, stream->buf, FILE_BUF_CAP);
    if (nr == -1) {
        set_flag(stream, FFLG_ERR);
        return EOF;
    }
    if (nr == 0) {
        set_flag(stream, FFLG_EOF);
        return 0;
    }
    stream->buf_len = nr;
    return nr;
}

struct file *fopen(const char *fname, const char *mode) {
    int open_flags = 0, flags = 0;
    if (!strcmp(mode, "r") || !strcmp(mode, "rb")) {
        open_flags = O_RDONLY;
        flags = FFLG_R;
    } else if (!strcmp(mode, "w") || !strcmp(mode, "wb")) {
        open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        flags = FFLG_W;
    } else if (!strcmp(mode, "a") || !strcmp(mode, "ab")) {
        open_flags = O_WRONLY | O_CREAT | O_APPEND;
        flags = FFLG_W;
    } else {
        errno = EINVAL;
        return 0;
    }

    struct file *stream = file_alloc(flags);
    if (!stream)
        return 0;

    int fd = open(fname, open_flags, 420);
    if (fd == -1) {
        file_free(stream);
        return 0;
    }

    stream->fd = fd;
    return stream;
}

int fclose(struct file *stream) {
    int res = 0;
    if ((stream->flags & FFLG_W) && fflush(stream) == EOF) {
        res = EOF;
    }

    if (close(stream->fd) != 0) {
        res = EOF;
    }

    file_free(stream);
    return res;
}

unsigned long fwrite(const void *ptr, unsigned long size, unsigned long count, struct file *stream) {
    unsigned long to_write = size * count;
    if (to_write == 0)
        return 0;
    const char *buf = ptr;

    unsigned long i = 0;
    while (i < to_write) {
        if (stream->buf_len == FILE_BUF_CAP) {
            if (fflush(stream) == EOF) {
                break;
            }
        }
        stream->buf[stream->buf_len++] = buf[i++];
    }

    return i / size;  // number of total objects written
}

unsigned long fread(void *ptr, unsigned long size, unsigned long count, struct file *stream) {
    unsigned long to_read = size * count;
    if (to_read == 0)
        return 0;
    char *buf = ptr;

    unsigned long i = 0;
    while (i < to_read) {
        if (stream->buf_pos == stream->buf_len) {
            stream->buf_pos = stream->buf_len = 0;
            if (ffill(stream) <= 0) {
                break;
            }
        }

        buf[i++] = stream->buf[stream->buf_pos++];
    }

    return i / size;  // number of total objects read
}

int fgetc(struct file *stream) {
    char ch;
    if (fread(&ch, 1, 1, stream) == 0) {
        return EOF;
    }
    return ch < 0 ? (int)ch + 256 : ch;
}

int fputc(int c, struct file *stream) {
    char ch = c;
    if (fwrite(&ch, 1, 1, stream) == 0) {
        return EOF;
    }
    return c;
}

int putchar(int c) {
    return fputc(c, stdout);
}

int puts(const char *s) {
    unsigned long len = strlen(s);
    if (fwrite(s, 1, len, stdout) != len) {
        return EOF;
    }
    if (fputc('\n', stdout) == EOF) {
        return EOF;
    }
    return 0;
}

//==============================================================================
//= hooks

void _init_stdio() {
    stdin = file_alloc(FFLG_R);
    stdin->fd = 0;
    stdout = file_alloc(FFLG_W);
    stdout->fd = 1;
    stderr = file_alloc(FFLG_W);
    stderr->fd = 2;
}

void _fini_stdio() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (streams[i].flags & FFLG_W) {
            fflush(&streams[i]);
        }
    }
}
