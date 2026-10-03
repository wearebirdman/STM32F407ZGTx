#ifndef __TOUCH_TASK_H
#define __TOUCH_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* 会话触摸流事件（q_touch_evt 泛化为"独占会话 app 的触摸流"）：
 * touch_task 是唯一生产者（常开推送，满则丢，无消费者自动丢弃）；
 * 同一时刻只有一个会话消费者（calib 读 raw_*，draw 读 x/y + event）。
 * 按值传递 8 字节，无指针。 */
typedef struct {
    uint8_t  event;       /* TOUCH_EVENT_PRESS_DOWN / _HOLD（UP 不入队） */
    uint8_t  _res;        /* 对齐填充 */
    uint16_t x, y;        /* 屏幕坐标（未校准时为原始 AD 值） */
    uint16_t raw_x, raw_y;/* 原始 AD 值（校准取点用） */
} TouchEvt_t;

/* 会话触摸流队列句柄（定义于 freertos.c）：touch_task -> 会话任务 */
extern osMessageQueueId_t q_touch_evtHandle;

void touch_proc(void *argument);

#endif /* __TOUCH_TASK_H */