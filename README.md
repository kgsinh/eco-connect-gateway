# Eco-Connect IoT Gateway — NUCLEO-H723ZG

IoT gateway on the **NUCLEO-H723ZG** (STM32H723ZG, Cortex-M7) running Zephyr 4.4.0. It reads a BME280 temperature/pressure/humidity sensor over I2C and is reachable over Ethernet through an ENC28J60 on SPI. The firmware mixes three API layers on purpose: Zephyr APIs, STM32Cube HAL and CMSIS register access, one LED each. A separate bare-metal blinky shows the same board driven from registers alone.

## Week-one deliverables

| # | Deliverable | Where | Tested on hardware |
|---|---|---|---|
| 1 | Register blinky, no HAL, no RTOS | [baremetal/blinky](baremetal/blinky) | LEDs at 1000/500/250 ms |
| 2 | Zephyr blinky | [src/main.c](src/main.c) | LED chase LD1 → LD2 → LD3 |
| 3 | I2C sensor, real readings | [src/sensor](src/sensor), BME280 on I2C1 | T/P/H logged every 2 s |
| 4 | ENC28J60 via Zephyr's driver | [boards/nucleo_h723zg.overlay](boards/nucleo_h723zg.overlay), [src/net](src/net) | 10 Mbps link, ping replies |
| 5/7 | Repo and README | this file: requirements, build, flash | |
| 6 | Pins and clocks note | [docs/pins-and-clocks.md](docs/pins-and-clocks.md) | |

## Status

LED chase: one LED on at a time, LD1 → LD2 → LD3, 250 ms per step. (The bare-metal blinky in `baremetal/blinky` blinks all three independently, so the running image is easy to tell apart.)

| LED | Pin | Driven by | Chase order |
|---|---|---|---|
| LD1 green | PB0 | Zephyr GPIO API (devicetree `led0`), `src/main.c` | 1st |
| LD2 yellow | PE1 | STM32Cube HAL GPIO, `src/led/hal/led_hal.c` | 2nd |
| LD3 red | PB14 | CMSIS registers, `src/led/cmsis/led_cmsis.c` | 3rd |

**Ethernet** (`src/net/app_net.c`): ENC28J60 module on SPI1 (10BASE-T) via Zephyr's `microchip,enc28j60` driver; the on-board RMII Ethernet is disabled in the overlay. The board has one IPv4 address, and `prj.conf` picks how it gets it (Kconfig choice `APP_NET_MODE`):

- **Static `192.168.50.2/24`** (`CONFIG_APP_NET_STATIC=y`, the default), for a cable straight to a PC. Set the PC's Ethernet adapter to `192.168.50.1` / `255.255.255.0`, no gateway, then `ping 192.168.50.2`. Change the address with `CONFIG_APP_NET_STATIC_IPV4_ADDR` / `_NETMASK`.
- **DHCP** (`CONFIG_APP_NET_DHCP=y`; comment out the static line in `prj.conf`), for a router. The leased address is printed on the console.

Tested: 10 Mb half-duplex link (the ENC28J60 has no autonegotiation; the PC adapter detects 10 Mb), ping from the PC over a direct cable.

**TCP/IP services** (details and test status in [docs/networking.md](docs/networking.md)):

