/**
 * @file main.c
 * @brief Bare-metal register blinky for NUCLEO-H723ZG.
 *
 * No HAL, no RTOS, no vendor headers. Same pattern as the Zephyr app:
 * LD1 green PB0 1000 ms, LD2 yellow PE1 500 ms, LD3 red PB14 250 ms.
 *
 * sys_ck is the reset default, HSI at 64 MHz (RM0468 8.7.2, HSIDIV = 00).
 * PWR_CR3 only has to be configured before changing VOS or the sys_ck
 * frequency (RM0468 6.8.4), so the supply configuration is left at reset.
 */
#include <stdint.h>

/** @brief 32-bit memory-mapped register at an absolute address. */
#define REG32(addr)  (*(volatile uint32_t *)(addr))

/** @name RCC (RM0468 Table 7, 8.7.42)
 * @{ */
#define RCC_BASE        0x58024400U
#define RCC_AHB4ENR     (RCC_BASE + 0x0E0U)
#define RCC_GPIOBEN     (1UL << 1U)
#define RCC_GPIOEEN     (1UL << 4U)
/** @} */

/** @name GPIO (RM0468 Table 7, 11.4)
 * @{ */
#define GPIOB_BASE      0x58020400U
#define GPIOE_BASE      0x58021000U
#define GPIO_MODER      0x00U
#define GPIO_ODR        0x14U
#define GPIO_BSRR       0x18U
#define GPIO_MODE_MASK  3UL
#define GPIO_MODE_OUT   1UL   /**< MODERy = 01: general purpose output */
/** @} */

/** @name SysTick (PM0253 4.4)
 * @{ */
#define SYST_CSR        0xE000E010U
#define SYST_RVR        0xE000E014U
#define SYST_CVR        0xE000E018U
#define SYST_ENABLE     (1UL << 0U)
#define SYST_CLKSOURCE  (1UL << 2U)    /**< 1: processor clock */
#define SYST_COUNTFLAG  (1UL << 16U)
/** @} */

#define SYSCLK_HZ       64000000U
#define TICK_MS         250U

/** @brief One user LED: GPIO port base address and pin number. */
struct led {
	uint32_t port;   /**< GPIO port base address */
	uint32_t pin;    /**< Pin number, 0..15 */
};

static const struct led ld1 = { GPIOB_BASE, 0U };
static const struct led ld2 = { GPIOE_BASE, 1U };
static const struct led ld3 = { GPIOB_BASE, 14U };

int main(void);

/**
 * @brief Configure an LED pin as a push-pull output, initially off.
 * @param l LED to configure. Its port clock must already be enabled.
 */
static void led_init(const struct led *l)
{
	uint32_t moder = REG32(l->port + GPIO_MODER);

	/* Drive low before leaving analog mode so the LED does not flash on */
	REG32(l->port + GPIO_BSRR) = 1UL << (l->pin + 16U);
	moder &= ~(GPIO_MODE_MASK << (l->pin * 2U));
	moder |= GPIO_MODE_OUT << (l->pin * 2U);
	REG32(l->port + GPIO_MODER) = moder;
}

/**
 * @brief Invert an LED output.
 * @param l LED to toggle.
 * @note Written through BSRR, so other pins on the same port are not affected.
 */
static void led_toggle(const struct led *l)
{
	if ((REG32(l->port + GPIO_ODR) & (1UL << l->pin)) != 0U) {
		REG32(l->port + GPIO_BSRR) = 1UL << (l->pin + 16U);
	} else {
		REG32(l->port + GPIO_BSRR) = 1UL << l->pin;
	}
}

/** @brief Start SysTick with a 1 ms period on the processor clock, no interrupt. */
static void systick_init(void)
{
	REG32(SYST_RVR) = (SYSCLK_HZ / 1000U) - 1U;
	REG32(SYST_CVR) = 0U;
	REG32(SYST_CSR) = SYST_CLKSOURCE | SYST_ENABLE;
}

/**
 * @brief Busy-wait for a number of SysTick periods.
 * @param ms Delay in milliseconds.
 * @pre systick_init() has been called.
 */
static void delay_ms(uint32_t ms)
{
	uint32_t left = ms;

	while (left > 0U) {
		/* COUNTFLAG is set on each wrap to 0 and cleared by this read */
		if ((REG32(SYST_CSR) & SYST_COUNTFLAG) != 0U) {
			left--;
		}
	}
}

/**
 * @brief Enable the GPIO clocks, set up the LEDs and blink them forever.
 * @return Does not return.
 */
int main(void)
{
	uint32_t tick = 0U;

	REG32(RCC_AHB4ENR) |= RCC_GPIOBEN | RCC_GPIOEEN;
	/* Read back so the clock enable has completed before the first GPIO access */
	(void)REG32(RCC_AHB4ENR);

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
