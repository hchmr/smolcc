test *args:
    ./scripts/test {{args}}
    ./scripts/bootstrap

build:
    make

bootstrap:
    ./scripts/bootstrap

self-test:
    make && ./out/smolcc main.c | ./scripts/asm-fmt

format:
    clang-format -i *.c
    ./scripts/asm-fmt -i *.s

configure-clangd:
    make -qp | grep '^CFLAGS' | tr ' ' '\n' | tail -n +3 > compile_flags.txt
