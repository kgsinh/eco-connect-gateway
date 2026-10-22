#!/usr/bin/env bash
# MISRA C:2025 check of project sources using MPLAB X's bundled cppcheck 2.11.1 + misra.exe.
#
# `cppcheck --addon=misra` does not work with this install: it prints
# "Did not find addon misra" and exits 0, so it silently checks nothing.
# This script runs the two stages explicitly instead.
#
# Usage (from project root, Git Bash):  tools/misra_check.sh [files...]
# Default: every .c file in Core/Src except the excluded syscalls.c/sysmem.c.
# Exit code: 0 = no violations, 1 = violations found, 2 = tool/setup error.
set -u

CPPCHECK_DIR="/c/Program Files/Microchip/MPLABX/v6.30/mplab_platform/bin/cppcheck"
RULE_TEXTS="docs/misra-c-2025-rule-texts.txt"
# 15.5 is Disapplied in MISRA C:2025; the addon still implements it from 2012.
# The addon reports 2012 numbers. Renumbered in 2025 (report under the new number):
#   21.1 -> 20.15, 21.2 -> 5.10, 11.7 -> merged into 11.4, 17.6 -> merged into 17.5
SUPPRESS="15.5"

if [ ! -f "$RULE_TEXTS" ]; then
    echo "Missing $RULE_TEXTS - regenerate it with tools/misra_rule_texts.py" >&2
    exit 2
fi

if [ "$#" -gt 0 ]; then
    files=("$@")
else
    files=()
    for f in Core/Src/*.c; do
        case "$f" in
            */syscalls.c|*/sysmem.c) ;;
            *) files+=("$f") ;;
        esac
    done
fi

"$CPPCHECK_DIR/cppcheck.exe" --dump --quiet --platform=win64 --std=c11 \
    -ICore/Inc -IDrivers/BSP/STM32H7xx_Nucleo -IDrivers/CMSIS/Include \
    -IDrivers/CMSIS/Device/ST/STM32H7xx/Include -IDrivers/STM32H7xx_HAL_Driver/Inc \
    -DSTM32H723xx -DUSE_HAL_DRIVER -DUSE_PWR_LDO_SUPPLY -D__GNUC__=13 \
    "${files[@]}" > /dev/null 2>&1 || exit 2

dumps=()
for f in "${files[@]}"; do
    if [ ! -f "$f.dump" ]; then
        echo "cppcheck produced no dump for $f" >&2
        exit 2
    fi
    dumps+=("$f.dump")
done

out="$("$CPPCHECK_DIR/addons/misra.exe" --rule-texts="$RULE_TEXTS" --suppress-rules "$SUPPRESS" "${dumps[@]}" 2>&1)"
rm -f "${dumps[@]}" "${files[@]/%/.ctu-info}"

# Show only findings in the files checked, not in vendor headers they include.
findings="$(printf '%s\n' "$out" | grep -E '^\[[^]]+\] \(style\)' | grep -vE '^\[Drivers[/\\]' || true)"
if [ -n "$findings" ]; then
    printf '%s\n' "$findings"
    echo
    echo "By rule:"
    printf '%s\n' "$findings" | grep -oE '\((Mandatory|Required|Advisory|Undefined)\) \[misra-c2012-[0-9.]+\]' | sort | uniq -c | sort -rn
    exit 1
fi
echo "MISRA C:2025: no violations in ${#files[@]} file(s)."
