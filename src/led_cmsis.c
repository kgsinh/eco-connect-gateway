/*
 * LD3 (red, PB14) driver - CMSIS register access.
 * Registers: RM0468 Rev 3, sections 8.7.42 (RCC_AHB4ENR) and 11.4 (GPIO).
 * PB14's TIM12 PWM is disabled in boards/nucleo_h723zg.overlay so no
 * Zephyr driver touches this pin.
 */
#include "led_cmsis.h"
#include <soc.h>

void led_cmsis_init(void)
{
	/* Port B clock may already be on (Zephyr gpiob driver); read-back makes
	 * sure the enable has taken effect before the first GPIOB access.
	 */
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
	(void)RCC->AHB4ENR;

	/* Latch low before switching to output so the LED cannot flash on */
	GPIOB->BSRR = GPIO_BSRR_BR14;

	GPIOB->OTYPER &= ~GPIO_OTYPER_OT14;          /* 0: push-pull */
	GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED14;    /* 00: low speed */
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD14;          /* 00: no pull */
	GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE14) | GPIO_MODER_MODE14_0;  /* 01: output */
}

/* BSRR write affects only PB14, so PB0 (LD1, Zephyr driver) on the same port is untouched */
void led_cmsis_toggle(void)
{
	if ((GPIOB->ODR & GPIO_ODR_OD14) != 0UL) {
		GPIOB->BSRR = GPIO_BSRR_BR14;
	} else {
		GPIOB->BSRR = GPIO_BSRR_BS14;
	}
}
