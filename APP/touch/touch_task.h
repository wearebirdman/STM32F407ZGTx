#ifndef __TOUCH_TASK_H
#define __TOUCH_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* 触摸按下事件（PRESS_DOWN 边沿，携带按下瞬间原始 AD 坐标）：
 * touch_task 是本结构的唯一生产者（q_touch_evt 常开推送，满则丢），
 * calib_task 在校准会话内消费。4 字节按值传递，无指针。 */
typedef struct {
    uint16_t raw_x;
    uint16_t raw_y;
} TouchEvt_t;

/* 按下事件队列句柄（定义于 freertos.c）：touch_task -> calib_task */
extern osMessageQueueId_t q_touch_evtHandle;

void touch_proc(void *argument);

#endif /* __TOUCH_TASK_H */