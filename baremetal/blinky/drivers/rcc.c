/**
 * @file rcc.c
 * @brief Peripheral clock enables (RM0468 8.7.42).
 */
#include "drivers/rcc.h"

#define RCC_BASE    0x58024400U
#define RCC_AHB4ENR (*(volatile uint32_t *)(RCC_BASE + 0x0E0U))

void rcc_ahb4_enable(uint32_t mask)
{
	RCC_AHB4ENR |= mask;
	/* Read back so the clock enable has completed before the first peripheral access */
	(void)RCC_AHB4ENR;
}
