# Eco-Connect IoT Gateway — NUCLEO-H723ZG

Zephyr application for the **NUCLEO-H723ZG** board that mixes three API layers in one firmware: Zephyr APIs, STM32Cube HAL, and CMSIS register access. It serves as a workflow pilot for build, flash and static analysis before real work starts.

## Status

LED chase: one LED on at a time, LD1 → LD2 → LD3, 250 ms per step. (The bare-metal blinky in `baremetal/blinky` blinks all three independently, so the running image is easy to tell apart.)

| LED | Pin | Driven by | Chase order |
|---|---|---|---|
| LD1 green | PB0 | Zephyr GPIO API (devicetree `led0`), `src/main.c` | 1st |
| LD2 yellow | PE1 | STM32Cube HAL GPIO, `src/led_hal.c` | 2nd |
| LD3 red | PB14 | CMSIS registers, `src/led_cmsis.c` | 3rd |

**Ethernet** (`src/app_net.c`): on-board RMII Ethernet (LAN8742A PHY) via Zephyr's `eth_stm32_hal` driver. The board answers ping on two IPv4 addresses:

- **Static `192.168.50.2/24`**, for a cable straight to a PC. Set the PC's Ethernet adapter to `192.168.50.1` / `255.255.255.0`, no gateway, then `ping 192.168.50.2`. Change or disable it with `CONFIG_APP_NET_STATIC_IPV4_ADDR` in `prj.conf` (empty = off).
- **DHCP**, for a router. The leased address is printed on the console.

Tested: 100 Mb full-duplex link, ping from the PC over a direct cable.

**Console:** ST-Link virtual COM port, 115200 8N1. It shows boot logs and the Zephyr shell (`uart:~$`; try `net iface`, `net ping <ip>`, `help`). The shell uses ANSI colour/cursor codes, so use a terminal that understands them: `python -m serial.tools.miniterm --filter direct COMx 115200` or PuTTY. Plain miniterm shows them as `␛[..m`.

See NOTES.md for current state, gotchas and open items, and CLAUDE.md for the rules on mixing layers.

### Clock configuration

Set by Zephyr from the upstream board devicetree (`boards/st/nucleo_h723zg` in the Zephyr tree):

| Clock | Source / setting | Frequency |
|---|---|---|
| HSE | 8 MHz from ST-Link MCO (bypass) | 8 MHz |
| PLL1 | HSE / M=4 × N=275 / P=1 | — |
| SYSCLK / CPU | PLL1P | 550 MHz |
| HCLK (AHB) | SYSCLK / 2 | 275 MHz |
| APB1/2/3/4 | HCLK / 2 | 137.5 MHz |

## Project layout

```
CMakeLists.txt      Zephyr app build file
Kconfig             App Kconfig: selects STM32Cube HAL/LL modules; static IP settings
prj.conf            App Kconfig settings
debug.conf          Extra settings for a debug build (-Og, asserts, thread info)
boards/
  nucleo_h723zg.overlay   Devicetree overlay (frees PB14 from TIM12 PWM)
src/
  main.c            LD1 via Zephyr, main loop
  led_hal.c/.h      LD2 via STM32Cube HAL
  led_cmsis.c/.h    LD3 via CMSIS registers
  app_net.c/.h      Ethernet bring-up, DHCPv4, logs the lease
docs/               Reference manuals (local only, see below)
tools/              Build env, VS Code debug templates, static analysis, MISRA deviations
```

## Mixing Zephyr, HAL and CMSIS

- Zephyr owns startup, clocks, MPU, SysTick, interrupts and linking. No `HAL_Init()` or `SystemClock_Config()`; `HAL_GetTick()`/`HAL_Delay()` map to the Zephyr kernel.
- HAL/LL comes from Zephyr's `hal_stm32` module. Enable a module by selecting `USE_STM32_HAL_<X>` from the app `Kconfig` (these symbols can't be set in `prj.conf`).
- CMSIS device headers come via `<soc.h>`.
- One owner per peripheral (per pin for GPIO): disable a devicetree node in the overlay when HAL/CMSIS takes it over.

## Build

Zephyr version and modules are pinned in [west.yml](west.yml) (Zephyr v4.4.0 + cmsis_6, hal_stm32, picolibc, hal_st, littlefs). On this PC the existing west workspace at `D:\Workspace` is used (same commits). On a new machine, create a workspace from the pin:

```
git config --global core.longpaths true   # Windows: Zephyr has paths over 260 chars
mkdir zephyr-ws && cd zephyr-ws
git clone https://github.com/kgsinh/eco-connect-gateway
west init -l eco-connect-gateway
west update
```

