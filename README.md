# New-J-Compiler

## Examples
Note: these examples work in the latest version of the language.
Later **major** versions are permitted to break any code.

Basic Hello World
```
func main print("Hello World")
```

Recursive Fibonacci
```
func fib(n : int32) : int64
    if (n <= 1) {
        ret n;
    } else {
        return fib(n - 1) + fib(n - 2)
    }
```

Iterative Factorial
```
func factorial(n : int32) : int64 {
    let x = 1;
    while(n > 1){
        x *= n, n -= 1
    }
    ret x
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

Future:
- Full type checking
- `for` Loops
- Multi-file programs

## Architecture

Phases used in compilation:
1. Tokenize input
2. Generate syntax tree
3. Typecheck tree
4. Generate IR by walking syntax tree
5. Optimize IR
6. Output bytecode from IR
