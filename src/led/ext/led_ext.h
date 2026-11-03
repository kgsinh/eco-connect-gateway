/**
 * @file led_ext.h
 * @brief External LED on D7 (PG12) blinked by a kernel timer.
 */
#ifndef LED_EXT_H
#define LED_EXT_H

/**
 * @brief Configure the external LED and start blinking it.
 *
 * The LED toggles every 500 ms from a k_timer, independent of the main loop.
 *
 * @retval 0 Blinking started.
 * @retval -ENODEV GPIO port not ready.
 * @retval -EIO Pin configuration failed.
 */
int led_ext_start(void);

#endif /* LED_EXT_H */
