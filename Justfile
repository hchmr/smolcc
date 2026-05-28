test *args:
    ./scripts/test {{args}}
    ./scripts/bootstrap

bootstrap:
    ./scripts/bootstrap

format:
    find . -name '*.c' | xargs clang-format -i
    find . -name '*.s' | xargs ./scripts/asm-fmt -i

configure-clangd:
    make -qp | grep '^CFLAGS' | tr ' ' '\n' | tail -n +3 > compile_flags.txt
