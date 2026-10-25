/**
  ******************************************************************************
  * @file    led.h
  * @brief   User LED (LD1, PB0) driver - CMSIS register access.
  ******************************************************************************
  */
#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

void LED_Init(void);
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
