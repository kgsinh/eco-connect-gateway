/**
 * @file led_hal.h
 * @brief LD2 (yellow, PE1) driven through the STM32Cube HAL GPIO API.
 */
#ifndef LED_HAL_H
#define LED_HAL_H

/**
 * @brief Enable the GPIOE clock and configure PE1 as a push-pull output, LED off.
 */
void led_hal_init(void);

/**
 * @brief Invert LD2.
 * @pre led_hal_init() has been called.
 */
void led_hal_toggle(void);

#endif /* LED_HAL_H */
