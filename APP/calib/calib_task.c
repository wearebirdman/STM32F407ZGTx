/*
 * calib_task.c —— 独立校准任务（阻塞式客户端）
 *
 * 本任务不碰任何硬件：触摸按下经 q_touch_evt 由 touch_task 转发（触摸
 * 硬件属主仍是 touch_task），LCD 操作经 Display_* 消息队列由 lcd_task
 * 执行（显示服务属主仍是 lcd_task）。本任务只做"校准业务流程"的编排。
 *
 * 流程（与协议一一对应）：
 *   1. 阻塞等 sem_calib（唯一 release 方 = home_task，让屏后触发）；
 *   2. Display_Acquire 接管校准界面（失败=界面被占，回报 CALIB_DONE 回 1）；
 *   3. 逐点：Calib_GetPoint = Display_Draw(CROSS) 画十字并同步等应答
 *      -> 应答 OK 后从 q_touch_evt 阻塞取一次按下（10s 超时）；共 4 点；
 *   4. Display_Acquire(param 1/2) 同持有者重渲染：清屏显示成功/失败；
 *   5. 再等一次按下（5s 兜底）-> Display_Release 退出
 *      -> 向 q_ui_evt 投 CALIB_DONE，home 重新接管主界面。
 *
 * 会话边界 flush q_touch_evt：丢弃会话开始前的残留按下，保证"每个
 * 按下恰好对应一次取点"。
 */

#include "calib_task.h"
#include "touch_task.h"      /* TouchEvt_t / q_touch_evtHandle */
#include "home_task.h"       /* UiEvt_t / q_ui_evtHandle（CALIB_DONE 回报） */
#include "touch.h"           /* Touch_Adjust / Touch_SaveCalibration */
#include "display.h"
#include "ui.h"              /* HOME_NAV_Y/HOME_NAV_W（导航栏命中测试） */

/* 排空消息队列中的残留事件（非阻塞读到空为止） */
static void Queue_Flush(osMessageQueueId_t q)
{
    TouchEvt_t e;
    while (osMessageQueueGet(q, &e, 0, 0) == osOK) { }
}

/* 用户在导航栏按 HOME 主动中止校准（与"取点失败"区分：不显示失败态） */
static uint8_t s_abort;

/**
 * @brief  校准取点回调（供 Touch_Adjust 调用）：
 *         请求 LCD 画十字 -> 同步等应答 -> flush 残留 -> 等一次按下。
 *         导航栏区域（y >= HOME_NAV_Y）的按下 = HOME 键中止校准。
 * @retval 1 = 取点成功, 0 = 失败/超时/中止（Touch_Adjust 将中止）
 */
static uint8_t Calib_GetPoint(uint16_t x, uint16_t y,
                              uint16_t *raw_x, uint16_t *raw_y)
{
    TouchEvt_t e;

    /* 画十字：同步等待服务层应答（非持有者/未就绪会立即回 FORBIDDEN） */
    if (Display_Draw(DISP_CLIENT_CALIB, DISP_DRAW_CROSS, x, y, RED) != DISP_OK)
        return 0;

    /* 丢弃上一次按压残留的 HOLD/UP 流，保证"每个新按下恰好一次取点" */
    Queue_Flush(q_touch_evtHandle);

    /* 等用户按下：DOWN 或 HOLD 都算（10s 超时） */
    for (;;)
    {
        if (osMessageQueueGet(q_touch_evtHandle, &e, 0, 10000) != osOK)
            return 0;
        if (e.event == TOUCH_EVENT_PRESS_UP)
            continue;                       /* 残留抬起：忽略继续等 */

        if (e.y >= HOME_NAV_Y)              /* 导航栏按下：HOME 键 = 中止 */
        {
            if (e.x < HOME_NAV_W)
            {
                s_abort = 1;
                return 0;
            }
            continue;                       /* < / > 键：校准中无意义，忽略 */
        }

        *raw_x = e.raw_x;
        *raw_y = e.raw_y;
        return 1;
    }
}

/**
 * @brief  校准任务主循环：阻塞等触发 -> 一次完整 4 点校准会话
 */
void calib_proc(void *argument)
{
    TouchEvt_t e;
    UiEvt_t done = { .type = UIEVT_CALIB_DONE };

    for (;;)
    {
        /* 1. 阻塞等校准触发（home 让屏后 release） */
        if (osSemaphoreAcquire(sem_calibHandle, osWaitForever) != osOK)
            continue;

        /* 2. 接管校准界面（服务层渲染底版）；失败也必须回报 CALIB_DONE，
         *    否则 home 永远等在 IN_CALIB 态（home 已让屏，正常不会失败） */
        if (Display_Acquire(DISP_CLIENT_CALIB, DISP_SCREEN_CALIBRATE, 0) != DISP_OK)
        {
            (void)osMessageQueuePut(q_ui_evtHandle, &done, 0, 0);
            continue;
        }

        /* 3. 排空会话前残留按下，逐点采集（限内容区，避开常驻导航栏） */
        Queue_Flush(q_touch_evtHandle);
        s_abort = 0;

        uint8_t ok = (Touch_Adjust(LCD_WIDTH, HOME_NAV_Y, 20, Calib_GetPoint) == TOUCH_OK);

        if (s_abort)
        {
            /* 用户按导航栏 HOME 主动中止：不显示结果态，直接归还 */
            Display_Release(DISP_CLIENT_CALIB);
            (void)osMessageQueuePut(q_ui_evtHandle, &done, 0, 0);
            continue;
        }

        if (ok)
        {
#if TOUCH_USE_EEPROM_CAL
            Touch_SaveCalibration();   /* 成功：写 EEPROM，掉电不丢 */
#endif
        }

        /* 4. 同持有者再次 Acquire = 会话内换状态（1=成功 2=失败），清屏重写 */
        Display_Acquire(DISP_CLIENT_CALIB, DISP_SCREEN_CALIBRATE, ok ? 1 : 2);

        /* 5. 等用户再看一次点击返回（5s 兜底防卡死），归还屏幕并回报 home */
        Queue_Flush(q_touch_evtHandle);
        (void)osMessageQueueGet(q_touch_evtHandle, &e, 0, 5000);

        Display_Release(DISP_CLIENT_CALIB);
        (void)osMessageQueuePut(q_ui_evtHandle, &done, 0, 0);
    }
}
