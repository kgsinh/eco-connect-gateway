/**
 * @file led.h
 * @brief NUCLEO-H723ZG user LEDs LD1..LD3.
 */
#ifndef LED_H
#define LED_H

/** @brief User LED, named by colour. */
enum led_id {
	LED_GREEN,  /**< LD1, PB0 */
	LED_YELLOW, /**< LD2, PE1 */
	LED_RED,    /**< LD3, PB14 */
	LED_COUNT   /**< Number of LEDs */
};

/** @brief Enable the GPIO port clocks and configure all LEDs as outputs, off. */
void led_init(void);

/**
 * @brief Invert one LED.
 * @param id LED to toggle; out-of-range values are ignored.
 * @pre led_init() has been called.
 */
void led_toggle(enum led_id id);

#endif /* LED_H */
