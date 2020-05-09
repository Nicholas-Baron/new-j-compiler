# New-J-Compiler
A Compiler for the New-J programming language

## Status

Current features:
- `if` w/o `else`
- printable syntax tree
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
