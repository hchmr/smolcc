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

extern int _do_syscall(int num  , ...);

static int set_errno(int res) {
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int openat(int dirfd, const char *pathname, int flags, int mode) {
    return set_errno(_do_syscall(Sys_OpenAt, dirfd, pathname, flags, mode));
}

int open(const char *pathname, int flags, int mode) {
    return openat(AT_FDCWD, pathname, flags, mode);
}

int close(int fd) {
    return set_errno(_do_syscall(Sys_Close, fd));
}

int read(int fd, void *buf, int nbyte) {
    return set_errno(_do_syscall(Sys_Read, fd, buf, nbyte));
}

int write(int fd, const void *buf, int nbyte) {
    return set_errno(_do_syscall(Sys_Write, fd, buf, nbyte));
}

int _exit(int status) {
    return _do_syscall(Sys_Exit, status);
}
