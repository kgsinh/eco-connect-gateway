# h7_blinky — NUCLEO-H723ZG

Minimal STM32CubeIDE project for the **NUCLEO-H723ZG** board. It serves as a workflow pilot: it shows the build, flash and static-analysis flow on a trivial application before real work starts.

## Status

| Item | State |
|---|---|
| MPU, HAL, clock and GPIO init | Done (CubeMX-generated) |
| User LED (PB0, green) | Switched **on** via BSP at startup |
| Blink loop | **Not implemented yet** — `while (1)` in `main()` is empty |

### Clock configuration (as generated)

| Clock | Source / setting | Frequency |
|---|---|---|
| HSI | Internal RC | 64 MHz |
| PLL1 | HSI / M=4 × N=12 / P=1 | — |
| SYSCLK | PLL1P | 192 MHz |
| HCLK (AHB) | SYSCLK / 2 | 96 MHz |
| APB1/2/3/4 | HCLK / 2 | 48 MHz |

Supply: LDO, voltage scale 2, flash latency 1 WS.

## Project layout

```
Core/
  Inc/, Src/        Application, HAL config, interrupt handlers, MSP init
  Startup/          startup_stm32h723zgtx.s (vector table, reset handler)
Drivers/
  CMSIS/            Core + STM32H7xx device headers
  STM32H7xx_HAL_Driver/
  BSP/              Nucleo board support (LEDs, button, COM)
docs/               Reference manual, programming manual, datasheet, board UM
h7_blinky.ioc       STM32CubeMX configuration
STM32H723ZGTX_FLASH.ld / _RAM.ld   Linker scripts (run from flash / RAM)
```

## Coding rules

- Mixed HAL/CMSIS: HAL where it stays clear, CMSIS register access where direct control matters.
- MISRA C:2012 mandatory and required rules apply.
- No dynamic memory allocation.
- `syscalls.c` and `sysmem.c` are **excluded from the build on purpose**, so any accidental `malloc` or `printf` fails at link time instead of quietly working.

## Build

```
stm32cubeide.exe --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -import D:\Workspace\STM32H7\h7_blinky -cleanBuild h7_blinky/Debug
```

Output: `Debug/h7_blinky.elf`

## Flash

```
STM32_Programmer_CLI.exe -c port=SWD -w Debug/h7_blinky.elf 0x08000000 -v -rst
```

## Static analysis

The work is split between two tools because native cppcheck 2.21 has no MISRA addon.

Native checks:
```
cppcheck --enable=all --inconclusive -ICore/Inc -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32H7xx/Include -IDrivers/STM32H7xx_HAL_Driver/Inc -DSTM32H723xx -DUSE_HAL_DRIVER -D__GNUC__=13 Core/Src Core/Inc
```

MISRA (MPLAB X bundled cppcheck 2.11.1):
```
"C:\Program Files\Microchip\MPLABX\v6.30\mplab_platform\bin\cppcheck\cppcheck.exe" --addon=misra -ICore/Inc -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32H7xx/Include -IDrivers/STM32H7xx_HAL_Driver/Inc -DSTM32H723xx -DUSE_HAL_DRIVER -D__GNUC__=13 Core/Src Core/Inc
```

---

## STM32H723ZG — controller features

Source: datasheet DS13313 Rev 5 ([docs/stm32h723zg.pdf](docs/stm32h723zg.pdf)). The datasheet covers the whole STM32H723xE/G family, so "up to" figures are family maximums. The **ZG** part has 1 MB flash, and the Nucleo board uses the **LQFP144** package.

### Core
- 32-bit Arm Cortex-M7, up to **550 MHz**, 2778 CoreMark
- Double-precision FPU, DSP instructions, MPU
- L1 cache: 32 KB I-cache + 32 KB D-cache (0-wait-state execution from flash and external memory)

### Memories
- **1 MB** embedded flash with ECC (ZG variant)
- **564 KB SRAM**, all with ECC:
  - 128 KB DTCM RAM (critical real-time data)
  - 432 KB system RAM (up to 256 KB can be remapped as ITCM)
  - 4 KB backup SRAM (kept in the lowest-power modes)
