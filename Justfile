test *args:
    ./scripts/test {{args}}
    ./scripts/bootstrap

stage0:
    make

bootstrap:
    ./scripts/bootstrap

self-test:
    make && ./out/smolcc main.c | ./scripts/asm-fmt

format:
    find . -name '*.c' | xargs clang-format -i
    ./scripts/asm-fmt -i *.s

configure-clangd:
    make -qp | grep '^CFLAGS' | tr ' ' '\n' | tail -n +3 > compile_flags.txt
