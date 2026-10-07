/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "led.h"
#include "key.h"
#include "touch.h"
#include "ui_msg.h"
#include "uart_task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for t_Lcd_Disp */
osThreadId_t t_Lcd_DispHandle;
const osThreadAttr_t t_Lcd_Disp_attributes = {
  .name = "t_Lcd_Disp",
  .stack_size = 1536 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for t_Led_Disp */
osThreadId_t t_Led_DispHandle;
const osThreadAttr_t t_Led_Disp_attributes = {
  .name = "t_Led_Disp",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for t_Key_Proc */
osThreadId_t t_Key_ProcHandle;
const osThreadAttr_t t_Key_Proc_attributes = {
  .name = "t_Key_Proc",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for t_Touch_Proc */
osThreadId_t t_Touch_ProcHandle;
const osThreadAttr_t t_Touch_Proc_attributes = {
  .name = "t_Touch_Proc",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for t_Uart_Send */
osThreadId_t t_Uart_SendHandle;
const osThreadAttr_t t_Uart_Send_attributes = {
  .name = "t_Uart_Send",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for t_Uart_Recv */
osThreadId_t t_Uart_RecvHandle;
const osThreadAttr_t t_Uart_Recv_attributes = {
  .name = "t_Uart_Recv",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for q_LedReq */
osMessageQueueId_t q_LedReqHandle;
const osMessageQueueAttr_t q_LedReq_attributes = {
  .name = "q_LedReq"
};
/* Definitions for q_KeyMsg */
osMessageQueueId_t q_KeyMsgHandle;
const osMessageQueueAttr_t q_KeyMsg_attributes = {
  .name = "q_KeyMsg"
};
/* Definitions for q_TouchMsg */
osMessageQueueId_t q_TouchMsgHandle;
const osMessageQueueAttr_t q_TouchMsg_attributes = {
  .name = "q_TouchMsg"
};
/* Definitions for q_UiMsg */
osMessageQueueId_t q_UiMsgHandle;
const osMessageQueueAttr_t q_UiMsg_attributes = {
  .name = "q_UiMsg"
};
/* Definitions for q_Usart1RxMsg */
osMessageQueueId_t q_Usart1RxMsgHandle;
const osMessageQueueAttr_t q_Usart1RxMsg_attributes = {
  .name = "q_Usart1RxMsg"
};
/* Definitions for q_Usart1TxMsg */
osMessageQueueId_t q_Usart1TxMsgHandle;
const osMessageQueueAttr_t q_Usart1TxMsg_attributes = {
  .name = "q_Usart1TxMsg"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void Lcd_Disp(void *argument);
void Led_Disp(void *argument);
void Key_Proc(void *argument);
void Touch_Proc(void *argument);
void Uart_Send(void *argument);
void Uart_Recv(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of q_LedReq */
  q_LedReqHandle = osMessageQueueNew (4, sizeof(LedReq_t), &q_LedReq_attributes);

  /* creation of q_KeyMsg */
  q_KeyMsgHandle = osMessageQueueNew (16, sizeof(KeyMsg_t), &q_KeyMsg_attributes);

  /* creation of q_TouchMsg */
  q_TouchMsgHandle = osMessageQueueNew (16, sizeof(TouchMsg_t), &q_TouchMsg_attributes);

  /* creation of q_UiMsg */
  q_UiMsgHandle = osMessageQueueNew (8, sizeof(UiMsg_t), &q_UiMsg_attributes);

  /* creation of q_Usart1RxMsg */
  q_Usart1RxMsgHandle = osMessageQueueNew (8, sizeof(UartMsg_t), &q_Usart1RxMsg_attributes);

  /* creation of q_Usart1TxMsg */
  q_Usart1TxMsgHandle = osMessageQueueNew (8, sizeof(UartMsg_t), &q_Usart1TxMsg_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of t_Lcd_Disp */
  t_Lcd_DispHandle = osThreadNew(Lcd_Disp, NULL, &t_Lcd_Disp_attributes);

  /* creation of t_Led_Disp */
  t_Led_DispHandle = osThreadNew(Led_Disp, NULL, &t_Led_Disp_attributes);

  /* creation of t_Key_Proc */
  t_Key_ProcHandle = osThreadNew(Key_Proc, NULL, &t_Key_Proc_attributes);

  /* creation of t_Touch_Proc */
  t_Touch_ProcHandle = osThreadNew(Touch_Proc, NULL, &t_Touch_Proc_attributes);

  /* creation of t_Uart_Send */
  t_Uart_SendHandle = osThreadNew(Uart_Send, NULL, &t_Uart_Send_attributes);

  /* creation of t_Uart_Recv */
  t_Uart_RecvHandle = osThreadNew(Uart_Recv, NULL, &t_Uart_Recv_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_Lcd_Disp */
/**
* @brief Function implementing the t_Lcd_Disp thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Lcd_Disp */
__weak void Lcd_Disp(void *argument)
{
  /* USER CODE BEGIN Lcd_Disp */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Lcd_Disp */
}

/* USER CODE BEGIN Header_Led_Disp */
/**
* @brief Function implementing the t_Led_Disp thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Led_Disp */
__weak void Led_Disp(void *argument)
{
  /* USER CODE BEGIN Led_Disp */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Led_Disp */
}

/* USER CODE BEGIN Header_Key_Proc */
/**
* @brief Function implementing the t_Key_Proc thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Key_Proc */
__weak void Key_Proc(void *argument)
{
  /* USER CODE BEGIN Key_Proc */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Key_Proc */
}

/* USER CODE BEGIN Header_Touch_Proc */
/**
* @brief Function implementing the t_Touch_Proc thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Touch_Proc */
__weak void Touch_Proc(void *argument)
{
  /* USER CODE BEGIN Touch_Proc */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Touch_Proc */
}

/* USER CODE BEGIN Header_Uart_Send */
/**
* @brief Function implementing the t_Uart_Send thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Uart_Send */
__weak void Uart_Send(void *argument)
{
  /* USER CODE BEGIN Uart_Send */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Uart_Send */
}

/* USER CODE BEGIN Header_Uart_Recv */
/**
* @brief Function implementing the t_Uart_Recv thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Uart_Recv */
__weak void Uart_Recv(void *argument)
{
  /* USER CODE BEGIN Uart_Recv */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Uart_Recv */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

