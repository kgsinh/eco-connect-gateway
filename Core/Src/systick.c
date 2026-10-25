/**
  ******************************************************************************
  * @file    systick.c
  * @brief   1 ms SysTick time base and blocking delay - CMSIS register access.
  *          Registers: PM0253 Rev 6, section 4.4.
  ******************************************************************************
  */
#include "systick.h"
#include "stm32h7xx.h"

#define TICK_RATE_HZ  (1000UL)

static volatile uint32_t s_tick_ms = 0UL;

/**
  * @brief  Start SysTick on the processor clock with a 1 ms interrupt.
  * @retval false if core_clock_hz gives a RELOAD outside 1..0xFFFFFF.
  */
bool Tick_Init(uint32_t core_clock_hz)
{
  const uint32_t reload = (core_clock_hz / TICK_RATE_HZ) - 1UL;
  bool ok = false;

  if ((reload != 0UL) && (reload <= SysTick_LOAD_RELOAD_Msk))
  {
    SysTick->CTRL = 0UL;                 /* stop while configuring */
    SysTick->LOAD = reload;              /* period = RELOAD + 1 cycles */
    SysTick->VAL = 0UL;                  /* any write clears counter and COUNTFLAG */
    NVIC_SetPriority(SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);   /* lowest */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk     /* processor clock */
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;
    ok = true;
  }

  return ok;
}

uint32_t Tick_GetMs(void)
{
  return s_tick_ms;
}

/**
  * @brief  Busy-wait. Actual delay is between ms - 1 and ms, since the first
  *         tick is partial. Wrap-safe: unsigned subtraction.
  */
void Tick_DelayMs(uint32_t ms)
{
  const uint32_t start = Tick_GetMs();

  while ((Tick_GetMs() - start) < ms)
  {
  }
}

void SysTick_Handler(void)
{
  s_tick_ms++;
}
