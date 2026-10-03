/*
 * draw_task.c —— 画板会话任务（参考 calib_task 的独占会话框架）
 *
 * 流程：home 格 2 触发 -> sem_draw -> Acquire 画板界面 -> 消费 q_touch_evt
 * 笔画流 -> 导航栏 HOME 退出 -> Release + 投 UIEVT_DRAW_DONE 回 home。
 *
 * 触摸流约定（q_touch_evt 泛化后）：
 *   DOWN=起笔/导航命中（仅 DOWN 触发导航动作，防 HOLD 连发误触）；
 *   HOLD=行笔坐标流（15ms 一条，位移 < DRAW_MOVE_MIN 丢弃防抖）；
 *   UP  =收笔提交。
 *
 * 笔画历史在任务私有 RAM（不进消息体）：undo 保留历史可 redo，
 * 新笔画落下即截断 redo 分支。重放 = PostAcquire 清屏 + 逐笔 PostDrawLine，
 * 单队列 FIFO 保证"先清屏后重放"的顺序，全程 fire-and-forget 不等应答。
 *
 * 常驻导航栏（本设计变更）：内容区 = 0..HOME_NAV_Y-1，导航键命中测试
 * 用 ui.h 的 HOME_* 宏（与渲染同一事实源）：HOME=退出 < =撤销 > =重做。
 */

#include <string.h>          /* memmove（历史满丢最旧笔画） */
#include "draw_task.h"
#include "touch_task.h"      /* TouchEvt_t / q_touch_evtHandle */
#include "home_task.h"       /* UiEvt_t / q_ui_evtHandle（DRAW_DONE 回报） */
#include "touch.h"           /* TOUCH_EVENT_* */
#include "display.h"
#include "ui.h"              /* HOME_NAV_Y / HOME_NAV_W（导航命中测试） */

#define DRAW_STROKES_MAX   12u    /* 历史笔画上限（undo/redo 栈深度） */
#define DRAW_PTS_MAX       24u    /* 单笔画记录点上限（防抖后约 70px 一笔） */
#define DRAW_MOVE_MIN      3u     /* HOLD 位移阈值（px）：防抖 + 降消息流量 */
#define DRAW_PEN           BLACK  /* 笔色：白底画布黑笔 */

typedef struct { uint16_t x, y; } DrawPt_t;
typedef struct { uint8_t cnt; DrawPt_t pt[DRAW_PTS_MAX]; } DrawStroke_t;

/* ---- 会话私有状态（仅 draw_proc 上下文访问）---- */
static DrawStroke_t s_hist[DRAW_STROKES_MAX]; /* 已提交笔画 */
static uint8_t s_hist_cnt;                    /* 历史笔画数 */
static uint8_t s_shown_cnt;                   /* 当前屏上已绘制的笔画数 */
static DrawStroke_t s_cur;                    /* 正在进行的笔画 */
static uint8_t s_cur_active;                  /* 1=有笔画进行中 */

/* 排空消息队列中的残留事件（非阻塞读到空为止） */
static void Queue_Flush(osMessageQueueId_t q)
{
    TouchEvt_t e;
    while (osMessageQueueGet(q, &e, 0, 0) == osOK) { }
}

/* 绘制一条历史笔画（单点=大点，多点=逐段连线），全部异步投递 */
static void Draw_Stroke(const DrawStroke_t *st)
{
    if (st->cnt == 0)
        return;
    Display_PostDraw(DISP_CLIENT_DRAW, DISP_DRAW_POINT,
                     st->pt[0].x, st->pt[0].y, DRAW_PEN);
    for (uint8_t i = 1; i < st->cnt; i++)
        Display_PostDrawLine(DISP_CLIENT_DRAW,
                             st->pt[i - 1].x, st->pt[i - 1].y,
                             st->pt[i].x, st->pt[i].y, DRAW_PEN);
}

/* 撤销：回退一画 -> 清屏重放剩余（redo 分支保留在 s_hist 中） */
static void Draw_Undo(void)
{
    if (s_shown_cnt == 0)
        return;
    s_shown_cnt--;
    Display_PostAcquire(DISP_CLIENT_DRAW, DISP_SCREEN_DRAW, 0);  /* 清屏 */
    for (uint8_t i = 0; i < s_shown_cnt; i++)
        Draw_Stroke(&s_hist[i]);
}

/* 重做：前推一画 -> 仅补画该笔（无需清屏） */
static void Draw_Redo(void)
{
    if (s_shown_cnt >= s_hist_cnt)
        return;
    Draw_Stroke(&s_hist[s_shown_cnt]);
    s_shown_cnt++;
}

