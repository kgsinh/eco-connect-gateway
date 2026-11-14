/**
 * @file main.c
 * @brief Eco-Connect gateway entry point: LED chase, network and sensor start.
 *
 * One LED per API layer, all driven from this thread. Exactly one LED is on
 * at a time, moving LD1 -> LD2 -> LD3 every TICK_MS. The pattern differs on
 * purpose from the bare-metal blinky, so the running image is obvious.
 * | LED        | Pin  | Layer                              |
 * |------------|------|------------------------------------|
 * | LD1 green  | PB0  | Zephyr GPIO API (devicetree led0)  |
 * | LD2 yellow | PE1  | STM32Cube HAL (led/hal)            |
 * | LD3 red    | PB14 | CMSIS registers (led/cmsis)        |
 */
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "led/hal/led_hal.h"
#include "led/cmsis/led_cmsis.h"
#include "net/app_net.h"
#include "net/tcp/app_tcp.h"
#include "net/http/app_http.h"
#include "sensor/app_sensor.h"

#define TICK_MS   250
#define LED_COUNT 3U

static const struct gpio_dt_spec ld1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/**
 * @brief Invert one LED, each through its own API layer.
 * @param idx 0 = LD1 (Zephyr GPIO), 1 = LD2 (HAL), 2 = LD3 (CMSIS).
 * @retval 0 Success.
 * @retval -EIO LD1 toggle failed.
 */
static int led_toggle(uint32_t idx)
{
	int ret = 0;

	switch (idx) {
	case 0U:
		if (gpio_pin_toggle_dt(&ld1) < 0) {
			ret = -EIO;
		}
		break;
	case 1U:
		led_hal_toggle();
		break;
	default:
		led_cmsis_toggle();
		break;
	}

	return ret;
}

/**
 * @brief Initialise the LEDs, start networking and run the LED chase.
 * @retval -ENODEV LD1 GPIO controller not ready.
 * @retval -EIO LD1 configure or toggle failed.
 * @note Does not return in normal operation.
 */
int main(void)
{
	uint32_t cur = 0U;

	if (!gpio_is_ready_dt(&ld1)) {
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&ld1, GPIO_OUTPUT_INACTIVE) < 0) {
		return -EIO;
	}

	led_hal_init();
	led_cmsis_init();

	/* Returns before the DHCP lease, so the LEDs keep running meanwhile */
	(void)app_net_start();

	/* The server thread waits for the interface; main() continues at once */
	(void)app_tcp_start();

	/* Listens on all addresses, so it can start before the link is up */
	(void)app_http_start();

	/* A missing sensor is logged; the LEDs and network keep running */
	(void)app_sensor_start();

	/* All LEDs start off, so toggling keeps exactly one of them on */
	if (led_toggle(cur) < 0) {
		return -EIO;
	}

	while (true) {
		uint32_t next = (cur + 1U) % LED_COUNT;

		(void)k_msleep(TICK_MS);

		if (led_toggle(cur) < 0) {
			return -EIO;
		}

		if (led_toggle(next) < 0) {
			return -EIO;
		}

		cur = next;
	}
}
