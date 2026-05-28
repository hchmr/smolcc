smolcc

A minimal self-hosting C compiler for a small subset of C, targeting
aarch64-unknown-linux in around 2k lines of code.

FEATURES

Basic support for the following features:
- types: void, char, int, pointer, array, struct
- global declarations: functions, globals, structs, enums
- statements: declarations, if, while, for, break, continue, return
- varargs: va_list, va_start, va_arg, va_end

No preprocessor, typedefs, compound assignments, etc. The feature set is mainly
guided by the needs of the bootstrap, but some additional features like for
loops and varargs were added for convenience.

DEPENDENCIES

- gcc/clang and a Linux user-space
- neither the compiler nor the runtime depend on libc:
    - compiler is statically linked with a thin syscall wrapper (sys.s)
    - generated code is runtime agnostic, can be linked with glibc
    - this is both for practical reasons (small language surface required) and
      for philosophical reasons (tiny set of primitives)

IMPLEMENTATION NOTES

- everything is in one file for simplicity and because there is no preprocessor
- static pre-allocated arena memory management
- types and strings are interned to save memory and simplify comparisons
- parsing:
    - simple lexer and recursive descent parser
    - string based token matching to reduce boilerplate
    - simplified declarator parsing that supports common cases
- error reporting: precise source locations for easier debugging
- semantic analysis follows C semantics fairly closely, with some
  minor simplifications
- code generation:
    - native calling convention with x0-x7 for arguments and result
    - only a few registers are used, stack used for intermediate values
    - outputs aarch64 assembly directly to stdout
