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
/* Definitions for t_lcd_disp */
osThreadId_t t_lcd_dispHandle;
const osThreadAttr_t t_lcd_disp_attributes = {
  .name = "t_lcd_disp",
  .stack_size = 1536 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for t_led_disp */
osThreadId_t t_led_dispHandle;
const osThreadAttr_t t_led_disp_attributes = {
  .name = "t_led_disp",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for t_key_proc */
osThreadId_t t_key_procHandle;
const osThreadAttr_t t_key_proc_attributes = {
  .name = "t_key_proc",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for t_touch_proc */
osThreadId_t t_touch_procHandle;
const osThreadAttr_t t_touch_proc_attributes = {
  .name = "t_touch_proc",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
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

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void lcd_disp(void *argument);
void led_disp(void *argument);
void key_proc(void *argument);
void touch_proc(void *argument);

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

  /* USER CODE BEGIN RTOS_QUEUES */
  /* creation of q_UiMsg */
  q_UiMsgHandle = osMessageQueueNew(8, sizeof(UiMsg_t), NULL);
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of t_lcd_disp */
  t_lcd_dispHandle = osThreadNew(lcd_disp, NULL, &t_lcd_disp_attributes);

  /* creation of t_led_disp */
  t_led_dispHandle = osThreadNew(led_disp, NULL, &t_led_disp_attributes);

  /* creation of t_key_proc */
  t_key_procHandle = osThreadNew(key_proc, NULL, &t_key_proc_attributes);

  /* creation of t_touch_proc */
  t_touch_procHandle = osThreadNew(touch_proc, NULL, &t_touch_proc_attributes);

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

/* USER CODE BEGIN Header_lcd_disp */
/**
* @brief Function implementing the t_lcd_disp thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_lcd_disp */
__weak void lcd_disp(void *argument)
{
  /* USER CODE BEGIN lcd_disp */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END lcd_disp */
}

/* USER CODE BEGIN Header_led_disp */
/**
* @brief Function implementing the t_led_disp thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_led_disp */
__weak void led_disp(void *argument)
{
  /* USER CODE BEGIN led_disp */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END led_disp */
}

/* USER CODE BEGIN Header_key_proc */
/**
* @brief Function implementing the key_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_key_proc */
__weak void key_proc(void *argument)
{
  /* USER CODE BEGIN key_proc */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END key_proc */
}

/* USER CODE BEGIN Header_touch_proc */
/**
* @brief Function implementing the touch_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_touch_proc */
__weak void touch_proc(void *argument)
{
  /* USER CODE BEGIN touch_proc */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END touch_proc */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

