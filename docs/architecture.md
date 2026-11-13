# Architecture

How the Eco-Connect gateway firmware fits together, in three views. GitHub renders the diagrams; in VS Code use the Markdown preview with the *Markdown Preview Mermaid Support* extension. Pins and clock values are in [pins-and-clocks.md](pins-and-clocks.md).

## What talks to what

Left to right: our code (`src/`) uses Zephyr's kernel and drivers, which drive the STM32 peripherals, which connect to the parts on the breadboard, the PC and the router.

```mermaid
flowchart LR
    subgraph SRC["Our code: src/"]
        MAIN["main.c<br/>LED chase 250 ms"]
        HAL["led/hal<br/>LD2"]
        CMSIS["led/cmsis<br/>LD3"]
        SENSOR["sensor/app_sensor.c<br/>thread, every 2 s"]
        APPNET["net/app_net.c<br/>static IP or DHCP<br/>(Kconfig choice)"]
    end
    subgraph ZEPHYR["Zephyr"]
        KERNEL["Kernel<br/>threads · timers"]
        BMEDRV["BME280 driver"]
        NET["IPv4 stack<br/>ENC28J60 driver"]
        LOG["Log + shell"]
    end
    subgraph MCU["STM32H723 peripherals"]
        GPIO["GPIOB / GPIOE"]
        I2C1["I2C1<br/>400 kHz"]
        SPI1["SPI1 17.2 MHz<br/>INT PD15"]
        USART3["USART3<br/>115200"]
    end
    subgraph OFF["Off-board"]
        LEDS["LD1-LD3<br/>PB0 · PE1 · PB14"]
        BME["BME280<br/>I2C 0x76"]
        ENC["ENC28J60<br/>10BASE-T"]
        PC["PC<br/>ping · COM5"]
        ROUTER["Router<br/>DHCP server"]
    end
    KERNEL --> MAIN
    KERNEL --> SENSOR
    MAIN -->|"LD1 via Zephyr GPIO"| GPIO
    MAIN --> HAL --> GPIO
    MAIN --> CMSIS --> GPIO
    GPIO --> LEDS
    SENSOR --> BMEDRV
    BMEDRV --> I2C1
    I2C1 <--> BME
    SENSOR -->|LOG_INF| LOG
    APPNET -->|"lease, DHCP mode"| LOG
    LOG --> USART3
    USART3 --> PC
    APPNET --> NET
    NET --> SPI1
    SPI1 <--> ENC
    ENC -.->|INT| SPI1
    ENC <-->|"direct cable, static mode"| PC
    ENC <-->|"Ethernet, DHCP mode"| ROUTER
```

## What happens when

Boot runs once; then two of our jobs share the CPU. `app_net_start()` sets up one IPv4 address, static or DHCP depending on `prj.conf`, and returns without waiting for a lease. `main()` sleeps between chase steps and the sensor thread sleeps between readings, so the kernel runs whichever one is due.

```mermaid
sequenceDiagram
    participant R as Reset
    participant K as Zephyr kernel
    participant D as Drivers
    participant M as main()
    participant S as sensor thread
    participant N as net stack
    R->>K: reset vector
    K->>K: clocks, 8 MHz HSE to PLL1 550 MHz
    K->>D: init from devicetree
    D-->>K: ENC28J60 reset (SPI), BME280 ID 0x60 (I2C)
    K->>M: start main()
    M->>M: init LD1 (Zephyr), LD2 (HAL), LD3 (CMSIS)
    alt CONFIG_APP_NET_STATIC (default)
        M->>N: add 192.168.50.2/24 (manual, no expiry)
    else CONFIG_APP_NET_DHCP
        M->>N: register IPV4_ADDR_ADD callback, start DHCPv4
    end
    M->>S: app_sensor_start(), create thread
    par every 250 ms
        M->>M: chase LD1 to LD2 to LD3
    and every 2 s
        S->>S: read BME280, log T/P/H
    and DHCP mode, once the router answers
        N->>N: lease bound, callback logs IP, mask, gateway, lease
    end
```

Ping replies are not in this picture because no application code runs for them: the ENC28J60 interrupt wakes the driver's receive thread, and Zephyr's IPv4 stack answers.

## How the files become firmware

The devicetree describes the hardware, Kconfig picks the software features, and `west build` turns both plus our sources into one image.

```mermaid
flowchart LR
    PRJ["prj.conf<br/>features · APP_NET_STATIC / _DHCP"] --> KC["Kconfig system<br/>.config"]
    KCFG["Kconfig<br/>HAL GPIO · net mode · static IP"] --> KC
    BDTS["Board DTS<br/>upstream Zephyr"] --> DT["Devicetree<br/>zephyr.dts"]
    OVL["boards/nucleo_h723zg.overlay"] --> DT
    SRCS["src/**/*.c"] --> WEST["west build<br/>CMake · Ninja · GCC 14.3"]
    KC --> WEST
    DT --> WEST
    WEST --> ELF["build/zephyr/zephyr.elf<br/>~168 KB flash"]
    ELF -->|"west flash · ST-Link"| BOARD["STM32 flash<br/>0x0800 0000"]
```

The bare-metal blinky in `baremetal/blinky` is separate: no Zephyr, no devicetree, its own startup code and linker script, built by `baremetal/blinky/build.sh`.
