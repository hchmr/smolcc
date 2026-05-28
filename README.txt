smolcc

A minimal self-hosting C compiler for a small subset of C, targeting
aarch64-unknown-linux, in ~2k lines of code.

FEATURES

Basic support for the following features:
- types: void, char, int, pointer, array, struct
- global declarations: functions, globals, structs, enums
- statements: declarations, if, while, for, break, continue, return
- varargs: va_list, va_start, va_arg, va_end
- interop: can call foreign functions and be called from foreign code

No preprocessor, typedefs, compound assignments, etc. The feature set is guided 
mainly by the needs of the bootstrap, but some additional features like for
loops and varargs were added for convenience.

DEPENDENCIES

- gcc/clang and a Linux user-space
- neither the compiler nor the runtime depend on libc:
    - the compiler is statically linked with a thin syscall wrapper (sys.s)
    - generated code is runtime agnostic, can be linked with glibc
    - this is both for practical reasons (small language surface required) and
      for philosophical reasons (tiny set of primitives)

IMPLEMENTATION NOTES

- everything is in one file, for simplicity and since there's no preprocessor
- static pre-allocated bump allocator for dynamic memory
- types and strings are interned to save memory and simplify comparisons
- parsing:
    - simple lexer and recursive descent parser
    - string based token matching to reduce boilerplate
    - simplified declarator parsing that supports common cases
- error reporting: precise source locations for easier debugging
- semantic analysis follows C semantics fairly closely, with some
  minor simplifications
- code generation:
    - only a few registers are used, stack used for intermediate values
    - parameters passed in x0-x7 and return value in x0 (subset of the
      AArch64 procedure call standard, max 8 parameters)
    - outputs AArch64 assembly directly to stdout
