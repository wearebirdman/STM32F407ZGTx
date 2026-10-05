#include "ui_chrome.h"
#include "ui.h"
#include "lcd.h"
#include "rtc.h"

#include <stdio.h>

/* 外壳尺寸与颜色定义 */
#define CHROME_STATUS_H        28      // 状态栏高度
#define CHROME_NAV_H           40      // 导航栏高度
#define CHROME_CLK_MS          500     // 时钟轮询周期
#define CHROME_ANIM_MS         250     // 切屏滑动动画时长
#define CHROME_COLOR_STATUS    0x37474F // 状态栏底色
#define CHROME_COLOR_NAV       0xFFFFFF // 导航栏底色
#define CHROME_COLOR_BG        0xF5F5F5 // 页面底色

static lv_obj_t *s_time_label;              // 状态栏时间标签
static lv_obj_t *s_date_label;              // 状态栏日期标签
static lv_timer_t *s_clock_timer;           // 时钟定时器（全局仅建一次）
static uint8_t s_last_sec = 0xFF;           // 上次显示的秒，变化才刷新
static UiPageId_t s_current_page = UI_PAGE_HOME; // 活动页 id

/* 页面表：工厂 + 该页 BACK/NEXT 语义动作 */
static const UiPageCbs_t s_page_table[UI_PAGE_COUNT] = {
    { Ui_CreateHome, Ui_HomeBack, Ui_HomeNext }, // UI_PAGE_HOME：翻页菜单
    { Ui_StubCreate, NULL, NULL },               // UI_PAGE_CALIB
    { Ui_StubCreate, NULL, NULL },               // UI_PAGE_DRAW
    { Ui_StubCreate, NULL, NULL },               // UI_PAGE_LED
    { Ui_StubCreate, NULL, NULL },               // UI_PAGE_KEY
    { Ui_StubCreate, NULL, NULL },               // UI_PAGE_SET
    { Ui_StubCreate, NULL, NULL },               // UI_PAGE_ABOUT
};

/* 星期缩写表：下标 = RTC WeekDay（1=周一…7=周日，取模对齐） */
static const char *const s_week_name[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

/* 时钟 tick：读 RTC，秒变才更新状态栏文本 */
static void Chrome_ClockTick(lv_timer_t *timer)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    char buf[16];

    (void)timer;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN); // 必须先 Time 后 Date（影子寄存器）
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    if (t.Seconds == s_last_sec)
        return;

    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.Hours, t.Minutes, t.Seconds);
    lv_label_set_text(s_time_label, buf);
    snprintf(buf, sizeof(buf), "%02d-%02d %s", d.Month, d.Date, s_week_name[d.WeekDay % 7]);
    lv_label_set_text(s_date_label, buf);
    s_last_sec = t.Seconds;
}

static void Chrome_OnHomeClicked(lv_event_t *e)
{
    (void)e;
    Ui_Home();
}

static void Chrome_OnBackClicked(lv_event_t *e)
{
    (void)e;
    Ui_Back();
}

static void Chrome_OnNextClicked(lv_event_t *e)
{
    (void)e;
    Ui_Next();
}

/* 建导航栏文字按钮 */
static lv_obj_t *Chrome_NavButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *lab;

    lv_obj_set_size(btn, 64, 30);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lab = lv_label_create(btn);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_center(lab);
    return btn;
}

/* 建通用外壳：状态栏 + 内容容器 + 导航栏 */
void Ui_ChromeBuild(lv_obj_t *scr, lv_obj_t **content_out)
{
    lv_obj_t *status;
    lv_obj_t *nav;
    lv_obj_t *content;

    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(scr, lv_color_hex(CHROME_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 状态栏：左时间右日期 */
    status = lv_obj_create(scr);
    lv_obj_remove_style_all(status);
    lv_obj_set_size(status, LV_PCT(100), CHROME_STATUS_H);
    lv_obj_set_style_bg_color(status, lv_color_hex(CHROME_COLOR_STATUS), 0);
    lv_obj_set_style_bg_opa(status, LV_OPA_COVER, 0);

    s_time_label = lv_label_create(status);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_label_set_text(s_time_label, "--:--:--");
    lv_obj_align(s_time_label, LV_ALIGN_LEFT_MID, 6, 0);

    s_date_label = lv_label_create(status);
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_date_label, lv_color_white(), 0);
    lv_label_set_text(s_date_label, "--/-- ---");
    lv_obj_align(s_date_label, LV_ALIGN_RIGHT_MID, -6, 0);

    /* 内容区：flex 生长填满中部 */
    content = lv_obj_create(scr);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_all(content, 4, 0);

    /* 导航栏：Home/Back/Next */
    nav = lv_obj_create(scr);
    lv_obj_remove_style_all(nav);
    lv_obj_set_size(nav, LV_PCT(100), CHROME_NAV_H);
    lv_obj_set_style_bg_color(nav, lv_color_hex(CHROME_COLOR_NAV), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    Chrome_NavButton(nav, "Home", Chrome_OnHomeClicked);
    Chrome_NavButton(nav, "Back", Chrome_OnBackClicked);
    Chrome_NavButton(nav, "Next", Chrome_OnNextClicked);

    *content_out = content;

    /* 新标签立即填充一次；时钟定时器全局只建一次 */
    s_last_sec = 0xFF;
    if (s_clock_timer == NULL)
        s_clock_timer = lv_timer_create(Chrome_ClockTick, CHROME_CLK_MS, NULL);
    Chrome_ClockTick(NULL);
}

/* 建页并左滑加载（旧页动画结束后自动销毁），成为新的活动页 */
void Ui_Push(UiPageId_t id)
{
    lv_obj_t *scr;

    if (id >= UI_PAGE_COUNT)
        return;

    scr = s_page_table[id].create(id);
    if (scr == NULL)
        return;
    s_current_page = id;
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, CHROME_ANIM_MS, 0, true);
}

/* BACK：执行当前页语义动作（菜单页=翻上页，App 页=上一步） */
void Ui_Back(void)
{
    if (s_page_table[s_current_page].on_back != NULL)
        s_page_table[s_current_page].on_back();
}

/* NEXT：执行当前页语义动作（菜单页=翻下页，App 页=下一步） */
void Ui_Next(void)
{
    if (s_page_table[s_current_page].on_next != NULL)
        s_page_table[s_current_page].on_next();
}

/* 清场回主菜单（无动画） */
void Ui_Home(void)
{
    lv_obj_t *scr = s_page_table[UI_PAGE_HOME].create(UI_PAGE_HOME);

    s_current_page = UI_PAGE_HOME;
    if (scr == NULL)
        return;
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

/* 应用 UI 消息：只允许在 lcd_task 上下文调用（LVGL 非线程安全） */
void Ui_ApplyMsg(const UiMsg_t *msg)
{
    if (msg->type >= UI_MSG_COUNT)
        return;

    switch (msg->type)
    {
        case UI_MSG_SET_TEXT:
            break; // 主菜单改版后暂无演示标签，接收端待接入

        case UI_MSG_REQ_CALIB:
            Ui_Push(UI_PAGE_CALIB);
            break;

        default:
            break;
    }
}
