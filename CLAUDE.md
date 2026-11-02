# Project: Eco-Connect IoT Gateway — Nucleo H723ZG, Zephyr + HAL + CMSIS (workflow pilot)

## Project notes
Read NOTES.md at the start of every session: it holds the current architecture, gotchas and open items. Keep it up to date as you work. When something worth remembering comes up (a "things to know" item, a decision, a deviation, an open item, what was tested on hardware), add it there in the same session. Add a dated entry to its session log and update the sections above it so they always describe the current state. Keep it terse.

## Hardware
- Board: NUCLEO-H723ZG (STM32H723ZG, Cortex-M7)
- User LEDs: LD1 green PB0, LD2 yellow PE1, LD3 red PB14. User button PC13. Console: USART3 (ST-Link VCOM).
- Ethernet: ENC28J60 module (10BASE-T, 3.3 V) on SPI1: SCK PA5/D13, MISO PA6/D12, MOSI PB5/D11, CS PD14/D10, INT PD15/D9. The on-board RMII MAC + LAN8742A is disabled in the overlay (stretch goal for later).
- Sensor: BME280 breakout on I2C1 (SCL PB8/D15, SDA PB9/D14), address 0x76.
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
- Comments: Doxygen style, written the way an experienced firmware engineer would. Applies to all our code (src/, baremetal/).
  - Every .c/.h starts with `/** @file` + `@brief` (one line), then a short paragraph only if the file needs context (ownership, clock, constraints).
  - Public functions are documented once, in the header: `@brief`, `@param`, `@return` (`@retval` for specific error codes), `@note` / `@pre` only when they carry real information. Static functions get a `/** @brief ... */` in the .c. Don't repeat the header doc in the .c.
  - Struct members, enum values and non-obvious macros: trailing `/**< ... */`. Group related register/bit macros with `@name` / `@{` `@}` where it helps.
  - Inline `/* */` comments only where the code can't say it: why something is done, hardware constraints, required ordering, the RM/datasheet section a value comes from. Never restate what the line does.
  - Short, technical, present tense. No narration, no conversational asides, no filler, no decorative banners, no references to the task, the session or who wrote it.

## Build
West workspace: D:\Workspace (Zephyr 4.4.0); west finds it automatically from the repo. From the repo root, Git Bash.

Versions are pinned in `west.yml` (Zephyr v4.4.0, modules cmsis_6, hal_stm32, picolibc, hal_st, littlefs via `name-allowlist`). D:\Workspace still uses its own manifest (zephyr/west.yml) at the same commits; west.yml only applies to a workspace made with `west init -l`. A new module (e.g. a sensor HAL) goes in the allowlist. Upgrading Zephyr = change `revision`, `west update`, rebuild, retest, update this file.

The Zephyr SDK (C:\Users\ksrgo\zephyr-sdk-1.0.1) is currently unreadable by the user account (see NOTES.md), and the user env var `ZEPHYR_SDK_INSTALL_DIR` points at it, which forces CMake to look for it. Until that's fixed, build with CubeIDE's Arm GCC via the gnuarmemb toolchain variant. `tools/zephyr_env.sh` sets that up (and `$STM32_PROGRAMMER_CLI`):

    source tools/zephyr_env.sh
    west build -p always -b nucleo_h723zg                                   # release (-Os)
    west build -p always -b nucleo_h723zg -- -DEXTRA_CONF_FILE=debug.conf  # debug (-Og, asserts, thread info)

Success = memory-usage table and `Generating files from .../zephyr.elf`. Output: build/zephyr/zephyr.elf (build/ is gitignored). Both variants use the same build dir, so check which one is in it before flashing (`grep DEBUG_OPTIMIZATIONS build/zephyr/.config`). The first configure takes ~2 min. Toolchain paths in zephyr_env.sh change when CubeIDE updates.

## Flash
STM32CubeProgrammer is bundled inside the CubeIDE plugins (path changes when CubeIDE updates):

    source tools/zephyr_env.sh
    west flash --cli "$STM32_PROGRAMMER_CLI"

Check the probe first with `"$STM32_PROGRAMMER_CLI" -l st-link` ("No ST-Link detected!" = board unplugged).

## Debug
VS Code + Cortex-Debug (installed) with ST's ST-LINK_gdbserver, arm-none-eabi-gdb and the STM32H723 SVD, all from the CubeIDE install. Committed templates are in tools/vscode/ (`launch.json`, `tasks.json`); the live copies are in .vscode/ (gitignored). Copy them again if .vscode is regenerated. Configs:
- "Debug (ST-LINK, build + flash, stop at main)": runs the "Zephyr: build (debug)" task, flashes, stops at main.
- "Attach (ST-LINK, no reset, no flash)": halts the running firmware where it is; the ELF in build/ must match what's flashed.
Without a debugger: Zephyr prints a fatal-error dump (faulting PC/LR, thread) on the console. Resolve addresses with `arm-none-eabi-addr2line -e build/zephyr/zephyr.elf <addr>` (gdb/addr2line live next to GDB in the CubeIDE toolchain bin). Shell: `kernel threads`, `kernel stacks`, `device list`.

## Console
ST-Link VCOM (COM5 on this PC; confirm with `Get-CimInstance Win32_PnPEntity | ? Name -match 'COM\d+'`), 115200 8N1. Boot log + Zephyr shell (`uart:~$`). Only one program can hold the port. To read it non-interactively, use pyserial (installed). For interactive use, `python -m serial.tools.miniterm --filter direct COM5 115200`. Without `--filter direct`, miniterm prints the shell's ANSI codes as `␛[..m`.

