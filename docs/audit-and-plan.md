# Audit and Implementation Plan

This file records the faults found in the original version (commit `fc82250`), how each one was
confirmed, and what was changed. Every "before" result below came from building and running that
commit; every "after" result is covered by the CTest suite (`ctest --test-dir build`).

## 1. Faults found

Severity: **Critical** = wrong results, hangs, or a claimed safety feature that does not work.
**High** = accepts invalid programs or behaves in an undefined way. **Medium** = robustness,
portability or usability.

| # | Severity | Area | Fault | How it showed up before the fix |
|---|----------|------|-------|-------------------------------|
| 1 | Critical | Codegen | `&&` and `||` emitted a jump to address 0 that was never patched | `if (true \|\| false)` looped forever (killed after 3 s) |
| 2 | Critical | Codegen | Parameter slots were offset by the locals of earlier functions, so `CALL` wrote arguments into the wrong slots | `twice(21)` calling `add(n, n)` printed `0` instead of `42` |
| 3 | Critical | ARC | Locals declared at function level were never released; `DEC_REF` only ran for nested `{ }` blocks | `refcount.cppsafe` claims `p` is freed; the bytecode had no `DEC_REF` at all |
| 4 | Critical | ARC | Arrays were allocated with refcount 0 and never released | every `int a[N]` leaked |
| 5 | High | Semantic | Functions had to be defined before use (no forward references) | `main` calling a later `add` was a compile error |
| 6 | High | Semantic | Any expression was accepted on the left of `=` | `5 = 3;` compiled and ran, corrupting the operand stack |
| 7 | High | Parser/Codegen | Global variables parsed, but codegen read them from the current frame | `int g = 9;` then `print(g)` printed `0` |
| 8 | High | VM | Signed overflow in `ADD`/`SUB`/`MUL` and `INT_MIN / -1` are undefined behaviour in the host C++ | contradicts the "no undefined behaviour" goal |
| 9 | High | Semantic | Non-void functions could fall off the end (undefined behaviour in C++) | silently returned 0 |
| 10 | High | Semantic | Whole-array assignment was allowed (`a = b`) | two variables would own one heap object |
| 11 | High | Bytecode | `CALL` used `std::hash` of the function name: differs between standard libraries and can collide | `.jbc` files were not portable between compilers |
| 12 | High | VM | `.jbc` files were trusted completely: no checks on opcodes, constant indices, jump targets or locals | a corrupted file could read outside VM memory |
| 13 | Medium | VM/CLI | Runtime exceptions still exited with status 0 | scripts and tests could not detect failures |
| 14 | Medium | Lexer | Integer literals above `INT_MAX` crashed in `std::stoi` | message was just `Error: stoi` |
| 15 | Medium | Lexer | `std::isalpha`/`isdigit` called on plain `char` (undefined for bytes >= 0x80) | the UTF-16 `test.cppsafe` in the repo contains such bytes |
| 16 | Medium | Parser | `++`, `--` were lexed but not parsed; no `+=` etc. | `i++;` was a syntax error |
| 17 | Medium | Parser | `expressionStatement()` read `expr->loc` in the same call that moved `expr` | use-after-move, depends on argument evaluation order |
| 18 | Medium | `delete` | `delete p` dropped a reference but left `p` pointing at the freed object | a later use, or `DEC_REF` at block exit, would hit the freed object |
| 19 | Medium | Docs | README listed "pointer arithmetic" as supported; it was (correctly) rejected | documentation did not match behaviour |
| 20 | Medium | Repo | No tests at all; `CMakeLists.txt` referenced a missing `tests/` folder; stray UTF-16 file | nothing guarded against regressions |

## 2. Implementation plan

Work was ordered so each step could be checked on its own.

1. **Hygiene**: ignore every build folder, remove the stray UTF-16 file, strip trailing whitespace.
2. **Lexer**: widen chars before `<cctype>` calls, range-check literals, report unterminated
   comments, give specific errors for excluded keywords, types, strings and `&`; lex `+= -= *= /= %=`.
3. **Parser**: reject globals, `T**` and `new T[n]`; desugar `++`/`--`/`op=` into plain
   assignments on side-effect-free targets; fix the use-after-move.
4. **Semantic analysis**: declare all functions first; require a valid `main`; check l-values;
   forbid whole-array assignment, arrays of pointers, `void*`, `new void`; require a return on
   every path of non-void functions; record declared types for the code generator.
5. **Bytecode**: ordered function table indexed by `CALL`; a source line per instruction;
   explicit little-endian file format (version 2); a verifier run on every load.
6. **Code generator**: real short-circuit jumps; per-function slot numbering with parameters in
   slots 0..n-1; a single ARC convention (owned vs borrowed values) applied to declarations,
   assignment, arguments, returns, block exit and `delete`.
7. **VM**: wrapping 32-bit arithmetic, range checks on every stack/local/heap access, runtime
   errors with line and function, non-zero exit status, heap statistics (`--stats`), stack shown
   in `--trace`.
8. **CLI**: `tokens` and `ast` commands to show each phase, compiler errors that quote the source
   line with a caret.
9. **Tests**: every example checked for exact output, exit status and exception text, from source
   and through a `.jbc` round trip; 28 programs that must be rejected with a specific message.
10. **Docs**: README, language/bytecode/safety specs and the report updated to match the code.

## 3. Status

All ten steps are done. Results from the final build:

- `ctest --test-dir build`: **60/60 passed** on Windows (MinGW g++ 15.2) and on Ubuntu under WSL
  (g++ 15.2).
- Build is warning-free with `-Wall -Wextra -Wpedantic`.
- A `.jbc` file compiled on Windows runs on Linux with identical output.
- `examples/ownership.cppsafe --stats`: `allocations=13 frees=13 live-at-exit=0`.

## 4. Known limitations (not bugs, kept out of scope)

- Reference counting cannot free cycles, but the subset has no pointers inside heap objects
  (`int**` and arrays of pointers are rejected), so cycles cannot be built.
- Only `int`, `bool`, `int*`, `bool*` and fixed-size local arrays exist; no strings, structs,
  globals or heap arrays (`new int[n]`).
- `print` only takes an `int`.
- The VM is a straightforward interpreter; it is built for clarity, not speed.
