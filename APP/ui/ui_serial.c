#include "ui_serial.h"
#include "ui_chrome.h"
#include "ui_msg.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#define SERIAL_MSG_SLOTS  8 /* 在途消息槽：生产者写、Lcd_Disp 任务消费后复制走（调试速率不会回卷覆盖） */
#define SERIAL_BOTTOM_TOL 2 /* 距底部不超过该像素数视为“跟随最新” */

/* 环形文本槽：静态存储，UiMsg 传槽指针 */
static char s_Ring[SERIAL_MSG_SLOTS][UI_SERIAL_LINE_MAX];
static uint8_t s_RingHead; /* 下一写入槽 */

/* 历史环形缓冲：逻辑下标 0 = 最旧保留行；跨页面访问保留 */
static char s_Hist[UI_SERIAL_HIST_LINES][UI_SERIAL_LINE_MAX];
static uint8_t s_HistCnt;  /* 有效行数 */
static uint8_t s_HistBase; /* 逻辑行 0 的物理下标 */

static char s_Disp[UI_SERIAL_HIST_LINES * (UI_SERIAL_LINE_MAX + 1)]; /* 全部历史的拼接文本 */

static uint8_t s_Active;    /* 页面激活标志（刷新/翻页开关） */
static lv_obj_t *s_Content; /* 内容容器（纵向滚动载体） */
static lv_obj_t *s_Label;   /* 多行文本标签 */
static lv_obj_t *s_Scr;     /* 本页屏对象（过滤滞后到达的 DELETE） */

/* 按逻辑下标取历史行 */
static const char *Serial_HistAt(uint8_t i)
{
    return s_Hist[(uint8_t)((s_HistBase + i) % UI_SERIAL_HIST_LINES)];
}

/* 重建全部历史文本并刷新；视口停在底部则继续跟随最新，翻历史中不打断 */
static void Serial_Refresh(void)
{
    if (!s_Active || s_Label == NULL)
        return;
    uint8_t follow = (lv_obj_get_scroll_bottom(s_Content) <= SERIAL_BOTTOM_TOL); /* 追加前先记录是否在底部 */

    s_Disp[0] = '\0';
    for (uint8_t i = 0; i < s_HistCnt; i++)
    {
        if (i > 0)
            strcat(s_Disp, "\n");
        strcat(s_Disp, Serial_HistAt(i));
    }
    lv_label_set_text(s_Label, s_Disp);
    if (follow)
    {
        lv_obj_update_layout(s_Content); /* 让新文本高度生效，滚动极值才是准的 */
        lv_obj_scroll_to_y(s_Content, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

/* 任意任务上下文：写环形槽并投递行消息（Uart_Send / Uart_Recv 双生产者，临界区保护槽位推进） */
void Ui_SerialPostLine(const char *line)
{
    size_t n = strlen(line);

    if (n >= UI_SERIAL_LINE_MAX)
        n = UI_SERIAL_LINE_MAX - 1;

    taskENTER_CRITICAL();
    char *slot = s_Ring[s_RingHead];
    memcpy(slot, line, n);
    slot[n] = '\0';
    s_RingHead = (uint8_t)((s_RingHead + 1) % SERIAL_MSG_SLOTS);
    taskEXIT_CRITICAL();

    UiMsg_t msg;
    msg.type = UI_MSG_SERIAL_LINE;
    msg.data.text = slot;
    Ui_PostMsg(&msg);
}

/* Lcd_Disp 任务上下文：追加历史环（满则覆盖最旧）并刷新 */
void Ui_SerialPush(const char *line)
{
    uint8_t idx;

    if (s_HistCnt < UI_SERIAL_HIST_LINES)
    {
        idx = (uint8_t)((s_HistBase + s_HistCnt) % UI_SERIAL_HIST_LINES);
        s_HistCnt++;
    }
    else
    {
        idx = s_HistBase; /* 环满：写位置即最旧行，逻辑下标整体前移 */
        s_HistBase = (uint8_t)((s_HistBase + 1) % UI_SERIAL_HIST_LINES);
    }
    snprintf(s_Hist[idx], UI_SERIAL_LINE_MAX, "%s", line);
    Serial_Refresh();
}

/* 页面销毁：停刷新（仅当销毁的是本页，避免旧页滞后 DELETE 关掉新页） */
static void Serial_OnDelete(lv_event_t *e)
{
    if (lv_event_get_target(e) != s_Scr)
        return;
    s_Scr = NULL;
    s_Content = NULL;
    s_Label = NULL;
    s_Active = 0;
}

/* 串口页工厂：内容区开纵向滚动，多行标签随历史增长；BACK/NEXT 翻页浏览 */
lv_obj_t *Ui_SerialCreate(UiPageId_t id)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *content;

    (void)id;
    Ui_ChromeBuild(scr, &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(content, true); /* 外壳默认关滚动，本页需要纵向滚动看历史 */
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_elastic(content, false); /* 硬边界：拖到首/末条即停，不越界露空白 */

    s_Scr = scr;
    s_Content = content;
    s_Active = 1;
    s_Label = lv_label_create(content);
    lv_obj_set_width(s_Label, LV_PCT(100));
    lv_obj_set_style_text_font(s_Label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_Label, LV_LABEL_LONG_WRAP);
    Serial_Refresh(); /* 用已有历史重建显示并停到底部 */

    lv_obj_add_event_cb(scr, Serial_OnDelete, LV_EVENT_DELETE, NULL);
    return scr;
}
