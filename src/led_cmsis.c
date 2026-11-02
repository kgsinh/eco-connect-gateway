/**
 * @file led_cmsis.c
 * @brief LD3 (red, PB14) driven through CMSIS register access.
 *
 * Registers: RM0468 Rev 3, 8.7.42 (RCC_AHB4ENR) and 11.4 (GPIO). The board
 * DTS routes PB14 to TIM12 PWM; boards/nucleo_h723zg.overlay disables that
 * so no Zephyr driver claims the pin.
 */
#include "led_cmsis.h"
#include <soc.h>

void led_cmsis_init(void)
{
	/* The Zephyr gpiob driver may have enabled it already; the read-back
	 * makes sure the enable has completed before the first GPIOB access.
	 */
	RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
	(void)RCC->AHB4ENR;

	/* Drive low before switching to output so the LED does not flash on */
	GPIOB->BSRR = GPIO_BSRR_BR14;

	GPIOB->OTYPER &= ~GPIO_OTYPER_OT14;          /* 0: push-pull */
	GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED14;    /* 00: low speed */
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD14;          /* 00: no pull */
	GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE14) | GPIO_MODER_MODE14_0;  /* 01: output */
}

void led_cmsis_toggle(void)
{
	if ((GPIOB->ODR & GPIO_ODR_OD14) != 0UL) {
		GPIOB->BSRR = GPIO_BSRR_BR14;
	} else {
		GPIOB->BSRR = GPIO_BSRR_BS14;
	}
}
