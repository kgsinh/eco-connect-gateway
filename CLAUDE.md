# Project: Nucleo H723ZG — Zephyr + HAL + CMSIS (workflow pilot)

## Project notes
Read NOTES.md at the start of every session: it holds the current architecture, gotchas and open items. Keep it up to date as you work. When something worth remembering comes up (a "things to know" item, a decision, a deviation, an open item, what was tested on hardware), add it there in the same session. Add a dated entry to its session log and update the sections above it so they always describe the current state. Keep it terse.

## Hardware
- Board: NUCLEO-H723ZG (STM32H723ZG, Cortex-M7)
- User LEDs: LD1 green PB0, LD2 yellow PE1, LD3 red PB14. User button PC13. Console: USART3 (ST-Link VCOM).
- Board definition (pins, clocks, console) is upstream: D:\Workspace\zephyr\boards\st\nucleo_h723zg. Read it before changing hardware config; override in boards/nucleo_h723zg.overlay, never edit the upstream tree.

## Architecture
This repo is a single Zephyr application (repo root = app dir). Zephyr owns startup, vector table, clocks (devicetree), MPU, system timer (SysTick), interrupts and linking. Inside the app, code may use any of three layers:
- **Zephyr APIs** (devicetree, drivers, kernel) — default choice.
- **STM32Cube HAL/LL** from Zephyr's hal_stm32 module (`<stm32h7xx_hal.h>`, `<stm32_ll_*.h>`). HAL/LL modules are promptless Kconfig symbols: enable one by adding a `config APP_...` entry that `select`s `USE_STM32_HAL_<X>` / `USE_STM32_LL_<X>` in the app `Kconfig`, not in prj.conf.
- **CMSIS register access** (`GPIOB->BSRR`, `RCC->...`) via `<soc.h>`.

Pick the layer per job; when the choice isn't obvious, state which was picked and why.

Rules for mixing:
- One owner per peripheral (per pin for GPIO). If HAL/CMSIS drives something a Zephyr driver would claim, disable that node in the overlay (`status = "disabled"`), as done for TIM12/PWM12 on PB14.
- Never touch clocks tree config (`HAL_RCC_OscConfig`/`ClockConfig`, PLL/RCC_CFGR registers), SysTick, or NVIC directly — Zephyr owns them. Peripheral clock *enables* (`__HAL_RCC_xxx_CLK_ENABLE`, `RCC->AHBxENR |=`) are fine.
- Interrupts for HAL/CMSIS-driven peripherals: `IRQ_CONNECT()` + `irq_enable()`, not vector-table edits.
- No `HAL_Init()`, `SystemClock_Config()`, `HAL_MspInit()`. `HAL_GetTick()`/`HAL_Delay()` are already mapped to the Zephyr kernel (zephyr/soc/st/stm32/common/stm32cube_hal.c).
- Shared GPIO ports: write single pins with BSRR (HAL WritePin/TogglePin do), never `ODR ^=`/`ODR =` read-modify-write from different threads.

## Coding standard
- MISRA C:2025 mandatory + required rules apply to our code (src/) regardless of layer used.
- No dynamic memory allocation (malloc/free, k_malloc).
- Zephyr code style: tabs, braces on every block.

## Build
West workspace: D:\Workspace (Zephyr 4.4.0); west finds it automatically from the repo. From the repo root, Git Bash.

The Zephyr SDK (C:\Users\ksrgo\zephyr-sdk-1.0.1) is currently unreadable by the user account (see NOTES.md), and the user env var `ZEPHYR_SDK_INSTALL_DIR` points at it, which forces CMake to look for it. Until that's fixed, build with CubeIDE's Arm GCC via the gnuarmemb toolchain variant:

    unset ZEPHYR_SDK_INSTALL_DIR
    export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
    export GNUARMEMB_TOOLCHAIN_PATH="C:/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools"
    west build -p always -b nucleo_h723zg

