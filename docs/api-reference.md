# API reference

Every API the application code in `src/` uses, grouped by layer: what it does and why it is used here. Checked against the Zephyr 4.4.0 headers and sources, the STM32Cube HAL in `modules/hal/stm32`, and this repo's build configuration (`build/zephyr/.config`).

The three LEDs show one job done three ways: Zephyr API (LD1), STM32Cube HAL (LD2), CMSIS registers (LD3). Everything else (sensor, network, web server, logging, threads) uses Zephyr's portable APIs plus the devicetree, so `src/` contains no I2C, SPI or Ethernet controller register code. The one place that talks to a chip directly is `enc28j60_revid.c`, which sends three ENC28J60 commands through Zephyr's SPI API (section 11).

## 1. Devicetree and device model

Hardware is described once in the devicetree (see [architecture.md](architecture.md)); C code refers to it by name and gets constants at compile time.

| API | Where | What it does | Why |
|---|---|---|---|
| `DT_ALIAS(led0)` | main.c | Node the board calls `led0` (green_led, PB0) | No pin number in C; the board file decides |
| `DT_NODELABEL(bme280)` | app_sensor.c | Our overlay node `bme280: bme280@76` | Same, for a node defined in this repo |
| `GPIO_DT_SPEC_GET(node, gpios)` | main.c | Builds `{ .port, .pin, .dt_flags }` from the node | One struct carries everything the GPIO calls need |
| `DT_NODELABEL(enc28j60)` | enc28j60_revid.c | Our overlay node `enc28j60: ethernet@0` on SPI1 | The ID read uses the same bus, CS pin and clock limit as the driver |
| `DEVICE_DT_GET(node)` | app_sensor.c | Pointer to that node's driver instance (`__device_dts_ord_144`) | No lookup at run time |
| `device_is_ready(dev)` | app_sensor.c | True only if the driver initialised successfully at boot | A missing or miswired sensor gives a clear error, not a crash |

## 2. Zephyr GPIO (LD1)

| API | Where | What it does | Why |
|---|---|---|---|
| `gpio_is_ready_dt(&spec)` | main.c | Checks the GPIO port driver is ready | Same safety check as `device_is_ready`, for a GPIO spec |
| `gpio_pin_configure_dt(&spec, GPIO_OUTPUT_INACTIVE)` | main.c | Output, logically off | "Inactive" plus the active-high flag from the devicetree = 0 V; rewiring the LED only changes the devicetree |
| `gpio_pin_toggle_dt(&spec)` | main.c | Inverts the pin | One portable call; the driver does the register work |

## 3. Zephyr kernel

| API | Where | What it does | Why |
|---|---|---|---|
| `k_msleep(ms)` | main.c, app_sensor.c | Sleeps; the CPU runs other threads or idles | Timing without busy-waiting. Returns 0, or the time left if woken early by `k_wakeup()`; unused, hence `(void)` |
| `K_THREAD_STACK_DEFINE(name, size)` | app_sensor.c | Reserves a 1 KB stack at compile time | No dynamic memory (project rule) |
| `K_THREAD_STACK_SIZEOF(stack)` | app_sensor.c | Size to pass to `k_thread_create()` | The real stack object may be larger (guard area, alignment) |
| `struct k_thread` | app_sensor.c | Thread control block, static | No dynamic memory |
| `k_thread_create(...)` | app_sensor.c | Starts `sensor_thread_fn`, priority 7, at once (`K_NO_WAIT`) | A slow I2C read never delays the LED chase in `main()` (priority 0; lower number = higher priority) |
| `k_thread_name_set(thread, "sensor")` | app_sensor.c | Names the thread (`CONFIG_THREAD_NAME=y`) | Shown by the shell's `kernel threads` and in the debugger |
| `K_THREAD_STACK_DEFINE` / `k_thread_create` / `k_thread_name_set` | app_tcp.c | Same pattern for the echo server: 2 KB stack, priority 8, name `tcp` | Socket calls need more stack than the sensor thread |
| `K_SEM_DEFINE(net_up_sem, 0, 1)` | app_tcp.c | Binary semaphore, starts taken, count never above 1 | Signals "interface up" from the net_mgmt callback to the echo thread; a second `IF_UP` (cable replugged) doesn't pile up |
| `k_sem_give()` / `k_sem_take(&sem, K_FOREVER)` | app_tcp.c | Give from the callback, block in the thread | The callback runs in the net_mgmt thread and must not block; the thread waits instead |
| `k_sleep(K_SECONDS(1))` | app_tcp.c | Sleep after a failed `accept()` | A persistent error doesn't turn into a busy loop |
| `SYS_INIT(fn, POST_KERNEL, 55)` | enc28j60_revid.c | Runs `fn` once during boot, after SPI init (50) and before the ENC28J60 driver (60) | The only moment our code can use the chip without sharing it with the driver |
| `BUILD_ASSERT(cond, msg)` | enc28j60_revid.c | Compile-time check | Fails the build if the SPI or Ethernet init priorities ever change the order above |
| `ARG_UNUSED(x)` | app_sensor.c, app_net.c, app_tcp.c | Marks a parameter as intentionally unused | Thread and callback signatures are fixed by Zephyr |

