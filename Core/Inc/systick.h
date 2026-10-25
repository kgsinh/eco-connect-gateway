/**
  ******************************************************************************
  * @file    systick.h
  * @brief   1 ms SysTick time base and blocking delay - CMSIS register access.
  ******************************************************************************
  */
#ifndef SYSTICK_H
#define SYSTICK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool Tick_Init(uint32_t core_clock_hz);
uint32_t Tick_GetMs(void);
void Tick_DelayMs(uint32_t ms);

void SysTick_Handler(void);   /* vector table entry, defined in systick.c */

#ifdef __cplusplus
}
#endif

#endif /* SYSTICK_H */
