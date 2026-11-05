/**
 * @file led.c
 * @brief NUCLEO-H723ZG user LEDs LD1..LD3.
 *
 * Pin mapping from the board DTS (boards/st/nucleo_h723zg). PB0 and PB14 share
 * GPIOB; gpio_toggle() writes BSRR, so neither disturbs the other.
 */
#include "board/led.h"
#include "drivers/gpio.h"
#include "drivers/rcc.h"
#include <stdint.h>

/** @brief GPIO pin an LED is wired to. */
struct led_pin {
	struct gpio_regs *port; /**< GPIO port */
	uint32_t pin;           /**< Pin number, 0..15 */
};

static const struct led_pin leds[LED_COUNT] = {
	[LED_GREEN] = {GPIOB, 0U},
	[LED_YELLOW] = {GPIOE, 1U},
	[LED_RED] = {GPIOB, 14U},
};

void led_init(void)
{
	rcc_ahb4_enable(RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOEEN);

	for (uint32_t i = 0U; i < (uint32_t)LED_COUNT; i++) {
		gpio_output_init(leds[i].port, leds[i].pin);
	}
}

void led_toggle(enum led_id id)
{
	if (id < LED_COUNT) {
		gpio_toggle(leds[id].port, leds[id].pin);
	}
}
