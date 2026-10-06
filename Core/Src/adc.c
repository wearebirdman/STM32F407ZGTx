/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
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
/* Includes ------------------------------------------------------------------*/
#include "adc.h"

/* USER CODE BEGIN 0 */
#include "tim.h"
#include "uart_task.h"

#define ADC_RING_SIZE   1024 // 采样环点数：DMA 循环写入，UI 快照最新窗口
#define ADC_TRIG_CNT_HZ 10000u // TIM2 计数频率 = 84MHz(APB1 定时器时钟)/(8399+1)
#define ADC_STALL_TICKS 5    // 看门狗：连续 N 帧 CNDTR 无进展判停摆（50Hz 档 5 帧=250ms 漏 12 样本）

static uint16_t s_adc_ring[ADC_RING_SIZE]; // DMA 落货环：硬件连续写，读侧快照
static uint8_t s_adc_capturing;            // 采集运行标志
static uint16_t s_rate_cur = 50;           // 最近一次启动的采样率（看门狗重启用）
static uint16_t s_watch_cndtr;             // 看门狗：上帧 CNDTR 快照
static uint8_t s_watch_stall;              // 看门狗：连续无进展帧数
/* USER CODE END 0 */

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

/* ADC1 init function */
void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV6;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  SET_BIT(ADC1->CR2, ADC_CR2_DMA); // DMA 连续请求：单转换序列 CubeMX 默认关，不开只搬走第一个样本
  /* USER CODE END ADC1_Init 2 */

}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */
    /* ADC1 clock enable */
    __HAL_RCC_ADC1_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PC0     ------> ADC1_IN10
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ADC1 DMA Init */
    /* ADC1 Init */
    hdma_adc1.Instance = DMA2_Stream0;
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle,DMA_Handle,hdma_adc1);

  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ADC1_CLK_DISABLE();

    /**ADC1 GPIO Configuration
    PC0     ------> ADC1_IN10
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0);

    /* ADC1 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* 启动采集：按 rate_hz 配 TIM2 ARR 并重启 TIM→ADC/DMA 循环流（幂等：先停旧再启新）。
   50..1000Hz 档在 10kHz 计数频率下均为整除，无累计误差。返回 0=启动失败 */
uint8_t Adc_CaptureStart(uint16_t rate_hz)
{
    uint32_t arr;

    if (rate_hz == 0 || rate_hz > ADC_TRIG_CNT_HZ)
        return 0;
    Adc_CaptureStop();
    s_rate_cur = rate_hz;
    arr = ADC_TRIG_CNT_HZ / rate_hz;
    __HAL_TIM_SET_AUTORELOAD(&htim2, arr - 1);
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
        return 0;
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC | ADC_FLAG_OVR); // 清残留状态，防上次异常卡死本次启动
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_ring, ADC_RING_SIZE) != HAL_OK)
    {
        HAL_TIM_Base_Stop(&htim2);
        return 0;
    }
    /* 循环流自续，三类传输中断全关：TE 若放行会走 HAL 错误回调把 State 打成
       ERROR_DMA 且 Stop_DMA 不清 ERROR 态，重启永远失败——停摆改由看门狗兜底 */
    __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT | DMA_IT_TC | DMA_IT_TE);
    s_watch_cndtr = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_adc1);
    s_watch_stall = 0;
    s_adc_capturing = 1;
    return 1;
}

/* 停止采集：先停触发源再断流。HAL 的 Stop 系列对 ERROR/LOCKED 残留有盲区
   （Stop_DMA 仅 BUSY 态才 Abort、ADC_DMAError 直写 State 无从公开 API 清除），
   故收尾统一强制复位状态机、软锁与 DMA 标志，保证下一次 Start 必进 READY 门控 */
void Adc_CaptureStop(void)
{
    uint32_t guard;

    s_adc_capturing = 0;
    HAL_TIM_Base_Stop(&htim2);
    HAL_ADC_Stop_DMA(&hadc1);
    /* HAL 状态为 ERROR 时 Stop_DMA 不 Abort，流可能仍 EN=1：无条件断流并等 EN 落 */
    __HAL_DMA_DISABLE(&hdma_adc1);
    guard = 1000u;
    while ((hdma_adc1.Instance->CR & DMA_SxCR_EN) && --guard)
    {
        /* 等待 */
    }
    __HAL_UNLOCK(&hadc1);
    __HAL_UNLOCK(&hdma_adc1);
    __HAL_UNLOCK(&htim2);
    hadc1.State = HAL_ADC_STATE_READY;
    hadc1.ErrorCode = HAL_ADC_ERROR_NONE;
    hdma_adc1.State = HAL_DMA_STATE_READY;
    htim2.State = HAL_TIM_STATE_READY;
    __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_TCIF0_4 | DMA_FLAG_HTIF0_4 | DMA_FLAG_TEIF0_4 |
                          DMA_FLAG_DMEIF0_4 | DMA_FLAG_FEIF0_4);
}

/* 帧级看门狗（lcd_task 每刷新帧调用）：未跑则拉起；连续 ADC_STALL_TICKS 帧
   CNDTR 无进展判停摆并强制重启。诊断日志区分两类：
   start fail=HAL 启动门控没过；stall=流在但不出数（寄存器现场定位硬件侧原因） */
uint8_t Adc_CaptureHealth(void)
{
    uint16_t cndtr;

    if (!s_adc_capturing)
    {
        if (!Adc_CaptureStart(s_rate_cur))
            Uart_Printf("OSC: start FAIL st=%d\r\n", (int)hadc1.State);
        return 1u;
    }
    cndtr = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_adc1);
    if (cndtr != s_watch_cndtr)
    {
        s_watch_cndtr = cndtr;
        s_watch_stall = 0;
        return 0;
    }
    if (++s_watch_stall < ADC_STALL_TICKS)
        return 0;
    s_watch_stall = 0;
    /* 现场快照：SR 看 EOC(bit1)/OVR(bit3)；CR2 看 ADON(bit0)/DMA(bit8)/EXTEN(bit27:25)；
       DCR 看流使能 EN(bit0) */
    Uart_Printf("OSC: STALL nd=%u sr=%X\r\n", cndtr, (unsigned)ADC1->SR);
    Uart_Printf("OSC:  cr2=%X dcr=%X\r\n", (unsigned)ADC1->CR2, (unsigned)DMA2_Stream0->CR);
    if (!Adc_CaptureStart(s_rate_cur))
        Uart_Printf("OSC: restart FAIL st=%d\r\n", (int)hadc1.State);
    return 1u;
}

/* 快照最新 n 点转 mV：读写指针→拷贝→复查。返回 1 表示拷贝期间写指针动了（撕裂） */
uint8_t Adc_ReadWindow(int32_t mv[], uint16_t n)
{
    uint16_t cndtr0 = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_adc1);
    uint16_t w = (uint16_t)((ADC_RING_SIZE - cndtr0) % ADC_RING_SIZE); // 下一写入位置
    uint16_t i;

    if (n > ADC_RING_SIZE)
        n = ADC_RING_SIZE;
    for (i = 0; i < n; i++)
    {
        uint16_t src = (uint16_t)(((uint32_t)w + ADC_RING_SIZE - n + i) % ADC_RING_SIZE);
        mv[i] = (int32_t)s_adc_ring[src] * 3300 / 4095;
    }
    return ((uint16_t)__HAL_DMA_GET_COUNTER(&hdma_adc1) != cndtr0) ? 1 : 0;
}

/* USER CODE END 1 */