## Static analysis
Run only when explicitly requested. Both scripts need a current `west build` (they take include paths, defines and Kconfig from build/compile_commands.json via tools/analysis_flags.py) and report only findings in src/. From the repo root, Git Bash:

    tools/cppcheck_check.sh     # general checks, ~3 min
    tools/misra_check.sh        # MISRA C:2025, ~7 min
    # both: [files...] to limit scope; exit 0 = clean, 1 = findings, 2 = tool/setup error

- Tools: native cppcheck 2.21 (on PATH) for both preprocessing and general checks. MISRA uses the official cppcheck **2.21.0 misra.py addon** (misra.py, cppcheckdata.py, misra_9.py from github.com/danmar/cppcheck tag 2.21.0, GPLv3), run with Python from `D:\Workspace\cppcheck-addons-2.21.0` (outside the repo; override with `MISRA_ADDON_DIR`). The native install ships no addons/.
- Don't go back to MPLAB X's cppcheck 2.11.1 + misra.exe: its preprocessor can't expand Zephyr macros (`IS_ENABLED`, devicetree `DT_CAT3`) and dumps an empty file for anything that includes the kernel, so it reports "clean" without checking. misra.exe also can't read 2.21 dumps. misra_check.sh refuses dumps under 1 MB for this reason.
- analysis_flags.py works around cppcheck limits in a generated build/analysis/prelude.h: one forced include (3+ `--include` options lose macros), GCC predefined macros, `__asm__(reg)` erased (GCC register binding in arm-m-switch.h), and `NUM_IRQ_PRIO_BITS` pre-set from the devicetree. It writes LF line endings (Windows CRLF broke the flags).
- Deviations live in tools/misra_deviations.txt (ID, rules, regex on the source line, rationale). tools/misra_filter.py applies them and prints a count per deviation. Add an entry only for findings that come from Zephyr/HAL/CMSIS macro expansions, never for our own code. cppcheck_check.sh drops invalidPointerCast/knownConditionTrueFalse on `LOG_*` lines (Zephyr logging macro expansion) and prints the count.
- Rule texts/categories come from docs/misra-c-2025-rule-texts.txt, extracted from the licensed docs/MISRA-C-2025.pdf by tools/misra_rule_texts.py. Both are gitignored: the licence forbids storing copies in a retrieval system. Never commit, push or quote them at length.
- The addon is written for MISRA C:2012 and reports 2012 numbers. Renumbered in 2025: 21.1→20.15, 21.2→5.10, 11.7→merged into 11.4, 17.6→merged into 17.5. 15.5 is Disapplied in 2025 and is suppressed.
- Not checked by the tool at all (list derived for the 2.11 addon; the 2.21 addon may cover a few more, so treat it as conservative) — review manually when the code touches them:
  - Mandatory: 7.5 9.1 9.7 12.5 17.4 17.9 18.10 19.1 21.13 21.17 21.18 21.22 22.2 22.4 22.6 22.12 22.14 22.20
  - Required: 1.1 1.3 1.5 2.1 5.3 5.10 6.3 7.6 8.3 8.15 8.18 9.6 11.10 11.11 12.6 13.2 14.3 17.5 17.10 17.13 18.1 18.2 18.3 18.6 18.9 19.3 20.6 20.15 21.23 21.24 21.25 21.26 22.1 22.3 22.11 22.13 22.15 22.16 22.17 22.18 22.19 23.2 23.4 23.6 23.8
  - All directives (Dir x.y).
- Only analyse our code (src/), not Zephyr or the HAL module.

When requested, fix all findings before returning code. Explain any deviation rather than suppressing silently.

## Reference material
Located in /docs (gitignored, local only — see README for download names):
- rm0468.pdf — Reference Manual: peripheral registers, bit definitions, memory map. Use for register-level configuration questions.
- pm0253-stm32h7-series-cortexm7.pdf — Programming Manual: Cortex-M7 core, NVIC, exceptions, instruction set. Use for interrupt/core-level questions.
- stm32h723zg.pdf — Datasheet: electrical characteristics, pinout, package. Use for pin/voltage/timing limit questions.
- um3252-stm32h7-series.pdf — X-CUBE-CLASSB self-test library guide for STM32H7 (IEC 60730 Class B). Not the board manual.
- Board manual (Nucleo-144 user manual UM2407: jumpers, solder bridges, Ethernet/LED wiring) is not in docs/ yet — download it before relying on board-level wiring details; until then the Zephyr board DTS is the reference.
- Board-Suitability-P1-P2.pdf, ESC-Embedded-Software-Design-Mentor.pdf (7-stage method), Presentation - Embedded Design Cohort - Batch 01.pdf (week-one deliverables) — mentor's cohort material; the requirements this project is built against.
- Arm-Cortex-M7-Processor-Datasheet.pdf — Arm's Cortex-M7 processor overview.
- enc28j60.pdf — ENC28J60 data sheet (DS39662C). bst-bme280-ds002.pdf — BME280 data sheet (rev 1.24).
- MISRA-C-2025.pdf — MISRA C:2025 (licensed to the user personally; local only). Use for rule rationale and compliant examples.
Read the relevant document before writing new CMSIS-level code for anything not touched yet this session — don't rely on memory for register addresses or bit definitions. For Zephyr APIs, read the headers/docs in D:\Workspace\zephyr.
