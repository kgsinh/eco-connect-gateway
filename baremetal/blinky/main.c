/**
 * @file main.c
 * @brief Bare-metal register blinky for NUCLEO-H723ZG.
 *
 * No HAL, no RTOS, no vendor headers. Each LED toggles at its own rate:
 * LD1 green every 1000 ms, LD2 yellow every 500 ms, LD3 red every 250 ms.
 * Hardware access lives in drivers/ (registers) and board/ (pin mapping).
 */
#include <stdint.h>
#include "blinky.h"
#include "board/led.h"
#include "drivers/systick.h"

#define TICK_MS          250U
#define GREEN_TOGGLE_MS  1000U
#define YELLOW_TOGGLE_MS 500U
#define RED_TOGGLE_MS    250U

_Static_assert(((GREEN_TOGGLE_MS % TICK_MS) == 0U) && ((YELLOW_TOGGLE_MS % TICK_MS) == 0U) &&
		       ((RED_TOGGLE_MS % TICK_MS) == 0U),
	       "LED toggle intervals must be multiples of TICK_MS");

/** @brief One entry of the blink pattern. */
struct blink {
	enum led_id led;       /**< LED to toggle */
	uint32_t toggle_ticks; /**< Toggle every this many TICK_MS steps */
};

/**
 * @brief Set up the LEDs and the tick, then run the blink pattern forever.
 * @return Does not return.
 */
int main(void)
{
	static const struct blink pattern[] = {
		{LED_GREEN, GREEN_TOGGLE_MS / TICK_MS},
		{LED_YELLOW, YELLOW_TOGGLE_MS / TICK_MS},
		{LED_RED, RED_TOGGLE_MS / TICK_MS},
	};
	uint32_t step = 0U;

	led_init();
	systick_init();

	for (;;) {
		for (uint32_t i = 0U; i < (sizeof(pattern) / sizeof(pattern[0])); i++) {
			if ((step % pattern[i].toggle_ticks) == 0U) {
				led_toggle(pattern[i].led);
			}
		}

		step++;
		systick_delay_ms(TICK_MS);
	}
}
