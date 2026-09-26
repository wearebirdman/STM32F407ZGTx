/*
 * home_task.c —— 主界面属主任务（常驻交互界面）
 *
 * 属主统一模型下的第一个常驻应用：开机 Acquire 主界面成为其属主，
 * 消费 UI 事件队列做网格命中测试；触发校准时"让屏 -> 通知 calib ->
 * 等 CALIB_DONE -> 重新接管"，跳转关系由 home<->calib 显式握手，
 * 服务层不感知业务。
 *
 * 本任务不碰任何硬件：渲染经 Display_* 消息队列由 lcd_task 执行，
 * 触摸事件由 touch_task 广播进 q_ui_evt。命中测试用 display.h 的
 * HOME_* 几何宏（与渲染同一事实源）。
 */

#include "home_task.h"
#include "calib_task.h"    /* sem_calibHandle */
#include "display.h"       /* Display_* 客户端 API */
#include "ui.h"            /* HOME_* 几何宏（命中测试） */

#include <stdio.h>

typedef enum {
    HOME_UI = 0,      /* 正常主界面：消费触摸做命中测试 */
    HOME_IN_CALIB,    /* 校准进行中：已让屏，等 CALIB_DONE */
} HomeState_t;

static HomeState_t s_state = HOME_UI;

/* 排空 UI 事件队列残留（非阻塞读到空为止） */
static void Home_FlushQueue(void)
{
    UiEvt_t e;
    while (osMessageQueueGet(q_ui_evtHandle, &e, 0, 0) == osOK) { }
}

/* 让出主界面并触发校准：先 Release（使 calib Acquire 必成），再 release
 * 信号量，进入等待态。即便 Release 超时未生效，calib 的 Acquire 会回
 * BUSY 并立即回报 CALIB_DONE，home 重新接管，不会死锁。 */
static void Home_StartCalibration(void)
{
    if (Display_Release(DISP_CLIENT_HOME) != DISP_OK)
        printf("home: release failed, calib will bounce back\r\n");

    s_state = HOME_IN_CALIB;
    osSemaphoreRelease(sem_calibHandle);
}

/* 触摸命中测试（仅 HOME_UI 态调用）：导航栏 -> 网格 */
static void Home_HandleTouch(UiEvt_t *e)
{
    /* 未校准时 x/y 为原始 AD 值（可达 4095），先做越界丢弃 */
    if (e->x >= LCD_WIDTH || e->y >= LCD_HEIGHT)
        return;

    if (e->y >= HOME_NAV_Y)                 /* 底部导航栏 */
    {
        uint8_t key = e->x / HOME_NAV_W;    /* 0=HOME 1=< 2=> */
        if (key == 0)
            Display_Acquire(DISP_CLIENT_HOME, DISP_SCREEN_HOME, 0);  /* 重绘整屏 */
        else
            printf("home: nav key %u (page switch reserved)\r\n", key);
        return;
    }

    uint8_t col = e->x / HOME_CELL_W;
    uint8_t row = e->y / HOME_CELL_H;
    uint8_t idx = row * HOME_COLS + col;    /* 0..11，格 1 即 idx 0 */

    if (idx == 0)
        Home_StartCalibration();            /* 触摸校准按钮 */
    else
        printf("home: cell %u not implemented\r\n", idx + 1);
}

void home_proc(void *argument)
{
    UiEvt_t e;

    /* 开机接管主界面（覆盖服务层的 DEFAULT 占位屏）；失败则退避重试 */
    while (Display_Acquire(DISP_CLIENT_HOME, DISP_SCREEN_HOME, 0) != DISP_OK)
        osDelay(100);

    for (;;)
    {
        if (osMessageQueueGet(q_ui_evtHandle, &e, 0, osWaitForever) != osOK)
            continue;

        if (s_state == HOME_UI)
        {
            switch (e.type)
            {
                case UIEVT_TOUCH:      Home_HandleTouch(&e);     break;
                case UIEVT_KICK_CALIB: Home_StartCalibration();  break;
                default: break;          /* 游离 CALIB_DONE：忽略 */
            }
        }
        else /* HOME_IN_CALIB：只等校准结束，触摸丢弃 */
        {
            if (e.type == UIEVT_CALIB_DONE)
            {
                Home_FlushQueue();        /* 丢弃校准期间积压的触摸/事件 */
                Display_Acquire(DISP_CLIENT_HOME, DISP_SCREEN_HOME, 0);
                s_state = HOME_UI;
            }
        }
    }
}