Until the Zephyr SDK is usable, builds use CubeIDE's Arm GCC; `tools/zephyr_env.sh` sets that up (paths are for this PC's CubeIDE install):

```
source tools/zephyr_env.sh
west build -p always -b nucleo_h723zg                                   # release (-Os)
west build -p always -b nucleo_h723zg -- -DEXTRA_CONF_FILE=debug.conf  # debug
```

Output: `build/zephyr/zephyr.elf` (about 147 KB flash, 34 KB RAM with networking and the shell).

## Flash

```
source tools/zephyr_env.sh
west flash --cli "$STM32_PROGRAMMER_CLI"
```

## Debug

Step-by-step debugging in VS Code with the **Cortex-Debug** extension and ST's ST-LINK GDB server (both come with / via the CubeIDE install):

1. Copy `tools/vscode/launch.json` and `tasks.json` into `.vscode/` (already done on this PC).
2. Plug in the board, press **F5**, and pick **Debug (ST-LINK, build + flash, stop at main)**. It builds the debug image (`debug.conf`: `-Og`, asserts, thread info), flashes it and stops at `main`.
3. Use breakpoints, F10/F11 to step, and the Variables, Call Stack and Watch views. **Cortex-Debug: Peripherals** shows live registers (GPIOB, ETH, RCC ...) from the STM32H723 SVD.

**Attach (ST-LINK, no reset, no flash)** halts the firmware that's already running, which is useful when it hangs. The ELF in `build/` must match what's on the board.

**STM32CubeIDE** also works as a debugger front end: import `build/zephyr/zephyr.elf` via File → Import → C/C++ → STM32 Cortex-M Executable, and set Reset behaviour to *Connect under reset*. To build in CubeIDE as well, start it with `tools/cubeide.sh` and import the repo as an STM32 CMake Project; see NOTES.md.

Without a debugger: a crash prints Zephyr's fatal-error dump (faulting PC, thread) on the console; resolve the PC with `arm-none-eabi-addr2line -e build/zephyr/zephyr.elf <pc>`. The shell has `kernel threads`, `kernel stacks` and `device list`.

## Static analysis

Needs a current `west build` (flags come from `build/compile_commands.json`). Only our code in `src/` is reported. From Git Bash:

```
tools/cppcheck_check.sh     # native cppcheck 2.21, general checks (~3 min)
tools/misra_check.sh        # MISRA C:2025 (~7 min)
```

- MISRA uses the official cppcheck 2.21.0 `misra.py` addon (not shipped with the Windows cppcheck install; see CLAUDE.md for where it lives) and `docs/misra-c-2025-rule-texts.txt`, generated from your licensed copy of the standard by [tools/misra_rule_texts.py](tools/misra_rule_texts.py). The PDF and the generated file are gitignored and must not be committed.
- Documented deviations (register access, Zephyr/HAL macro expansions, one checker false positive) are in [tools/misra_deviations.txt](tools/misra_deviations.txt) and are counted in every report.
- The checker was written for MISRA C:2012. Rules it cannot check are listed in CLAUDE.md for manual review.

## History

Earlier versions of this repo were an STM32CubeIDE/CubeMX project, first CMSIS-only and then HAL-only. They are in git history before the Zephyr switch.

---

## STM32H723ZG — controller features

Source: datasheet DS13313 Rev 5 (`docs/stm32h723zg.pdf`). The datasheet covers the whole STM32H723xE/G family, so "up to" figures are family maximums. The **ZG** part has 1 MB flash, and the Nucleo board uses the **LQFP144** package.

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

Not stored in git (`docs/*.pdf` is gitignored). Download the ST documents from st.com by document number (RM0468, PM0253, DS13313, UM3252; board manual UM2407 recommended) and save them under `docs/` with the file names below; CLAUDE.md refers to them by these paths.

| File | Contents |
|---|---|
| `docs/rm0468.pdf` | Reference manual: peripherals, registers, memory map |
| `docs/pm0253-stm32h7-series-cortexm7.pdf` | Programming manual: Cortex-M7 core, NVIC, instruction set |
| `docs/stm32h723zg.pdf` | Datasheet: electrical characteristics, pinout, package |
| `docs/um3252-stm32h7-series.pdf` | X-CUBE-CLASSB self-test library guide (IEC 60730 Class B) |
| (not yet downloaded) UM2407 | Nucleo-144 board user manual: jumpers, solder bridges, board wiring |
| `docs/MISRA-C-2025.pdf` | MISRA C:2025: licensed copy, local only |
