# Safety Model

The guarantees come from two layers working together: the compiler rejects programs that could
create an unsafe pointer, and the code it generates (plus the VM itself) checks every remaining
memory access at run time.

## 1. No way to forge a pointer

A pointer can only come from `new`, from `nullptr`, or by copying another pointer. The language
has no address-of operator, no pointer arithmetic, no casts and no integer-to-pointer conversion.
Inside the VM a pointer is a heap handle (1, 2, 3, ...), not a machine address, and handles are
never reused, so even an old handle cannot end up naming a newer object.

## 2. Null checks

Every `*p` is compiled as `<evaluate p>; CHECK_NULL; LOAD_POINTER` (or `STORE_POINTER`).
`CHECK_NULL` throws `NullPointerException` if the handle is 0. Dereferencing the literal `nullptr`
is rejected at compile time.

## 3. Bounds checks

Every `a[i]` is compiled as `LOAD_LOCAL a; <evaluate i>; CHECK_BOUNDS; LOAD_ARRAY`.
The heap stores the size of every object, and `CHECK_BOUNDS` throws
`ArrayIndexOutOfBoundsException` unless `0 <= i < size`. `LOAD_ARRAY` and `STORE_ARRAY_INV` check
again, so bytecode written by hand cannot skip the check.

## 4. Automatic reference counting

Each heap object has a reference count. The code generator follows one convention:

- A local pointer or array variable owns one reference.
- `new` and a call that returns a pointer produce an **owned** value (count already +1); storing
  it transfers that reference.
- Reading a variable produces a **borrowed** value; storing it emits `INC_REF`.
- Pointer arguments are passed as owned references, and the callee releases its parameters when
  it returns.
- `DEC_REF` is emitted when a variable is reassigned (after the new value is retained, so `p = p`
  is safe), at the end of every block, and on every `return` for all locals in scope.
- A pointer result that nobody stores (`new int;` or `make();`) is released immediately.

When a count reaches zero the object is freed at once. `jscpp run file --stats` prints the
number of allocations and frees and how many objects were still alive at exit; for a program that
finishes normally that last number is 0 (see `tests/expected/*.out`).

Cycles cannot form, because heap cells only hold `int`/`bool` values: `int**` and arrays of
pointers are rejected.

## 5. `delete` cannot dangle

`delete p;` releases `p`'s reference and then sets `p` to `nullptr`. If another variable still
refers to the object it stays alive; if not, it is freed. A later `*p` is a clean
`NullPointerException`, never a read of freed memory (`examples/use_after_delete.cppsafe`).

## 6. No undefined behaviour in arithmetic

The VM does `+`, `-` and `*` on `uint32_t` and converts back, so overflow wraps exactly like Java
instead of being undefined behaviour in the host C++. `INT_MIN / -1` gives `INT_MIN`, and
`INT_MIN % -1` gives 0. Division or modulo by zero throws `DivisionByZeroException`.

## 7. Static typing

`int`, `bool` and pointer types never mix implicitly, conditions must be `bool`, and every
non-void function must return on all paths. Since the VM stores everything as an `int`, these
compile-time checks are what stop a boolean or an integer from being used as a pointer.

## 8. Bounded resources

The call stack is limited to 1000 frames (`StackOverflowException`), the operand stack to 100,000
values, and the heap to 1,000,000 live objects.

## 9. Verified bytecode

`.jbc` files are checked before they run: magic number, version, table sizes, every opcode, every
constant and function index, every jump target and `ALLOC` size. Locals and the operand stack are
also range-checked while running. A corrupted file produces an error message instead of reading
outside the VM's memory.

## What happens on an error

The VM stops, prints the exception, the source line and the function, and `jscpp` exits with
status 1:

```
Runtime Error:
DivisionByZeroException: division by zero
    at line 2 in average() (instruction 2)
```
