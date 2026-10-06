#include "ui_serial.h"
#include "ui_chrome.h"
#include "ui_msg.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#define SERIAL_MSG_SLOTS 8 // 在途消息槽：生产者写、lcd_task 消费后复制走（调试速率不会回卷覆盖）

/* 环形文本槽：静态存储，UiMsg 传槽指针 */
static char s_ring[SERIAL_MSG_SLOTS][UI_SERIAL_LINE_MAX];
static uint8_t s_ring_head; // 下一写入槽

/* 历史环形缓冲：逻辑下标 0 = 最旧保留行；跨页面访问保留 */
static char s_hist[UI_SERIAL_HIST_LINES][UI_SERIAL_LINE_MAX];
static uint8_t s_hist_cnt;  // 有效行数
static uint8_t s_hist_base; // 逻辑行 0 的物理下标

static char s_disp[UI_SERIAL_HIST_LINES * (UI_SERIAL_LINE_MAX + 1)]; // 全部历史的拼接文本

static uint8_t s_active;    // 页面激活标志（刷新/翻页开关）
static lv_obj_t *s_content; // 内容容器（纵向滚动载体）
static lv_obj_t *s_label;   // 多行文本标签
static lv_obj_t *s_scr;     // 本页屏对象（过滤滞后到达的 DELETE）

/* 按逻辑下标取历史行 */
static const char *Serial_HistAt(uint8_t i)
{
    return s_hist[(uint8_t)((s_hist_base + i) % UI_SERIAL_HIST_LINES)];
}

/* 重建全部历史文本并刷新；视口停在底部则继续跟随最新，翻历史中不打断 */
static void Serial_Refresh(void)
{
    uint8_t follow;
    uint8_t i;

    if (!s_active || s_label == NULL)
        return;
    follow = (lv_obj_get_scroll_bottom(s_content) <= 2); // 追加前先记录是否在底部

    s_disp[0] = '\0';
    for (i = 0; i < s_hist_cnt; i++)
    {
        if (i > 0)
            strcat(s_disp, "\n");
        strcat(s_disp, Serial_HistAt(i));
    }
    lv_label_set_text(s_label, s_disp);
    if (follow)
    {
        lv_obj_update_layout(s_content); // 让新文本高度生效，滚动极值才是准的
        lv_obj_scroll_to_y(s_content, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

/* 任意任务上下文：写环形槽并投递行消息（uart_send / uart_recv 双生产者，临界区保护槽位推进） */
void Ui_SerialPostLine(const char *line)
{
    UiMsg_t msg;
    char *slot;
    size_t n;

    n = strlen(line);
    if (n >= UI_SERIAL_LINE_MAX)
        n = UI_SERIAL_LINE_MAX - 1;

    taskENTER_CRITICAL();
    slot = s_ring[s_ring_head];
    memcpy(slot, line, n);
    slot[n] = '\0';
    s_ring_head = (uint8_t)((s_ring_head + 1) % SERIAL_MSG_SLOTS);
    taskEXIT_CRITICAL();

    msg.type = UI_MSG_SERIAL_LINE;
    msg.data.text = slot;
    Ui_PostMsg(&msg);
}

/* lcd_task 上下文：追加历史环（满则覆盖最旧）并刷新 */
void Ui_SerialPush(const char *line)
{
    uint8_t idx;

    if (s_hist_cnt < UI_SERIAL_HIST_LINES)
    {
        idx = (uint8_t)((s_hist_base + s_hist_cnt) % UI_SERIAL_HIST_LINES);
        s_hist_cnt++;
    }
    else
    {
        idx = s_hist_base; // 环满：写位置即最旧行，逻辑下标整体前移
        s_hist_base = (uint8_t)((s_hist_base + 1) % UI_SERIAL_HIST_LINES);
    }
    snprintf(s_hist[idx], UI_SERIAL_LINE_MAX, "%s", line);
    Serial_Refresh();
}

/* 页面销毁：停刷新（仅当销毁的是本页，避免旧页滞后 DELETE 关掉新页） */
static void Serial_OnDelete(lv_event_t *e)
{
    if (lv_event_get_target(e) != s_scr)
        return;
    s_scr = NULL;
    s_content = NULL;
    s_label = NULL;
    s_active = 0;
}

/* 串口页工厂：内容区开纵向滚动，多行标签随历史增长；BACK/NEXT 翻页浏览 */
lv_obj_t *Ui_SerialCreate(UiPageId_t id)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *content;

    (void)id;
    Ui_ChromeBuild(scr, &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(content, true); // 外壳默认关滚动，本页需要纵向滚动看历史
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_elastic(content, false); // 硬边界：拖到首/末条即停，不越界露空白

    s_scr = scr;
    s_content = content;
    s_active = 1;
    s_label = lv_label_create(content);
    lv_obj_set_width(s_label, LV_PCT(100));
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    Serial_Refresh(); // 用已有历史重建显示并停到底部

    lv_obj_add_event_cb(scr, Serial_OnDelete, LV_EVENT_DELETE, NULL);
    return scr;
}
