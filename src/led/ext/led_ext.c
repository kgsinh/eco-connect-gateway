/**
 * @file led_ext.c
 * @brief External LED on D7 (PG12) blinked by a kernel timer.
 *
 * The pin comes from the devicetree alias led-ext in
 * boards/nucleo_h723zg.overlay. The timer callback runs in interrupt context,
 * so it only toggles the pin.
 */
#include "led_ext.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_EXT_PERIOD_MS 500

static const struct gpio_dt_spec led_ext = GPIO_DT_SPEC_GET(DT_ALIAS(led_ext), gpios);

/**
 * @brief Timer expiry: invert the LED.
 * @param timer Expired timer (unused).
 */
static void led_ext_expiry(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	(void)gpio_pin_toggle_dt(&led_ext);
}

K_TIMER_DEFINE(led_ext_timer, led_ext_expiry, NULL);

int led_ext_start(void)
{
	if (!gpio_is_ready_dt(&led_ext)) {
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&led_ext, GPIO_OUTPUT_INACTIVE) < 0) {
		return -EIO;
	}

	k_timer_start(&led_ext_timer, K_MSEC(LED_EXT_PERIOD_MS), K_MSEC(LED_EXT_PERIOD_MS));

	return 0;
}
