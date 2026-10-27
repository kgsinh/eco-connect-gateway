# Project notes

Running log of decisions, gotchas and open items. Newest session at the top of the session log; the sections above it hold the current state of the project.

## Current architecture

- **HAL only, CubeMX workflow.** `main.c`, `main.h`, `stm32h7xx_it.c/.h` and `stm32h7xx_hal_msp.c` are CubeMX-generated; our code lives only in `USER CODE` blocks and in `led.c`/`led.h`.
- Clock (from `.ioc`): HSI 64 MHz → PLL1 (M4, N12, P1) = 192 MHz SYSCLK/CPU, AHB /2 = 96 MHz, APB1–4 /2 = 48 MHz. VOS2, flash 1 WS, LDO supply.
- MPU: CubeMX region 0 makes 0x6000_0000–0xDFFF_FFFF no-access/XN (blocks M7 speculative reads to external-memory space).
- Time base: HAL SysTick 1 ms (`HAL_Init` → `HAL_InitTick`, `SysTick_Handler` → `HAL_IncTick`). The delay is `HAL_Delay()`.
- `led.c` / `led.h`: PB0 (LD1) via `HAL_GPIO_Init` / `WritePin` / `TogglePin`. `LED_Init()` is called in `USER CODE 2`.
- Main loop (`USER CODE 3`): `LED_Toggle()` + `HAL_Delay(LED_BLINK_HALF_PERIOD_MS)` (1000 ms).

## Things to know

- **The BSP LED must be disabled in CubeMX.** The `.ioc` still has `NUCLEO-H723ZG.LD1=true`. The generated `BSP_LED_Init(LED_GREEN)` call (outside USER CODE) was removed from main.c by hand, because led.c owns PB0. Until LD1 is unticked in CubeMX (Pinout & Configuration → Middleware and Software Packs / BSP → NUCLEO-H723ZG → untick LD1), regenerating will put `BSP_LED_Init` back. That is harmless (same pin config) but duplicates init. Don't hand-edit the `.ioc` ("do not modify" header).
- **Supply config:** the startup file calls `ExitRun0Mode()` (system_stm32h7xx.c) before `SystemInit()`, which selects the LDO because `USE_PWR_LDO_SUPPLY` is defined; `HAL_PWREx_ConfigSupply()` then confirms it. PWR_CR3 is write-once after power-on. Don't remove that define.
- **PLL1 DIVP** does not allow odd values except 1.
- **`.vscode/` is gitignored** (machine-specific; created by the C/C++ Runner extension). `c_cpp_properties.json` is the user's own config. If false IntelliSense errors appear, point `compilerPath` at CubeIDE's arm-none-eabi-gcc with `intelliSenseMode` `gcc-arm`.
- **`docs/*.pdf` is gitignored.** The ST manuals were untracked on 2026-10-27; local copies are kept, and README says where to download them. History was rewritten on 2026-10-27 to purge them (filter-branch, force-push), so all commit IDs before that date changed.
- **History:** a full-CMSIS version (own SysTick driver, 64 MHz HSI) existed in commits 6043207–bbb873f and was tested on the board. Switched to HAL-only by user decision.

## Static analysis status (last run 2026-10-25)

- cppcheck: only `staticFunction` on `Error_Handler`, a deviation: it is CubeMX-generated, declared in main.h by convention so generated files can call it.
- MISRA: only Rule 11.4 ×55 (baseline 49 plus 6 in led.c). `GPIOB` / `__HAL_RCC_*` macros expand to peripheral-address casts; this is a documented deviation. Rule 12.3 ×6 is in vendor system_stm32h7xx.c (baseline).
- Generated `while (1)` loops violate MISRA 14.4, but the tool doesn't flag them. They are generated code, so they're left as is.

## Open items

- Untick LD1 (BSP LED) in CubeMX so regeneration matches the code (see above).
- HAL version not yet tested on the board.

## Session log

### 2026-10-27
- Untracked the 4 ST PDFs in docs/ (~74 MB) and gitignored `docs/*.pdf`. README notes they must be downloaded. Purged them from all history with `git filter-branch` and force-pushed main (user decision). A pre-purge backup bundle was saved in the session scratchpad.

### 2026-10-25 (later)
- Switched from CMSIS to HAL-only at the user's request. Restored the CubeMX-generated main.c/main.h/stm32h7xx_it.c/.h from the initial commit (now ef9ec0b) (192 MHz PLL, MPU, `HAL_Init`, `HAL_IncTick`). Deleted systick.c/.h. Rewrote led.c with HAL GPIO; hooked it into `USER CODE` blocks; removed the generated BSP LED init/on calls. CLAUDE.md coding standard is now HAL only + CubeMX workflow.

### 2026-10-24 / 25
- Built a CMSIS-only version: register-level LED driver, own SysTick delay, 64 MHz HSI default clock (PLL, MPU, power config and flash wait tweak removed). Tested on the board. Ran cppcheck + MISRA and fixed the findings. `.vscode/` gitignored. Superseded by the HAL switch above.
