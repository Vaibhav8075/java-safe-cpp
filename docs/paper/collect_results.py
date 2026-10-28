"""Runs the real compiler and writes every number used in the paper to results/*.json.

Usage (from the repository root, after building both a normal and a Release build):
    python docs/paper/collect_results.py build/jscpp build-release/jscpp

Nothing in the paper is typed by hand: make_paper_assets.py reads these files.
"""
import json
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / "results"
OUT.mkdir(exist_ok=True)

exe = Path(sys.argv[1]).resolve()
exe_rel = Path(sys.argv[2]).resolve()
if not exe.exists() and exe.with_suffix(".exe").exists():
    exe = exe.with_suffix(".exe")
if not exe_rel.exists() and exe_rel.with_suffix(".exe").exists():
    exe_rel = exe_rel.with_suffix(".exe")


def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True, cwd=ROOT, **kw)


def save(name, data):
    (OUT / f"{name}.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


# ---------------------------------------------------------------- code size
loc = {}
for module in ["lexer", "parser", "semantic", "bytecode", "vm", "common"]:
    files = list((ROOT / "src" / module).glob("*"))
    loc[module] = sum(len(f.read_text(encoding="utf-8").splitlines()) for f in files)
loc["main"] = len((ROOT / "src" / "main.cpp").read_text(encoding="utf-8").splitlines())
loc["total"] = sum(loc.values())
opcodes = re.search(r"enum class Opcode \{(.*?)\};", (ROOT / "src/bytecode/Opcode.h").read_text(), re.S).group(1)
loc["opcodes"] = len(re.findall(r"\b[A-Z][A-Z_]+\b", re.sub(r"//.*", "", opcodes)))
save("code_size", loc)

# ---------------------------------------------------------------- test suite
ctest = run(["ctest", "--test-dir", "build"])
m = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", ctest.stdout)
names = re.findall(r"Test +#\d+: (\w+)\.", ctest.stdout)
save("tests", {
    "total": int(m.group(3)), "failed": int(m.group(2)),
    "run": names.count("run"), "bytecode": names.count("bytecode"), "reject": names.count("reject"),
    "examples": len(list((ROOT / "examples").glob("*.cppsafe"))),
    "rejected_programs": len(list((ROOT / "tests/compile_errors").glob("*.cppsafe"))),
})

# ---------------------------------------------------------------- audit
audit = (ROOT / "docs/audit-and-plan.md").read_text(encoding="utf-8")
rows = re.findall(r"^\| (\d+) \| (Critical|High|Medium) \|", audit, re.M)
sev = [s for _, s in rows]
save("audit", {"total": len(rows), "critical": sev.count("Critical"), "high": sev.count("High"),
               "medium": sev.count("Medium")})

# ---------------------------------------------------------------- showcase program
show = ROOT / "examples/showcase.cppsafe"
tokens = run([exe, "tokens", show]).stdout.strip().splitlines()
ast = run([exe, "ast", show]).stdout.strip().splitlines()
dump = run([exe, "dump", show]).stdout
code_lines = [l for l in dump.splitlines() if re.match(r"\s+\d{4}\s", l)]
trace = run([exe, "run", show, "--trace"]).stdout.splitlines()
stats = re.search(r"allocations=(\d+) frees=(\d+) peak-live=(\d+) live-at-exit=(\d+)",
                  run([exe, "run", show, "--stats"]).stdout)
count_op = lambda op: sum(1 for l in code_lines if re.search(rf"\s{op}(\s|$)", l))
save("showcase", {
    "source_lines": len(show.read_text(encoding="utf-8").splitlines()),
    "functions": len(re.findall(r"^#\d+ ", dump, re.M)),
    "tokens": len(tokens), "ast_nodes": len(ast), "instructions": len(code_lines),
    "executed": sum(1 for l in trace if l.startswith("IP=")),
    "check_null": count_op("CHECK_NULL"), "check_bounds": count_op("CHECK_BOUNDS"),
    "inc_ref": count_op("INC_REF"), "dec_ref": count_op("DEC_REF"),
    "allocations": int(stats.group(1)), "frees": int(stats.group(2)),
    "peak_live": int(stats.group(3)), "live_at_exit": int(stats.group(4)),
    "printed_values": len(run([exe, "run", show]).stdout.split()),
    "python_match": run([sys.executable, ROOT / "docs/paper/verify_showcase.py", exe]).returncode == 0,
})

# ---------------------------------------------------------------- heap balance across all examples
heap = []
for f in sorted((ROOT / "examples").glob("*.cppsafe")):
    r = run([exe, "run", f, "--stats"])
    s = re.search(r"allocations=(\d+) frees=(\d+) peak-live=(\d+) live-at-exit=(\d+)", r.stdout)
    heap.append({"name": f.stem, "exit": r.returncode, "allocations": int(s.group(1)), "frees": int(s.group(2)),
                 "live_at_exit": int(s.group(4))})
clean = [h for h in heap if h["exit"] == 0]
save("heap", {"programs": heap, "clean_programs": len(clean),
              "clean_leaks": sum(h["live_at_exit"] for h in clean),
              "clean_allocations": sum(h["allocations"] for h in clean),
              "clean_frees": sum(h["frees"] for h in clean)})

# ---------------------------------------------------------------- safety demos
safety = []
for f in sorted((ROOT / "demo/safety").glob("*.cppsafe")):
    r = run([exe, "run", f])
    text = r.stderr
    exc = re.search(r"(\w+Exception)", text)
    comp = re.search(r"error: (.*)", text)
    where = re.search(r"at line (\d+) in (\w+)\(\)", text) or re.search(r":(\d+):\d+: error", text)
    safety.append({
        "name": f.stem, "exit": r.returncode, "stage": "compile" if comp else "run",
        "outcome": exc.group(1) if exc else ("compile error" if comp else "completes"),
        "line": int(where.group(1)) if where else None,
        "stdout": r.stdout.split(),
    })
save("safety", {"demos": safety})


# ---------------------------------------------------------------- Linux (WSL) test run
linux = {"available": False}
try:
    r = subprocess.run(["wsl.exe", "--cd", str(ROOT), "-e", "bash", "-lc",
                        "cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release >/dev/null && "
                        "cmake --build build-linux -j4 >/dev/null 2>&1 && ctest --test-dir build-linux | tail -4; "
                        "g++ --version | head -1"], capture_output=True, timeout=900)
    out = r.stdout.decode("utf-8", "replace").replace("\x00", "")
    m = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", out)
    g = re.search(r"g\+\+ \(.*?\) (\d+\.\d+\.\d+)", out)
    if m:
        linux = {"available": True, "total": int(m.group(3)), "failed": int(m.group(2)),
                 "gpp": g.group(1) if g else "unknown"}
except Exception as exc:  # WSL not installed
    linux["error"] = str(exc)
save("linux_tests", linux)

# ---------------------------------------------------------------- performance
def timed(cmd, reps=7):
    times = []
    for _ in range(reps):
        t0 = time.perf_counter()
        r = run(cmd)
        times.append(time.perf_counter() - t0)
        assert r.returncode == 0, (cmd, r.stderr)
    return {"median": statistics.median(times), "min": min(times), "max": max(times), "reps": reps}


native = ROOT / "docs/paper/bench/native"
subprocess.run(["g++", "-O2", str(ROOT / "docs/paper/bench/native.cpp"), "-o", str(native)], check=True)
native_exe = native.with_suffix(".exe") if native.with_suffix(".exe").exists() else native
hello = ROOT / "examples/hello.cppsafe"
perf = {"startup_vm": timed([exe_rel, "run", hello]), "startup_native": timed([native_exe, "fib", "0"])}
for prog, nat in [("fib27", "fib"), ("loop3m", "loop"), ("sort1500", "sort")]:
    src = ROOT / f"docs/paper/bench/{prog}.cppsafe"
    perf[prog] = {"vm_release": timed([exe_rel, "run", src]), "vm_default": timed([exe, "run", src]),
                  "native": timed([native_exe, nat, "1"])}
    out_vm = run([exe_rel, "run", src]).stdout.split()
    out_nat = run([native_exe, nat, "1"]).stdout.split()
    perf[prog]["outputs_match"] = out_vm == out_nat
native.unlink(missing_ok=True)
native.with_suffix(".exe").unlink(missing_ok=True)

cpu = subprocess.run(["powershell", "-NoProfile", "-Command",
                      "(Get-CimInstance Win32_Processor).Name"], capture_output=True, text=True).stdout.strip()
gpp = subprocess.run(["g++", "--version"], capture_output=True, text=True).stdout.splitlines()[0]
perf["machine"] = {"cpu": cpu, "compiler": gpp, "os": "Windows 11", "release_flags": "CMAKE_BUILD_TYPE=Release (-O3)",
                   "default_flags": "no optimisation flags (CMAKE_BUILD_TYPE empty)"}
save("performance", perf)
print("wrote", sorted(p.name for p in OUT.glob("*.json")))
