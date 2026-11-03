# Project notes

Running log of decisions, gotchas and open items. Newest session at the top of the session log; the sections above it hold the current state of the project.

## Current architecture

- **Single Zephyr app at the repo root** (Zephyr 4.4.0, west workspace `D:\Workspace`, board `nucleo_h723zg`). Zephyr owns startup, clocks, MPU, SysTick, IRQs and linking. The CubeIDE/CubeMX project was removed (it's in git history).
- Source layout: `src/main.c`, `src/net/` (Ethernet/IPv4), `src/sensor/` (BME280), `src/led/hal/` and `src/led/cmsis/` (LD2/LD3 drivers); include root is `src/` (`#include "led/hal/led_hal.h"`).
- Three API layers in one app, one LED each:
  - LD1 green PB0: Zephyr GPIO (`led0` DT alias) in `src/main.c`.
  - LD2 yellow PE1: STM32Cube HAL GPIO in `src/led/hal/led_hal.c`, every 500 ms. HAL comes from Zephyr's hal_stm32 module, enabled by `config APP_STM32_HAL_GPIO` → `select USE_STM32_HAL_GPIO` in `Kconfig`.
  - LD3 red PB14: CMSIS registers in `src/led/cmsis/led_cmsis.c`.
- Main loop: LED chase, one LED on at a time, LD1 → LD2 → LD3, 250 ms per step (`led_toggle()` dispatches to the layer). Deliberately different from the bare-metal blinky (independent 1000/500/250 ms) so the running image is obvious.
- `boards/nucleo_h723zg.overlay` disables `timers12`, `pwm12` and `/pwmleds`, so the Zephyr PWM driver doesn't claim PB14.
- Clock (upstream board DTS): HSE 8 MHz (ST-Link MCO, bypass) → PLL1 550 MHz CPU, HCLK 275 MHz, APB 137.5 MHz. Console: USART3 (ST-Link VCOM).
- LED-only build was 21,168 B flash / 4,608 B RAM and was tested on the board (2026-10-27).
- **BME280** (`src/sensor/app_sensor.c`): Zephyr `bosch,bme280` driver on I2C1 at 0x76, own thread (prio 7, 1 KB static stack), logs `T .. C  P .. kPa  H .. %RH` every 2 s without float printf (milli split). `app_sensor_start()` returns -ENODEV and logs if the sensor doesn't probe; the rest keeps running. I2C and sensor shells enabled for bring-up (`i2c scan i2c@40005400`, `sensor get bme280@76`). Release build 169,752 B flash, 42,112 B RAM.
- **Ethernet + IPv4** (`src/net/app_net.c`): ENC28J60 on SPI1 (Zephyr `microchip,enc28j60` driver, node `enc28j60: ethernet@0` in the overlay): CS PD14/D10, INT PD15/D9 active low, SPI max 10 MHz → 8.6 MHz actual (PLL1_Q 137.5 MHz / 16; errata wants ≥ 8 MHz), MAC `00:04:A3:50:00:02` (driver forces Microchip's OUI on the first 3 bytes). 10BASE-T half duplex, no autonegotiation. On-board MAC, MDIO and `eth_phy` are disabled in the overlay. `app_net_start()` is unchanged: static `192.168.50.2/24` + non-blocking DHCPv4, logs the lease. Net shell on the console. Release build 164,668 B flash, 40 KB RAM.

## Things to know

- **The Zephyr SDK is unusable for now.** `C:\Users\ksrgo\zephyr-sdk-1.0.1` is unreadable by the user account (Access denied even for icacls; probably installed elevated), and the user env var `ZEPHYR_SDK_INSTALL_DIR` points at it, which forces CMake to require it. Workaround in use: `unset ZEPHYR_SDK_INSTALL_DIR` + `ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb` with CubeIDE's arm-none-eabi-gcc 14.3 (see CLAUDE.md). Fix: in an admin PowerShell, `takeown /f <dir> /r /d y` and `icacls <dir> /grant "ksrgo:(OI)(CI)F" /t`, then run `<dir>\setup.cmd`.
- **STM32Cube HAL/LL Kconfig symbols are promptless.** `CONFIG_USE_STM32_HAL_GPIO=y` in prj.conf is silently ignored; select it from the app `Kconfig`. For H7, `<stm32h7xx_hal.h>` exposes all module headers; Kconfig only decides which HAL .c files compile.
- **`HAL_GetTick()`/`HAL_Delay()` map to `k_uptime_get_32()`/`k_msleep()`** (zephyr/soc/st/stm32/common/stm32cube_hal.c). HAL timeouts work without `HAL_Init`.
- **GPIOB is shared** by the Zephyr driver (PB0) and CMSIS code (PB14). This is safe because CMSIS writes BSRR (single pin), and everything runs in one thread. Keep it BSRR-only if threads are added.
- **Port clocks:** HAL/CMSIS code enables GPIOE/GPIOB clocks itself even though the Zephyr gpio driver already did; enabling is idempotent.
- **The board's pwm-led0 alias still points at the disabled PWM LED.** Don't enable CONFIG_LED_PWM / use pwm-led0 unless the overlay is changed back.
- **CubeIDE recreates `.project` while the old project is still open in its workspace.** Eclipse files are gitignored now. Remove the old h7_blinky project from the CubeIDE workspace (don't delete contents), or import the folder as an *STM32 CMake Project* to use CubeIDE as editor/debugger (Zephyr doc: develop/tools/stm32cubeide.rst, validated on Linux 1.16 only).
- **Disabling a bus node doesn't disable its children.** With `&mdio` disabled, `eth_phy` (ethernet-phy@0) stayed okay and phy_mii.c failed to build (`__device_dts_ord_N undeclared`). Disable the child too.
- **SRAM2 is free since the ENC28J60 switch.** The on-board MAC used it for DMA buffers via an MPU non-cacheable region in zephyr/soc/st/stm32/stm32h7x/mpu_regions.c; that region and the linker section only exist while `&mac` is enabled. The ENC28J60 has no DMA in our build (SPI without `dmas`), so nothing needs cache maintenance.
- **docs/um3252-stm32h7-series.pdf is the X-CUBE-CLASSB self-test library guide, not the board manual.** It was mislabelled in CLAUDE.md/README until 2026-10-27. The Nucleo-144 board manual is UM2407 (not downloaded yet).
- **The user's terminal is fish (MSYS2).** zephyr_env.sh is bash: run `bash` first or use the VS Code tasks. A zephyr_env.fish attempt was dropped (the user removed it). Gotchas found: MSYS2 fish started from Git Bash has no USERPROFILE (west fails), fish started from Windows has no coreutils, fish redirections need /c/... paths.
- **VS Code "Serial monitor" task may not take keyboard input.** Workaround: run `python -m serial.tools.miniterm --filter direct COM5 115200` in a normal terminal. `CONFIG_GPIO_SHELL=y` is on for pin bring-up: `gpio conf gpio@58021800 12 o`, `gpio set|get gpio@58021800 12 ...` (port name = devicetree node name).
- **`.clang-format` = Zephyr's** (copied from `zephyr/.clang-format`). Without it, VS Code's C/C++ extension formats in its "Visual Studio" fallback (4 spaces, Allman braces), against the Zephyr style rule. Its clang-format is bundled at `~/.vscode/extensions/ms-vscode.cpptools-*/LLVM/bin/clang-format.exe` (v23). A dry run on existing files changes a few alignment lines (app_sensor.c 24, baremetal main.c 50): not applied yet.
- **Console:** ST-Link VCOM is **COM5** on this PC, 115200 8N1. Use `python -m serial.tools.miniterm --filter direct COM5 115200` (without `--filter direct`, miniterm shows the ANSI colour codes as `␛[..m`) or PuTTY `-serial COM5 -sercfg 115200,8,n,1,N`. The shell prompt is `uart:~$`.
- **PC side of the direct cable is a USB adapter, "Ethernet 3" (ASIX USB to Gigabit).** It had no static address on 2026-11-02 (only APIPA 169.254.x), so `ping 192.168.50.2` failed until a matching address existed. Set it to 192.168.50.1/24 (admin: `New-NetIPAddress -InterfaceAlias 'Ethernet 3' -IPAddress 192.168.50.1 -PrefixLength 24`), or for a quick test add one on the board: `net ipv4 add 1 169.254.50.2 255.255.0.0` (lost on reset).
- **Windows Firewall blocks inbound ping by default**, so `net ping 192.168.50.1` from the board may time out. Test PC → board.
- **Debugging:** VS Code Cortex-Debug + ST-LINK_gdbserver + CubeIDE GDB + STM32H723 SVD. Templates are in tools/vscode/, the live copies in .vscode/ (the old C/C++ Runner launch.json is kept as .vscode/launch.json.bak). Debug build = `-DEXTRA_CONF_FILE=debug.conf` (-Og, ASSERT, DEBUG_THREAD_INFO, INIT_STACKS): 219,920 B flash vs 147,448 B release. Same build/ dir for both, so check `.config` before flashing.
- **The C/C++ Runner extension re-adds a "C/C++ Runner: Debug Session" entry to .vscode/launch.json** (PC program `Core/Src/build/Debug/outDebug`). Selecting it gives "launch: program ... outDebug does not exist". Delete that entry, pick a "(ST-LINK ...)" config in the Run and Debug dropdown, and disable C/C++ Runner for this workspace (Extensions → C/C++ Runner → Disable (Workspace)).
- **H7 + Zephyr idle = no hot attach.** Zephyr idles in WFI, and with DBGMCU debug-in-sleep off the H7's debug port stops answering: ST-LINK_gdbserver says "Could not verify ST device" and CubeProgrammer says "Unable to read device id from ROM table". Fixes: connect under reset (`-k` in the launch config; CubeProgrammer `mode=UR`; `west flash` already uses hw reset), and `CONFIG_STM32_ENABLE_DEBUG_SLEEP_STOP=y` in debug.conf so Attach works on debug builds. The release build doesn't set it, so attaching to it needs a reset.
- **STM32CubeIDE build (working 2026-10-30):** start CubeIDE via `tools/cubeide.sh` (inherits zephyr_env.sh). Project = File → Import → Existing Code as Makefile Project (not the STM32 CMake Project wizard: it writes its own template and can't adopt Zephyr). C/C++ Build: Build command `west` alone (with args in that field CDT runs the whole string as one program name); Behaviour: Build ticked with target `build -b nucleo_h723zg -- -DEXTRA_CONF_FILE=debug.conf` (drop the `--` part for release), Clean `build -t pristine`. Indexer: autoconf.h as a CDT User Setting Entry (include file `build/zephyr/include/generated/zephyr/autoconf.h`; Zephyr ≥3.5 path, the Zephyr CubeIDE doc's is outdated), stored in workspace metadata, not the repo. Debug: File → Import → STM32 Cortex-M Executable → build/zephyr/zephyr.elf, Reset behaviour = Connect under reset (zephyr.launch, gitignored).
- **Zephyr version pinned in `west.yml`** (2026-11-02): zephyr v4.4.0 + allowlist cmsis_6, hal_stm32, picolibc (what the build compiles), hal_st (sensors later), littlefs (P2). D:\Workspace keeps its own manifest (`[manifest] path = zephyr`) and is not affected; its module commits match the pin. A fresh `west init -l` checkout on Windows needs `git config --global core.longpaths true` (Zephyr has paths over 260 chars).
- **`tools/zephyr_env.sh`** picks the toolchain: `GNUARMEMB_TOOLCHAIN_PATH` → readable Zephyr SDK → CubeIDE GCC under `CUBEIDE_PLUGINS`; programmer from `STM32_PROGRAMMER_CLI` → CubeIDE → PATH. Environment values win, so other machines export their own paths. CubeIDE plugin paths are the one place to update when CubeIDE changes. Gotcha: in Git Bash `[ -r file ]` ignores Windows ACLs and said the unreadable SDK's `sdk_version` was readable; the script does a real read (`head -c 1`). Once the SDK permissions are fixed, the script uses the SDK automatically.
- **`west flash` needs `--cli <path>`** because STM32_Programmer_CLI lives in the CubeIDE plugins folder, not on PATH or in Program Files.
- **Python on Windows defaults to cp1252.** Always pass `encoding="utf-8"` when editing repo files from Python; an earlier README edit got mis-encoded this way (fixed 2026-10-27).
- **Project brief: docs/Board-Suitability-P1-P2.pdf** (mentor's cohort doc, gitignored). H723ZG suits P1 (gateway) and P2 (ledger logger). P1 targets: RAM ~48–59 KB (target 64 KB), ROM 384 KB; ENC28J60 on SPI + GPIO INT, BME280 on I2C, IWDG, UART console, Zephyr IP stack + HTTP server. P2 adds W25Q SPI NOR + littlefs, retained-RAM panic buffer, USB CDC shell (board's USB OTG HS on PA11/PA12, `zephyr_udc0`). It requires `ram_report`/`rom_report` output in docs/evidence/ram-rom/ and a written choice for DMA buffers: non-cached MPU region vs cache clean/invalidate, never DTCM.
- **Memory map (H723, Zephyr):** ITCM 64 KB @0, DTCM 128 KB @0x20000000 (`zephyr,dtcm`, no DMA), AXI SRAM 320 KB @0x24000000 (`zephyr,sram`, cached), SRAM1/2 16 KB each @0x30000000/0x30004000 (SRAM2 = ETH DMA, non-cacheable), SRAM4 16 KB @0x38000000. D/I-cache, MPU and NOCACHE_MEMORY are on.
- **`.vscode/` and `docs/*.pdf` are gitignored.** The ST PDFs were purged from history on 2026-10-27 (filter-branch + force-push), so commit IDs before that date changed. README lists the download names.

## Static analysis status (last run 2026-10-27)

- Setup reworked for Zephyr: `tools/analysis_flags.py` (flags from build/compile_commands.json + generated prelude.h), `tools/cppcheck_check.sh`, `tools/misra_check.sh` (cppcheck 2.21 dumps + official 2.21.0 misra.py in `D:\Workspace\cppcheck-addons-2.21.0`, outside the repo), `tools/misra_filter.py` + `tools/misra_deviations.txt`. Details in CLAUDE.md.
- cppcheck: no findings in src/. 24 dropped as Zephyr `LOG_*` macro expansion (invalidPointerCast, knownConditionTrueFalse).
- MISRA: no findings outside deviations. D1 11.4 register access (15), D2 `LOG_*` macro expansion (153), D3 `NET_EVENT_*` macro 10.1/10.4 (4), D4 misra.py 7.3 regex false positive on `gpio.Pull` (1). Fixed in our code: 8.9 ×2 (`ld1` in main.c and `ipv4_cb` in app_net.c moved to block-scope statics).
- The old MPLAB 2.11 setup silently "passed" main.c/app_net.c: its preprocessor failed on Zephyr macros and produced an empty dump. misra_check.sh now rejects dumps under 1 MB.
- `while (true)` in main.c is essentially Boolean, so it satisfies Rule 14.4.

## Week-one deliverables (due Sat 2026-11-07)

From docs/Presentation - Embedded Design Cohort - Batch 01.pdf. The bar is "runs on hardware", not "compiles".
1. Register blinky: bare-metal registers, no HAL, **no RTOS**. `baremetal/blinky/` (own startup.c, linker script, register defines from RM0468/PM0253, HSI 64 MHz, polled SysTick; CMake+Ninja via build.sh). Builds clean, 460 B. **Done 2026-11-02:** flashed with `build.sh flash`, user confirmed all three LEDs at the right rates; ODR sampled over SWD (HOTPLUG) showed PB14/PE1 toggling.
2. Zephyr blinky: **done 2026-11-02.** src/main.c runs an LED chase (LD1 → LD2 → LD3, 250 ms) across the three API layers, visibly different from the bare-metal pattern. Release build flashed with `west flash`; boot log on COM5 and the chase confirmed by the user.
3. I2C sensor (BME280), real readings: **done 2026-11-02.** 4-pin 3.3 V breakout on I2C1 (D15 SCL / D14 SDA, 3V3, GND), node `bme280@76` in the overlay, src/app_sensor.c logs T/P/H every 2 s from its own thread. On hardware: `i2c scan` finds only 0x76; humidity reads, so it is a real BME280 (0x60), not a BMP280. First readings: 28.6 C, 99.29 kPa, 34 %RH.
4. ENC28J60 via Zephyr's driver: **done 2026-11-02.** Driver logs `ethernet@0: Initialized`; `net iface` shows carrier ON, 10 Mbit; PC adapter links at 10 Mbps half duplex; ping from the PC 6/6, 1 ms, TTL 64, ARP shows 00-04-a3-50-00-02 (tested on a temporary 169.254.50.2 address, see Things to know).
5/7. Repo + README: **done 2026-11-02.** README has the deliverables map, requirements with tested versions (Zephyr 4.4.0, west 1.5.0, CMake 4.4.3, Ninja 1.13.2, GCC 14.3.1 from CubeIDE 1.17.0, CubeProgrammer 2.23.0), fresh-workspace steps from west.yml, toolchain setup, build and flash for both programs. zephyr_env.sh no longer hardcodes this PC only. No fresh-clone test (user's call): working tree clean, the only ignored files are build output, IDE files and PDFs. Not tested on a second machine. The brief says *public* repo; GitHub repo is still PRIVATE (user's call).
6. Pins & clocks note: **done 2026-11-02**, docs/pins-and-clocks.md (Zephyr and bare-metal clocks, LEDs, I2C1, SPI1 + 8.6 MHz SCK, console). Values from the board DTS, the overlay and RM0468 8.7.2/8.7.19/8.7.20.
8. One-minute live demo.

## Parts facts (from the data sheets in docs/)

- **ENC28J60** (DS39662C): 3.1-3.6 V, inputs 5 V tolerant, SPI **mode 0 only** (SCK idle low), up to 20 MHz, needs a 25 MHz crystal (on the module), 10BASE-T only. INT output is active low (binding `int-gpios` required, `GPIO_ACTIVE_LOW`). Current up to 180 mA while transmitting (160 typ). Silicon errata (not in docs/) is said to require SCK >= 8 MHz: check before choosing `spi-max-frequency`.
- **BME280** (rev 1.24): VDD 1.71-3.6 V, I2C up to 3.4 MHz. Address 0x76 (SDO to GND) or 0x77 (SDO to VDDIO); SDO must not float. CSB tied to VDDIO selects I2C. chip_id register 0xD0 reads 0x60 (a BMP280 reads 0x58: cheap modules are often BMP280s).
- **Board buses for them (upstream DTS):** `arduino_spi` = SPI1: SCK PA5 (D13), MISO PA6 (D12), MOSI PB5 (D11), CS PD14 (D10), already `okay`. `arduino_i2c` = I2C1: SCL PB8 (D15), SDA PB9 (D14), 400 kHz, `okay`. MOSI is on PB5, not PA7, because PA7 is RMII CRS_DV.

## Open items

- Make the VS Code "Serial monitor" task accept input (e.g. `type: process`), or drop it in favour of a terminal command.
- Bulk-format existing sources with `.clang-format` in a separate commit (small alignment changes only).

- Optional later: `ETH_PHY_DRIVER`/`MDIO` are still `y` by default with no node; harmless, could be turned off to trim the build.
- P1 parts not started: BME280 (I2C), IWDG, HTTP server, ram/rom reports in docs/evidence/ram-rom/, written DMA/cache choice.

- Confirm F5 in VS Code end to end. The same chain is verified from the CLI on the board (see the session log); the Peripherals view is not tried yet.
- Test Ethernet via a router (DHCP lease, `net ping <gateway>`) with the ENC28J60; the direct-PC path is tested.
- Set the PC's USB Ethernet adapter to 192.168.50.1/24 so the documented static path works without the 169.254 workaround.
- Download UM2407 (Nucleo-144 board manual) into docs/.
- Fix the Zephyr SDK permissions (see Things to know), or decide to stay on gnuarmemb.
- Re-establish the no-malloc rule for Zephyr (the old link-time trick was CubeIDE-specific). Options: `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0`, no `CONFIG_HEAP_MEM_POOL_SIZE`, and a check that `malloc`/`k_malloc` aren't linked.

## Session log

### 2026-11-03 (external LED added, then removed)
- User wrote an external LED on D7/PG12 step by step (shell test with `CONFIG_GPIO_SHELL`, overlay node + alias `led-ext`, src/led/ext with a `k_timer`), tested at 1 Hz, committed 0f7daf0. Then removed it: LD1 already shows the Zephyr GPIO API, so it added nothing to the gateway. Reverted the commit (no history rewrite); kept `CONFIG_GPIO_SHELL=y`. D7/PG12 is free again. `.clang-format` went with the revert and was added back on its own right after.

### 2026-11-02 (history rewrite: no AI co-author)
- User decision: removed the `Co-Authored-By: Claude` trailer from all 35 commits (`git filter-branch --msg-filter`). Authors, dates, subjects and the final tree are unchanged; **all commit IDs changed** (old HEAD 5872f28, rewritten 58682e5). After the force-push, any other clone must re-clone or `git reset --hard origin/main`. CLAUDE.md now has a Git section: no AI attribution in commits or PRs. Pre-rewrite backup bundle was saved in the session scratchpad.

### 2026-11-02 (VS Code tasks, CubeIDE check)
- Checked the CubeIDE setup after today's changes: project files untouched since 2026-10-30, CubeIDE's build command builds the debug image (247,564 B) with the new zephyr_env.sh; user confirmed CubeIDE debugging works. After the src/ moves: refresh the project, re-set breakpoints.
- tools/vscode/tasks.json (+ .vscode copy): added Bare-metal build, build + flash, and Serial monitor (miniterm, port prompt default COM5). README: VS Code tasks table, equivalent commands, serial monitor notes; fixed the CubeIDE line (Makefile project, not STM32 CMake Project).

### 2026-11-02 (deliverables 5/7: repo + README)
- zephyr_env.sh made portable (env overrides, SDK detection with a real read, clear message when nothing is found); tested on this PC (gnuarmemb), a simulated readable SDK, a user GCC path and no tools. baremetal toolchain file also searches PATH. README: new intro, deliverables map, requirements/get code/toolchain/build/flash sections. Pristine builds of both programs OK in a fresh shell.

### 2026-11-02 (deliverable 4: ENC28J60)
- Switched Ethernet to the ENC28J60 on SPI1 (overlay: disable &mac, &mdio, &eth_phy; enc28j60 node). First build failed: eth_phy stays enabled under a disabled mdio. 10-pin module is 3.3 V only; RESET/CS have internal pull-ups (DS39662C pin table note 4), RESET left open.
- Hardware: driver initialised on first boot (SPI OK). No link at first: cable was not in the module's RJ45. Then PC "Ethernet 3" Up at 10 Mbps half duplex, board carrier ON. PC adapter had only an APIPA address, so tested via `net ipv4 add 1 169.254.50.2 255.255.0.0` on the board: ping 6/6.

### 2026-11-02 (source folders)
- src/ split by function: `net/`, `sensor/`, `led/hal/`, `led/cmsis/`, with `src/` as the single include root. misra_check.sh now finds .c files recursively (`find src -name '*.c'`). Same image size (169,752 B); flashed, boots and reads the sensor.

### 2026-11-02 (deliverable 3: BME280)
- Added the BME280 (overlay node, prj.conf I2C/SENSOR + shells, app_sensor.c/.h). Flashed and tested: scan shows 0x76, readings every 2 s, `sensor get` agrees. CONFIG_REBOOT is off, so `kernel reboot cold` isn't available in the shell.

### 2026-11-02 (comment style)
- User rule, now in CLAUDE.md Coding standard: Doxygen comments (`@file`/`@brief` per file, public API documented once in the header, `/**< */` for members), inline comments only for why/hardware/RM references. Converted src/ and baremetal/blinky; comment-only change, both images rebuild to the same size.

### 2026-11-02 (deliverable 1: register blinky)
- Added baremetal/blinky: standalone, no HAL/RTOS/vendor headers. Register addresses cross-checked in RM0468 Table 7 (pdftotext shifts that table's peripheral names one row down: RCC is 0x58024400, GPIOB 0x58020400, GPIOE 0x58021000), Zephyr DTS and ST's stm32h723xx.h. No PWR_CR3 setup needed because sys_ck stays at the reset HSI 64 MHz (RM0468 6.8.4). Vector table as a struct (SP pointer + 15 handlers) avoids a data-to-function pointer cast. `-fno-tree-loop-distribute-patterns` keeps the startup loops from turning into memcpy/memset calls under -nostdlib. No make on this PC: CMake + Ninja.

### 2026-11-02 (new docs)
- Read the four new PDFs in docs/ (two cohort documents, ENC28J60 and BME280 data sheets). Added week-one deliverables and part facts above, and the files to CLAUDE.md's reference list.

### 2026-11-02 (Zephyr pin)
- Added west.yml (T2: app repo as manifest) pinning zephyr v4.4.0 with a 5-module allowlist. Validated with `west manifest --validate` in a scratch workspace (shared local clone, no download): resolves to the same commits as D:\Workspace. Not tested: a full `west update` + build in a fresh workspace (needs ~1 GB download).

### 2026-11-01 (board suitability review)
- Reviewed docs/Board-Suitability-P1-P2.pdf against the board and the build. The H723ZG fits. The current debug build (219,920 B flash, ~34 KB RAM release) is within the P1 budget. The ETH DMA buffers already follow the cache rule (non-cacheable SRAM2). Main gap: the brief wants ENC28J60, we use onboard ETH. User decided to switch, starting 2026-11-02.

### 2026-10-30 (rename, CubeIDE import)
- Project renamed to **Eco-Connect IoT Gateway** (CMake `project(eco_connect_iot_gateway)`, README/CLAUDE titles). Repo folder, GitHub repo (kgsinh/eco-connect-gateway; old URL redirects) and CubeIDE project renamed `eco-connect-gateway`; after moving the folder, rebuild with `-p always` (CMake cache holds absolute paths).
- The New → STM32 CMake Project wizard always writes its own template (CMakeLists, Startup/, Sources/, .ld) and can't adopt the Zephyr tree; a name with spaces also breaks its CMake target. Use File → Import → Existing Code as Makefile Project with build command `west build ...` instead.
- `tools/cubeide.sh` opens workspace D:/Workspace/STM32H7 (`-data`). analysis_flags.py no longer hardcodes the folder name.
- CDT include file for the indexer: Zephyr ≥3.5 puts autoconf.h at `build/zephyr/include/generated/zephyr/autoconf.h` (the Zephyr CubeIDE doc's `generated/autoconf.h` is outdated). Added in Paths and Symbols → Include Files (GNU C). In the Add dialog, OK stays greyed out until the File field holds a valid path (untick "Is a workspace path" for a `D:\...` path). The file is gone after `-p always` until the next build.
- After the folder rename, `west build -p always` itself failed (CMakeCache still pointed at h7_blinky, pristine can't clean it): `rm -rf build` and rebuilt, 147,448 B as before. The CubeIDE project in the workspace is still the STM32 CMake Project wizard one (`cmake --build Debug` via its CMakePresets.json, untracked Sources/, Startup/, Debug/, .ld, cubeide-gcc.cmake in the repo root), so CubeIDE's Build doesn't run west yet.
- Fixed by hand (CubeIDE closed): re-imported as Makefile project, builder command `west build -b nucleo_h723zg -- -DEXTRA_CONF_FILE=debug.conf`, clean target `-t pristine`. Unticking the Behaviour "Build" checkbox (instead of just clearing `all`) made Build do nothing; set `enabledIncrementalBuild="true" incrementalBuildTarget=""` in .cproject. Removed the leftover CMake nature/builder (.project) and cmakeTarget module (.cproject), deleted the wizard template files. Then the builder failed with 'Program "west build ..." not found': moved the args into the Behaviour Build target, command `west` only. CubeIDE build OK (debug). The autoconf.h include is a CDT User Setting Entry (Preprocessor Include Paths → GNU C, workspace path `/eco-connect-gateway/build/zephyr/include/generated/zephyr/autoconf.h`). It lives in the workspace metadata (`D:/Workspace/STM32H7/.metadata/.plugins/org.eclipse.cdt.core/eco-connect-gateway.language.settings.xml`), not in the repo, so a fresh workspace/clone needs it added again.
- CubeIDE debug: one session suspended in `arch_system_halt(reason=4)` = K_ERR_KERNEL_PANIC (in the debug build usually a failed `__ASSERT`). Not reproduced: debug build ran 3 min standalone, under CLI GDB with breaks on assert_post_action/z_fatal_error, and in a second CubeIDE session (Suspend landed in arch_cpu_idle = normal idle); no assert/fatal text on the console in any run. If it recurs: keep COM5 open to catch the message, `bt` in the Debugger Console. Suspects in zephyr.launch: RTOS proxy set to ThreadX, live expressions, halt on exception.

### 2026-10-29 (debugger bring-up)
- F5 first ran the C/C++ Runner entry ("outDebug does not exist"): it was still the remembered selection and re-added itself until the user disabled the extension per workspace. I removed the entry.
- Then "Could not verify ST device": the H7 was asleep in WFI. Added `-k` (connect under reset) to the launch config and `STM32_ENABLE_DEBUG_SLEEP_STOP` to debug.conf. Verified on the board via CLI: ST-LINK_gdbserver -k + GDB load (219,912 B), break at main, `next` ×2 (lines 24 → 28), read GPIOB->ODR.

### 2026-10-29 (debugging)
- Added the debug setup: debug.conf, tools/zephyr_env.sh, tools/vscode/{launch,tasks}.json (installed into .vscode/). Debug build OK (219,920 B). Not tried live: no ST-Link was connected.

### 2026-10-27 (static analysis rework)
- Reworked static analysis for the Zephyr layout. MPLAB's cppcheck 2.11 + misra.exe can't handle Zephyr headers (silent empty dumps), so with the user's approval switched MISRA to the official cppcheck 2.21.0 misra.py addon, downloaded to `D:\Workspace\cppcheck-addons-2.21.0`. I reviewed the code first: it only reads dumps, and its one subprocess call is for a premium addon that isn't present.
- Worked around these cppcheck issues: CRLF in flags, 3+ `--include`, GCC register binding, 2.11 DT macro expansion. Found a misra.py 2.21 rule 7.3 regex bug (`Pull`).
- Fixed 2× Rule 8.9. Everything else is a documented deviation (tools/misra_deviations.txt). Rebuilt OK (147,448 B flash). The user reflashed and tested on 2026-10-29: LEDs and ping still work.

### 2026-10-27 (docs refresh)
- Brought README (Ethernet static IP + PC setup, console how-to, build size, Kconfig role), CLAUDE.md (Ethernet in Hardware, new Console section) and NOTES (build size) in line with the code.

### 2026-10-27 (Ethernet)
- Added Ethernet + DHCPv4 (`src/app_net.c`) and the net shell, plus a static 192.168.50.2/24 address alongside DHCP for a direct PC link (user asked for the best way to cable to a PC); prj.conf has the networking, logging, entropy and shell options. Builds clean with gnuarmemb. Tested on the board, direct cable to the PC (PC 192.168.50.1/24): PHY ID 0x7C131 (LAN8742A) detected, link 100 Mb full duplex, `ping 192.168.50.2` from the PC 4/4 replies <1 ms, TTL 64. DHCP got no lease (expected without a server).
- Found that docs/um3252 is the Class B library guide, not the board UM; corrected CLAUDE.md and README.

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
