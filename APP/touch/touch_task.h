#ifndef __TOUCH_TASK_H
#define __TOUCH_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

extern osMessageQueueId_t q_TouchMsgHandle; /* 触摸消息队列句柄（touch_task 生产） */

void Touch_Proc(void *argument);            /* 触摸任务入口 */

#endif /* __TOUCH_TASK_H */
