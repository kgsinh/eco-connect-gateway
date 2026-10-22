"""Build a cppcheck misra.py --rule-texts file from the licensed MISRA C:2025 PDF.

The output is derived from a personally licensed document: keep it local
(it is gitignored), never commit or publish it.

Usage:
    pdftotext -layout docs/MISRA-C-2025.pdf layout.txt
    pdftotext docs/MISRA-C-2025.pdf raw.txt
    python -I tools/misra_rule_texts.py layout.txt docs/misra-c-2025-rule-texts.txt <path-to-addons/misra.py> raw.txt

Uses the per-rule headings in the body (Rule X.Y <headline> ... Category <cat>),
which extract cleanly, rather than the Appendix A table, which does not.
Also prints which 2025 rules the cppcheck addon does not implement.
"""
import re
import sys
from collections import Counter

lines = open(sys.argv[1], encoding="utf-8", errors="ignore").read().replace("\f", "\n").split("\n")
app_a = next(i for i, l in enumerate(lines) if i > 1000 and "A.1 Active guidelines" in l)

HEAD = re.compile(r"^\s*(Rule|Dir) (\d+\.\d+)\s+([A-Z_<\[(].*)$")
CAT_LINE = re.compile(r"^\s*Category\b")
CAT_WORD = re.compile(r"\b(Mandatory|Required|Advisory|Disapplied)\b")


def category_after(j):
    # The value may sit on the Category line or a few lines below it (layout columns).
    for k in range(j, min(j + 8, app_a)):
        w = CAT_WORD.search(lines[k])
        if w:
            return w.group(1)
    return None


def first_col(s):
    # Layout mode can put a page header to the right; keep only the left column.
    return re.split(r"\s{3,}", s.strip())[0]


found = {}
for i, line in enumerate(lines[:app_a]):
    m = HEAD.match(line)
    if not m:
        continue
    kind, num, text = m.group(1), m.group(2), first_col(m.group(3))
    cat = None
    joining = True
    for j in range(i + 1, min(i + 12, app_a)):
        if CAT_LINE.match(lines[j]):
            cat = category_after(j)
            break
        if HEAD.match(lines[j]):
            break
        if not lines[j].strip():
            joining = False
            continue
        if joining:
            text += " " + first_col(lines[j])
    if cat and (kind, num) not in found:
        found[(kind, num)] = (cat, re.sub(r"\s+", " ", text).strip())


# Fallback: plain (non-layout) pdftotext output keeps some headings intact that
# layout mode splits across columns. Fill only what is still missing.
if len(sys.argv) > 4:
    raw = open(sys.argv[4], encoding="utf-8", errors="ignore").read().split("\n")
    raw_end = next(i for i, l in enumerate(raw) if i > 1000 and "A.1 Active guidelines" in l)
    raw_head = re.compile(r"^(Rule|Dir) (\d+\.\d+) ([A-Z_<#\[(].*)$")
    for i, line in enumerate(raw[:raw_end]):
        m = raw_head.match(line)
        if not m or (m.group(1), m.group(2)) in found:
            continue
        window = raw[i + 1:i + 7]
        if not any(l.startswith("Category") for l in window):
            continue
        for l in window:
            w = CAT_WORD.search(l)
            if w:
                found[(m.group(1), m.group(2))] = (w.group(1), m.group(3).strip())
                break


def key(r):
    return tuple(int(x) for x in r.split("."))


rules = {n: v for (k, n), v in found.items() if k == "Rule"}
dirs = {n: v for (k, n), v in found.items() if k == "Dir"}
print("rules", len(rules), "dirs", len(dirs), dict(Counter(v[0] for v in rules.values())))

with open(sys.argv[2], "w", encoding="utf-8") as f:
    f.write("Appendix A Summary of guidelines\n")
    for r in sorted(rules, key=key):
        f.write(f"Rule {r} {rules[r][0]}\n{rules[r][1]}\n\n")
    f.write("Appendix B\n")

src = open(sys.argv[3], encoding="utf-8", errors="ignore").read()
impl = {f"{a}.{b}" for a, b in re.findall(r"def misra_(\d+)_(\d+)\(", src)}
active = {r for r, v in rules.items() if v[0] != "Disapplied"}
missing = sorted(active - impl, key=key)
for c in ("Mandatory", "Required", "Advisory"):
    print(f"NOT_CHECKED_{c}:", " ".join(r for r in missing if rules[r][0] == c))
print("CHECKED_BUT_NOT_ACTIVE:", " ".join(sorted(impl - active, key=key)))
print("DISAPPLIED:", " ".join(sorted((r for r, v in rules.items() if v[0] == "Disapplied"), key=key)))
print("DIRS:", " ".join(f"{d}({v[0][0]})" for d, v in sorted(dirs.items(), key=lambda x: key(x[0]))))
