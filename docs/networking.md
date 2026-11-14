# Networking: ENC28J60 + Zephyr TCP/IP

How the gateway's Ethernet and TCP/IP stack is set up, why, and what has been tested on the board. Pin and clock values are in [pins-and-clocks.md](pins-and-clocks.md); the code layers are in [architecture.md](architecture.md).

## Goal (week of 2026-11-13)

Bring up the Zephyr TCP/IP stack over the ENC28J60. Done when the board gets an IP address, answers ping, and serves a static web page.

| Criterion | Status |
|---|---|
| IP address | Static 192.168.50.2/24: tested. DHCPv4: builds, not tested yet (needs a router or a DHCP server). |
| Ping | Tested, PC → board, 1-2 ms, TTL 64. |
| TCP | Echo server on port 4242 (`src/net/tcp/app_tcp.c`): tested, `tools/echo_test.py` 5/5 lines echoed, twice in a row (the accept loop serves a new client after the previous one closes). |
| Static web page | Zephyr HTTP server on port 80 (`src/net/http/app_http.c`): tested: `/` shows the page in a browser, unknown paths return `404 Not Found` (curl). |

## Stack configuration

| Kconfig | Why |
|---|---|
| `NET_IPV4`, `NET_ARP`, `NET_UDP` | IPv4, ARP, and UDP for DHCP. Ping (ICMPv4) is part of `NET_IPV4`; Zephyr 4.4 has no `NET_ICMPV4` symbol. |
| `NET_TCP`, `NET_SOCKETS` | TCP state machine and the `zsock_*` socket API. Cost: +22.7 KB flash, +6.1 KB RAM. |
| `NET_CONFIG_SETTINGS=n` | `app_net.c` sets the address itself (static or DHCP, Kconfig choice `APP_NET_MODE`). This also avoids `NET_CONFIG_AUTO_INIT`, which can hold boot for up to 30 s without a cable. |
| `ENTROPY_GENERATOR` | TCP needs random numbers. The STM32H7 RNG is the source (`ENTROPY_STM32_RNG`); `TEST_RANDOM_GENERATOR` (not secure) is not used. |
| `NET_IF_UNICAST_IPV4_ADDR_COUNT=1` | One address per interface, static or DHCP. |

Socket file descriptors: `ZVFS_OPEN_MAX=0` does not mean "none". `lib/os/CMakeLists.txt` sizes the table as the sum of the `ZVFS_OPEN_ADD_SIZE_*` requests (networking 6 + socket service 1 = 7).

## ENC28J60 driver

Zephyr's `microchip,enc28j60` driver, node `enc28j60: ethernet@0` on SPI1 in the board overlay.

- **SPI clock 17.2 MHz** (`spi-max-frequency` 20 MHz; SPI1 runs from PLL1_Q = 137.5 MHz with power-of-2 prescalers, and `spi_stm32.c` takes the first one at or below the limit, so /8). Reason: the driver gives the chip a 3 KB receive FIFO. With back-to-back maximum-size frames, frame 1 must be read and freed within about 1.26 ms of its interrupt. Reading 1539 bytes takes 0.72 ms at 17.2 MHz; the earlier 8.6 MHz needed 1.43 ms, over budget. These are calculated, not measured. Works over jumper wires.
- **MAC address:** the driver replaces the first three bytes with Microchip's OUI 00:04:A3; only the last three from the overlay survive.
- **Interrupt:** INT (PD15) is active low. The ISR only gives a semaphore; the driver's RX thread does all SPI work and counts pending frames with EPKTCNT (EIR.PKTIF is unreliable per the errata).
- **No reset pin:** the binding has none; the driver resets the chip with the SPI soft-reset command.
- **Silicon revision:** the driver defines `EREVID` (bank 3, 0x12) but never reads it, so `src/net/enc28j60_revid.c` reads it once at boot and logs it. It runs as `SYS_INIT(POST_KERNEL, 55)`, between the SPI controller (50) and the ENC28J60 driver (60), so it is the chip's only SPI user while it runs (order checked with `west build -t initlevels`; a `BUILD_ASSERT` guards the priorities). Three SPI commands: BFS ECON1.BSEL = 3, RCR 0x12, BFC ECON1.BSEL back to bank 0. ETH registers are readable before the oscillator start-up timer expires (DS39662C 2.2), so it doesn't wait for CLKRDY. Values from errata DS80349C table 1: 0x02 B1, 0x04 B4, 0x05 B5, 0x06 B7. **Our module reads 0x06 = B7** (current silicon; the < 8 MHz SPI erratum does not apply).

### Driver vs. errata (Zephyr 4.4.0 `drivers/ethernet/eth_enc28j60.c`)

Found by reading the source against the datasheet and errata. Not reproduced on hardware; questions for the bench, not confirmed bugs. The upstream tree is not edited.

| Topic | Errata / datasheet | Driver |
|---|---|---|
| ERXRDPT must be odd (errata 14) | Write `NextPacketPointer - 1` (or ERXND) | Writes even values; workaround commented out (lines 648-655) |
| Delay after soft reset | ≥ 1 ms; CLKRDY is not cleared by a soft reset | 11 µs, then polls CLKRDY |
| TX completion (errata 12) | Treat TXIF or TXERIF as done | Polls TXIF only, no timeout, while holding the semaphore that also guards RX (lines 517-522) |
| Duplex per instance | Each node has its own setting | `DT_INST_PROP(0, full_duplex)` (line 892); harmless with one chip |

