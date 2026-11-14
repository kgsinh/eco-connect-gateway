# Architecture

How the Eco-Connect gateway firmware fits together, in four views. GitHub renders the diagrams; in VS Code use the Markdown preview with the *Markdown Preview Mermaid Support* extension. Pins and clocks are in [pins-and-clocks.md](pins-and-clocks.md), the network setup and its test results in [networking.md](networking.md), every API used in `src/` in [api-reference.md](api-reference.md).

## What talks to what

Left to right: our code (`src/`) uses Zephyr's kernel, network stack and drivers, which drive the STM32 peripherals, which connect to the parts on the breadboard and to the PC. The HTTP server and the network threads belong to Zephyr; our network code only configures the address, runs the echo server and describes the web page.

```mermaid
flowchart LR
    subgraph SRC["Our code: src/"]
        MAIN["main.c<br/>LED chase 250 ms<br/>starts the rest"]
        HAL["led/hal<br/>LD2"]
        CMSIS["led/cmsis<br/>LD3"]
        SENSOR["sensor/app_sensor.c<br/>thread, every 2 s"]
        APPNET["net/app_net.c<br/>static IP or DHCP"]
        REVID["net/enc28j60_revid.c<br/>EREVID at boot"]
        TCP["net/tcp/app_tcp.c<br/>echo server, port 4242"]
        HTTP["net/http/app_http.c<br/>site: GET / on port 80"]
        WEB["web/index.html<br/>gzipped at build time"]
    end
    subgraph ZEPHYR["Zephyr"]
        KERNEL["Kernel<br/>threads, semaphores"]
        BMEDRV["BME280 driver"]
        HTTPD["HTTP server thread"]
        NET["IPv4 · TCP · sockets<br/>net_mgmt events"]
        ENCDRV["ENC28J60 driver<br/>RX thread"]
        LOG["Log + shell"]
    end
    subgraph MCU["STM32H723 peripherals"]
        GPIO["GPIOB / GPIOE / GPIOD"]
        I2C1["I2C1<br/>400 kHz"]
        SPI1["SPI1<br/>17.2 MHz"]
        USART3["USART3<br/>115200"]
    end
    subgraph OFF["Off-board"]
        LEDS["LD1-LD3<br/>PB0 · PE1 · PB14"]
        BME["BME280<br/>I2C 0x76"]
        ENC["ENC28J60<br/>10BASE-T, B7"]
        PC["PC<br/>browser · curl · ping<br/>echo_test.py · shell"]
    end
    KERNEL --> MAIN
    KERNEL --> SENSOR
    MAIN -->|"LD1 via Zephyr GPIO"| GPIO
    MAIN --> HAL --> GPIO
    MAIN --> CMSIS --> GPIO
    GPIO --> LEDS
    SENSOR --> BMEDRV --> I2C1
    I2C1 <--> BME
    SENSOR -->|"LOG_DBG, hidden by default"| LOG
    APPNET --> NET
    TCP -->|"zsock_* sockets"| NET
    HTTP -->|"service + resource tables"| HTTPD
    WEB --> HTTP
    HTTPD --> NET
    NET --> ENCDRV
    REVID -->|"SPI, before the driver starts"| SPI1
    ENCDRV --> SPI1
    SPI1 <--> ENC
    ENC -.->|"INT, PD15"| GPIO
    ENC <-->|"direct cable, 192.168.50.0/24"| PC
    LOG --> USART3 --> PC
```

DHCP mode replaces the direct cable with a router: the address then comes from its DHCP server and is logged on the console.

## What happens at boot

Zephyr initialises the drivers in priority order before `main()` runs. Our EREVID read is slotted between the SPI controller and the ENC28J60 driver, so it is the only SPI user while it runs. `main()` starts everything else without waiting: the address is set, the two servers start their threads, and the link comes up about 50-60 ms after reset.

