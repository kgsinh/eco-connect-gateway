/**
 * @file systick.c
 * @brief 1 ms SysTick time base and millisecond delay (PM0253 4.4).
 *
 * SysTick counts processor clock cycles. The core runs on the reset clock,
 * HSI at 64 MHz (RM0468 8.7.2, HSIDIV = 00). PWR_CR3 only has to be
 * configured before changing VOS or the sys_ck frequency (RM0468 6.8.4), so
 * the supply configuration is left at reset.
 */
#include "drivers/systick.h"
#include <stddef.h>

/** @brief SysTick register block. */
struct systick_regs {
	volatile uint32_t CSR; /**< 0x00 Control and status */
	volatile uint32_t RVR; /**< 0x04 Reload value, 24 bits */
	volatile uint32_t CVR; /**< 0x08 Current value */
};

_Static_assert(offsetof(struct systick_regs, CVR) == 0x08U, "SysTick CVR offset");

#define SYSTICK ((struct systick_regs *)0xE000E010U)

/** @name SYST_CSR bits (PM0253 Table 72)
 * @{ */
#define SYST_ENABLE    (1UL << 0U)
#define SYST_TICKINT   (1UL << 1U) /**< Exception on each count to 0 */
#define SYST_CLKSOURCE (1UL << 2U) /**< 1: processor clock */
/** @} */

#define SYSCLK_HZ   64000000U
#define SYST_RELOAD ((SYSCLK_HZ / 1000U) - 1U) /**< N - 1 for N cycles (PM0253 4.4.2) */

_Static_assert(SYST_RELOAD <= 0xFFFFFFU, "SysTick reload is 24 bits");

/** Milliseconds since systick_init(); written only by SysTick_Handler. */
static volatile uint32_t ms_ticks;

void SysTick_Handler(void)
{
	ms_ticks++;
}

void systick_init(void)
{
	SYSTICK->RVR = SYST_RELOAD;
	SYSTICK->CVR = 0U;
	SYSTICK->CSR = SYST_CLKSOURCE | SYST_TICKINT | SYST_ENABLE;
}

void systick_delay_ms(uint32_t ms)
{
	/* A 32-bit aligned load is single-copy atomic: no need to mask the IRQ */
	const uint32_t start = ms_ticks;

	/* Unsigned subtraction stays correct across the 2^32 wrap */
	while ((ms_ticks - start) < ms) {
	}
}