- **Web page** on port 80 (`src/net/http/app_http.c`, Zephyr's HTTP server): open `http://192.168.50.2/` in a browser. The page is `src/web/index.html`, gzipped into the image at build time; edit it and rebuild. With curl, use `curl --compressed`. Tested in a browser and with curl.
- **TCP echo server** on port 4242 (`src/net/tcp/app_tcp.c`): `python tools/echo_test.py [host]` sends five lines and checks each echo. Tested.

ENC28J60 wiring (3.3 V module, no regulator):

| ENC28J60 | Nucleo | MCU pin |
|---|---|---|
| VCC | 3V3 | — |
| GND | GND | — |
| SCK | D13 | PA5 |
| SO | D12 | PA6 (MISO) |
| SI | D11 | PB5 (MOSI) |
| CS | D10 | PD14 |
| INT | D9 | PD15 |
| RESET, WOL, CLKOUT | not connected (RESET has an internal pull-up) | — |

BME280 wiring (3.3 V breakout): VCC to 3V3, GND to GND, SCL to D15 (PB8), SDA to D14 (PB9). I2C address 0x76.

**Console:** ST-Link virtual COM port, 115200 8N1. It shows boot logs and the Zephyr shell (`uart:~$`; try `net iface`, `net ping <ip>`, `help`). The shell uses ANSI colour/cursor codes, so use a terminal that understands them: `python -m serial.tools.miniterm --filter direct COMx 115200` or PuTTY. Plain miniterm shows them as `␛[..m`.

See [docs/gotchas.md](docs/gotchas.md) for pitfalls found along the way, [docs/networking.md](docs/networking.md) for the Ethernet/TCP/IP setup and its test status, and CLAUDE.md for the rules on mixing layers.

### Clock configuration

Full pin and clock list (LEDs, I2C, SPI, console, bare-metal clock): [docs/pins-and-clocks.md](docs/pins-and-clocks.md).

Set by Zephyr from the upstream board devicetree (`boards/st/nucleo_h723zg` in the Zephyr tree):

| Clock | Source / setting | Frequency |
|---|---|---|
| HSE | 8 MHz from ST-Link MCO (bypass) | 8 MHz |
| PLL1 | HSE / M=4 × N=275 / P=1 | — |
| SYSCLK / CPU | PLL1P | 550 MHz |
| HCLK (AHB) | SYSCLK / 2 | 275 MHz |
| APB1/2/3/4 | HCLK / 2 | 137.5 MHz |

## Architecture

Diagrams of how the parts connect, the boot and runtime sequence, and the build: [docs/architecture.md](docs/architecture.md). Every API used in `src/`, with its purpose: [docs/api-reference.md](docs/api-reference.md).

## Project layout

```
CMakeLists.txt      Zephyr app build file
Kconfig             App Kconfig: selects STM32Cube HAL/LL modules; network mode, static IP
prj.conf            App Kconfig settings
debug.conf          Extra settings for a debug build (-Og, asserts, thread info)
boards/
  nucleo_h723zg.overlay   Devicetree overlay (frees PB14 from TIM12 PWM)
src/
  main.c                LD1 via Zephyr, LED chase, starts network and sensor
  net/
    app_net.c/.h        Ethernet bring-up: static IP or DHCPv4, logs the lease
  sensor/
    app_sensor.c/.h     BME280 thread: temperature, pressure, humidity every 2 s
  led/
    hal/led_hal.c/.h      LD2 via STM32Cube HAL
    cmsis/led_cmsis.c/.h  LD3 via CMSIS registers
baremetal/blinky/       Register-only blinky: no HAL, no RTOS (own startup + linker script)
docs/               Reference manuals (local only, see below)
tools/              Build env, VS Code debug templates, static analysis, MISRA deviations
```

## Mixing Zephyr, HAL and CMSIS

- Zephyr owns startup, clocks, MPU, SysTick, interrupts and linking. No `HAL_Init()` or `SystemClock_Config()`; `HAL_GetTick()`/`HAL_Delay()` map to the Zephyr kernel.
- HAL/LL comes from Zephyr's `hal_stm32` module. Enable a module by selecting `USE_STM32_HAL_<X>` from the app `Kconfig` (these symbols can't be set in `prj.conf`).
- CMSIS device headers come via `<soc.h>`.
- One owner per peripheral (per pin for GPIO): disable a devicetree node in the overlay when HAL/CMSIS takes it over.

## Requirements

Versions this repo is built and tested with:

