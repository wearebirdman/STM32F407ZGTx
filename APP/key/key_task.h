#ifndef __KEY_TASK_H
#define __KEY_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

extern osMessageQueueId_t q_KeyMsgHandle;  /* 按键事件队列，freertos.c 创建 */

void Key_Proc(void *argument);  /* 按键任务入口 */

#endif /* __KEY_TASK_H */
