/**
  ******************************************************************************
  * @file    main.h
  * @brief   Shared by main.c and the CubeMX IRQ/MSP files.
  ******************************************************************************
  */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"   /* HAL_Delay / HAL_IncTick; also pulls in CMSIS */

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
