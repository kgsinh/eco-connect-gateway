#!/usr/bin/env bash
# General cppcheck (native 2.21, on PATH) of the app sources.
#
# Preprocessor flags come from the real build via tools/analysis_flags.py, so
# run `west build` first. Only findings in src/ are reported, not in
# Zephyr/HAL/CMSIS headers. unusedFunction is skipped: ISRs, callbacks and
# API functions called only from Zephyr look unused.
#
# Findings that come from inside Zephyr's LOG_* macro expansion on a LOG_*
# line (invalidPointerCast: cbprintf packaging casts; knownConditionTrueFalse:
# compile-time log-level check) are dropped and counted, not hidden silently.
#
# Usage (from project root, Git Bash):  tools/cppcheck_check.sh [files...]
# Default: src/. Takes ~3 min (normal check level).
# Exit code: 0 = clean, 1 = findings, 2 = tool/setup error.
set -u

mapfile -t flags < <(python tools/analysis_flags.py) || exit 2
[ "${#flags[@]}" -gt 0 ] || exit 2

if [ "$#" -gt 0 ]; then
    targets=("$@")
else
    targets=(src)
fi

out="$(cppcheck --enable=warning,style,performance,portability --inconclusive --std=c11 \
    --platform=unix32 --max-configs=1 \
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
    "${flags[@]}" "${targets[@]}" 2>&1)" || exit 2

src_findings="$(printf '%s\n' "$out" | grep -E '^src[/\\]' | grep -v ': information: ' | sort -u || true)"

kept=""
dropped=0
while IFS= read -r line; do
    [ -n "$line" ] || continue
    file="${line%%:*}"
    rest="${line#*:}"
    lineno="${rest%%:*}"
    case "$line" in
        *'[invalidPointerCast]'|*'[knownConditionTrueFalse]')
            if sed -n "${lineno}p" "${file//\\//}" | grep -qE '\bLOG_(ERR|WRN|INF|DBG)\('; then
                dropped=$((dropped + 1))
                continue
            fi
            ;;
    esac
    kept+="$line"$'\n'
done <<< "$src_findings"

[ "$dropped" -gt 0 ] && echo "($dropped finding(s) from Zephyr LOG_* macro expansion dropped)"
if [ -n "$kept" ]; then
    printf '%s' "$kept"
    exit 1
fi
echo "cppcheck: no findings in ${targets[*]}."
