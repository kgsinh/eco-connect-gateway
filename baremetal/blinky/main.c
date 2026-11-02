/*
 * Bare-metal register blinky for NUCLEO-H723ZG: no HAL, no RTOS, no vendor
 * headers. Same pattern as the Zephyr app (src/main.c):
 *   LD1 green  PB0   1000 ms
 *   LD2 yellow PE1    500 ms
 *   LD3 red    PB14   250 ms
 *
 * Clock: the reset default, HSI 64 MHz (RM0468 8.7.2, HSIDIV = 00) as
 * sys_ck. The supply configuration (PWR_CR3) only has to be set before
 * changing VOS or the sys_ck frequency (RM0468 6.8.4), so it is left alone.
 * Timing: SysTick on the processor clock, polled through COUNTFLAG; no
 * interrupts are enabled.
 */
#include <stdint.h>

/* Register access by absolute address (RM0468, PM0253) */
#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* RCC (RM0468 Table 7, 8.7.42) */
#define RCC_BASE        0x58024400U
#define RCC_AHB4ENR     (RCC_BASE + 0x0E0U)
#define RCC_GPIOBEN     (1UL << 1U)
#define RCC_GPIOEEN     (1UL << 4U)

/* GPIO (RM0468 Table 7, 11.4) */
#define GPIOB_BASE      0x58020400U
#define GPIOE_BASE      0x58021000U
#define GPIO_MODER      0x00U
#define GPIO_ODR        0x14U
#define GPIO_BSRR       0x18U
#define GPIO_MODE_MASK  3UL
#define GPIO_MODE_OUT   1UL   /* MODERy = 01: general purpose output */

/* SysTick (PM0253 4.4) */
#define SYST_CSR        0xE000E010U
#define SYST_RVR        0xE000E014U
#define SYST_CVR        0xE000E018U
#define SYST_ENABLE     (1UL << 0U)
#define SYST_CLKSOURCE  (1UL << 2U)    /* processor clock */
#define SYST_COUNTFLAG  (1UL << 16U)

#define SYSCLK_HZ       64000000U
#define TICK_MS         250U

struct led {
	uint32_t port;
	uint32_t pin;
};

static const struct led ld1 = { GPIOB_BASE, 0U };
static const struct led ld2 = { GPIOE_BASE, 1U };
static const struct led ld3 = { GPIOB_BASE, 14U };

int main(void);

static void led_init(const struct led *l)
{
	uint32_t moder = REG32(l->port + GPIO_MODER);

	/* Start off, then switch the pin from analog (reset) to output */
	REG32(l->port + GPIO_BSRR) = 1UL << (l->pin + 16U);
	moder &= ~(GPIO_MODE_MASK << (l->pin * 2U));
	moder |= GPIO_MODE_OUT << (l->pin * 2U);
	REG32(l->port + GPIO_MODER) = moder;
}

/* BSRR writes one pin only, so the read of ODR is the only shared access */
static void led_toggle(const struct led *l)
{
	if ((REG32(l->port + GPIO_ODR) & (1UL << l->pin)) != 0U) {
		REG32(l->port + GPIO_BSRR) = 1UL << (l->pin + 16U);
	} else {
		REG32(l->port + GPIO_BSRR) = 1UL << l->pin;
	}
}

static void systick_init(void)
{
	REG32(SYST_RVR) = (SYSCLK_HZ / 1000U) - 1U;   /* 1 ms period */
	REG32(SYST_CVR) = 0U;
	REG32(SYST_CSR) = SYST_CLKSOURCE | SYST_ENABLE;
}

/* COUNTFLAG is set on each wrap to 0 and cleared by reading CSR */
static void delay_ms(uint32_t ms)
{
	uint32_t left = ms;

	while (left > 0U) {
		if ((REG32(SYST_CSR) & SYST_COUNTFLAG) != 0U) {
			left--;
		}
	}
}

int main(void)
{
	uint32_t tick = 0U;

	REG32(RCC_AHB4ENR) |= RCC_GPIOBEN | RCC_GPIOEEN;
	(void)REG32(RCC_AHB4ENR);   /* read back: clock is on before GPIO access */

	led_init(&ld1);
	led_init(&ld2);
	led_init(&ld3);
	systick_init();

	for (;;) {
		led_toggle(&ld3);

		if ((tick % 2U) == 0U) {
			led_toggle(&ld2);
		}

		if ((tick % 4U) == 0U) {
			led_toggle(&ld1);
		}

		tick++;
		delay_ms(TICK_MS);
	}
}
