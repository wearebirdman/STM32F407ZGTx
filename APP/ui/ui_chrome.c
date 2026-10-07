#include "ui_chrome.h"
#include "lcd.h"
#include "rtc.h"
#include "ui.h"
#include "ui_calib.h"
#include "ui_draw.h"
#include "ui_osc.h"
#include "ui_serial.h"

#include <stdio.h>

/* 外壳颜色与运行参数定义（尺寸宏见 ui_chrome.h） */
#define CHROME_CLK_MS         500      /* 时钟轮询周期 */
#define CHROME_ANIM_MS        250      /* 切屏滑动动画时长 */
#define CHROME_WEEK_DAY_NUM   7        /* 一周天数（s_WeekName 表长，取模对齐） */
#define CHROME_COLOR_STATUS   0x37474F /* 状态栏底色 */
#define CHROME_COLOR_NAV      0xFFFFFF /* 导航栏底色 */
#define CHROME_COLOR_BG       0xF5F5F5 /* 页面底色 */

static lv_obj_t *s_TimeLabel;                   /* 状态栏时间标签 */
static lv_obj_t *s_DateLabel;                   /* 状态栏日期标签 */
static lv_timer_t *s_ClockTimer;                /* 时钟定时器（全局仅建一次） */
static uint8_t s_LastSec = 0xFF;                /* 上次显示的秒，变化才刷新 */
static UiPageId_t s_CurrentPage = UI_PAGE_HOME; /* 活动页 id */
static lv_obj_t *s_BackLab;                     /* 导航栏 BACK 按钮文字（页面定制） */
static lv_obj_t *s_NextLab;                     /* 导航栏 NEXT 按钮文字（页面定制） */

/* 页面表：工厂 + 该页 BACK/NEXT 语义动作 + 按钮文字（NULL=默认） */
static const UiPageCbs_t s_PageTable[UI_PAGE_COUNT] = {
    { Ui_CreateHome,   Ui_HomeBack,   Ui_HomeNext,   NULL,      NULL    }, /* UI_PAGE_HOME：翻页菜单 */
    { Ui_CalibCreate,  Ui_CalibBack,  Ui_CalibNext,  NULL,      NULL    }, /* UI_PAGE_CALIB：四点校准 */
    { Ui_DrawCreate,   Ui_DrawBack,   Ui_DrawNext,   NULL,      NULL    }, /* UI_PAGE_DRAW：画板（BACK=撤销 NEXT=换色） */
    { Ui_OscCreate,    Ui_OscBack,    Ui_OscNext,    "Rate:50", "Pause" }, /* UI_PAGE_OSC：示波器（BACK=采样率 NEXT=暂停/播放） */
    { Ui_SerialCreate, NULL,          NULL,          NULL,      NULL    }, /* UI_PAGE_SERIAL：串口助手（BACK/NEXT 无动作） */
    { Ui_StubCreate,   NULL,          NULL,          NULL,      NULL    }, /* UI_PAGE_SET：占位 */
    { Ui_StubCreate,   NULL,          NULL,          NULL,      NULL    }, /* UI_PAGE_ABOUT：占位 */
};

/* 星期缩写表：下标 = RTC WeekDay（1=周一…7=周日，取模对齐） */
static const char *const s_WeekName[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

/* 时钟 tick：读 RTC，秒变才更新状态栏文本 */
static void Chrome_ClockTick(lv_timer_t *timer)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    char buf[16];

    (void)timer;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN); /* 必须先 Time 后 Date（影子寄存器） */
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    if (t.Seconds == s_LastSec)
        return;

    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.Hours, t.Minutes, t.Seconds);
    lv_label_set_text(s_TimeLabel, buf);
    snprintf(buf, sizeof(buf), "%02d-%02d %s", d.Month, d.Date, s_WeekName[d.WeekDay % CHROME_WEEK_DAY_NUM]);
    lv_label_set_text(s_DateLabel, buf);
    s_LastSec = t.Seconds;
}

/* HOME 按钮点击：回主菜单 */
static void Chrome_OnHomeClicked(lv_event_t *e)
{
    (void)e;
    Ui_Home();
}

/* BACK 按钮点击：执行当前页 BACK 语义 */
static void Chrome_OnBackClicked(lv_event_t *e)
{
    (void)e;
    Ui_Back();
}

/* NEXT 按钮点击：执行当前页 NEXT 语义 */
static void Chrome_OnNextClicked(lv_event_t *e)
{
    (void)e;
    Ui_Next();
}

/* 建导航栏文字按钮，输出内部文字标签指针供页面定制 */
static lv_obj_t *Chrome_NavButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb, lv_obj_t **lab_out)
{
    lv_obj_t *btn = lv_button_create(parent);

    lv_obj_set_size(btn, 64, 30);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_center(lab);
    *lab_out = lab;
    return btn;
}

