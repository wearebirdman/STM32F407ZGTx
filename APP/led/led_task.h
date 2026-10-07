#ifndef __LED_TASK_H
#define __LED_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

extern osMessageQueueId_t q_LedReqHandle;  /* LED请求队列，freertos.c 创建 */

void Led_Disp(void *argument);  /* LED显示任务入口 */

#endif /* __LED_TASK_H */
