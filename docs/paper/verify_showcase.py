"""Independent check of examples/showcase.cppsafe.

Recomputes every value the showcase program prints, in plain Python, and compares them with
what the jscpp virtual machine prints.

Usage: python docs/paper/verify_showcase.py path/to/jscpp
Exit status 0 when all values match.
"""
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def fib(n):
    return n if n < 2 else fib(n - 1) + fib(n - 2)


def digit_sum(n):
    return sum(int(c) for c in str(n))


def collatz(n):
    steps = 0
    while n != 1:
        n = n // 2 if n % 2 == 0 else 3 * n + 1
        steps += 1
    return steps


def is_prime(n):
    if n < 2:
        return False
    d = 2
    while d * d <= n:
        if n % d == 0:
            return False
        d += 1
    return True


def expected():
    out = [fib(20), math.factorial(10), math.gcd(1071, 462), 2 ** 20, digit_sum(98765), collatz(27),
           sum(is_prime(i) for i in range(60))]
    a, seed = [], 17
    for _ in range(10):
        seed = (seed * 31 + 7) % 101
        a.append(seed)
    a.sort()
    out += a + [sum(a)]
    target, lo, hi, found = a[6], 0, 9, -1
    while lo <= hi and found < 0:
        mid = (lo + hi) // 2
        if a[mid] == target:
            found = mid
        elif a[mid] < target:
            lo = mid + 1
        else:
            hi = mid - 1
    out.append(found)
    out.append(sum((k + 1) * (k + 1) for k in range(4)))      # diagonal of the 4x4 product table
    out += [100 + 5 + 7, 9, 3, 100 + 5 + 7, 1]                 # counter, swap (x, y), alias after delete, counter == nullptr
    out += [0, 1]                                              # short-circuit guard, isPrime(97) && !isPrime(91)
    out.append(-2 ** 31)                                       # INT_MAX + 1 wraps
    return out


if __name__ == "__main__":
    exe = str(Path(sys.argv[1]).resolve())
    run = subprocess.run([exe, "run", str(ROOT / "examples/showcase.cppsafe")], capture_output=True, text=True)
    got = [int(x) for x in run.stdout.split()]
    want = expected()
    print(f"{len(got)} values from the VM, {len(want)} from Python:", "MATCH" if got == want else "DIFFER")
    sys.exit(0 if got == want else 1)
