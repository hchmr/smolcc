minicc

A minimal self-hosting C compiler for a subset of C99, targeting
aarch64-unknown-linux. Around 2.5k lines of code.

Basic support for the following features:
- types: void, char, int, pointer, array, struct
- global declarations: functions, globals, structs, enums
- statements: declarations, if, while, break, continue, return

No preprocessor, typedefs, varargs, etc.

Implementation notes:
- dependencies: C99 compiler + POSIX libc; builds on Linux and macOS
- static pre-allocated arena memory management
- types and strings are interned to save memory and simplify comparisons
- parsing:
    - hand-written recursive descent parser
    - string based token matching to reduce boilerplate
    - simplified declarator parsing, no support for function pointers
- error reporting: precise source locations with line and column numbers for
  easier debugging
- semantic analysis:
    - follows C semantics fairly closely, with some simplifications
    - type checking and implicit conversions for expressions
- code generation:
    - native calling convention with x0-x7 for arguments and result
    - locals allocated on the stack with fixed offsets
    - only a few registers are used; intermediates are spilled to the stack
    - outputs aarch64 assembly directly to stdout
