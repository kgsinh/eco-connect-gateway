/**
 * @file rcc.h
 * @brief Peripheral clock enables (RM0468 8.7.42).
 *
 * Only clock gates: sys_ck stays at the reset HSI 64 MHz.
 */
#ifndef RCC_H
#define RCC_H

#include <stdint.h>

/** @name RCC_AHB4ENR bits
 * @{ */
#define RCC_AHB4ENR_GPIOBEN (1UL << 1U)
#define RCC_AHB4ENR_GPIOEEN (1UL << 4U)
/** @} */

/**
 * @brief Enable AHB4 peripheral clocks.
 * @param mask RCC_AHB4ENR_* bits to set; other bits are kept.
 * @note Returns once the enable has reached the RCC, so the peripheral can be
 *       accessed immediately.
 */
void rcc_ahb4_enable(uint32_t mask);

#endif /* RCC_H */