Success = memory-usage table and `Generating files from .../zephyr.elf`. Output: build/zephyr/zephyr.elf (build/ is gitignored). The first configure takes ~2 min. The toolchain path changes when CubeIDE updates.

## Flash
STM32CubeProgrammer is bundled inside the CubeIDE plugins (path changes when CubeIDE updates):

    west flash --cli "C:/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin/STM32_Programmer_CLI.exe"

Check the probe first with `<that exe> -l st-link`.

## Static analysis
Run only when explicitly requested. **Needs rework for the Zephyr layout** (open item in NOTES.md): the commands must target src/ and add Zephyr's include paths plus build/zephyr/include/generated. Tool facts that still hold:

- Native cppcheck 2.21 for general checks; MISRA C:2025 via MPLAB X's bundled cppcheck 2.11.1 + misra.exe through tools/misra_check.sh — `cppcheck --addon=misra` is broken in this install: it prints "Did not find addon misra", exits 0 and checks nothing. Script exit: 0 = clean, 1 = violations, 2 = tool error.
- Rule texts/categories come from docs/misra-c-2025-rule-texts.txt, extracted from the licensed docs/MISRA-C-2025.pdf by tools/misra_rule_texts.py. Both are gitignored: the licence forbids storing copies in a retrieval system. Never commit, push or quote them at length.
- The addon is written for MISRA C:2012 and reports 2012 numbers. Renumbered in 2025: 21.1→20.15, 21.2→5.10, 11.7→merged into 11.4, 17.6→merged into 17.5. 15.5 is Disapplied in 2025 and is suppressed.
- Not checked by the tool at all — review manually when the code touches them:
  - Mandatory: 7.5 9.1 9.7 12.5 17.4 17.9 18.10 19.1 21.13 21.17 21.18 21.22 22.2 22.4 22.6 22.12 22.14 22.20
  - Required: 1.1 1.3 1.5 2.1 5.3 5.10 6.3 7.6 8.3 8.15 8.18 9.6 11.10 11.11 12.6 13.2 14.3 17.5 17.10 17.13 18.1 18.2 18.3 18.6 18.9 19.3 20.6 20.15 21.23 21.24 21.25 21.26 22.1 22.3 22.11 22.13 22.15 22.16 22.17 22.18 22.19 23.2 23.4 23.6 23.8
  - All directives (Dir x.y).
- Rule 11.4 on CMSIS peripheral-address casts (`GPIOB->ODR`, also inside HAL macros like `GPIOE`) is inherent to register access — documented deviation. Only analyse our code (src/), not Zephyr or the HAL module.

When requested, fix all findings before returning code. Explain any deviation rather than suppressing silently.

## Reference material
Located in /docs (gitignored, local only — see README for download names):
- rm0468.pdf — Reference Manual: peripheral registers, bit definitions, memory map. Use for register-level configuration questions.
- pm0253-stm32h7-series-cortexm7.pdf — Programming Manual: Cortex-M7 core, NVIC, exceptions, instruction set. Use for interrupt/core-level questions.
- stm32h723zg.pdf — Datasheet: electrical characteristics, pinout, package. Use for pin/voltage/timing limit questions.
- um3252-stm32h7-series.pdf — X-CUBE-CLASSB self-test library guide for STM32H7 (IEC 60730 Class B). Not the board manual.
- Board manual (Nucleo-144 user manual UM2407: jumpers, solder bridges, Ethernet/LED wiring) is not in docs/ yet — download it before relying on board-level wiring details; until then the Zephyr board DTS is the reference.
- MISRA-C-2025.pdf — MISRA C:2025 (licensed to the user personally; local only). Use for rule rationale and compliant examples.
Read the relevant document before writing new CMSIS-level code for anything not touched yet this session — don't rely on memory for register addresses or bit definitions. For Zephyr APIs, read the headers/docs in D:\Workspace\zephyr.
