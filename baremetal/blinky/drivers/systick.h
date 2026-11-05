/**
 * @file systick.h
 * @brief 1 ms SysTick time base and millisecond delay (PM0253 4.4).
 */
#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/** @brief Start SysTick: 1 ms period on the processor clock, exception enabled. */
void systick_init(void);

/**
 * @brief Wait until ms SysTick periods have elapsed.
 * @param ms Delay in milliseconds.
 * @pre systick_init() has been called.
 * @note Counts tick boundaries, so a single call waits between ms - 1 and ms
 *       milliseconds. Back-to-back calls stay aligned to the tick and do not
 *       drift.
 */
void systick_delay_ms(uint32_t ms);

/**
 * @brief SysTick exception handler: advances the millisecond count.
 * @note Referenced from the vector table in startup.c.
 */
void SysTick_Handler(void);

#endif /* SYSTICK_H */
