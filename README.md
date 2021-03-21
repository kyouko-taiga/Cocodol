# Cocodol

Cocodol is a simplistic (to not say dumb) programming language, designed to illustrate how to build a small interpreter/compiler.
Cocodol support a very small set of features, namely unbounded loops, higher-order functions and a handful of built-in data types.

The core of the compiler is written in C.
This includes the parser, basic static analysis and a (relatively slow) interpreter, built as an [AST walker](https://en.wikipedia.org/wiki/Interpreter_(computing)#Abstract_syntax_tree_interpreters).
Code generation and tests are written in [Swift](https://swift.org).

## Example

The following example implements the factorial function and applies it to an argument:

```cocodol
fun factorial(n) {
  if (n > 1) {
    ret n * factorial(n - 1)
  } else {
    ret 1
  }
}

print(6)
// Prints 720
```

See the `Examples` folder for more program examples.

## Installation

Cocodol's compiler is written in [Swift](https://swift.org) and distributed in the form of a package, which can be built with [Swift Package Manager](https://swift.org/package-manager/).

You will also need to install [LLVM](https://llvm.org).
Use your favorite package manager (e.g., `port` on macOS or `apt` on Ubuntu) and make sure `llvm-config` is in your `PATH`.
Then, create a `pkgconfig` file for your specific installation.
The maintainers of [`LLVMSwift`](https://github.com/llvm-swift/LLVMSwift) were kind enough to provide a script:

```bash
swift package resolve
swift .build/checkouts/LLVMSwift/utils/make-pkgconfig.swift
```

> On Ubuntu, you will also need [libc++](https://libcxx.llvm.org) to link your code with LLVM:
>
> ```bash
> apt-get install libc++-dev
> apt-get install libc++abi-dev
> ```

You can then build and install Cocodoc's compiler using the `install.sh` script, at the root of the repository.
The  build should take a couple of minutes:

```bash
./install.sh
cocodoc Examples/Inc.cocodol
./Inc
# Prints 1048575
```

By default, the compiler's binary (i.e., `cocodoc`) is installed at `/usr/local/bin` and the language's runtime library at `/usr/local/lib`.
You can change both of these locations at the top of the script.
Alternatively, you can also build the compiler and the runtime library manually:

```bash
swift build -c release
# Creates .build/release/cocodoc
cd Runtime && make
# Creates Runtime/build/libcocodol_rt.a
```

### Compiling the C interpreter

You can run the walker interpreter by executing `cocodoc` with the `--eval` flag.
Alternatively, you can also compile the interpreter as a standalone application.
Its sources are contained in `Sources/CCocodol`.
To compile them, navigate to this directory and run `make`:

```bash
cd Sources/CCocodol
make
```

This will produce an executable named `cocodol` in `Sources/CCocodol/build`.
It takes the path to a Cocodol program as its sole argument:

```bash
cocodol Examples/Inc.cocodol
// Prints 1048575
```

## License

Cocodol and its compiler are licensed under the MIT License.