- Flexible memory controller (FMC), up to 16-bit bus: SRAM, PSRAM, SDRAM/LPSDR SDRAM, NOR/NAND
- 2× Octo-SPI with execute-in-place (XiP)
- 2× SD/SDIO/MMC
- System bootloader

### Graphics
- Chrom-ART Accelerator (DMA2D)
- LCD-TFT controller, up to XGA resolution

### Clock, reset and supply
- 1.62 V to 3.6 V application supply and I/O
- POR, PDR, PVD, BOR
- Dedicated USB supply, embedded LDO regulator
- Internal oscillators: 64 MHz HSI, 48 MHz HSI48, 4 MHz CSI, 32 kHz LSI
- External oscillators: 4–50 MHz HSE, 32.768 kHz LSE

### Low power
- Sleep, Stop and Standby modes
- V<sub>BAT</sub> supply for RTC and 32 × 32-bit backup registers

### Analog
- 2× 16-bit ADC: up to 3.6 MSPS, up to 18 channels, 7.2 MSPS in double-interleaved mode
- 1× 12-bit ADC: up to 5 MSPS, up to 12 channels
- 2× comparators
- 2× operational amplifiers (GBW = 8 MHz)
- 2× 12-bit DAC
- DFSDM: 8 channels / 4 filters (sigma-delta modulators)
- Digital temperature sensor

### DMA (4 controllers)
- 1× MDMA with linked-list support
- 2× dual-port DMA with FIFO
- 1× basic DMA (BDMA) with request router

### Timers (24 total)
- 17× 16-bit timers (including 5 low-power timers available in Stop mode)
- 4× 32-bit timers
- Each with up to 4 IC/OC/PWM channels or pulse counter and quadrature encoder input
- 2× watchdogs, 1× SysTick

### Communication interfaces (up to 35)
- Up to 5× I²C Fm+ (SMBus/PMBus)
- Up to 6× USART/UART/LPUART in total (SPI, ISO7816, LIN, IrDA, modem control)
- Up to 6× SPI, 4 with multiplexed full-duplex I²S (audio-class accuracy via internal audio PLL or external clock)
- 2× SAI (serial audio interface)
- 1× FD/TTCAN + 2× FDCAN
- 8- to 14-bit camera interface (DCMI)
- 16-bit parallel slave synchronous interface (PSSI)
- SPDIF-IN, HDMI-CEC
- Ethernet MAC with DMA
- USB 2.0 HS/FS OTG with dedicated DMA, on-chip FS PHY, ULPI for external HS PHY
- SWPMI single-wire protocol master
- MDIO slave

### Math acceleration
- CORDIC (trigonometric functions)
- FMAC (filter math accelerator)

### Security and misc
- True random number generator (RNG)
- CRC calculation unit
- RTC with subsecond accuracy and hardware calendar
- ROP, PC-ROP, tamper detection
- 96-bit unique ID

### I/O and debug
- Up to 114 I/O ports with interrupt capability
- SWD and JTAG, 2 KB embedded trace buffer

### Packages
- LQFP100, TFBGA100, **LQFP144** (Nucleo), UFBGA144, all ECOPACK2 compliant

## Reference documents

| File | Contents |
|---|---|
| [docs/rm0468.pdf](docs/rm0468.pdf) | Reference manual: peripherals, registers, memory map |
| [docs/pm0253-stm32h7-series-cortexm7.pdf](docs/pm0253-stm32h7-series-cortexm7.pdf) | Programming manual: Cortex-M7 core, NVIC, instruction set |
| [docs/stm32h723zg.pdf](docs/stm32h723zg.pdf) | Datasheet: electrical characteristics, pinout, package |
| [docs/um3252-stm32h7-series.pdf](docs/um3252-stm32h7-series.pdf) | Nucleo board user manual: board wiring, jumpers |
