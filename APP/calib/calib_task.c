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
 *   3. 逐点：Calib_GetPoint = Display_DrawCross 画十字并同步等应答
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

/* 排空消息队列中的残留事件（非阻塞读到空为止） */
static void Queue_Flush(osMessageQueueId_t q)
{
    TouchEvt_t e;
    while (osMessageQueueGet(q, &e, 0, 0) == osOK) { }
}

/**
 * @brief  校准取点回调（供 Touch_Adjust 调用）：
 *         请求 LCD 画十字 -> 同步等应答 -> 从事件队列等一次按下
 * @retval 1 = 取点成功, 0 = 失败/超时（Touch_Adjust 将中止）
 */
static uint8_t Calib_GetPoint(uint16_t x, uint16_t y,
                              uint16_t *raw_x, uint16_t *raw_y)
{
    TouchEvt_t e;

    /* 画十字：同步等待服务层应答（非持有者/未就绪会立即回 FORBIDDEN） */
    if (Display_DrawCross(DISP_CLIENT_CALIB, x, y, RED) != DISP_OK)
        return 0;

    /* 等用户按下：从队列阻塞取（10s 超时），坐标为按下瞬间的原始 AD 值 */
    if (osMessageQueueGet(q_touch_evtHandle, &e, 0, 10000) != osOK)
        return 0;

    *raw_x = e.raw_x;
    *raw_y = e.raw_y;
    return 1;
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

        /* 3. 排空会话前残留按下，逐点采集 */
        Queue_Flush(q_touch_evtHandle);

        uint8_t ok = (Touch_Adjust(LCD_WIDTH, LCD_HEIGHT, 20, Calib_GetPoint) == TOUCH_OK);

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
