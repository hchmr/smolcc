test *args:
    make test ARGS="{{args}}"

bootstrap:
    make bootstrap

format:
    #!/usr/bin/env bash
    find . -name '*.c' | xargs clang-format -i
    find . -name '*.s' | xargs ./scripts/asm-fmt -i
    black scripts/{asm-fmt,test} --quiet

configure-clangd:
    make -f mk/stage.mk STAGE=stage0 -qp | sed -n 's/^CFLAGS = //p' | tr ' ' '\n' > compile_flags.txt
