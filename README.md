# New-J-Compiler

## Examples

Basic Hello World
```
func main print("Hello World")
```

Recursive Fibonacci
```
func fib(n : int32) : int64
    if (n <= 1) {
        ret n
    } else {
        return fib(n - 1) + fib(n - 2)
    }
```

## Status

Current features:
- `if` with or without `else`
- function calls
- printable syntax tree
  - `-fsyntax-tree` in the command line
- printable intermediate representation "IR"

## Goals

In progress:
- Bytecode printing and output

Future:
- Full type checking
- Loops
- Multi-file programs

## Architecture

Planned Phases:
1. Tokenize input
2. Generate syntax tree
3. Typecheck tree
4. Generate IR by walking syntax tree
5. Optimize IR
6. Output bytecode from IR