/* 建通用外壳：状态栏 + 内容容器 + 导航栏 */
void Ui_ChromeBuild(lv_obj_t *scr, lv_obj_t **content_out)
{
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    /* 三栏高度正好等于屏高，不需要滚动；关掉可省去拖动时的回弹位移 */
    lv_obj_set_scrollable(scr, false);
    lv_obj_set_style_bg_color(scr, lv_color_hex(CHROME_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 状态栏：左时间右日期 */
    lv_obj_t *status = lv_obj_create(scr);
    lv_obj_remove_style_all(status);
    lv_obj_set_size(status, LV_PCT(100), CHROME_STATUS_H);
    lv_obj_set_style_bg_color(status, lv_color_hex(CHROME_COLOR_STATUS), 0);
    lv_obj_set_style_bg_opa(status, LV_OPA_COVER, 0);

    s_TimeLabel = lv_label_create(status);
    lv_obj_set_style_text_font(s_TimeLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_TimeLabel, lv_color_white(), 0);
    lv_label_set_text(s_TimeLabel, "--:--:--");
    lv_obj_align(s_TimeLabel, LV_ALIGN_LEFT_MID, 6, 0);

    s_DateLabel = lv_label_create(status);
    lv_obj_set_style_text_font(s_DateLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_DateLabel, lv_color_white(), 0);
    lv_label_set_text(s_DateLabel, "--/-- ---");
    lv_obj_align(s_DateLabel, LV_ALIGN_RIGHT_MID, -6, 0);

    /* 内容区：flex 生长填满中部（不可滚动，避免拖动触发回弹改变子对象原点） */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_scrollable(content, false);
    lv_obj_set_style_pad_all(content, 4, 0);

    /* 导航栏：Home/Back/Next */
    lv_obj_t *nav = lv_obj_create(scr);
    lv_obj_remove_style_all(nav);
    lv_obj_set_size(nav, LV_PCT(100), CHROME_NAV_H);
    lv_obj_set_style_bg_color(nav, lv_color_hex(CHROME_COLOR_NAV), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    Chrome_NavButton(nav, "Home", Chrome_OnHomeClicked, NULL);
    Chrome_NavButton(nav, "Back", Chrome_OnBackClicked, &s_BackLab);
    Chrome_NavButton(nav, "Next", Chrome_OnNextClicked, &s_NextLab);

    /* 按当前页表项覆盖 BACK/NEXT 按钮文字（NULL=保持默认） */
    if (s_PageTable[s_CurrentPage].back_label != NULL)
        lv_label_set_text(s_BackLab, s_PageTable[s_CurrentPage].back_label);
    if (s_PageTable[s_CurrentPage].next_label != NULL)
        lv_label_set_text(s_NextLab, s_PageTable[s_CurrentPage].next_label);

    *content_out = content;

    /* 新标签立即填充一次；时钟定时器全局只建一次 */
    s_LastSec = 0xFF;
    if (s_ClockTimer == NULL)
        s_ClockTimer = lv_timer_create(Chrome_ClockTick, CHROME_CLK_MS, NULL);
    Chrome_ClockTick(NULL);
}

/* 运行时改导航按钮文字（页面切档等状态实时上钮；NULL=保持） */
void Ui_ChromeSetNavLabels(const char *back, const char *next)
{
    if (back != NULL && s_BackLab != NULL)
        lv_label_set_text(s_BackLab, back);
    if (next != NULL && s_NextLab != NULL)
        lv_label_set_text(s_NextLab, next);
}

/* 建页并左滑加载（旧页动画结束后自动销毁），成为新的活动页
 * 注意：s_CurrentPage 必须先于 create 赋值——工厂内建壳按页 id 取导航按钮定制文字 */
void Ui_Push(UiPageId_t id)
{
    if (id >= UI_PAGE_COUNT)
        return;

    s_CurrentPage = id;
    lv_obj_t *scr = s_PageTable[id].create(id);
    if (scr == NULL)
        return;
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, CHROME_ANIM_MS, 0, true);
}

/* BACK：执行当前页语义动作（菜单页=翻上页，App 页=上一步） */
void Ui_Back(void)
{
    if (s_PageTable[s_CurrentPage].on_back != NULL)
        s_PageTable[s_CurrentPage].on_back();
}

/* NEXT：执行当前页语义动作（菜单页=翻下页，App 页=下一步） */
void Ui_Next(void)
{
    if (s_PageTable[s_CurrentPage].on_next != NULL)
        s_PageTable[s_CurrentPage].on_next();
}

/* 清场回主菜单（无动画）；页 id 先于建页赋值，同 Ui_Push */
void Ui_Home(void)
{
    s_CurrentPage = UI_PAGE_HOME;
    lv_obj_t *scr = s_PageTable[UI_PAGE_HOME].create(UI_PAGE_HOME);
    if (scr == NULL)
        return;
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

/* 应用 UI 消息：只允许在 Lcd_Disp 任务上下文调用（LVGL 非线程安全） */
void Ui_ApplyMsg(const UiMsg_t *msg)
{
    if (msg->type >= UI_MSG_COUNT)
        return;

    switch (msg->type)
    {
        case UI_MSG_SET_TEXT:
            break; /* 主菜单改版后暂无演示标签，接收端待接入 */

        case UI_MSG_REQ_CALIB:
            Ui_Push(UI_PAGE_CALIB);
            break;

        case UI_MSG_SERIAL_LINE:
            Ui_SerialPush(msg->data.text);
            break;

        default:
            break;
    }
}
