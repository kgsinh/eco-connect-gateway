/**
  ******************************************************************************
  * @file    main.c
  * @brief   NUCLEO-H723ZG blinky. Runs on the reset clock left by SystemInit():
  *          HSI 64 MHz, all prescalers /1, LDO supply, VOS3.
  ******************************************************************************
  */
#include "main.h"
#include "led.h"
#include "systick.h"

#define LED_BLINK_HALF_PERIOD_MS  (1000U)

static void Error_Handler(void);

int main(void)
{
  SystemCoreClockUpdate();
  if (!Tick_Init(SystemCoreClock))
  {
    Error_Handler();
  }

  LED_Init();

  while(1)
  {
    LED_Toggle();
    Tick_DelayMs(LED_BLINK_HALF_PERIOD_MS);
  }
}

static void Error_Handler(void)
{
  __disable_irq();
  for (;;)
  {
  }
}
