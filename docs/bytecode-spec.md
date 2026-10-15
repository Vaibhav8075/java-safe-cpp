# Bytecode Specification

The VM runs a stack-based instruction set. Each instruction has an opcode, one integer operand
(unused by most opcodes) and the source line it came from.

## Instructions

| Opcode | Operand | Stack effect / purpose |
|--------|---------|------------------------|
| `PUSH_CONST` | constant index | push `constants[index]` |
| `LOAD_LOCAL` | slot | push local `slot` of the current frame |
| `STORE_LOCAL` | slot | pop into local `slot` |
| `ADD` `SUB` `MUL` | - | pop b, pop a, push a op b (wraps on overflow) |
| `DIV` `MOD` | - | as above; throws `DivisionByZeroException` if b is 0 |
| `EQ` `NE` `LT` `GT` `LE` `GE` | - | pop b, pop a, push 1 or 0 |
| `NOT` | - | pop a, push !a |
| `AND` `OR` | - | non-short-circuit forms; kept in the VM but the compiler uses jumps instead |
| `JUMP` | target | continue at instruction `target` |
| `JUMP_IF_FALSE` | target | pop; if 0, continue at `target` |
| `CALL` | function index | pop the arguments into slots 0..n-1 of a new frame and jump to the function |
| `RETURN` | - | pop the return value, drop the frame, push the value for the caller |
| `ALLOC` | size | allocate an object of `size` cells with refcount 1, push its handle |
| `INC_REF` | - | add a reference to the handle on top of the stack (stack unchanged) |
| `DEC_REF` | - | pop a handle and drop a reference; free the object at zero |
| `CHECK_NULL` | - | throw `NullPointerException` if the top handle is 0 (stack unchanged) |
| `CHECK_BOUNDS` | - | with `[handle, index]` on top, throw unless `0 <= index < size` (stack unchanged) |
| `LOAD_ARRAY` | - | pop index, pop handle, push the cell |
| `STORE_ARRAY` | - | pop value, index, handle and store (not emitted by the compiler) |
| `STORE_ARRAY_INV` | - | pop index, handle, value; store; push value |
| `LOAD_POINTER` | - | pop handle, push cell 0 |
| `STORE_POINTER` | - | pop handle, pop value, store into cell 0, push value |
| `PRINT` | - | pop and print |
| `POP` | - | discard the top value |
| `HALT` | - | stop |

Assignments leave their value on the stack, because in C++ an assignment is an expression;
an expression statement then removes it with `POP`.

## Short-circuit operators

```
a && b:   <a>  JUMP_IF_FALSE L1  <b>  JUMP L2  L1: PUSH_CONST 0  L2:
a || b:   <a>  JUMP_IF_FALSE L1  PUSH_CONST 1  JUMP L2  L1: <b>  L2:
```

## Frames

Each call gets a fresh, zeroed block of `numLocals` slots. Parameters occupy slots `0..numArgs-1`
and every other local in the function gets its own slot after them.

## `.jbc` file format (version 2)

All integers are 4 bytes, little-endian, on every platform.

```
"JSCP"                    magic
2                         format version
mainEntry                 instruction index of main
constCount, constants...
funcCount, { nameLength, name bytes, entryPoint, numLocals, numArgs } ...
codeCount, { opcode, operand, sourceLine } ...
```

The loader rejects a file with the wrong magic or version, a truncated body, or any operand that
points outside its table (see `BytecodeProgram::verify`).

`jscpp dump <file>` prints the tables and the code with the source line of each instruction.