```mermaid
sequenceDiagram
    participant K as Kernel init
    participant RV as enc28j60_revid<br/>(SYS_INIT 55)
    participant D as ENC28J60 driver<br/>(init 60)
    participant M as main() [prio 0]
    participant T as echo thread [prio 8]
    participant H as HTTP server [coop -1]
    participant E as net_mgmt [coop -1]
    K->>K: clocks: HSE 8 MHz to PLL1, CPU 550 MHz
    K->>K: SPI1 init (priority 50)
    K->>RV: POST_KERNEL, priority 55
    RV->>RV: SPI: bank 3, read EREVID = 0x06 (B7), bank 0
    K->>D: priority 60
    D->>D: SPI: soft reset, buffers, MAC, PHY, interrupts, RXEN (0.7 ms)
    K->>M: start main()
    M->>M: LD1 (Zephyr), LD2 (HAL), LD3 (CMSIS)
    M->>M: app_net_start(): 192.168.50.2/24 (or start DHCPv4)
    M->>E: app_tcp_start(): register NET_EVENT_IF_UP
    M->>T: create thread, waits on net_up_sem
    M->>H: app_http_start(): wake server thread
    H->>H: listen on 0.0.0.0:80 (works before link-up)
    M->>M: app_sensor_start(), then LED chase every 250 ms
    Note over D: ~50-60 ms after reset: PHY link interrupt
    D-)E: carrier on, NET_EVENT_IF_UP
    E-)T: k_sem_give(net_up_sem)
    T->>T: listen on 0.0.0.0:4242, accept loop
```

In DHCP mode, `app_net_start()` registers an `IPV4_ADDR_ADD` callback and starts the client; the lease is logged when the router answers. The sensor thread logs a reading every 2 s at debug level (`log enable dbg app_sensor` shows them).

## Serving a page

Each lifeline is an execution context with its priority (cooperative priorities are negative and are never preempted by other threads). The interrupt handler only gives a semaphore; every SPI transfer happens in a thread. There is no network TX thread (`NET_TC_TX_COUNT=0`), so a frame is written to the ENC28J60 by the thread that sends it: the network RX thread for handshake ACKs, the HTTP server thread for its response.

```mermaid
sequenceDiagram
    participant PC as PC browser
    participant C as ENC28J60
    participant I as GPIO ISR<br/>(PD15)
    participant R as ENC RX thread<br/>[coop -14]
    participant N as net RX thread<br/>[coop -1]
    participant H as HTTP server<br/>[coop -1]
    PC->>C: SYN, then GET / (frame complete in chip buffer)
    C-)I: INT falls
    I-)R: k_sem_give(int_sem)
    R->>C: SPI: mask INTIE, read EPKTCNT, read frame, free it
    R-)N: net_recv_data(): queue packet
    N->>N: Ethernet, IPv4, TCP: connection found
    N->>C: SPI: ACK (or SYN-ACK) frame, sent from this thread
    N-)H: socket readable, poll() returns
    H->>H: recv, parse "GET /", match index_resource
    H->>C: SPI: headers (89 B), then gzipped page (773 B)
    C->>PC: HTTP/1.1 200 OK, Content-Encoding: gzip
    Note over H: client idle 10 s: server closes it (FIN)
```

Measured on the bench (captures in [evidence/network/README.md](evidence/network/README.md)): SYN answered in about 1 ms, request ACKed in about 2 ms, page delivered 50-250 ms after the request. The body waits for the PC's ACK of the header segment (likely Nagle against delayed ACK; open item). Up to 3 HTTP clients and 1 echo client are served at the same time. Ping replies follow the same receive path but are answered inside the net RX thread by the IPv4 stack.

## How the files become firmware

The devicetree describes the hardware, Kconfig picks the software features, CMake adds the generated page and the HTTP resource section, and `west build` turns everything into one image.

```mermaid
flowchart LR
    PRJ["prj.conf<br/>net, TCP, sockets, HTTP server<br/>connection limits, logging"] --> KC["Kconfig<br/>build/zephyr/.config"]
    KCFG["Kconfig<br/>HAL GPIO · net mode · static IP"] --> KC
    BDTS["Board DTS<br/>upstream Zephyr"] --> DT["Devicetree<br/>zephyr.dts"]
    OVL["boards/nucleo_h723zg.overlay<br/>ENC28J60 on SPI1 · BME280 on I2C1"] --> DT
    HTML["src/web/index.html"] -->|"file2hex.py --gzip"| INC["index.html.gz.inc<br/>build/.../generated"]
    LD["src/net/http/http_sections.ld"] -->|"zephyr_linker_sources"| LINK["Linker script"]
    SRCS["src/**/*.c"] --> WEST["west build<br/>CMake · Ninja · GCC 14.3"]
    INC --> WEST
    KC --> WEST
    DT --> WEST
    LINK --> WEST
    WEST --> ELF["build/zephyr/zephyr.elf<br/>~214 KB flash · ~57 KB RAM"]
    ELF -->|"west flash · ST-Link"| BOARD["STM32 flash<br/>0x0800 0000"]
```

The bare-metal blinky in `baremetal/blinky` is separate: no Zephyr, no devicetree, its own startup code and linker script, built by `baremetal/blinky/build.sh`.