## 4. Zephyr sensor API (BME280)

| API | Where | What it does | Why |
|---|---|---|---|
| `sensor_sample_fetch(dev)` | app_sensor.c | Driver waits until the sensor's "measuring" bit clears, then burst-reads 8 bytes from 0xF7 over I2C | All three values come from the same measurement |
| `sensor_channel_get(dev, chan, &val)` | app_sensor.c | One compensated value: `SENSOR_CHAN_AMBIENT_TEMP` (°C), `SENSOR_CHAN_PRESS` (kPa), `SENSOR_CHAN_HUMIDITY` (%RH) | The driver applies Bosch's calibration formulas |
| `struct sensor_value` | app_sensor.c | `val1` = integer part, `val2` = millionths | Fixed point, no float needed |
| `sensor_value_to_milli(&val)` | app_sensor.c | Value in thousandths, as `int64_t` | Prints "28.59" without float support in printf |

## 5. Zephyr logging

| API | Where | What it does | Why |
|---|---|---|---|
| `LOG_MODULE_REGISTER(name, LOG_LEVEL_INF)` | app_net.c, app_tcp.c, enc28j60_revid.c | Declares a log source with its own compiled-in level | Lines are tagged (`<inf> app_tcp:`); levels can be changed per module |
| `LOG_MODULE_REGISTER(app_sensor, LOG_LEVEL_DBG)` + `LOG_DBG` | app_sensor.c | Readings are compiled in at debug level | The shell backend starts every module at info (`CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL_INF`), so readings are hidden until `log enable dbg app_sensor` |
| `LOG_INF` / `LOG_WRN` / `LOG_ERR` | all modules | Info, warning, error | `CONFIG_LOG_MODE_DEFERRED=y`: the call only queues the message and the log thread prints it on USART3, so the caller is not slowed down |

## 6. Zephyr networking (app_net.c, app_tcp.c)

