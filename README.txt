smolcc

A minimal self-hosting C compiler for a small subset of C, targeting
aarch64-unknown-linux in around 2k lines of code.

Basic support for the following features:
- types: void, char, int, pointer, array, struct
- global declarations: functions, globals, structs, enums
- statements: declarations, if, while, for, break, continue, return
- varargs: va_list, va_start, va_arg, va_end

No preprocessor, typedefs, compound assignments, etc. The feature set is mainly
guided by the needs of the bootstrap, but some additional features like for
loops and varargs were added for convenience.

Implementation notes:
- dependencies: C99 compiler + POSIX libc; builds on Linux and macOS
- static pre-allocated arena memory management
- types and strings are interned to save memory and simplify comparisons
- parsing:
    - simple recursive descent parser
    - string based token matching to reduce boilerplate
    - simplified declarator parsing that still supports common cases
- error reporting: precise source locations with line and column numbers for
  easier debugging
- semantic analysis:
    - follows C semantics fairly closely, with some simplifications
    - implicit conversion between integer types
- code generation:
    - native calling convention with x0-x7 for arguments and result
    - locals allocated on the stack with fixed offsets
    - only a few registers are used, stack used for intermediate values
    - outputs aarch64 assembly directly to stdout
