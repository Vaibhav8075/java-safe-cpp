"""Self-check for the paper sources: every \\res key exists, every \\cite key is in the bib,
every \\ref has a label, every included file exists, and no TBD / placeholder text is left.

Usage: python docs/paper/check_paper.py [--submission]
--submission also fails on the member-name placeholders in the author block.
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
tex = (HERE / "main.tex").read_text(encoding="utf-8")
bib = (HERE / "references.bib").read_text(encoding="utf-8")
numbers = (HERE / "generated/numbers.tex").read_text(encoding="utf-8")
problems = []

for key in sorted(set(re.findall(r"\\res\{([\w]+)\}", tex))):
    if f"res@{key}\\endcsname" not in numbers:
        problems.append(f"\\res key without a result: {key}")

bib_keys = set(re.findall(r"@\w+\{([^,\s]+),", bib))
cited = set()
for group in re.findall(r"\\cite\{([^}]+)\}", tex):
    cited.update(k.strip() for k in group.split(","))
for k in sorted(cited - bib_keys):
    problems.append(f"cited but not in references.bib: {k}")
for k in sorted(bib_keys - cited):
    problems.append(f"in references.bib but never cited: {k}")

labels = set(re.findall(r"\\label\{([^}]+)\}", tex))
for ref in sorted(set(re.findall(r"\\(?:ref|autoref)\{([^}]+)\}", tex)) - labels):
    problems.append(f"\\ref without label: {ref}")

for f in re.findall(r"\\input\{([^}]+)\}", tex):
    if not (HERE / (f if f.endswith(".tex") else f + ".tex")).exists():
        problems.append(f"missing input file: {f}")

if "TBD" in tex or "TODO" in tex.replace("% TODO", ""):
    problems.append("TBD/TODO text in main.tex")
if "--submission" in sys.argv and "[Name of" in tex:
    problems.append("author placeholders still present")

if problems:
    print("\n".join(problems))
    sys.exit(1)
print(f"paper check OK: {len(cited)} citations, {len(labels)} labels")
