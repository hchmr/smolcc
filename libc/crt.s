    .section .text

// void _start(void);
    .globl  _start
_start:
    bl      _init_stdio

    // main(argc, argv);
    ldr     x0, [sp]                    // argc
    add     x1, sp, #8                  // argv
    bl      main

    bl      exit

// void exit(int status);
    .globl  exit
exit:
    str     w0, [sp, #-16]!             // push(status)
    bl      _fini_stdio
    ldr     w0, [sp], #16               // pop(status)
    bl      _exit
