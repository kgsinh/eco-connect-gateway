/**
 * @file gpio.h
 * @brief GPIO port registers and push-pull output pins (RM0468 11.4).
 *
 * The port clock must be enabled through rcc_ahb4_enable() before any access.
 */
#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

/** @brief GPIO port register block, up to BSRR. */
struct gpio_regs {
	volatile uint32_t MODER;   /**< 0x00 Mode, 2 bits per pin */
	volatile uint32_t OTYPER;  /**< 0x04 Output type, 1 bit per pin */
	volatile uint32_t OSPEEDR; /**< 0x08 Output speed, 2 bits per pin */
	volatile uint32_t PUPDR;   /**< 0x0C Pull-up/pull-down, 2 bits per pin */
	volatile uint32_t IDR;     /**< 0x10 Input data */
	volatile uint32_t ODR;     /**< 0x14 Output data */
	volatile uint32_t BSRR;    /**< 0x18 Bit set (0..15) / reset (16..31) */
};

/** @name Port register blocks (RM0468 Table 7)
 * @{ */
#define GPIOB ((struct gpio_regs *)0x58020400U)
#define GPIOE ((struct gpio_regs *)0x58021000U)
/** @} */

/**
 * @brief Configure a pin as a low-speed push-pull output without pull, driven low.
 * @param port GPIO port.
 * @param pin Pin number, 0..15.
 */
void gpio_output_init(struct gpio_regs *port, uint32_t pin);

/**
 * @brief Invert an output pin.
 * @param port GPIO port.
 * @param pin Pin number, 0..15.
 * @note Written through BSRR, so other pins on the same port are not affected.
 */
void gpio_toggle(struct gpio_regs *port, uint32_t pin);

#endif /* GPIO_H */
