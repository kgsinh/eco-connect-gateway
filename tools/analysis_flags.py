#!/usr/bin/env python3
"""Print cppcheck preprocessor flags for the app sources, one per line.

Taken from the real build (build/compile_commands.json), so cppcheck sees the
same include paths, defines and Kconfig (autoconf.h) as the compiler. GCC's
predefined macros (__GNUC__, __ARM_ARCH_*, ...) are dumped to
build/analysis/gcc_predef.h, because Zephyr's toolchain headers need them and
cppcheck does not define them.

Usage: tools/analysis_flags.py [build_dir]     (default: build)
Exit 2 if the build directory has no compile_commands.json.
"""
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

APP_SRC = "src/"


def app_entries(cc_path: Path) -> list:
    entries = json.loads(cc_path.read_text(encoding="utf-8"))
    return [e for e in entries
            if Path(e["file"]).as_posix().split("/h7_blinky/")[-1].startswith(APP_SRC)]


def args_of(entry: dict) -> list:
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"], posix=False)


def gcc_predef(compiler: str, cflags: list, out: Path) -> None:
    arch = [a for a in cflags if a.startswith(("-mcpu=", "-mthumb", "-mabi=", "-mfp", "-std="))]
    res = subprocess.run([compiler, *arch, "-dM", "-E", "-x", "c", "-"],
                         input="", capture_output=True, text=True, check=True)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(res.stdout, encoding="utf-8")


def nvic_prio_bits(build: Path) -> str:
    gen = build / "zephyr" / "include" / "generated" / "zephyr" / "devicetree_generated.h"
    m = re.search(r"_P_arm_num_irq_priority_bits (\d+)", gen.read_text(encoding="utf-8"))
    if m is None:
        raise SystemExit(f"arm,num-irq-priority-bits not found in {gen}")
    return m.group(1)


def main() -> int:
    build = Path(sys.argv[1] if len(sys.argv) > 1 else "build")
    cc_path = build / "compile_commands.json"
    if not cc_path.is_file():
        print(f"{cc_path} not found - run west build first", file=sys.stderr)
        return 2

    entries = app_entries(cc_path)
    if not entries:
        print(f"no {APP_SRC} entries in {cc_path}", file=sys.stderr)
        return 2

    # All app files share one target, so the first entry's flags apply to all.
    args = args_of(entries[0])
    predef = build / "analysis" / "gcc_predef.h"
    gcc_predef(args[0], args[1:], predef)

    # Forced includes (gcc predefs, then -imacros/-include in compiler order)
    # go through one prelude header: cppcheck mishandles 3+ --include options
    # (the predef macros were lost and picolibc hit "#error Endianess").
    forced = [predef.resolve().as_posix()]
    flags = []
    it = iter(args[1:])
    for a in it:
        if a in ("-isystem", "-imacros", "-include"):
            val = next(it)
            if a == "-isystem":
                flags.append(f"-I{val}")
            else:
                forced.append(val)
        elif a.startswith(("-I", "-D")):
            flags.append(a)

    # cppcheck can't parse GCC register binding (`register T r4 __asm__("r4")`
    # in zephyr/arch/arm/arm-m-switch.h) and drops every file that includes
    # the kernel. Erase the binding for analysis only; being function-like,
    # the macro leaves `__asm__ volatile(...)` statements alone.
    #
    # cppcheck 2.11 (MISRA) fails to expand NUM_IRQ_PRIO_BITS
    # (DT_PROP -> "Invalid ## usage when expanding DT_CAT3") and then dumps an
    # empty file, so misra.exe reports nothing. Pre-define it with the value
    # from the generated devicetree and skip nvic.h via its include guard.
    prio_bits = nvic_prio_bits(build)
    prelude = build / "analysis" / "prelude.h"
    prelude.write_text("".join(f'#include "{f}"\n' for f in forced)
                       + "#define __asm__(reg)\n"
                       + "#define ZEPHYR_INCLUDE_ARCH_ARM_CORTEX_M_NVIC_H_\n"
                       + f"#define NUM_IRQ_PRIO_BITS {prio_bits}\n", encoding="utf-8")
    flags.insert(0, f"--include={prelude.resolve().as_posix()}")
    # Raw bytes with LF: on Windows, print() emits CRLF and bash keeps the \r
    sys.stdout.buffer.write(("\n".join(flags) + "\n").encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
