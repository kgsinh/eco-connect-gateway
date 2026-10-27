#!/usr/bin/env python3
"""Filter misra.py output: keep findings in src/, apply documented deviations.

Reads misra.py output on stdin. Deviations come from tools/misra_deviations.txt
(ID | rules | regex on the source line | rationale). Prints the remaining
findings, a per-rule summary and a per-deviation count.
Exit 0 = no remaining findings, 1 = findings remain.
"""
import re
import sys
from collections import Counter
from pathlib import Path

FINDING = re.compile(r"^\[(src[/\\][^:\]]+):(\d+)\] \(style\) (.*) "
                     r"\((Mandatory|Required|Advisory|Undefined)\) \[misra-c2012-([\d.]+)\]$")


def load_deviations(path: Path) -> list:
    devs = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        # Fields are separated by " | " (spaced) so regexes can use | freely
        dev_id, rules, regex, _rationale = (p.strip() for p in line.split(" | ", 3))
        rule_set = None if rules == "*" else {r.strip() for r in rules.split(",")}
        devs.append((dev_id, rule_set, re.compile(regex)))
    return devs


def main() -> int:
    devs = load_deviations(Path("tools/misra_deviations.txt"))
    src_cache = {}
    kept = set()
    applied = Counter()

    for line in sys.stdin:
        m = FINDING.match(line.rstrip("\r\n"))
        if m is None:
            continue
        file, lineno, rule = m.group(1).replace("\\", "/"), int(m.group(2)), m.group(5)
        if file not in src_cache:
            src_cache[file] = Path(file).read_text(encoding="utf-8").splitlines()
        lines = src_cache[file]
        text = lines[lineno - 1] if 0 < lineno <= len(lines) else ""
        for dev_id, rule_set, regex in devs:
            if (rule_set is None or rule in rule_set) and regex.search(text):
                applied[dev_id] += 1
                break
        else:
            kept.add((file, lineno, m.group(4), rule, m.group(3)))

    for dev_id, _rules, _regex in devs:
        if applied[dev_id]:
            print(f"Deviation {dev_id}: {applied[dev_id]} finding(s)")
    if not kept:
        print("MISRA C:2025: no findings outside documented deviations.")
        return 0

    print()
    for file, lineno, cat, rule, msg in sorted(kept):
        print(f"[{file}:{lineno}] {msg} ({cat}) [misra-c2012-{rule}]")
    print("\nBy rule:")
    for (cat, rule), n in Counter((k[2], k[3]) for k in kept).most_common():
        print(f"  {n:4d} ({cat}) {rule}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
