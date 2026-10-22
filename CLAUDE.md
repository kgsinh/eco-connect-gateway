# Project: Nucleo H723ZG — Blinky (workflow pilot)

## Hardware
- Board: NUCLEO-H723ZG (STM32H723ZG, Cortex-M7)
- Onboard User LED: PB0

## Coding standard
- Mixed HAL/CMSIS — no fixed boundary. Use HAL where it doesn't obscure what's happening; use CMSIS register access where direct control matters or HAL would hide something worth being explicit about.
- When the choice isn't obvious, state which was picked and why.
- MISRA C:2012 mandatory + required rules apply regardless of layer used.
- No dynamic memory allocation (malloc/free).

## Build (STM32CubeIDE headless)
    stm32cubeide.exe --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -import D:\Workspace\STM32H7\h7_blinky -cleanBuild h7_blinky/Debug

## Build notes
- sysmem.c and syscalls.c are deliberately excluded from the build. This enforces "no dynamic allocation" and "no printf/semihosting" at link time rather than by convention — an accidental malloc or printf call will fail to link rather than silently working. Do not re-add these files without discussing it first.

## Flash
    STM32_Programmer_CLI.exe -c port=SWD -w <path-to-.elf-or-.bin> 0x08000000 -v -rst

## Static analysis
Run only when explicitly requested. Two-tool split (native cppcheck 2.21 has no addons/ directory):

Native checks:
    cppcheck --enable=all --inconclusive -ICore/Inc -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32H7xx/Include -IDrivers/STM32H7xx_HAL_Driver/Inc -DSTM32H723xx -DUSE_HAL_DRIVER -D__GNUC__=13 Core/Src Core/Inc

MISRA (via MPLAB X's bundled cppcheck 2.11.1 + misra.exe):
    "C:\Program Files\Microchip\MPLABX\v6.30\mplab_platform\bin\cppcheck\cppcheck.exe" --addon=misra -ICore/Inc -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32H7xx/Include -IDrivers/STM32H7xx_HAL_Driver/Inc -DSTM32H723xx -DUSE_HAL_DRIVER -D__GNUC__=13 Core/Src Core/Inc

When requested, fix all findings before returning code. Explain any deviation rather than suppressing silently.

## Reference material
Located in /docs:
- rm0468.pdf — Reference Manual: peripheral registers, bit definitions, memory map. Use for register-level configuration questions.
- pm0253-stm32h7-series-cortexm7.pdf — Programming Manual: Cortex-M7 core, NVIC, exceptions, instruction set. Use for interrupt/core-level questions.
- stm32h723zg.pdf — Datasheet: electrical characteristics, pinout, package. Use for pin/voltage/timing limit questions.
- um3252-stm32h7-series.pdf — Nucleo board User Manual: board-level pin mapping (LED, button, ST-Link), jumpers. Use to confirm board-specific wiring like the LED pin.
Read the relevant document before writing new CMSIS-level code for anything not touched yet this session — don't rely on memory for register addresses or bit definitions.