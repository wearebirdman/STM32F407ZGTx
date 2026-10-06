/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */

/* 采集接口（adc.c 实现，TIM2→ADC1→DMA 循环，示波页使用） */
uint8_t Adc_CaptureStart(uint16_t rate_hz);       // 启动采集：配触发频率并重启 TIM→ADC/DMA 循环采样（幂等）；返回 0=启动失败
void Adc_CaptureStop(void);                       // 停止采集：停 TIM 并终止 ADC/DMA 流（强制复位 HAL 状态/锁/标志）
uint8_t Adc_CaptureHealth(void);                  // 帧级看门狗：停摆检测+强制重启（按最近档位）；返回 1=发生了重启，0=健康
uint8_t Adc_ReadWindow(int32_t mv[], uint16_t n); // 快照最新 n 点（0..3300mV）；返回 1=拷贝期间有新样本写入（撕裂，调用方重试）

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

