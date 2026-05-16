test *args:
    ./scripts/test {{args}}
    ./scripts/bootstrap

self-test:
    make && ./out/ucc main.c | ./scripts/asm-fmt

configure-clangd:
    make -qp | grep '^CFLAGS' | tr ' ' '\n' | tail -n +3 > compile_flags.txt