| API | What it does | Why |
|---|---|---|
| `net_if_get_default()` | The network interface (here the ENC28J60's) | The code does not depend on which Ethernet chip is used |
| `net_if_get_device(iface)->name` | Driver device behind it, `ethernet@0` | Log message only |
| `net_addr_pton(NET_AF_INET, str, &addr)` | Text "192.168.50.2" to binary | The address comes from Kconfig as a string |
| `net_addr_ntop(NET_AF_INET, &addr, buf, len)` | Binary to text | Logging the DHCP lease |
| `net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0)` | Adds the static address; lifetime 0 = never expires | Reachable on a direct PC cable without a DHCP server |
| `net_if_ipv4_set_netmask_by_addr(iface, &addr, &mask)` | Netmask (/24) for that address | Current per-address API; the per-interface variant is deprecated |
| `net_if_ipv4_addr_rm(iface, &addr)` | Removes an address | Undoes the static address if its netmask can't be set |
| `IS_ENABLED(CONFIG_APP_NET_STATIC)` / `(CONFIG_APP_NET_DHCP)` | 1 or 0 from Kconfig, usable in a normal `if` | Selects the IPv4 mode; both branches are still compiled and type-checked, the unused one is optimised away |
| `net_mgmt_init_event_callback(&cb, handler, NET_EVENT_IPV4_ADDR_ADD)` + `net_mgmt_add_event_callback(&cb)` | Calls `handler` when an IPv4 address is added | DHCP finishes at an unknown time; an event avoids waiting. `cb` is `static` because net_mgmt keeps it in a linked list |
| `net_mgmt_init_event_callback(&cb, handler, NET_EVENT_IF_UP)` | app_tcp.c: calls `handler` when the interface becomes usable (enabled and link up) | Starts the echo server only once there is a link |
| `net_if_is_up(iface)` | True when the interface is enabled **and** running (carrier on) | Checked right after registering the callback: the link can come up before `main()`, and that event would otherwise be missed |
| `net_dhcpv4_start(iface)` | Starts DHCP negotiation in the background and returns | DHCP mode: router support without blocking `main()` |
| `NET_IF_MAX_IPV4_ADDR` | Address slots per interface (`CONFIG_NET_IF_UNICAST_IPV4_ADDR_COUNT` = 1) | One address: static or DHCP, never both |
| `NET_IPV4_ADDR_LEN` | Buffer size for "xxx.xxx.xxx.xxx" | Correct size for `net_addr_ntop()` |
| `NET_ADDR_DHCP`, `NET_ADDR_MANUAL` | Address type | The handler logs only the DHCP address; the static one is added as manual |

Note: the event handler reads `iface->config.ip.ipv4->unicast[i]` and `iface->config.dhcpv4.lease_time` directly. These are Zephyr's internal structures, not a stable API, and may change in a later Zephyr version. Check them when upgrading from 4.4.0.

## 7. BSD sockets (app_tcp.c)

Zephyr's socket API with the `zsock_` prefix, so it doesn't clash with a C library's POSIX names. Every call returns -1 and sets `errno` on failure; `errno` is per thread.

| API | What it does | Why |
|---|---|---|
| `struct net_sockaddr` + `net_sin(&addr)` | Generic address storage, viewed as `struct net_sockaddr_in` through Zephyr's inline helper | No pointer cast in our code (MISRA C Rule 11.3); the cast is inside `net_sin()` |
| `net_htons(4242)` | Port in network byte order | TCP/IP headers are big-endian, the Cortex-M7 is little-endian |
| `zsock_socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP)` | New IPv4 TCP socket | |
| `zsock_bind(sock, &addr, sizeof(addr))` | Port 4242 on every local address (`sin_addr` left 0) | Works with the static address and with a DHCP lease |
| `zsock_listen(sock, 1)` | Backlog of 1 waiting connection | One client is served at a time |
| `zsock_accept(sock, NULL, NULL)` | Blocks until a client connects, returns its socket | The peer address isn't needed |
| `zsock_recv(client, buf, 256, 0)` | Blocks for data; returns 0 when the client closed | 0 = normal end of session, negative = error |
| `zsock_send(client, &buf[off], len - off, 0)` | Queues data; may take fewer bytes than asked | Called in a loop until everything is queued. Success means queued, not delivered |
| `zsock_close(fd)` | Releases the socket and its connection slot | Called by the accept loop after every client, and on setup errors |

## 8. Zephyr HTTP server (app_http.c)

The server and its thread are part of Zephyr (`subsys/net/lib/http`). The application only describes the site in tables that the linker collects.

| API | What it does | Why |
|---|---|---|
| `HTTP_SERVICE_DEFINE(gw_http_service, NULL, &http_port, CONFIG_HTTP_SERVER_MAX_CLIENTS, 2, NULL, NULL, NULL)` | A server on port 80, all addresses, up to 3 clients, backlog 2, no fallback page (404), default socket setup | `http_port` is passed by pointer because port 0 would request an ephemeral port that the server writes back |
| `HTTP_RESOURCE_DEFINE(index_resource, gw_http_service, "/", &index_detail)` | Binds the path `/` to a resource description; the entry goes into linker section `http_resource_desc_gw_http_service` | The section is added by `src/net/http/http_sections.ld` (`ITERABLE_SECTION_ROM`) through `zephyr_linker_sources(SECTIONS ...)` in CMakeLists.txt |
| `struct http_resource_detail_static` | Type `HTTP_RESOURCE_TYPE_STATIC`, methods `BIT(HTTP_GET)`, `content_type` `text/html`, `content_encoding` `gzip`, pointer and length of the page | GET only (others get 405); not `const`, because the macro casts it to `void *` (MISRA 11.8) |
| `http_server_start()` | Wakes the server thread, which opens the listener and serves | Returns at once; `-EALREADY` if already running |
| `generate_inc_file_for_target(app src/web/index.html ... --gzip)` (CMake) | Gzips the HTML at build time into a byte list under `build/`, `#include`d into a `uint8_t` array | The page stays a normal HTML file; editing it and rebuilding is enough |

## 9. STM32Cube HAL (LD2, led/hal/led_hal.c)

| API | What it does | Why |
|---|---|---|
| `__HAL_RCC_GPIOE_CLK_ENABLE()` | Port E clock on, with a read-back | A peripheral without its clock ignores writes; enabling twice is harmless |
| `HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET)` | Drives the pin low via BSRR | Off before switching to output, so the LED never flashes on |
| `GPIO_InitTypeDef` + `HAL_GPIO_Init()` | Mode (push-pull output), pull (none), speed (low) in one call | ST's standard style, shown next to Zephyr's |
| `HAL_GPIO_TogglePin()` | Reads ODR, writes BSRR | Toggles PE1 without touching other port E pins |

Not used on purpose: `HAL_Init()`, `SystemClock_Config()`, `HAL_RCC_*Config()`. Zephyr owns clocks and SysTick; `HAL_GetTick()`/`HAL_Delay()` are mapped to the Zephyr kernel (CLAUDE.md, Architecture).

## 10. CMSIS registers (LD3, led/cmsis/led_cmsis.c)

Register and bit names come from ST's CMSIS device header (`stm32h723xx.h`), included through Zephyr's `<soc.h>`.

| Register / macro | What it does | Why |
|---|---|---|
| `RCC->AHB4ENR \|= RCC_AHB4ENR_GPIOBEN`, then read back | Port B clock on; the read makes sure the write has completed | Same as the HAL macro, by hand |
| `GPIOB->BSRR = GPIO_BSRR_BR14` / `GPIO_BSRR_BS14` | Clears / sets only PB14 | PB0 (LD1, owned by the Zephyr driver) is on the same port: single-pin writes only (CLAUDE.md) |
| `GPIOB->OTYPER`, `GPIOB->OSPEEDR`, `GPIOB->PUPDR` | Push-pull, low speed, no pull | Explicit setup instead of relying on reset values |
| `GPIOB->MODER` (`GPIO_MODER_MODE14_0`) | Pin mode 01 = output | Written last, when the pin is already low |
| `GPIOB->ODR & GPIO_ODR_OD14` | Current output state | Chooses set or reset when toggling |

## 11. Zephyr SPI (enc28j60_revid.c)

| API | What it does | Why |
|---|---|---|
| `SPI_DT_SPEC_GET(DT_NODELABEL(enc28j60), SPI_WORD_SET(8))` | Bus, chip select (PD14) and `spi-max-frequency` from the devicetree node; 8-bit words, MSB first, mode 0 (defaults) | Same bus settings as the ENC28J60 driver, nothing hard-coded |
| `spi_is_ready_dt(&spec)` | Checks the SPI controller initialised | Logged and skipped if not; boot continues |
| `struct spi_buf` / `struct spi_buf_set` | Describe the bytes to send and the buffer for the bytes received | One 2-byte buffer each way per command |
| `spi_transceive_dt(&spec, &tx, &rx)` | One full-duplex transfer with CS held low for its whole length | The ENC28J60 ends a command when CS rises, so each command is one call |

## 12. Standard C and build configuration

| Item | Why |
|---|---|
| `<stdint.h>`: `uint32_t`, `int64_t` | Fixed-width types (MISRA) |
| `<stdbool.h>`: `true` | `while (true)` is an essentially Boolean condition (MISRA rule 14.4) |
| `<stddef.h>`: `size_t`; `ssize_t` from Zephyr's socket headers | Byte counts in the echo loop; the signed `ssize_t` is checked before it is cast to `size_t` |
| `<errno.h>`: `ENODEV`, `EIO`, `EINVAL`, `ENOMEM`, and `errno` | Negative error codes, same convention as Zephyr's APIs |
| `<inttypes.h>`: `PRIu32` | Matches `uint32_t` whatever its underlying type. Zephyr forces it to `unsigned int` (`-imacros zephyr_stdint.h`), while bare Arm GCC uses `unsigned long` |
| `CONFIG_APP_NET_STATIC_IPV4_ADDR`, `CONFIG_APP_NET_STATIC_IPV4_NETMASK` | From the app `Kconfig`: change the address without editing C |
