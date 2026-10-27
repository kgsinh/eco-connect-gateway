/*
 * NUCLEO-H723ZG: one LED per API layer, showing Zephyr, STM32 HAL and CMSIS
 * working together in one app.
 *   LD1 green  PB0   Zephyr GPIO API (devicetree led0)   1000 ms
 *   LD2 yellow PE1   STM32 HAL (led_hal.c)                500 ms
 *   LD3 red    PB14  CMSIS registers (led_cmsis.c)        250 ms
 */
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "led_hal.h"
#include "led_cmsis.h"
#include "app_net.h"

#define TICK_MS  250

int main(void)
{
	static const struct gpio_dt_spec ld1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
	uint32_t tick = 0U;

	if (!gpio_is_ready_dt(&ld1)) {
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&ld1, GPIO_OUTPUT_INACTIVE) < 0) {
		return -EIO;
	}

	led_hal_init();
	led_cmsis_init();

	/* Non-blocking: the LEDs keep running while DHCP waits for a lease */
	(void)app_net_start();

	while (true) {
		led_cmsis_toggle();

		if ((tick % 2U) == 0U) {
			led_hal_toggle();
		}

		if ((tick % 4U) == 0U) {
			if (gpio_pin_toggle_dt(&ld1) < 0) {
				return -EIO;
			}
		}

		tick++;
		(void)k_msleep(TICK_MS);
	}
}
