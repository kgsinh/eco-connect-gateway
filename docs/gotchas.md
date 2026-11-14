# Gotchas

Things that cost time on this project and are not obvious from the code. Networking-specific items are in [networking.md](networking.md).

## Zephyr, HAL and CMSIS together

- **STM32Cube HAL/LL Kconfig symbols are promptless.** `CONFIG_USE_STM32_HAL_GPIO=y` in prj.conf is silently ignored; select it from the app `Kconfig` (`config APP_... select USE_STM32_HAL_...`). For H7, `<stm32h7xx_hal.h>` exposes all module headers; Kconfig only decides which HAL .c files compile.
- **`HAL_GetTick()`/`HAL_Delay()` map to `k_uptime_get_32()`/`k_msleep()`** (`zephyr/soc/st/stm32/common/stm32cube_hal.c`). HAL timeouts work without `HAL_Init()`.
- **GPIOB is shared** by the Zephyr driver (PB0) and CMSIS code (PB14). Safe because the CMSIS code writes BSRR (single pin). Keep it BSRR-only if more threads touch the port.
- **Port clocks:** HAL/CMSIS code enables GPIOE/GPIOB clocks itself even though the Zephyr GPIO driver already did; enabling is idempotent.
- **The board's `pwm-led0` alias still points at the disabled PWM LED** (TIM12 is disabled so CMSIS can own PB14). Don't enable `CONFIG_LED_PWM` or use `pwm-led0` unless the overlay changes back.
- **Disabling a bus node doesn't disable its children.** With `&mdio` disabled, `eth_phy` stayed enabled and the PHY driver failed to build (`__device_dts_ord_N undeclared`). Disable the child too.
- **SRAM2 is free since the switch to the ENC28J60.** The on-board MAC used it for DMA buffers through a non-cacheable MPU region (`soc/st/stm32/stm32h7x/mpu_regions.c`); that region only exists while `&mac` is enabled. The ENC28J60 path has no DMA (SPI without `dmas`), so no cache maintenance is needed.
- **Memory map (H723 under Zephyr):** ITCM 64 KB @0, DTCM 128 KB @0x20000000 (`zephyr,dtcm`, no DMA), AXI SRAM 320 KB @0x24000000 (`zephyr,sram`, cached), SRAM1/2 16 KB each @0x30000000/0x30004000, SRAM4 16 KB @0x38000000. D/I-cache and MPU are on; `NOCACHE_MEMORY` is not (it was only needed for the on-board MAC). DMA1/DMA2 can't reach DTCM (RM0468 §2.1.6). DMA buffer policy: [evidence/ram-rom/README.md](evidence/ram-rom/README.md#dma-buffers-decision).

## Build and toolchain

- **An unreadable Zephyr SDK breaks every build.** If `ZEPHYR_SDK_INSTALL_DIR` points at an SDK the user account can't read (e.g. installed elevated), CMake fails with "Could not find ... Zephyr-sdk". `tools/zephyr_env.sh` falls back to CubeIDE's Arm GCC; run `source tools/zephyr_env.sh` in every new shell. In Git Bash, `[ -r file ]` ignores Windows ACLs, so the script tests with a real read.
- **`west flash` needs `--cli <path>`**: STM32_Programmer_CLI lives in the CubeIDE plugins folder, not on PATH.
- **Fresh `west init -l` checkout on Windows** needs `git config --global core.longpaths true` (Zephyr has paths over 260 characters).
- **After moving or renaming the repo folder**, rebuild with `-p always`; if CMake still points at the old path, delete `build/`.
- **Python on Windows defaults to cp1252.** Pass `encoding="utf-8"` when editing repo files from Python.

## Debugging

