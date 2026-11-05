# Bare-metal register blinky

Week-one deliverable 1: a three-LED blinky written against the registers only. No HAL, no RTOS, no CMSIS or vendor headers, and our own startup code and linker script. Each LED toggles at its own rate (the Zephyr app runs a chase instead, so the two images are easy to tell apart).

| LED | Pin | Toggles every |
|---|---|---|
| LD1 green | PB0 | 1000 ms |
| LD2 yellow | PE1 | 500 ms |
| LD3 red | PB14 | 250 ms |

## Files

```
main.c            blink pattern only: which LED toggles how often
blinky.h          main() declaration for startup.c
startup.c         vector table + Reset_Handler (.data copy, .bss zero, main)
stm32h723zg.ld    flash at 0x0800 0000 (boot address); data, bss, stack in DTCM at 0x2000 0000
board/led.c/.h    which pin each LED is on (LD1 PB0, LD2 PE1, LD3 PB14)
drivers/gpio.c/.h GPIO register block, output pin setup, toggle through BSRR
drivers/rcc.c/.h  peripheral clock enables (RCC_AHB4ENR)
drivers/systick.c/.h  1 ms SysTick interrupt, SysTick_Handler, systick_delay_ms()
```

Layers: `main.c` → `board/` → `drivers/`. Drivers know registers but not the board; `board/` knows the wiring; `main.c` knows neither. The include root is this folder (`#include "drivers/gpio.h"`).
- `CMakeLists.txt`, `arm-none-eabi.cmake`, `build.sh`: command-line build with CMake + Ninja and the Arm GCC toolchain.

The core runs on the reset clock, HSI at 64 MHz. Each register address and bit is annotated with its RM0468 or PM0253 section.

## Build and flash

In Git Bash, from any directory (the script loads `tools/zephyr_env.sh` itself):

```
baremetal/blinky/build.sh          # -> baremetal/blinky/build/blinky.elf
baremetal/blinky/build.sh flash    # build, then flash over ST-Link (connect under reset)
```

Flashing this overwrites the Zephyr application. To go back, run `west flash --cli "$STM32_PROGRAMMER_CLI"` from the repo root.
