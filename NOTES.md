# Project notes

Running log of decisions, gotchas and open items. Newest session at the top of the session log; the sections above it hold the current state of the project.

## Current architecture

- `main.c` is hand-written CMSIS. No HAL calls remain in application code.
- Clock: reset default left by `SystemInit()`. HSI 64 MHz, all prescalers /1, no PLL, LDO supply, VOS3.
- Flash: reset default 7 WS, left as `SystemInit()` sets it. That is safe at any clock; 1 WS (WRHIGHFREQ 01) would be enough for 64 MHz at VOS3 (RM0468 Table 16) and is purely a speed-up.
- `led.c` / `led.h`: PB0 (LD1) push-pull output, CMSIS registers. On/Off are single BSRR writes; Toggle reads ODR, then writes BSRR.
- `systick.c` / `systick.h`: 1 ms SysTick on the processor clock (lowest priority), with its own `volatile` ms counter, `Tick_GetMs()` and `Tick_DelayMs()`. The delay is wrap-safe and lasts between ms−1 and ms.
- No MPU config, no power config, no `HAL_Init()`, no `MX_GPIO_Init()`, no BSP calls.
- `Drivers/` (HAL, BSP, CMSIS) is untouched and still compiled; `--gc-sections` drops what is unused.

## Things to know

- **Do not regenerate code from `h7_blinky.ioc`.** main.c no longer has `USER CODE` blocks, so CubeMX would overwrite it, and it would add `SysTick_Handler` back to `stm32h7xx_it.c` (duplicate-definition link error).
- **HAL tick is dead.** `SysTick_Handler` lives in `systick.c` and does not call `HAL_IncTick()`, so `HAL_GetTick()` never changes and any HAL call with a timeout hangs. If a HAL driver is brought in, add `HAL_IncTick()` to our handler.
- **`HAL_MspInit()` no longer runs** (only `HAL_Init()` called it). The SYSCFG clock is therefore not enabled; turn it on if EXTI/SYSCFG is ever used.
- **MPU removed.** That is fine while caches are off and no external memory is used. If the I/D cache, FMC or OCTOSPI is enabled, restore the background region (0x6000_0000–0xDFFF_FFFF no-access, XN) to stop M7 speculative reads there.
- **Raising the clock later:** set VOS first, then flash wait states, then switch SYSCLK (RM0468 Table 16). Previously verified PLL config: HSI/M4 = 16 MHz ref, ×N12 = 192 MHz VCO (wide), P/1 = 192 MHz CPU, AHB /2 = 96 MHz, APB /2 = 48 MHz at VOS2 with 1 WS. PLL1 DIVP does not allow odd values except 1.
- **Supply config (PWR_CR3) is write-once after power-on.** Writing it clears SCUEN. The supply *is* configured: the startup file calls `ExitRun0Mode()` (system_stm32h7xx.c) before `SystemInit()`. With `USE_PWR_LDO_SUPPLY` defined in the build, it sets LDOEN and waits for ACTVOSRDY. Don't remove that define.
- **The main loop uses `while(1)`.** That breaks MISRA Rule 14.4 (non-boolean controlling expression); `for (;;)` passes. It was kept deliberately by the user, so expect it in MISRA output.
- **VS Code IntelliSense shows false errors** ("FLASH is undefined", etc.): `.vscode/` is missing `STM32H723xx`, `USE_HAL_DRIVER` and the include paths. The real build is clean.
- **`.vscode/` is untracked** and was not created by Claude; leave it alone unless asked.

## Open items

- MISRA / cppcheck not yet run on `led.c`, `systick.c` or the new `main.c`.
- Optional: fix the `.vscode` IntelliSense config.

## Session log

### 2026-10-24 / 25
- Added a CMSIS LED driver (`led.c`/`led.h`) for PB0.
- Rewrote `main.c` in CMSIS: first a full PLL to 192 MHz setup with MPU and power config, then simplified at the user's request to the default HSI 64 MHz with MPU and power config removed.
- Flash latency lowered from 7 WS to 1 WS.
- Blink half-period tried at 2000 ms; the user set it back to 1000 ms. Tested on the board with `HAL_Delay`: works.
- Flash 1 WS is an optimisation, not a requirement: 7 WS is correct at any clock. Removed at the user's request; flash runs at the 7 WS default.
- Replaced `SysTick_Config()`/`HAL_Delay()` with our own register-level `systick.c`. It builds clean; **not yet tested on the board.**
- Removed `SysTick_Handler` from `stm32h7xx_it.c`/`.h`; stripped the pin defines and BSP include from `main.h`.