## TCP behaviour (Zephyr 4.4.0 defaults)

- Initial retransmission timeout 200 ms, 9 retries: a lost peer is detected 15-23 s after the send.
- Blocking `connect()` gives up after 3000 ms (`NET_SOCKETS_CONNECT_TIMEOUT`).
- Keepalive is off by default.
- `send()` returning success means queued, not delivered; `send()` after the connection left ESTABLISHED returns `ENOTCONN`.

## Application code

- `src/net/app_net.c`: one IPv4 address, static or DHCPv4 (logs the lease from a `net_mgmt` callback).
- `src/net/tcp/app_tcp.c`: TCP echo server, port 4242, own thread (priority 8, 2 KB static stack). It registers for `NET_EVENT_IF_UP` and then checks `net_if_is_up()`, so an interface that came up before the registration is not missed. One client at a time; the echo loop handles short sends.
- Socket addresses are filled through Zephyr's `net_sin()` helper on a `struct net_sockaddr`, so our code has no pointer cast (MISRA C Rule 11.3).
- PC test client: `python tools/echo_test.py [host]` sends five lines and checks each echo.

## Static web page

Zephyr's HTTP server (`CONFIG_HTTP_SERVER`, marked experimental in 4.4, so `CONFIG_EXPERIMENTAL` is on and the build prints a warning; acceptable while Zephyr is pinned at v4.4.0). HTTP/1.1 only: `HTTP_SERVER_VERSION_2=n`, since browsers use HTTP/2 only over TLS.

- **The server owns its thread** (`K_THREAD_DEFINE` in `http_server_core.c`, 3 KB stack, lowest preemptible priority). `http_server_start()` only wakes it; it then listens and serves. The app only describes the site.
- **Site description** (`app_http.c`): `HTTP_SERVICE_DEFINE(gw_http_service, NULL, &port 80, MAX_CLIENTS 3, backlog 2, ...)` listens on all addresses, so it starts before the link is up and works with static or DHCP addresses. `HTTP_RESOURCE_DEFINE(index_resource, gw_http_service, "/", &detail)` with `HTTP_RESOURCE_TYPE_STATIC`, GET only, `text/html`. No fallback resource, so other paths get 404.
- **Linker section:** each `HTTP_RESOURCE_DEFINE` places an entry in section `http_resource_desc_<service>`, and the service walks it between `_..._list_start` and `_..._list_end`. Zephyr's linker script doesn't know the section, so `src/net/http/http_sections.ld` adds it (`ITERABLE_SECTION_ROM`) through `zephyr_linker_sources(SECTIONS ...)` in CMakeLists.txt. Without it the link fails on the undefined list symbols; the service name in both files must match.
- **MISRA:** the resource detail is not `const` because `HTTP_RESOURCE_DEFINE` casts it to `void *` (Rule 11.8); the server only reads it.
- **Cost:** +16.9 KB flash, +7.5 KB RAM for the server (3,136 B thread stack, 4,008 B `server_ctx` for 3 clients with 256 B buffers); the page and table add ~300 B flash.
- **Sockets:** `NET_MAX_CONTEXTS=6`. Idle, `net conn` shows two contexts (listeners on 80 and 4242); each connected client adds one, so the worst case (1 echo + 3 HTTP clients) uses all 6. The DHCP client (UDP 68) uses a connection handler, not a context.
- **The page is `src/web/index.html`.** `generate_inc_file_for_target(app src/web/index.html .../include/generated/index.html.gz.inc --gzip)` in CMakeLists.txt runs `scripts/build/file2hex.py` at build time: gzip level 9 with timestamp 0 (same HTML, same bytes), written as `0x1f, 0x8b, ...` under `build/` and `#include`d inside a `uint8_t` array in `app_http.c`. Editing the HTML and running `west build` regenerates it; no pristine build needed. 1,371 B of HTML becomes 773 B in flash and on the wire.
- **gzip is sent unconditionally:** for a static resource with `content_encoding` set, the HTTP/1 server always adds `Content-Encoding: gzip` and doesn't check the client's `Accept-Encoding` (`http_server_http1.c`). Browsers always accept it; with curl use `--compressed`.
- Tested: `curl -v --compressed` → `200 OK`, `Content-Encoding: gzip`, `Content-Length: 773`, page decoded; `/nothing` → 404; echo server still answers.

## Testing on the bench

PC adapter on the direct cable: 192.168.50.1/24. Board shell on the ST-Link serial port (115200 8N1).

1. `net iface` (board): carrier ON, 10 Mbit, address present.
2. `ping 192.168.50.2` (PC). Windows Firewall blocks inbound ping by default, so test PC → board.
3. `python tools/echo_test.py` (PC): `ok ping 0` … `ok ping 4`.
4. `net stats`, `net conn`, `net tcp` (board): counters and open connections.
5. Wireshark: `ip.addr == 192.168.50.2 and tcp.port == 4242`.

The first ping after boot can report "Destination host unreachable" until the board answers ARP; a retry works.
