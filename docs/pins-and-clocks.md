# Pins and clocks: NUCLEO-H723ZG

Week-one deliverable 6. Every value comes from the upstream Zephyr board devicetree (`boards/st/nucleo_h723zg`), the overlay in this repo (`boards/nucleo_h723zg.overlay`) or RM0468 Rev 3. The section numbers point at the source.

## System clock

### Zephyr application

Configured by Zephyr from the board devicetree. The application never touches the clock tree.

| Clock | Source / setting | Frequency |
|---|---|---|
| HSE | 8 MHz from the ST-Link MCO, bypass mode | 8 MHz |
| PLL1 VCO | HSE / M=4 × N=275 | 550 MHz |
| SYSCLK (CPU) | PLL1_P (P=1) | 550 MHz |
| HCLK (AHB) | SYSCLK / 2 | 275 MHz |
| APB1, APB2, APB3, APB4 | HCLK / 2 | 137.5 MHz |
| PLL1_Q | VCO / 4 | 137.5 MHz |

Supply: LDO (`&pwr power-supply = "ldo"`).

### Bare-metal blinky (`baremetal/blinky`)

| Clock | Source / setting | Frequency |
|---|---|---|
| SYSCLK (CPU) | HSI, reset default (HSIDIV = /1, RM0468 8.7.2) | 64 MHz |
| SysTick | Processor clock, reload 63 999 | 1 ms period |

The PLL, voltage scaling and supply configuration stay at their reset values. RM0468 6.8.4 only requires the supply configuration before VOS or the SYSCLK frequency changes.

## LEDs

| LED | Pin | Zephyr app driver | Bare-metal blinky |
|---|---|---|---|
| LD1 green | PB0 | Zephyr GPIO API (`led0` alias) | registers, 1000 ms |
| LD2 yellow | PE1 | STM32Cube HAL GPIO | registers, 500 ms |
| LD3 red | PB14 | CMSIS registers | registers, 250 ms |

All three are active high. The Zephyr app runs a chase, one LED on at a time, 250 ms per step. The board DTS routes PB14 to TIM12 PWM, so the overlay disables `timers12`, `pwm12` and `pwmleds` to keep the pin free for the CMSIS code.

## I2C: BME280

| Signal | Pin | Arduino header |
|---|---|---|
| SCL | PB8 | D15 |
| SDA | PB9 | D14 |

| Setting | Value |
|---|---|
| Peripheral | I2C1 |
| Bus speed | 400 kHz (`I2C_BITRATE_FAST`) |
| Kernel clock | rcc_pclk1 = APB1, 137.5 MHz (I2C1235SEL reset default, RM0468 8.7.20) |
| BME280 address | 0x76 (SDO tied to GND on the breakout) |
| Supply | 3V3 |

## SPI: ENC28J60

| Signal | Pin | Arduino header |
|---|---|---|
| SCK | PA5 | D13 |
| MISO (ENC28J60 SO) | PA6 | D12 |
| MOSI (ENC28J60 SI) | PB5 | D11 |
| CS | PD14 | D10 |
| INT (active low) | PD15 | D9 |

| Setting | Value |
|---|---|
| Peripheral | SPI1, mode 0 (the ENC28J60 supports only mode 0,0, DS39662C 4.1) |
| Kernel clock | PLL1_Q, 137.5 MHz (SPI123SEL reset default, RM0468 8.7.19) |
| SCK | 137.5 MHz / 16 = 8.6 MHz (`spi-max-frequency` = 10 MHz; the prescaler is a power of 2) |
| Why 8.6 MHz | ENC28J60 maximum is 20 MHz; its silicon errata asks for at least 8 MHz; a lower clock tolerates jumper wires |
| ENC28J60 clock | 25 MHz crystal on the module |
| Supply | 3V3 (3.1-3.6 V, no regulator on the 10-pin module) |

MOSI is on PB5 instead of the usual PA7 because PA7 is the RMII CRS_DV pin on this board.

## Console

USART3, TX PD8 / RX PD9, to the ST-Link virtual COM port. 115200 8N1. Kernel clock rcc_pclk1, 137.5 MHz (USART234578SEL reset default, RM0468 8.7.20).
