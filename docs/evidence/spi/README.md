# SPI evidence: ENC28J60 boot sequence on the wire

Logic-analyzer capture of SPI1 from reset to the first link-up interrupt, decoded and checked against the ENC28J60 data sheet (DS39662C), its errata (DS80349C) and Zephyr's driver.

| File | What |
|---|---|
| [spi-4m3-boot.csv](spi-4m3-boot.csv) | Raw edge times exported from Logic 2 (channels: 4 SCK, 2 MOSI, 0 MISO, 6 CS) |
| [spi-4m3-boot-decoded.txt](spi-4m3-boot-decoded.txt) | All 98 transactions, decoded with `python tools/spi_decode.py spi-4m3-boot.csv` |

## Why 4.3 MHz and not 17.2 MHz

The analyzer is a 24 MS/s unit. The firmware runs SPI1 at 17.2 MHz (PLL1_Q 137.5 MHz / 8), fewer than 1.5 samples per clock, so a capture at that speed aliases and decodes as garbage. For this capture the board ran a temporary image with `spi-max-frequency` = 5 MHz, giving /32 = 4.30 MHz (5.6 samples per clock). Firmware and protocol are otherwise identical; the release image was restored afterwards. A < 8 MHz clock is allowed here because the module is silicon B7 (errata issue 1 affects only B1/B4).

**Measured SCK: 230.9 ns = 4.33 MHz**, averaged over 205 bytes (7 periods each; per-byte spread 2.4 ns). Expected 232.7 ns = 4.297 MHz; the 0.8 % difference is within the analyzer's timebase accuracy. Each byte shows 8 rising edges with CS low, SCK idle low: SPI mode 0, as the ENC28J60 requires.

The 17.2 MHz setting itself is shown by the devicetree (`spi-max-frequency` = 20 MHz) and the prescaler rule in `drivers/spi/spi_stm32.c` (first divider at or below the limit), not by this capture.

## Decoded sequence

Notation: register writes are WCR (`0x40 | addr`), reads RCR (`addr`, plus a dummy byte for MAC/MII registers), bit set BFS (`0x80 | addr`), bit clear BFC (`0xA0 | addr`).

| # | Time | MOSI → MISO | Meaning |
|---|---|---|---|
| 0 | 1.5 µs | `9F 03` | BFS ECON1, BSEL = 3 (our `enc28j60_revid.c`) |
| 1 | 11.2 µs | `12 00` → `06` | RCR EREVID: **0x06 = silicon B7** |
| 2 | 20.6 µs | `BF 03` | BFC ECON1: back to bank 0 |
| 3 | 40.2 µs | `FF` | Driver: system reset command |
| 6-7 | 77 µs | `48 00`, `49 00` | ERXST = 0x0000 (RX buffer at 0, errata) |
| 8-9 | 95 µs | `4C 00`, `4D 00` | ERXRDPT = 0x0000 |
| 10-11 | 114 µs | `4A FF`, `4B 0B` | ERXND = 0x0BFF: RX FIFO 3,072 B |
| 12-15 | 132 µs | `44 00 45 0C`, `46 FF 47 11` | ETXST = 0x0C00, ETXND = 0x11FF: TX 1,536 B |
| 22 | 224 µs | `58 A3` | ERXFCON = 0xA3 (unicast, multicast, broadcast, CRC check) |
| 23 | 246 µs | `1D 00` → `01` | ESTAT.CLKRDY = 1 |
| 27-31 | 286 µs | `40 0D`, `42 32`, `46 12`, `44 12`, `43 40` | MACON1 = 0x0D (MARXEN, TX/RX pause), MACON3 = 0x32 (pad, CRC, length check), MAIPGL = 0x12, MABBIPG = 0x12, MACON4 = 0x40 (half duplex) |
| 34-39 | 352 µs | `41 02 40 00 43 50 42 A3 45 04 44 00` | MAC address 00:04:A3:50:00:02 (MAADR1..6), as in the boot log |
| 42-47 | 426 µs | MIREGADR 0x00, MIWR 0x0000, MISTAT read | PHCON1 = 0x0000: half duplex |
| 50-55 | 515 µs | MIREGADR 0x10, MIWR 0x0100 | PHCON2 = HDLDIS (no loopback in half duplex) |
| 56-58 | 585 µs | `9B 80`, `9B 40`, `9B 10` | BFS EIE: INTIE, PKTIE, LINKIE |
| 61-66 | 631 µs | MIREGADR 0x12, MIWR 0x0012 | PHIE = PGEIE + PLNKIE: link-change interrupt |
| 67 | 701 µs | `9F 04` | BFS ECON1.RXEN: **reception on, init done 0.7 ms after the first command** |
| 68 | 59.90 ms | `BB 80` | Link-up interrupt: RX thread clears EIE.INTIE first |
| 69 | 59.91 ms | `1C 00` → `10` | EIR = LINKIF |
| 72-81 | 59.94 ms | PHIR read → 0x0014 | PGIF + PLNKIF, read to clear |
| 84-93 | 60.07 ms | PHSTAT2 read → MIRDH 0x04 | LSTAT = 1: **link up** (boot log: `Link up` at 51-62 ms) |
| 95-96 | 60.20 ms | ECON1 bank 1, `19 00` → `00` | EPKTCNT = 0: no frames waiting (the driver counts frames with EPKTCNT, not EIR.PKTIF) |
| 97 | 60.22 ms | `9B 80` | BFS EIE.INTIE: interrupts on again |

## Driver vs. errata, seen on the wire

The two gaps found by reading the driver source are visible in this capture:

- **ERXRDPT is written even** (transactions 8-9: 0x0000). Errata issue 14 asks for an odd value (ERXND or next-packet-pointer − 1) to avoid RX buffer corruption.
- **14 µs between the soft reset and the next access** (end of transaction 3 at 43.9 µs, transaction 4 at 58.1 µs). The errata asks for at least 1 ms after a system reset, because ESTAT.CLKRDY isn't cleared by it and polling it proves nothing.

Neither has caused a fault on the bench so far; both are upstream driver behaviour (Zephyr 4.4.0).

## Other observations

- **Per-command overhead doesn't scale with the clock.** A 2-byte command keeps CS low 6.5 µs, of which 3.7 µs are clock cycles at 4.3 MHz, and commands follow each other every 9.3 µs. At 17.2 MHz the clock part shrinks to 0.9 µs but the ~5.6 µs of software and CS overhead per transaction stays. Frame reads use bulk buffer reads (up to 128 B per transfer), so this overhead adds roughly 70 µs to reading a full 1,518 B frame.
- **INT is not in this capture.** Channel 5 stays high throughout, although the driver services a link interrupt at 59.9 ms. The INT probe was not on channel 5; re-capture with INT on a free channel to see the ISR-to-SPI latency.
