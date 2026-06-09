enum {
    Sys_Read = 63,
    Sys_Write = 64,
    Sys_OpenAt = 56,
    Sys_Close = 57,
    Sys_Exit = 93,
};

enum {
    AT_FDCWD = -100,
};

extern int errno;

extern long _do_syscall(int num, ...);

static long set_errno(long res) {
    if (res < 0) {
        errno = (int)-res;
        return -1;
    }
    return res;
}

int openat(int dirfd, const char *pathname, int flags, int mode) {
    return (int)set_errno(_do_syscall(Sys_OpenAt, dirfd, pathname, flags, mode));
}

int open(const char *pathname, int flags, int mode) {
    return openat(AT_FDCWD, pathname, flags, mode);
}

int close(int fd) {
    return (int)set_errno(_do_syscall(Sys_Close, fd));
}

long read(int fd, void *buf, unsigned long nbyte) {
    return set_errno(_do_syscall(Sys_Read, fd, buf, nbyte));
}

long write(int fd, const void *buf, unsigned long nbyte) {
    return set_errno(_do_syscall(Sys_Write, fd, buf, nbyte));
}

void _exit(int status) {
    _do_syscall(Sys_Exit, status);
}
