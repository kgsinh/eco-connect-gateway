#!/usr/bin/env bash
# MISRA C:2025 check of the app sources: native cppcheck 2.21 --dump, then the
# official cppcheck 2.21.0 misra.py addon (run with Python).
#
# Why not MPLAB X's cppcheck 2.11.1 + misra.exe: its preprocessor can't expand
# Zephyr macros (IS_ENABLED, devicetree DT_CAT3) and silently dumps an empty
# file for anything that includes the kernel, and misra.exe can't read 2.21
# dumps. Native cppcheck 2.21 ships no addons/, so the addon scripts
# (misra.py, cppcheckdata.py, misra_9.py from github.com/danmar/cppcheck tag
# 2.21.0, GPLv3) live outside the repo in $MISRA_ADDON_DIR.
#
# Preprocessor flags come from the real build via tools/analysis_flags.py, so
# run `west build` first. Only findings in src/ are reported; documented
# deviations (tools/misra_deviations.txt) are applied and counted by
# tools/misra_filter.py.
#
# Usage (from project root, Git Bash):  tools/misra_check.sh [files...]
# Default: every .c file in src/. Takes ~6-7 min for src/ (dumps are 250-330 MB
# each and are deleted afterwards).
# Exit code: 0 = clean, 1 = findings remain, 2 = tool/setup error.
set -u

MISRA_ADDON_DIR="${MISRA_ADDON_DIR:-/d/Workspace/cppcheck-addons-2.21.0}"
RULE_TEXTS="docs/misra-c-2025-rule-texts.txt"
# 15.5 is Disapplied in MISRA C:2025; the addon still implements it from 2012.
# The addon reports 2012 numbers. Renumbered in 2025 (report under the new number):
#   21.1 -> 20.15, 21.2 -> 5.10, 11.7 -> merged into 11.4, 17.6 -> merged into 17.5
SUPPRESS="15.5"

if [ ! -f "$RULE_TEXTS" ]; then
    echo "Missing $RULE_TEXTS - regenerate it with tools/misra_rule_texts.py" >&2
    exit 2
fi
if [ ! -f "$MISRA_ADDON_DIR/misra.py" ]; then
    echo "Missing $MISRA_ADDON_DIR/misra.py - see CLAUDE.md (Static analysis)" >&2
    exit 2
fi

mapfile -t flags < <(python tools/analysis_flags.py) || exit 2
[ "${#flags[@]}" -gt 0 ] || exit 2

if [ "$#" -gt 0 ]; then
    files=("$@")
else
    files=(src/*.c)
fi

dumps=("${files[@]/%/.dump}")
trap 'rm -f "${dumps[@]}" "${files[@]/%/.ctu-info}"' EXIT

cppcheck --dump --quiet --std=c11 --platform=unix32 --max-configs=1 \
    "${flags[@]}" "${files[@]}" > /dev/null 2>&1 || exit 2

# A dump this small means the preprocessor gave up (the old setup's silent
# "no violations" failure mode); a real dump with Zephyr headers is >100 MB.
for d in "${dumps[@]}"; do
    size=$(stat -c %s "$d" 2>/dev/null || echo 0)
    if [ "$size" -lt 1000000 ]; then
        echo "Dump for ${d%.dump} is missing or truncated ($size bytes) - preprocessing failed" >&2
        exit 2
    fi
done

# -E -s: ignore the user's Python env/site; the script dir stays on sys.path
# so misra.py finds cppcheckdata.py and misra_9.py next to it.
python -E -s "$MISRA_ADDON_DIR/misra.py" --rule-texts="$RULE_TEXTS" \
    --suppress-rules "$SUPPRESS" "${dumps[@]}" 2>&1 | python tools/misra_filter.py
exit "${PIPESTATUS[1]}"
