# Project notes

Running log of decisions, gotchas and open items. Newest session at the top of the session log; the sections above it hold the current state of the project.

## Current architecture

- **Single Zephyr app at the repo root** (Zephyr 4.4.0, west workspace `D:\Workspace`, board `nucleo_h723zg`). Zephyr owns startup, clocks, MPU, SysTick, IRQs and linking. The CubeIDE/CubeMX project was removed (it's in git history).
- Three API layers in one app, one LED each:
  - LD1 green PB0: Zephyr GPIO (`led0` DT alias) in `src/main.c`, toggles every 1000 ms.
  - LD2 yellow PE1: STM32Cube HAL GPIO in `src/led_hal.c`, every 500 ms. HAL comes from Zephyr's hal_stm32 module, enabled by `config APP_STM32_HAL_GPIO` → `select USE_STM32_HAL_GPIO` in `Kconfig`.
  - LD3 red PB14: CMSIS registers in `src/led_cmsis.c`, every 250 ms.
- Main loop: 250 ms `k_msleep` tick; LD3 every tick, LD2 every 2nd, LD1 every 4th.
- `boards/nucleo_h723zg.overlay` disables `timers12`, `pwm12` and `/pwmleds`, so the Zephyr PWM driver doesn't claim PB14.
- Clock (upstream board DTS): HSE 8 MHz (ST-Link MCO, bypass) → PLL1 550 MHz CPU, HCLK 275 MHz, APB 137.5 MHz. Console: USART3 (ST-Link VCOM).
- Build size: 21,168 B flash, 4,608 B RAM. Tested on the board (2026-10-27).

## Things to know

- **The Zephyr SDK is unusable for now.** `C:\Users\ksrgo\zephyr-sdk-1.0.1` is unreadable by the user account (Access denied even for icacls; probably installed elevated), and the user env var `ZEPHYR_SDK_INSTALL_DIR` points at it, which forces CMake to require it. Workaround in use: `unset ZEPHYR_SDK_INSTALL_DIR` + `ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb` with CubeIDE's arm-none-eabi-gcc 14.3 (see CLAUDE.md). Fix: in an admin PowerShell, `takeown /f <dir> /r /d y` and `icacls <dir> /grant "ksrgo:(OI)(CI)F" /t`, then run `<dir>\setup.cmd`.
- **STM32Cube HAL/LL Kconfig symbols are promptless.** `CONFIG_USE_STM32_HAL_GPIO=y` in prj.conf is silently ignored; select it from the app `Kconfig`. For H7, `<stm32h7xx_hal.h>` exposes all module headers; Kconfig only decides which HAL .c files compile.
- **`HAL_GetTick()`/`HAL_Delay()` map to `k_uptime_get_32()`/`k_msleep()`** (zephyr/soc/st/stm32/common/stm32cube_hal.c). HAL timeouts work without `HAL_Init`.
- **GPIOB is shared** by the Zephyr driver (PB0) and CMSIS code (PB14). This is safe because CMSIS writes BSRR (single pin), and everything runs in one thread. Keep it BSRR-only if threads are added.
- **Port clocks:** HAL/CMSIS code enables GPIOE/GPIOB clocks itself even though the Zephyr gpio driver already did; enabling is idempotent.
- **The board's pwm-led0 alias still points at the disabled PWM LED.** Don't enable CONFIG_LED_PWM / use pwm-led0 unless the overlay is changed back.
- **CubeIDE recreates `.project` while the old project is still open in its workspace.** Eclipse files are gitignored now. Remove the old h7_blinky project from the CubeIDE workspace (don't delete contents), or import the folder as an *STM32 CMake Project* to use CubeIDE as editor/debugger (Zephyr doc: develop/tools/stm32cubeide.rst, validated on Linux 1.16 only).
- **`west flash` needs `--cli <path>`** because STM32_Programmer_CLI lives in the CubeIDE plugins folder, not on PATH or in Program Files.
- **Python on Windows defaults to cp1252.** Always pass `encoding="utf-8"` when editing repo files from Python; an earlier README edit got mis-encoded this way (fixed 2026-10-27).
- **`.vscode/` and `docs/*.pdf` are gitignored.** The ST PDFs were purged from history on 2026-10-27 (filter-branch + force-push), so commit IDs before that date changed. README lists the download names.

## Static analysis status

- Not run on the Zephyr code yet. `tools/misra_check.sh` and the cppcheck command still assume the old `Core/` layout and HAL include paths.

## Open items

- Fix the Zephyr SDK permissions (see Things to know), or decide to stay on gnuarmemb.
- Adapt the cppcheck and MISRA commands/script to `src/` + Zephyr include paths (+ `build/zephyr/include/generated`), then run them.
- Re-establish the no-malloc rule for Zephyr (the old link-time trick was CubeIDE-specific). Options: `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0`, no `CONFIG_HEAP_MEM_POOL_SIZE`, and a check that `malloc`/`k_malloc` aren't linked.

## Session log

### 2026-10-27 (Zephyr integration)
- User decision: one project with Zephyr owning the chip, plus HAL and CMSIS usable inside it. Removed the CubeIDE project (Core/, Drivers/, .ioc, .cproject, .project, .settings/, .mxproject, linker scripts, launch file). Moved the Zephyr app to the repo root.
- Demo: LD1 via Zephyr, LD2 via HAL (app Kconfig selects USE_STM32_HAL_GPIO), LD3 via CMSIS (overlay frees PB14 from TIM12 PWM). Built with gnuarmemb + CubeIDE GCC 14.3: no warnings in our code; verified the overlay in zephyr.dts and the HAL/app symbols in the ELF. Flashed and tested on the board by the user: all three LEDs blink at their rates.
- Rewrote CLAUDE.md (architecture, layer-mixing rules, west build/flash), README (and fixed its encoding), NOTES.

### 2026-10-27 (Zephyr, first try)
- Added a Zephyr blinky in a `zephyr/` subfolder. The build failed because the SDK was unreadable. Superseded by the integration above.

### 2026-10-27
- Untracked the 4 ST PDFs in docs/ (~74 MB) and gitignored `docs/*.pdf`. Purged them from all history with `git filter-branch` and force-pushed main (user decision). A pre-purge backup bundle was saved in the session scratchpad.

### 2026-10-25 (later)
- Switched from CMSIS to HAL-only (CubeMX workflow, 192 MHz PLL, MPU, `HAL_Init`, HAL GPIO led.c). Superseded by the Zephyr integration.

### 2026-10-24 / 25
- Built a CMSIS-only CubeIDE version (register-level LED driver, own SysTick delay, 64 MHz HSI). Tested on the board. Ran cppcheck + MISRA and fixed the findings. `.vscode/` gitignored. Superseded.
