# Bare-metal register blinky

Week-one deliverable 1: the same three-LED blinky as the Zephyr app, written against the registers only. No HAL, no RTOS, no CMSIS or vendor headers, and our own startup code and linker script.

| LED | Pin | Period |
|---|---|---|
| LD1 green | PB0 | 1000 ms |
| LD2 yellow | PE1 | 500 ms |
| LD3 red | PB14 | 250 ms |

## Files

- `startup.c`: vector table (initial stack pointer + core exception handlers) and `Reset_Handler`, which copies `.data` to RAM, zeroes `.bss` and calls `main()`.
- `stm32h723zg.ld`: flash at 0x0800 0000 (the boot address), data, bss and stack in DTCM at 0x2000 0000.
- `main.c`: RCC clock enable, GPIO setup through `MODER`, LED writes through `BSRR`, a 1 ms SysTick tick polled through `COUNTFLAG`.
- `CMakeLists.txt`, `arm-none-eabi.cmake`, `build.sh`: command-line build with CMake + Ninja and the Arm GCC toolchain.

The core runs on the reset clock, HSI at 64 MHz. Each register address and bit is annotated with its RM0468 or PM0253 section.

## Build and flash

In Git Bash, from any directory (the script loads `tools/zephyr_env.sh` itself):

```
baremetal/blinky/build.sh          # -> baremetal/blinky/build/blinky.elf
baremetal/blinky/build.sh flash    # build, then flash over ST-Link (connect under reset)
```

Flashing this overwrites the Zephyr application. To go back, run `west flash --cli "$STM32_PROGRAMMER_CLI"` from the repo root.
