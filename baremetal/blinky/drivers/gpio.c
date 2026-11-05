/**
 * @file gpio.c
 * @brief GPIO port registers and push-pull output pins (RM0468 11.4).
 */
#include "drivers/gpio.h"
#include <stddef.h>

_Static_assert(offsetof(struct gpio_regs, ODR) == 0x14U, "GPIO ODR offset");
_Static_assert(offsetof(struct gpio_regs, BSRR) == 0x18U, "GPIO BSRR offset");

#define GPIO_FIELD2_MASK 3UL /**< Width mask of a 2-bit per-pin field */
#define GPIO_MODE_OUT    1UL /**< MODERy = 01: general purpose output */

void gpio_output_init(struct gpio_regs *port, uint32_t pin)
{
	const uint32_t shift2 = pin * 2U;

	/* Drive low before leaving analog mode so the pin does not glitch high */
	port->BSRR = (1UL << pin) << 16U;
	port->OTYPER &= ~(1UL << pin);                  /* 0: push-pull */
	port->OSPEEDR &= ~(GPIO_FIELD2_MASK << shift2); /* 00: low speed */
	port->PUPDR &= ~(GPIO_FIELD2_MASK << shift2);   /* 00: no pull */
	port->MODER = (port->MODER & ~(GPIO_FIELD2_MASK << shift2)) | (GPIO_MODE_OUT << shift2);
}

void gpio_toggle(struct gpio_regs *port, uint32_t pin)
{
	const uint32_t bit = 1UL << pin;

	if ((port->ODR & bit) != 0U) {
		port->BSRR = bit << 16U;
	} else {
		port->BSRR = bit;
	}
}
