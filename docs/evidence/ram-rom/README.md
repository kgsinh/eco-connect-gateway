# RAM and ROM evidence

Zephyr's footprint reports for the gateway firmware, and the DMA buffer decision the project brief asks for.

## Build

| | |
|---|---|
| Commit | the commit that adds this file: `4bc2cf7` (static web page, TCP echo server, BME280, ENC28J60) plus `NET_MAX_CONN=8` and `ZVFS_POLL_MAX=5` |
| Variant | Release (`west build -p always -b nucleo_h723zg`, `-Os`, no debug.conf) |
| Toolchain | Zephyr 4.4.0, Arm GNU GCC 14.3.1 (STM32CubeIDE 1.17.0) |
| Network mode | Static IPv4 (`CONFIG_APP_NET_STATIC=y`) |

Files: [ram_report.txt](ram_report.txt) and [rom_report.txt](rom_report.txt), the full output of `west build -t ram_report` and `west build -t rom_report` (every symbol, grouped by source path). To regenerate, build the release variant, then run both targets; they also write `build/ram.json` and `build/rom.json`.

## Totals against the brief (P1 gateway)

| | Used | Budget | Linker summary |
|---|---|---|---|
| RAM | 57,318 B (56.0 KiB) | ~48-59 KB expected, target 64 KB | 57,392 B of 320 KB AXI SRAM |
| ROM | 213,640 B (208.6 KiB) | 384 KB | 213,656 B of 1 MB flash |

The report totals are slightly below the linker's memory-usage table because the linker also counts alignment padding between sections.

## Where it goes

| Component | RAM (B) | ROM (B) | Main items |
|---|---|---|---|
| Our code (`src/`) | 3,934 | 3,343 | TCP echo thread stack 2,112, sensor thread stack 1,088, echo buffer 256 |
| Network stack (`subsys/net/ip`, `l2`) | 23,627 | 37,592 | packet and buffer pools, TCP connections, 8-entry connection table, RX/TX threads |
| HTTP server (`subsys/net/lib/http`) | 7,361 | 11,032 | thread stack 3,136, client contexts 4,008 (3 clients) |
| Sockets, DHCPv4 (`subsys/net/lib`) | 92 | 9,691 | |
| Net shell (`subsys/net/lib/shell`) | 317 | 16,464 | bring-up commands (`net iface`, `net conn`, ...) |
| Shell core (`subsys/shell`) | 4,415 | 16,628 | shell thread stack, UART backend |
| Sensor shell (`drivers/sensor/sensor_shell.c`) | 5,061 | 5,134 | bring-up command `sensor get` |
| Logging | 2,246 | 5,510 | log thread, message buffer |
| Kernel | 5,586 | 11,914 | main, idle, ISR and work-queue stacks |
| Other drivers (ENC28J60, BME280, SPI, I2C, GPIO, UART, RNG, clocks) | 2,242 | 26,894 | ENC28J60 runtime 1,280 (RX thread with its 800 B stack) |
| RTIO, C library, HAL, arch, SoC, unattributed | rest | rest | |

## Reserves if memory gets tight

Bring-up tools that a production build can drop, measured above:

- Sensor shell (`CONFIG_SENSOR_SHELL=n`): about 5 KB RAM, 5 KB ROM.
- Net shell (`CONFIG_NET_SHELL=n`): about 16 KB ROM.
- TCP echo server (`src/net/tcp/`): about 2.4 KB RAM, two socket contexts.

## DMA buffers: decision

**Now: no DMA.** `CONFIG_DMA` is not set and no enabled peripheral has `dmas` in the devicetree; the only `dmas` properties belong to disabled SoC peripherals (I2S, DCMI, SAI). SPI1 (ENC28J60), I2C1 (BME280) and USART3 (console) all move data by CPU, so the D-cache needs no maintenance today. The on-board Ethernet MAC, which used DMA with a non-cacheable SRAM2 region, is disabled.

**When a peripheral gets DMA** (candidates: SPI1 for the ENC28J60, the W25Q flash in P2), buffers go in a **non-cacheable MPU region**, using Zephyr's `__nocache` attribute with `CONFIG_NOCACHE_MEMORY=y`.

Why non-cacheable instead of cache clean/invalidate:

- The D-cache is on (`CONFIG_DCACHE=y`), and the RAM is AXI SRAM at 0x24000000, which is cacheable. A buffer in normal RAM would need `sys_cache_data_flush_range()` before every DMA transmit and `sys_cache_data_invd_range()` after every receive.
- Invalidate works on whole 32-byte cache lines. A receive buffer that isn't 32-byte aligned and sized to a multiple of 32 shares lines with neighbouring variables, and invalidating it discards their pending writes. That bug is silent and depends on timing.
- With a non-cacheable region, the CPU and DMA always see the same memory, with no per-transfer maintenance to forget. The cost is slower CPU access to those buffers, which is acceptable for I/O buffers.

**Never DTCM.** DMA1 and DMA2 "can access all internal memories except ITCM and DTCM" (RM0468 Rev 3, §2.1.6 *Bus master peripherals*); only the MDMA reaches DTCM, through the Cortex-M7 AHBS port. A DMA buffer placed in DTCM is never read or written by the DMA.
