#ifndef __HOME_TASK_H
#define __HOME_TASK_H

#include "main.h"
#include "cmsis_os.h"

/*
 * UI 事件枢纽：home_task 是唯一消费者，生产者为
 *   touch_task（UIEVT_TOUCH，按下广播）、
 *   key_task （UIEVT_KICK_CALIB，按键请求校准）、
 *   calib_task（UIEVT_CALIB_DONE，校准会话结束回报）、
 *   draw_task （UIEVT_DRAW_DONE，画板会话结束回报）。
 * 会话期间的触摸进 q_touch_evt（会话任务消费），本队列中的
 * TOUCH 事件由 home 在 IN_CALIB/IN_DRAW 状态下丢弃，无互抢。
 */
typedef enum {
    UIEVT_TOUCH = 0,     /* 触摸按下（携带屏幕坐标 + 原始 AD） */
    UIEVT_KICK_CALIB,    /* 请求进入校准（经 home 仲裁后触发 calib） */
    UIEVT_CALIB_DONE,    /* 校准会话结束，home 可重新接管主界面 */
    UIEVT_DRAW_DONE,     /* 画板会话结束，home 可重新接管主界面 */
} UiEvtType_t;

typedef struct {
    uint8_t  type;       /* UiEvtType_t（uint8_t 而非枚举：GCC enum 占 4 字节） */
    uint16_t x, y;       /* UIEVT_TOUCH: 屏幕坐标（未校准时为原始 AD 值） */
    uint16_t raw_x, raw_y;
} UiEvt_t;

/* UI 事件队列句柄（定义于 freertos.c） */
extern osMessageQueueId_t q_ui_evtHandle;

void home_proc(void *argument);

#endif /* __HOME_TASK_H */
