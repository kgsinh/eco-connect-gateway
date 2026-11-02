# Project notes

Running log of decisions, gotchas and open items. Newest session at the top of the session log; the sections above it hold the current state of the project.

## Current architecture

- **Single Zephyr app at the repo root** (Zephyr 4.4.0, west workspace `D:\Workspace`, board `nucleo_h723zg`). Zephyr owns startup, clocks, MPU, SysTick, IRQs and linking. The CubeIDE/CubeMX project was removed (it's in git history).
- Source layout: `src/main.c`, `src/net/` (Ethernet/IPv4), `src/sensor/` (BME280), `src/led/` (HAL, CMSIS LED drivers); include root is `src/` (`#include "led/led_hal.h"`).
- Three API layers in one app, one LED each:
  - LD1 green PB0: Zephyr GPIO (`led0` DT alias) in `src/main.c`.
  - LD2 yellow PE1: STM32Cube HAL GPIO in `src/led/led_hal.c`, every 500 ms. HAL comes from Zephyr's hal_stm32 module, enabled by `config APP_STM32_HAL_GPIO` → `select USE_STM32_HAL_GPIO` in `Kconfig`.
  - LD3 red PB14: CMSIS registers in `src/led/led_cmsis.c`.
- Main loop: LED chase, one LED on at a time, LD1 → LD2 → LD3, 250 ms per step (`led_toggle()` dispatches to the layer). Deliberately different from the bare-metal blinky (independent 1000/500/250 ms) so the running image is obvious.
- `boards/nucleo_h723zg.overlay` disables `timers12`, `pwm12` and `/pwmleds`, so the Zephyr PWM driver doesn't claim PB14.
- Clock (upstream board DTS): HSE 8 MHz (ST-Link MCO, bypass) → PLL1 550 MHz CPU, HCLK 275 MHz, APB 137.5 MHz. Console: USART3 (ST-Link VCOM).
- LED-only build was 21,168 B flash / 4,608 B RAM and was tested on the board (2026-10-27).
- **BME280** (`src/sensor/app_sensor.c`): Zephyr `bosch,bme280` driver on I2C1 at 0x76, own thread (prio 7, 1 KB static stack), logs `T .. C  P .. kPa  H .. %RH` every 2 s without float printf (milli split). `app_sensor_start()` returns -ENODEV and logs if the sensor doesn't probe; the rest keeps running. I2C and sensor shells enabled for bring-up (`i2c scan i2c@40005400`, `sensor get bme280@76`). Release build 169,752 B flash, 42,112 B RAM.
- **Ethernet + IPv4** (`src/net/app_net.c`): Zephyr `eth_stm32_hal` driver (HAL ETH API v2, selects USE_STM32_HAL_ETH itself) + generic MII PHY driver for the LAN8742 at MDIO address 0, RMII pins from the board DTS. `app_net_start()` (called from main after LED init) first adds a static address `192.168.50.2/24` (`APP_NET_STATIC_IPV4_ADDR/_NETMASK` in the app Kconfig; empty = off) for a direct PC cable, with the PC set to 192.168.50.1/24. It then starts DHCPv4 without blocking (for router use; `NET_IF_UNICAST_IPV4_ADDR_COUNT=2` so both addresses fit) and logs IP/netmask/gateway/lease on `NET_EVENT_IPV4_ADDR_ADD`. Ping replies come from the IPv4 stack. The net shell is on the console (`net iface`, `net ping`, `net arp`). Build: 147,448 B flash, 33,984 B RAM.

## Things to know

- **The Zephyr SDK is unusable for now.** `C:\Users\ksrgo\zephyr-sdk-1.0.1` is unreadable by the user account (Access denied even for icacls; probably installed elevated), and the user env var `ZEPHYR_SDK_INSTALL_DIR` points at it, which forces CMake to require it. Workaround in use: `unset ZEPHYR_SDK_INSTALL_DIR` + `ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb` with CubeIDE's arm-none-eabi-gcc 14.3 (see CLAUDE.md). Fix: in an admin PowerShell, `takeown /f <dir> /r /d y` and `icacls <dir> /grant "ksrgo:(OI)(CI)F" /t`, then run `<dir>\setup.cmd`.
- **STM32Cube HAL/LL Kconfig symbols are promptless.** `CONFIG_USE_STM32_HAL_GPIO=y` in prj.conf is silently ignored; select it from the app `Kconfig`. For H7, `<stm32h7xx_hal.h>` exposes all module headers; Kconfig only decides which HAL .c files compile.
- **`HAL_GetTick()`/`HAL_Delay()` map to `k_uptime_get_32()`/`k_msleep()`** (zephyr/soc/st/stm32/common/stm32cube_hal.c). HAL timeouts work without `HAL_Init`.
- **GPIOB is shared** by the Zephyr driver (PB0) and CMSIS code (PB14). This is safe because CMSIS writes BSRR (single pin), and everything runs in one thread. Keep it BSRR-only if threads are added.
- **Port clocks:** HAL/CMSIS code enables GPIOE/GPIOB clocks itself even though the Zephyr gpio driver already did; enabling is idempotent.
- **The board's pwm-led0 alias still points at the disabled PWM LED.** Don't enable CONFIG_LED_PWM / use pwm-led0 unless the overlay is changed back.
- **CubeIDE recreates `.project` while the old project is still open in its workspace.** Eclipse files are gitignored now. Remove the old h7_blinky project from the CubeIDE workspace (don't delete contents), or import the folder as an *STM32 CMake Project* to use CubeIDE as editor/debugger (Zephyr doc: develop/tools/stm32cubeide.rst, validated on Linux 1.16 only).
- **Ethernet MAC address** is not configured; the driver builds it from the chip UID: `02:80:E1:` + 3 bytes of CRC32(UID), locally administered and stable per board. Set `local-mac-address` in the overlay to override it.
- **Ethernet DMA buffers live in SRAM2** (`memory-regions = <&sram2>` in stm32h7.dtsi). The DTS doesn't mark SRAM2 non-cacheable. While `&mac` is enabled, zephyr/soc/st/stm32/stm32h7x/mpu_regions.c adds the MPU region SRAM_ETH_BUF (16 KB, non-cacheable) and soc sections.ld places the buffers there. Disabling `&mac` (ENC28J60 switch) removes both, so SRAM2 is free again. That is why SRAM2 shows 100% used, even in the LED-only build. Don't put other data there.
- **docs/um3252-stm32h7-series.pdf is the X-CUBE-CLASSB self-test library guide, not the board manual.** It was mislabelled in CLAUDE.md/README until 2026-10-27. The Nucleo-144 board manual is UM2407 (not downloaded yet).
- **Console:** ST-Link VCOM is **COM5** on this PC, 115200 8N1. Use `python -m serial.tools.miniterm --filter direct COM5 115200` (without `--filter direct`, miniterm shows the ANSI colour codes as `␛[..m`) or PuTTY `-serial COM5 -sercfg 115200,8,n,1,N`. The shell prompt is `uart:~$`.
- **Windows Firewall blocks inbound ping by default**, so `net ping 192.168.50.1` from the board may time out. Test PC → board.
- **Debugging:** VS Code Cortex-Debug + ST-LINK_gdbserver + CubeIDE GDB + STM32H723 SVD. Templates are in tools/vscode/, the live copies in .vscode/ (the old C/C++ Runner launch.json is kept as .vscode/launch.json.bak). Debug build = `-DEXTRA_CONF_FILE=debug.conf` (-Og, ASSERT, DEBUG_THREAD_INFO, INIT_STACKS): 219,920 B flash vs 147,448 B release. Same build/ dir for both, so check `.config` before flashing.
- **The C/C++ Runner extension re-adds a "C/C++ Runner: Debug Session" entry to .vscode/launch.json** (PC program `Core/Src/build/Debug/outDebug`). Selecting it gives "launch: program ... outDebug does not exist". Delete that entry, pick a "(ST-LINK ...)" config in the Run and Debug dropdown, and disable C/C++ Runner for this workspace (Extensions → C/C++ Runner → Disable (Workspace)).
- **H7 + Zephyr idle = no hot attach.** Zephyr idles in WFI, and with DBGMCU debug-in-sleep off the H7's debug port stops answering: ST-LINK_gdbserver says "Could not verify ST device" and CubeProgrammer says "Unable to read device id from ROM table". Fixes: connect under reset (`-k` in the launch config; CubeProgrammer `mode=UR`; `west flash` already uses hw reset), and `CONFIG_STM32_ENABLE_DEBUG_SLEEP_STOP=y` in debug.conf so Attach works on debug builds. The release build doesn't set it, so attaching to it needs a reset.
- **STM32CubeIDE build (working 2026-10-30):** start CubeIDE via `tools/cubeide.sh` (inherits zephyr_env.sh). Project = File → Import → Existing Code as Makefile Project (not the STM32 CMake Project wizard: it writes its own template and can't adopt Zephyr). C/C++ Build: Build command `west` alone (with args in that field CDT runs the whole string as one program name); Behaviour: Build ticked with target `build -b nucleo_h723zg -- -DEXTRA_CONF_FILE=debug.conf` (drop the `--` part for release), Clean `build -t pristine`. Indexer: autoconf.h as a CDT User Setting Entry (include file `build/zephyr/include/generated/zephyr/autoconf.h`; Zephyr ≥3.5 path, the Zephyr CubeIDE doc's is outdated), stored in workspace metadata, not the repo. Debug: File → Import → STM32 Cortex-M Executable → build/zephyr/zephyr.elf, Reset behaviour = Connect under reset (zephyr.launch, gitignored).
- **Zephyr version pinned in `west.yml`** (2026-11-02): zephyr v4.4.0 + allowlist cmsis_6, hal_stm32, picolibc (what the build compiles), hal_st (sensors later), littlefs (P2). D:\Workspace keeps its own manifest (`[manifest] path = zephyr`) and is not affected; its module commits match the pin. A fresh `west init -l` checkout on Windows needs `git config --global core.longpaths true` (Zephyr has paths over 260 chars).
- **`tools/zephyr_env.sh`** holds the gnuarmemb toolchain and STM32_Programmer_CLI paths (one place to update when CubeIDE changes).
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
4. ENC28J60 via Zephyr's driver: planned for 2026-11-02.
5/7. Repo + README: clean build from a fresh clone, README states the board, toolchain version, build and flash. The brief says *public* repo; GitHub repo is currently PRIVATE (user's call). Fresh-clone build is untested; tools/zephyr_env.sh hardcodes this PC's CubeIDE paths.
6. Pins & clocks note: LED pins, system clock, I2C pins, ENC28J60 SPI pins + clock.
8. One-minute live demo.

## Parts facts (from the data sheets in docs/)

- **ENC28J60** (DS39662C): 3.1-3.6 V, inputs 5 V tolerant, SPI **mode 0 only** (SCK idle low), up to 20 MHz, needs a 25 MHz crystal (on the module), 10BASE-T only. INT output is active low (binding `int-gpios` required, `GPIO_ACTIVE_LOW`). Current up to 180 mA while transmitting (160 typ). Silicon errata (not in docs/) is said to require SCK >= 8 MHz: check before choosing `spi-max-frequency`.
- **BME280** (rev 1.24): VDD 1.71-3.6 V, I2C up to 3.4 MHz. Address 0x76 (SDO to GND) or 0x77 (SDO to VDDIO); SDO must not float. CSB tied to VDDIO selects I2C. chip_id register 0xD0 reads 0x60 (a BMP280 reads 0x58: cheap modules are often BMP280s).
- **Board buses for them (upstream DTS):** `arduino_spi` = SPI1: SCK PA5 (D13), MISO PA6 (D12), MOSI PB5 (D11), CS PD14 (D10), already `okay`. `arduino_i2c` = I2C1: SCL PB8 (D15), SDA PB9 (D14), 400 kHz, `okay`. MOSI is on PB5, not PA7, because PA7 is RMII CRS_DV.

## Open items

- **Ethernet differs from the brief:** P1 wants the ENC28J60 over SPI (the onboard RJ45 is a stretch goal for after P1), but app_net.c uses the onboard MAC + LAN8742. Decided 2026-11-01: switch to ENC28J60, planned for 2026-11-02 (`microchip,enc28j60` driver, SPI + INT pin, disable `&mac`/`&mdio` in the overlay).
- P1 parts not started: BME280 (I2C), IWDG, HTTP server, ram/rom reports in docs/evidence/ram-rom/, written DMA/cache choice.

- Confirm F5 in VS Code end to end. The same chain is verified from the CLI on the board (see the session log); the Peripherals view is not tried yet.
- Test Ethernet via a router (DHCP lease, `net ping <gateway>`); the direct-PC static path is tested.
- Download UM2407 (Nucleo-144 board manual) into docs/.
- Fix the Zephyr SDK permissions (see Things to know), or decide to stay on gnuarmemb.
- Re-establish the no-malloc rule for Zephyr (the old link-time trick was CubeIDE-specific). Options: `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0`, no `CONFIG_HEAP_MEM_POOL_SIZE`, and a check that `malloc`/`k_malloc` aren't linked.

## Session log

### 2026-11-02 (source folders)
- src/ split by function: `net/`, `sensor/`, `led/`, with `src/` as the single include root. misra_check.sh now finds .c files recursively (`find src -name '*.c'`). Same image size (169,752 B); flashed, boots and reads the sensor.

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
