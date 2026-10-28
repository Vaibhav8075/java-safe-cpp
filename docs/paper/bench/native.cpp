// Native C++ versions of the three benchmarks. The size is read from the command line so the
// optimiser cannot fold the work away at compile time. Same arithmetic as the .cppsafe files.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "fib";
    int scale = argc > 2 ? std::atoi(argv[2]) : 1;
    if (!std::strcmp(which, "fib")) {
        std::printf("%d\n", fib(27 * scale));
    } else if (!std::strcmp(which, "loop")) {
        int s = 0;
        for (int i = 0; i < 3000000 * scale; i++) s = (int)((uint32_t)s + (uint32_t)(i % 7));
        std::printf("%d\n", s);
    } else {
        int a[1500]; int seed = 12345;
        for (int i = 0; i < 1500; i++) { seed = (int)(((int64_t)seed * 1103 + 12345) % 100003); a[i] = seed; }
        for (int i = 0; i < 1499 * scale; i++)
            for (int j = 0; j < 1499 - i; j++)
                if (a[j] > a[j + 1]) { int t = a[j]; a[j] = a[j + 1]; a[j + 1] = t; }
        std::printf("%d\n%d\n", a[0], a[1499]);
    }
}
