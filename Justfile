test *args:
    ./scripts/test --stage=2 {{args}}

bootstrap:
    ./scripts/bootstrap

format:
    find . -name '*.c' | xargs clang-format -i
    find . -name '*.s' | xargs ./scripts/asm-fmt -i

configure-clangd:
    make -f stage0.mk -qp | grep '^CFLAGS' | tr ' ' '\n' | tail -n +3 > compile_flags.txt