| Item | Version / note |
|---|---|
| Board | NUCLEO-H723ZG (STM32H723ZG, Cortex-M7), USB to the ST-Link connector |
| Zephyr | v4.4.0, pinned with its modules in [west.yml](west.yml) |
| west | 1.5.0 (`pip install west`) |
| CMake / Ninja | 4.4.3 / 1.13.2 (CMake 3.20 or newer is enough) |
| Python | 3.14 (needs Zephyr's `scripts/requirements.txt`) |
| Toolchain | Arm GCC 14.3.1, "GNU Tools for STM32 14.3.rel1" from STM32CubeIDE 1.17.0. The Zephyr SDK or any Arm GNU toolchain also works. |
| Flashing | STM32CubeProgrammer 2.23.0 (`STM32_Programmer_CLI`), here the copy inside STM32CubeIDE |
| Shell | Git Bash on Windows, or bash on Linux/macOS |

## Get the code

Fresh workspace from the pinned versions (any empty folder):

```
git config --global core.longpaths true   # Windows: Zephyr has paths over 260 chars
mkdir zephyr-ws && cd zephyr-ws
git clone https://github.com/kgsinh/eco-connect-gateway
west init -l eco-connect-gateway
west update
pip install -r zephyr/scripts/requirements.txt
cd eco-connect-gateway
```

On the development PC the existing workspace at `D:\Workspace` is used instead; it is on the same commits.

## Toolchain setup

`tools/zephyr_env.sh` selects the toolchain and the programmer. Source it once per shell, from the repo root:

```
source tools/zephyr_env.sh
```

It uses, in this order: `GNUARMEMB_TOOLCHAIN_PATH` if set (root of an Arm GNU toolchain), a readable Zephyr SDK from `ZEPHYR_SDK_INSTALL_DIR`, then STM32CubeIDE's GCC under `CUBEIDE_PLUGINS`. The programmer comes from `STM32_PROGRAMMER_CLI`, CubeIDE, or `PATH`. The defaults match this PC's CubeIDE 1.17.0 install. On another machine, export your own paths before sourcing, for example:

```
export GNUARMEMB_TOOLCHAIN_PATH="/opt/arm-gnu-toolchain-14.3.rel1"
export STM32_PROGRAMMER_CLI="/opt/st/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
source tools/zephyr_env.sh
```

## Build

Zephyr application (the gateway), in Git Bash at the repo root after `source tools/zephyr_env.sh`:

```
west build -p always -b nucleo_h723zg                                   # release (-Os), static IP
west build -p always -b nucleo_h723zg -- -DEXTRA_CONF_FILE=debug.conf  # debug (-Og, asserts)
west build -p always -b nucleo_h723zg -- -DCONFIG_APP_NET_STATIC=n -DCONFIG_APP_NET_DHCP=y  # DHCP instead of static IP
```

`Could not find a package configuration file provided by "Zephyr-sdk"` means `zephyr_env.sh` was not sourced in this shell.

Output: `build/zephyr/zephyr.elf`, about 193 KB flash and 49 KB RAM (release, with TCP, sockets and the GPIO shell). Both variants use `build/`; `grep DEBUG_OPTIMIZATIONS build/zephyr/.config` shows which one is there.

Bare-metal register blinky (no Zephyr, CMake + Ninja):

```
baremetal/blinky/build.sh            # -> baremetal/blinky/build/blinky.elf, 496 B flash + 4 B RAM
```

## Flash

Check the probe first: `"$STM32_PROGRAMMER_CLI" -l st-link` (`No ST-Link detected!` means the board is not connected).

```
west flash --cli "$STM32_PROGRAMMER_CLI"     # Zephyr application
baremetal/blinky/build.sh flash              # bare-metal blinky (builds, then flashes)
```

Each one replaces whatever is on the board. The Zephyr image runs an LED chase and prints a boot log on the console; the bare-metal one blinks the three LEDs at 1000/500/250 ms.

## VS Code: build, flash and serial monitor

Tasks live in `tools/vscode/tasks.json`; copy it to `.vscode/` once (already done on this PC). Run them with **Terminal → Run Task...** (or `Ctrl+Shift+P` → *Tasks: Run Task*). `Ctrl+Shift+B` runs the default build.

| Task | What it does |
|---|---|
| Zephyr: build (release) | Pristine release build (default build task) |
| Zephyr: build (debug) | Pristine debug build (`debug.conf`) |
| Zephyr: flash | Flashes `build/zephyr/zephyr.elf` |
| Bare-metal: build | Builds `baremetal/blinky` |
| Bare-metal: build + flash | Builds and flashes the bare-metal blinky |
| Serial monitor | Opens the console (asks for the port, default `COM5`) |

The same from a Git Bash terminal, at the repo root:

```
source tools/zephyr_env.sh
west build -p always -b nucleo_h723zg          # build (release)
west flash --cli "$STM32_PROGRAMMER_CLI"       # flash the Zephyr app
baremetal/blinky/build.sh flash                # build + flash the bare-metal blinky
python -m serial.tools.miniterm --filter direct COM5 115200   # serial monitor
```

Serial monitor notes:
- The port is the ST-Link virtual COM port, 115200 8N1. Find its number in PowerShell with `Get-CimInstance Win32_PnPEntity | ? Name -match 'COM\d+'` (look for "STMicroelectronics STLink Virtual COM Port").
- Exit miniterm with `Ctrl+]`. Only one program can hold the port: close the monitor (or PuTTY, CubeIDE's console) before another tool opens it. Flashing does not need the port, so the monitor can stay open and shows the boot log after the reset.
- `--filter direct` passes the shell's ANSI colour codes through; without it they print as `␛[..m`.
- Useful shell commands: `net iface`, `net ping 192.168.50.1`, `sensor get bme280@76`, `i2c scan i2c@40005400`, `kernel threads`, `help`.

## Debug

Step-by-step debugging in VS Code with the **Cortex-Debug** extension and ST's ST-LINK GDB server (both come with / via the CubeIDE install):

1. Copy `tools/vscode/launch.json` and `tasks.json` into `.vscode/` (already done on this PC).
2. Plug in the board, press **F5**, and pick **Debug (ST-LINK, build + flash, stop at main)**. It builds the debug image (`debug.conf`: `-Og`, asserts, thread info), flashes it and stops at `main`.
3. Use breakpoints, F10/F11 to step, and the Variables, Call Stack and Watch views. **Cortex-Debug: Peripherals** shows live registers (GPIOB, ETH, RCC ...) from the STM32H723 SVD.

**Attach (ST-LINK, no reset, no flash)** halts the firmware that's already running, which is useful when it hangs. The ELF in `build/` must match what's on the board.

**STM32CubeIDE** also works as a debugger front end: import `build/zephyr/zephyr.elf` via File → Import → C/C++ → STM32 Cortex-M Executable, and set Reset behaviour to *Connect under reset*. To build in CubeIDE as well, start it with `tools/cubeide.sh` and import the repo as an existing Makefile project with build command `west`; see [docs/gotchas.md](docs/gotchas.md#stm32cubeide-as-front-end) for the exact settings.

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
