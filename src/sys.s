    .section .text

// ssize_t read(int fildes, void *buf, size_t nbyte);
    .globl  read
read:
    mov     w8, #63
    svc     #0
    ret

// ssize_t write(int fildes, const void *buf, size_t nbyte);
    .globl  write
write:
    mov     w8, #64
    svc     #0
    ret

// void _exit(int status);
    .globl  _exit
_exit:
    mov     w8, #93
    svc     #0
    ret

// void abort(void);
    .globl  abort
abort:
    // pid = getpid()
    mov     x8, #172
    svc     #0

    // kill(pid, SIGABRT)
    mov     x1, #6
    mov     x8, #129
    svc     #0
    ret

// void _start(void);
    .globl  _start
_start:
    ldr     x0, [sp]                    // argc
    add     x1, sp, #8                  // argv
    bl      main
    bl      _exit
