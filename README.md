# Cocodol

Cocodol is a simplistic (to not say dumb) programming language, designed to illustrate how to build a small interpreter/compiler.
Cocodol support a very small set of features, namely unbounded loops, higher-order functions and a handful of built-in data types.

The core of the compiler is written in C.
This includes the parser and a (relatively slow) interpreter, built as an [AST walker](https://en.wikipedia.org/wiki/Interpreter_(computing)#Abstract_syntax_tree_interpreters).
Static analysis, code generation and tests are written in Swift. 

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

## Using the C interpreter

A walker interpreter is provided as a standalone C program, whose sources are contained in `Sources/CCocodol`.
To compile it, navigate to this directory and run `make`:

```bash
cd Sources/CCocodol
make
```

This will produce an executable named `cocodol` in `Sources/CCocodol/build`.
It takes the path to a Cocodol program as its sole argument:

```
cocodol Examples/Factorial.cocodol
// Prints 1048575
```