- **H7 + Zephyr idle = no hot attach.** Zephyr idles in WFI, and with debug-in-sleep off the H7's debug port stops answering ("Could not verify ST device", "Unable to read device id from ROM table"). Connect under reset (`-k` for ST-LINK_gdbserver, `mode=UR` for CubeProgrammer; `west flash` already resets), and set `CONFIG_STM32_ENABLE_DEBUG_SLEEP_STOP=y` (in debug.conf) so Attach works on debug builds.
- **The C/C++ Runner extension re-adds a "C/C++ Runner: Debug Session" to `.vscode/launch.json`** ("program ... outDebug does not exist"). Delete it, pick an "(ST-LINK ...)" configuration, and disable the extension for this workspace.
- **IntelliSense** needs `compileCommands: build/compile_commands.json` in `.vscode/c_cpp_properties.json` and one `west build` first; a generic include path finds no Zephyr headers.

## STM32CubeIDE as front end

- Import the repo as **File → Import → Existing Code as Makefile Project** (the STM32 CMake Project wizard writes its own template and can't adopt Zephyr). Build command `west` alone; the arguments go in Behaviour → Build target (`build -b nucleo_h723zg -- -DEXTRA_CONF_FILE=debug.conf`), clean target `build -t pristine`. With arguments in the command field, CDT runs the whole string as one program name.
- Indexer: add `build/zephyr/include/generated/zephyr/autoconf.h` as an include file (Zephyr ≥ 3.5 path; the Zephyr CubeIDE doc's `generated/autoconf.h` is outdated). It lives in the workspace metadata, not the repo.
- Debug: File → Import → STM32 Cortex-M Executable → `build/zephyr/zephyr.elf`, Reset behaviour = Connect under reset.
- Start CubeIDE through `tools/cubeide.sh` so it inherits the toolchain environment.

## Console and shell

- The VS Code **Serial Monitor** extension sends text with no line ending by default ("Line ending: None"): the shell echoes the command but never runs it, and the next message is appended to the same line. Set the line ending to CR or CRLF, or use miniterm.
- Use `python -m serial.tools.miniterm --filter direct <port> 115200`; without `--filter direct`, miniterm shows the shell's ANSI colour codes as `␛[..m`. Only one program can hold the port.
- Zephyr's `net`, `kernel`, `i2c`, `sensor` and `gpio` commands run in the **board's** shell (`uart:~$`), not on the PC. In a Windows terminal, `net iface` runs Windows' own `net.exe`.
- GPIO shell port names are devicetree node names: `gpio conf gpio@58021800 12 o`.
- `CONFIG_REBOOT` is off, so `kernel reboot cold` isn't available.
- BME280 readings are logged at debug level and hidden at boot by `CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL_INF`. `LOG_RUNTIME_DEFAULT_LEVEL` alone doesn't hide them: when the shell backend starts, it sets every module to its own level, and its default ("system limit") is each module's compiled level. `log enable dbg app_sensor` shows them, `log enable inf app_sensor` hides them; `log status` lists every module's level. Not persistent across resets.

## Parts

- **ENC28J60** (DS39662C): 3.1-3.6 V, inputs 5 V tolerant, SPI mode 0 only, up to 20 MHz, 25 MHz crystal on the module, 10BASE-T only. INT is active low. Up to 180 mA while transmitting. Errata DS80349C issue 1: SPI below 8 MHz makes MAC register access unreliable on revisions B1 and B4 only.
- **BME280** (rev 1.24): VDD 1.71-3.6 V, I2C up to 3.4 MHz. Address 0x76 (SDO to GND) or 0x77 (SDO to VDDIO); SDO must not float. Chip ID register 0xD0 reads 0x60; a BMP280 reads 0x58 (cheap modules are often BMP280s, which have no humidity sensor).
- **Arduino header buses (board DTS):** `arduino_spi` = SPI1 (SCK PA5/D13, MISO PA6/D12, MOSI PB5/D11, CS PD14/D10); MOSI is on PB5, not PA7, because PA7 is RMII CRS_DV. `arduino_i2c` = I2C1 (SCL PB8/D15, SDA PB9/D14), 400 kHz.
