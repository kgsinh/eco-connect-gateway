/**
 * @file led_cmsis.h
 * @brief LD3 (red, PB14) driven through CMSIS register access.
 */
#ifndef LED_CMSIS_H
#define LED_CMSIS_H

/**
 * @brief Enable the GPIOB clock and configure PB14 as a push-pull output, LED off.
 */
void led_cmsis_init(void);

/**
 * @brief Invert LD3.
 * @pre led_cmsis_init() has been called.
 * @note Written through BSRR, so PB0 (LD1, owned by the Zephyr GPIO driver)
 *       on the same port is not affected.
 */
void led_cmsis_toggle(void);

#endif /* LED_CMSIS_H */
