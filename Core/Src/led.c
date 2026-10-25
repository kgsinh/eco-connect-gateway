/**
  ******************************************************************************
  * @file    led.c
  * @brief   User LED (LD1, PB0) driver - CMSIS register access.
  *
  *          CMSIS is used instead of HAL_GPIO so each register touched is
  *          explicit. Register/bit definitions: RM0468 Rev 3, sections 8.7.42
  *          (RCC_AHB4ENR) and 11.4 (GPIO registers).
  ******************************************************************************
  */
#include "led.h"
#include "stm32h7xx.h"

/**
  * @brief  Configure PB0 as push-pull output, low speed, no pull, LED off.
  */
void LED_Init(void)
{
  /* Enable the GPIOB clock. The read-back makes sure the enable has taken
     effect before the first GPIOB access. */
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
  (void)RCC->AHB4ENR;

  /* Drive the output latch low first so the pin cannot glitch high when it
     switches to output mode. */
  GPIOB->BSRR = GPIO_BSRR_BR0;

  GPIOB->OTYPER &= ~GPIO_OTYPER_OT0;          /* 0: push-pull */
  GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED0;    /* 00: low speed */
  GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD0;          /* 00: no pull-up/pull-down */
  GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE0) | GPIO_MODER_MODE0_0;  /* 01: output */
}

/**
  * @brief  Turn the LED on (BSRR write is atomic, no read-modify-write).
  */
void LED_On(void)
{
  GPIOB->BSRR = GPIO_BSRR_BS0;
}

/**
  * @brief  Turn the LED off.
  */
void LED_Off(void)
{
  GPIOB->BSRR = GPIO_BSRR_BR0;
}

/**
  * @brief  Toggle the LED. Reads ODR, then writes BSRR so only PB0 is affected.
  */
void LED_Toggle(void)
{
  if ((GPIOB->ODR & GPIO_ODR_OD0) != 0UL)
  {
    GPIOB->BSRR = GPIO_BSRR_BR0;
  }
  else
  {
    GPIOB->BSRR = GPIO_BSRR_BS0;
  }
}
