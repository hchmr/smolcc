# smolcc

[![CI](https://github.com/hchmr/smolcc/actions/workflows/ci.yml/badge.svg)](https://github.com/hchmr/smolcc/actions/workflows/ci.yml)

A small self-hosting C compiler for a subset of C, targeting `aarch64-unknown-linux`, implemented in roughly 2k lines of code.

## Features

A small but usable subset of C is supported, including:

* type system: scalar types, pointers, arrays, and structs
* declarations: functions, globals, structs, and enums
* statements: declarations, `if`, `while`, `for`, `break`, `continue`, and `return`
* varargs: the basic API of `stdarg.h` is built-in (`va_start`, `va_arg`, and `va_end`)
* interop: can call foreign functions and be called from foreign code

These features were chosen to support the compiler's own implementation and bootstrap process, and to be sufficient for writing small toy programs. Some of the most notable missing are switch statements, unions, compound assignment, and the preprocessor, of course.

## Dependencies

The dependencies are minimal: a C99 compiler and a Linux environment. Neither the compiler nor the runtime depend on libc. Instead, the compiler is statically linked against a tiny OS bridge, and the generated code is runtime-agnostic, allowing it to be linked against various C libraries or used as a freestanding binary.

The current bootstrap process relies on the host compiler's support for freestanding executables. Tested with both `gcc` and `clang`.

## Implementation Notes

The entire compiler is implemented in a single file. One reason for this is missing preprocessor support, which rules out the usual header-based organization. But the main reason is simplicity: having everything together makes the code straightforward and linear, with very little boilerplate.

All interaction with the outside world goes through four assembly routines: `read`, `write`, and `_exit`, plus `abort` (for debugging). This design limits the need for complex language features just to get going, but also ensures that everything is built on a tiny set of primitives not written in C, which feels less circular than relying on libc.

The lexer and parser are quite simple. Only a small number of token kinds are defined, and tokens are matched through ad-hoc string equality rather than a large token enum. This keeps the implementation compact, and most keywords are only recognized where the grammar expects them, so many keywords can be used as identifiers in certain contexts. Declarators are parsed in a simplified way that supports the common cases correctly with only a small amount of code.

Semantic analysis follows C semantics fairly closely, with some simplifications. Implicit conversions, pointer arithmetic, array decay, and similar features are supported in their basic form. Declaration scope, storage classes, and linkage are properly handled too.

Code generation is very simple and direct. The compiler uses only a small number of registers and pushes intermediate values to the stack. Parameters are passed in x0–x7, and return values are placed in x0. This naturally limits functions to eight parameters and scalar values only, and matches a subset of the AArch64 procedure call standard. Variadic functions are supported through the same mechanism.

Dynamic memory is achieved with a bump allocator backed by a statically allocated arena. Types and strings are interned both to reduce memory usage and to make comparisons cheap.

## Standard Library and Compiler Driver

A small companion library provides a minimal subset of the C standard library. It is not used in the compiler proper, but is used by the test suite. This standard library also serves as a place to experiment with low-level C library code in the same minimal environment as the compiler. The focus is not a full libc implementation, only a small set of useful functions, currently centered on parts of stdio.h, stdlib.h, and string.h.

The core compiler program is not very easy to use on its own, since it reads source code from standard input and writes assembly to standard output. A simple compiler driver is included for convenience. It provides a few command-line options and handles the full compilation pipeline, including running the host toolchain’s assembler and linker to produce an executable. The driver also links in the standard library.

## Development and Testing

**Requirements**
- an Arm64 Linux environment
- GNU Make
- GCC or Clang
- optional: Python >= 3.12 to run the test suite
- optional: errno from moreutils to generate strerror definition in stdlib

#### Building

`make all` (default) builds the compiler and the standard library.

#### Testing

`make test` runs the test suite using the built compiler. The test suite includes a variety of small C programs that are compiled with the smolcc driver and either executed or checked for expected compile-time errors.
