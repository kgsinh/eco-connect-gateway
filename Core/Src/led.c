/**
  ******************************************************************************
  * @file    led.c
  * @brief   User LED (LD1, PB0) driver - HAL GPIO.
  ******************************************************************************
  */
#include "led.h"
#include "stm32h7xx_hal.h"

#define LED_GPIO_PORT  GPIOB
#define LED_GPIO_PIN   GPIO_PIN_0

/**
  * @brief  Configure PB0 as push-pull output, low speed, no pull, LED off.
  */
void LED_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Latch low before switching to output so the LED cannot flash on */
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);

  gpio.Pin = LED_GPIO_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_PORT, &gpio);
}

void LED_On(void)
{
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
}

void LED_Off(void)
{
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

void LED_Toggle(void)
{
  HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
}
