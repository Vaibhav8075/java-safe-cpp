# Java-Safe C++ Language Specification

The accepted language is a small subset of C++ syntax that runs with Java-style safety checks.
Two differences from standard C++: `void main()` is allowed, and evaluation order is always left
to right, so expressions such as `x++ + ++x` have a defined result here.

## Program structure

A program is a list of function definitions. One of them must be `int main()` or `void main()`
with no parameters. Functions can call each other in any order and can be recursive. There are no
global variables.

## Types

| Type | Meaning |
|------|---------|
| `int` | 32-bit signed integer. Overflow wraps around (two's complement), as in Java. |
| `bool` | `true` or `false`. Not interchangeable with `int`. |
| `void` | Only as a function return type. |
| `int*`, `bool*` | Reference-counted pointer to one heap cell, or `nullptr`. |
| `int a[N]`, `bool a[N]` | Local array with a constant size `N >= 1`, zero-initialised. |

Variables without an initializer start at `0`, `false` or `nullptr`.

## Statements

- Variable declaration: `int x;`, `int x = expr;`, `int* p = new int;`, `int a[10];`
- `if (cond) stmt` and `if (cond) stmt else stmt`
- `while (cond) stmt`
- `for (init; cond; step) stmt` (rewritten by the parser into a `while` loop inside a block)
- `return;` and `return expr;`
- `delete p;` where `p` is a pointer variable
- Blocks `{ ... }` open a new scope
- Expression statements, e.g. `f(x);` or `x = 3;`

Conditions must have type `bool`; write `x != 0` for an `int`.

## Expressions, highest precedence first

| Level | Operators | Notes |
|-------|-----------|-------|
| Postfix | `f(args)`, `a[i]`, `x++`, `x--` | |
| Prefix | `-x`, `!b`, `*p`, `++x`, `--x` | |
| Multiplicative | `*` `/` `%` | `int` only; `/` and `%` truncate toward zero |
| Additive | `+` `-` | `int` only, so no pointer arithmetic |
| Relational | `<` `<=` `>` `>=` | `int` only |
| Equality | `==` `!=` | same type on both sides; pointers may be compared with `nullptr` |
| Logical and | `&&` | short-circuit |
| Logical or | `\|\|` | short-circuit |
| Assignment | `=` `+=` `-=` `*=` `/=` `%=` | right-associative |

Primary expressions: integer literals (0 to 2147483647), `true`, `false`, `nullptr`, variable
names, `new int`, `new bool`, and parenthesised expressions.

The left side of `=` must be a variable, an array element or `*pointer`. For `++`, `--` and
compound assignment the target must not have side effects (`x`, `a[i]`, `a[3]` or `*p`), because
`x += y` is rewritten as `x = x + y`.

Operands are evaluated left to right. For `=`, the right side is evaluated before the left, as
C++17 requires.

## Built-in

- `print(int)` writes the value and a newline.

## Rejected on purpose

Each of these produces a specific compile error: templates, classes, structs, unions, namespaces,
`using`, exceptions, `goto`, preprocessor directives, string and character literals, `char`,
`float`, `double`, `long`, `unsigned`, `auto`, the address-of operator `&`, bitwise `|`, pointer
arithmetic, `T**`, `void*`, `new T[n]`, arrays of pointers, global variables, assigning a whole
array, and a non-void function that can reach its end without `return`.