/* 收笔提交：新笔画截断 redo 分支；历史满则丢最旧一笔 */
static void Draw_CommitStroke(void)
{
    if (s_cur.cnt == 0)
        return;
    s_hist_cnt = s_shown_cnt;                 /* 截断未重做的 redo 分支 */
    if (s_hist_cnt >= DRAW_STROKES_MAX)
    {
        memmove(&s_hist[0], &s_hist[1], sizeof(DrawStroke_t) * (DRAW_STROKES_MAX - 1));
        s_hist_cnt = DRAW_STROKES_MAX - 1;
    }
    s_hist[s_hist_cnt] = s_cur;
    s_hist_cnt++;
    s_shown_cnt = s_hist_cnt;                 /* 该笔已实时绘出 */
    s_cur.cnt = 0;
}

/* 内容区坐标有效性（未校准时 x/y 为原始 AD 值，越界即丢弃） */
static uint8_t Draw_InCanvas(const TouchEvt_t *e)
{
    return (e->x < LCD_WIDTH && e->y < HOME_NAV_Y);
}

/**
 * @brief 画板任务主循环：阻塞等触发 -> 一次完整绘画会话
 */
void draw_proc(void *argument)
{
    TouchEvt_t e;
    UiEvt_t done = { .type = UIEVT_DRAW_DONE };

    for (;;)
    {
        /* 1. 阻塞等画板触发（home 让屏后 release） */
        if (osSemaphoreAcquire(sem_drawHandle, osWaitForever) != osOK)
            continue;

        /* 2. 接管画板界面（失败也必须回报，否则 home 卡等待态） */
        if (Display_Acquire(DISP_CLIENT_DRAW, DISP_SCREEN_DRAW, 0) != DISP_OK)
        {
            (void)osMessageQueuePut(q_ui_evtHandle, &done, 0, 0);
            continue;
        }

        /* 3. 会话初始化：新画布清空历史，排空残留触摸 */
        s_hist_cnt = s_shown_cnt = 0;
        s_cur.cnt = 0;
        s_cur_active = 0;
        Queue_Flush(q_touch_evtHandle);

        /* 4. 笔画流循环（仅导航栏 HOME 键退出，无超时——画布常驻会话） */
        for (;;)
        {
            if (osMessageQueueGet(q_touch_evtHandle, &e, 0, osWaitForever) != osOK)
                continue;

            /* ---- 导航栏：只认按下边沿，HOLD 连发防误触 ---- */
            if (e.event == TOUCH_EVENT_PRESS_DOWN &&
                e.x < LCD_WIDTH && e.y >= HOME_NAV_Y && e.y < LCD_HEIGHT)
            {
                uint8_t key = e.x / HOME_NAV_W;   /* 0=HOME 1=< 2=> */
                if (key == 0)
                    break;                        /* 退出会话 */
                else if (key == 1)
                    Draw_Undo();
                else if (key == 2)
                    Draw_Redo();
                continue;
            }

            if (!Draw_InCanvas(&e))
                continue;                         /* 越界/导航区 HOLD：忽略 */

            /* ---- 内容区：DOWN 起笔，HOLD 行笔，UP 收笔 ---- */
            if (e.event == TOUCH_EVENT_PRESS_UP)
            {
                if (s_cur_active)
                {
                    Draw_CommitStroke();
                    s_cur_active = 0;
                }
            }
            else if (e.event == TOUCH_EVENT_PRESS_DOWN)
            {
                if (s_cur_active)                 /* UP 丢失（队列溢出）兜底 */
                    Draw_CommitStroke();
                s_cur.cnt = 1;
                s_cur.pt[0].x = e.x;
                s_cur.pt[0].y = e.y;
                s_cur_active = 1;
                Display_PostDraw(DISP_CLIENT_DRAW, DISP_DRAW_POINT,
                                 e.x, e.y, DRAW_PEN);
            }
            else /* HOLD：行笔，位移达标才记点连线 */
            {
                if (!s_cur_active || s_cur.cnt == 0)
                    continue;                     /* DOWN 丢失：无笔可续，忽略 */
                DrawPt_t *last = &s_cur.pt[s_cur.cnt - 1];
                uint16_t dx = (e.x > last->x) ? e.x - last->x : last->x - e.x;
                uint16_t dy = (e.y > last->y) ? e.y - last->y : last->y - e.y;
                if (dx < DRAW_MOVE_MIN && dy < DRAW_MOVE_MIN)
                    continue;                     /* 抖动过滤 */
                Display_PostDrawLine(DISP_CLIENT_DRAW,
                                     last->x, last->y, e.x, e.y, DRAW_PEN);
                if (s_cur.cnt < DRAW_PTS_MAX)
                {
                    s_cur.pt[s_cur.cnt].x = e.x;
                    s_cur.pt[s_cur.cnt].y = e.y;
                    s_cur.cnt++;
                }
                /* 点数已满：屏上继续画，重放时该笔截断（可接受的降级） */
            }
        }

        /* 5. 收拢进行中的笔画，归还屏幕并回报 home */
        if (s_cur_active)
        {
            Draw_CommitStroke();
            s_cur_active = 0;
        }
        Display_Release(DISP_CLIENT_DRAW);
        (void)osMessageQueuePut(q_ui_evtHandle, &done, 0, 0);
    }
}
