/*
 * LD2 (yellow, PE1) driver - STM32Cube HAL GPIO.
 * HAL module enabled through APP_STM32_HAL_GPIO in the app Kconfig.
 */
#include "led_hal.h"
#include <stm32h7xx_hal.h>

#define LED_HAL_PORT  GPIOE
#define LED_HAL_PIN   GPIO_PIN_1

void led_hal_init(void)
{
	GPIO_InitTypeDef gpio = {0};

	/* Port E clock may already be on (Zephyr gpioe driver); enabling is idempotent */
	__HAL_RCC_GPIOE_CLK_ENABLE();

	/* Latch low before switching to output so the LED cannot flash on */
	HAL_GPIO_WritePin(LED_HAL_PORT, LED_HAL_PIN, GPIO_PIN_RESET);

	gpio.Pin = LED_HAL_PIN;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED_HAL_PORT, &gpio);
}

void led_hal_toggle(void)
{
	HAL_GPIO_TogglePin(LED_HAL_PORT, LED_HAL_PIN);
}
