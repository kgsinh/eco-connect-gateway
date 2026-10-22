# Project: Nucleo H723ZG — Blinky (workflow pilot)

## Hardware
- Board: NUCLEO-H723ZG (STM32H723ZG, Cortex-M7)
- Onboard User LED: PB0

## Coding standard
- Mixed HAL/CMSIS — no fixed boundary. Use HAL where it doesn't obscure what's happening; use CMSIS register access where direct control matters or HAL would hide something worth being explicit about.
- When the choice isn't obvious, state which was picked and why.
- MISRA C:2025 mandatory + required rules apply regardless of layer used.
- No dynamic memory allocation (malloc/free).

## Build (STM32CubeIDE headless)
Tools are not on PATH; use full paths. `-data` must point to a throwaway workspace — the GUI IDE is usually open and locks its own workspace. Run from Git Bash:

    "/c/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/stm32cubeidec.exe" --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data "<scratch-dir>/ws" -import "D:\Workspace\STM32H7\h7_blinky" -cleanBuild h7_blinky/Debug

Success = log ends with `Build Finished. 0 errors`; output is Debug/h7_blinky.elf. A headless build may rewrite the env-hash in .settings/language.settings.xml — revert that file (`git checkout -- .settings/language.settings.xml`) rather than committing the churn.

## Build notes
- syscalls.c and sysmem.c are excluded from the build in .cproject (Core source entry `excluding=`), and the linker "System calls" option is set to none (no `--specs=nosys.specs`). Both are needed: nosys.specs alone would supply stub _sbrk/_write and let malloc/printf link. Result: any *reachable* malloc/printf fails to link with `undefined reference to _sbrk/_write/...` (verified). Unreachable calls are discarded by --gc-sections and won't error. Do not re-add these files or change the option without discussing it first.
- If the open IDE later saves project settings, check .cproject still has both changes.

## Flash
STM32CubeProgrammer is bundled inside the IDE plugins (path changes when CubeIDE updates):

    "/c/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin/STM32_Programmer_CLI.exe" -c port=SWD -w Debug/h7_blinky.elf -v -rst

Check the probe first with `... STM32_Programmer_CLI.exe -l st-link`.

## Static analysis
Run only when explicitly requested. Two-tool split (native cppcheck 2.21 has no addons/ directory).

Native checks (Drivers/ findings and unusedFunction are suppressed: vendor code, and IRQ handlers/HAL callbacks look unused):

    cppcheck --enable=all --inconclusive --std=c11 --suppress=missingIncludeSystem --suppress=unusedFunction --suppress='*:Drivers/*' -ICore/Inc -IDrivers/BSP/STM32H7xx_Nucleo -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32H7xx/Include -IDrivers/STM32H7xx_HAL_Driver/Inc -DSTM32H723xx -DUSE_HAL_DRIVER -DUSE_PWR_LDO_SUPPLY -D__GNUC__=13 -iCore/Src/syscalls.c -iCore/Src/sysmem.c Core/Src Core/Inc

MISRA C:2025 (MPLAB X's bundled cppcheck 2.11.1 + misra.exe). Use the script — `cppcheck --addon=misra` is broken in this install: it prints "Did not find addon misra", exits 0 and checks nothing.

    tools/misra_check.sh            # all Core/Src except syscalls.c/sysmem.c
    tools/misra_check.sh <files>    # exit 0 = clean, 1 = violations, 2 = tool error

- Rule texts/categories come from docs/misra-c-2025-rule-texts.txt, extracted from the licensed docs/MISRA-C-2025.pdf by tools/misra_rule_texts.py. Both are gitignored: the licence forbids storing copies in a retrieval system. Never commit, push or quote them at length.
- The addon is written for MISRA C:2012 and reports 2012 numbers. Renumbered in 2025: 21.1→20.15, 21.2→5.10, 11.7→merged into 11.4, 17.6→merged into 17.5. 15.5 is Disapplied in 2025 and is suppressed.
- Not checked by the tool at all — review manually when the code touches them:
  - Mandatory: 7.5 9.1 9.7 12.5 17.4 17.9 18.10 19.1 21.13 21.17 21.18 21.22 22.2 22.4 22.6 22.12 22.14 22.20
  - Required: 1.1 1.3 1.5 2.1 5.3 5.10 6.3 7.6 8.3 8.15 8.18 9.6 11.10 11.11 12.6 13.2 14.3 17.5 17.10 17.13 18.1 18.2 18.3 18.6 18.9 19.3 20.6 20.15 21.23 21.24 21.25 21.26 22.1 22.3 22.11 22.13 22.15 22.16 22.17 22.18 22.19 23.2 23.4 23.6 23.8
  - All directives (Dir x.y).
- Baseline (CubeMX-generated code, before any user code): 49× Rule 11.4 (CMSIS peripheral-address casts, e.g. `GPIOB->ODR`; inherent to register access — treat as a documented deviation) and 6× Rule 12.3 (Advisory) in vendor system_stm32h7xx.c.

When requested, fix all findings before returning code. Explain any deviation rather than suppressing silently.

## Reference material
Located in /docs:
- rm0468.pdf — Reference Manual: peripheral registers, bit definitions, memory map. Use for register-level configuration questions.
- pm0253-stm32h7-series-cortexm7.pdf — Programming Manual: Cortex-M7 core, NVIC, exceptions, instruction set. Use for interrupt/core-level questions.
- stm32h723zg.pdf — Datasheet: electrical characteristics, pinout, package. Use for pin/voltage/timing limit questions.
- um3252-stm32h7-series.pdf — Nucleo board User Manual: board-level pin mapping (LED, button, ST-Link), jumpers. Use to confirm board-specific wiring like the LED pin.
- MISRA-C-2025.pdf — MISRA C:2025 (licensed to the user personally; local only, gitignored). Use for rule rationale and compliant examples.
Read the relevant document before writing new CMSIS-level code for anything not touched yet this session — don't rely on memory for register addresses or bit definitions.