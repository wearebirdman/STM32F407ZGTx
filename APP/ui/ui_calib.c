#include "ui_calib.h"
#include "ui_chrome.h"
#include "lcd.h"

#include <math.h>
#include <stdio.h>

/* 校准参数定义 */
#define CALIB_MARGIN     20   // 校准点距屏幕边缘
#define CALIB_POINT_NUM  4    // 校准点数（四角）
#define CALIB_TIMEOUT_MS 5000 // 单点采集超时
#define CALIB_SHOW_MS    1000 // 成功展示时长
#define CALIB_FAIL_MS    1500 // 失败展示时长
#define CALIB_TICK_MS    50   // 状态机周期
#define CALIB_COLOR_WAIT 0xE53935 // 待采集十字颜色
#define CALIB_COLOR_OK   0x43A047 // 完成十字颜色

/* 状态机状态定义 */
typedef enum {
    CALIB_STATE_RUN = 0, // 采集中
    CALIB_STATE_DONE,    // 4 点齐备，待计算
    CALIB_STATE_SHOW,    // 成功展示
    CALIB_STATE_FAIL,    // 失败展示
} CalibState_t;

static uint8_t s_active;               // 页面激活标志（事件喂入开关）
static uint8_t s_step;                 // 当前校准点 0..3
static CalibState_t s_state;           // 状态机状态
static uint16_t s_pos[CALIB_POINT_NUM][2]; // 各点原始 AD 值 [x,y]

/* 校准点屏幕坐标：内容区四角（避开状态栏/导航栏），顺序左上、右上、左下、右下 */
static const uint16_t s_tgt[CALIB_POINT_NUM][2] = {
    { CALIB_MARGIN, CHROME_STATUS_H + CALIB_MARGIN },                    // 左上
    { LCD_WIDTH - CALIB_MARGIN, CHROME_STATUS_H + CALIB_MARGIN },        // 右上
    { CALIB_MARGIN, LCD_HEIGHT - CHROME_NAV_H - CALIB_MARGIN },          // 左下
    { LCD_WIDTH - CALIB_MARGIN, LCD_HEIGHT - CHROME_NAV_H - CALIB_MARGIN }, // 右下
};

static uint16_t s_last_raw_x;          // 按住期间最新原始 AD X
static uint16_t s_last_raw_y;
static uint8_t s_pressing;             // 本地按下标志
static uint8_t s_swap_retry;           // 轴交换重试已用标志
static uint32_t s_step_start;          // 当前步骤起始时基
static lv_obj_t *s_hline;              // 十字横线
static lv_obj_t *s_vline;              // 十字竖线
static lv_obj_t *s_info_label;         // 提示标签
static lv_timer_t *s_timer;            // 状态机定时器
static lv_obj_t *s_scr;                // 本页屏对象（用于过滤滞后到达的 DELETE）

/* 设置十字颜色 */
static void Calib_SetColor(uint32_t color)
{
    lv_color_t c = lv_color_hex(color);

    lv_obj_set_style_bg_color(s_hline, c, 0);
    lv_obj_set_style_bg_color(s_vline, c, 0);
}

/* 显示第 step 个校准点并重置其超时预算 */
static void Calib_ShowTarget(uint8_t step)
{
    char buf[32];

    s_step_start = lv_tick_get();
    s_pressing = 0;
    lv_obj_align(s_hline, LV_ALIGN_TOP_LEFT, (int32_t)s_tgt[step][0] - 15, (int32_t)s_tgt[step][1] - 1);
    lv_obj_align(s_vline, LV_ALIGN_TOP_LEFT, (int32_t)s_tgt[step][0] - 1, (int32_t)s_tgt[step][1] - 15);
    Calib_SetColor(CALIB_COLOR_WAIT);
    snprintf(buf, sizeof(buf), "Point %d/4 - Tap cross", step + 1);
    lv_label_set_text(s_info_label, buf);
}

/* 进入失败展示：显示原因后自动回主菜单 */
static void Calib_SetFail(const char *text)
{
    lv_label_set_text(s_info_label, text);
    s_state = CALIB_STATE_FAIL;
    s_step_start = lv_tick_get();
}

/* 重新采集全部点（采样质量差/轴交换后使用） */
static void Calib_Restart(void)
{
    s_step = 0;
    s_state = CALIB_STATE_RUN;
    s_swap_retry = 0; // 交换后重试机会重新计
    Calib_ShowTarget(0);
}

/* 4 点齐备：计算校准参数，系数异常自动交换轴重采一次 */
static void Calib_Finish(void)
{
    TouchCal_t cal;
    char buf[32];

    Touch_GetCalibration(&cal); // 预取当前轴交换状态（CalcCalibrationEx 保留 swap 不改）
    if (Touch_CalcCalibrationEx(s_pos, s_tgt, &cal) != TOUCH_OK)
    {
        Calib_SetFail("Bad points!");
        return;
    }

    /* 系数异常 = 触摸屏轴与屏幕反向：翻转 cmd 映射后整体重采（仅一次机会） */
    if (fabsf(cal.xfac) > 2.0f || fabsf(cal.yfac) > 2.0f)
    {
        TouchCal_t cur;

        if (s_swap_retry)
        {
            Calib_SetFail("Calib failed!");
            return;
        }
        s_swap_retry = 1;
        Touch_GetCalibration(&cur);
        cur.swap = !cur.swap;
        Touch_SetCalibration(&cur); // 只更新轴映射，系数保持原状
        Calib_Restart();
        return;
    }

    s_swap_retry = 0;
    Touch_SetCalibration(&cal);
    if (Touch_SaveCalibration() != TOUCH_OK)
    {
        Calib_SetFail("Save failed!");
        return;
    }

    snprintf(buf, sizeof(buf), "OK  fx=%d fy=%d", (int)(cal.xfac * 100), (int)(cal.yfac * 100));
    lv_label_set_text(s_info_label, buf);
    Calib_SetColor(CALIB_COLOR_OK);
    s_state = CALIB_STATE_SHOW;
    s_step_start = lv_tick_get();
}

/* 状态机 tick：超时检测 + 计算 + 展示计时 */
static void Calib_Tick(lv_timer_t *timer)
{
    (void)timer;
    switch (s_state)
    {
        case CALIB_STATE_RUN:
            if (lv_tick_elaps(s_step_start) >= CALIB_TIMEOUT_MS)
                Calib_SetFail("Timeout!");
            break;

        case CALIB_STATE_DONE:
            Calib_Finish();
            break;

        case CALIB_STATE_SHOW:
            if (lv_tick_elaps(s_step_start) >= CALIB_SHOW_MS)
                Ui_Home();
            break;

        case CALIB_STATE_FAIL:
            if (lv_tick_elaps(s_step_start) >= CALIB_FAIL_MS)
                Ui_Home();
            break;

        default:
            break;
    }
}

/* 页面销毁（切离校准页时由 auto_del 触发）：停喂事件、删定时器 */
static void Calib_OnDelete(lv_event_t *e)
{
    /* 旧页动画滞后 DELETE 时不能误删新页的定时器，只有本页才回收 */
    if (lv_event_get_target(e) != s_scr)
        return;
    s_scr = NULL;
    s_active = 0;
    if (s_timer != NULL)
    {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
}

/* 校准页工厂：四角十字 + 状态机 */
lv_obj_t *Ui_CalibCreate(UiPageId_t id)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *content;

    (void)id;
    Ui_ChromeBuild(scr, &content);

    /* 十字挂在 scr 上（屏幕坐标系），FLOATING 脱离 flex 布局 */
    s_hline = lv_obj_create(scr);
    lv_obj_remove_style_all(s_hline);
    lv_obj_set_size(s_hline, 31, 3);
    lv_obj_set_style_bg_opa(s_hline, LV_OPA_COVER, 0);
    lv_obj_set_floating(s_hline, true);

    s_vline = lv_obj_create(scr);
    lv_obj_remove_style_all(s_vline);
    lv_obj_set_size(s_vline, 3, 31);
    lv_obj_set_style_bg_opa(s_vline, LV_OPA_COVER, 0);
    lv_obj_set_floating(s_vline, true);

    s_info_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_info_label, &lv_font_montserrat_16, 0);
    lv_obj_set_floating(s_info_label, true);
    lv_obj_align(s_info_label, LV_ALIGN_BOTTOM_MID, 0, -48);

    s_step = 0;
    s_state = CALIB_STATE_RUN;
    s_swap_retry = 0;
    s_pressing = 0;
    s_scr = scr;
    s_active = 1;
    Calib_ShowTarget(0);

    /* 上一实例若因滞后 DELETE 没能回收自己的定时器，这里兜底清掉，避免两条状态机并存 */
    if (s_timer != NULL)
    {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    s_timer = lv_timer_create(Calib_Tick, CALIB_TICK_MS, NULL);
    lv_obj_add_event_cb(scr, Calib_OnDelete, LV_EVENT_DELETE, NULL);
    return scr;
}

/* BACK：回到上一个校准点重采 */
void Ui_CalibBack(void)
{
    if (!s_active || s_state != CALIB_STATE_RUN || s_step == 0)
        return;
    s_step--;
    Calib_ShowTarget(s_step);
}

/* NEXT：重试当前校准点（重置超时） */
void Ui_CalibNext(void)
{
    if (!s_active || s_state != CALIB_STATE_RUN)
        return;
    Calib_ShowTarget(s_step);
}

/* 触摸事件喂入：RUN 态采集当前点原始 AD 值（取释放前最后一次稳定读数） */
void Ui_CalibFeed(const TouchMsg_t *msg)
{
    if (!s_active || s_state != CALIB_STATE_RUN)
        return;

    if (msg->event == TOUCH_EVENT_PRESS_DOWN)
    {
        s_pressing = 1;
        s_last_raw_x = msg->raw_x;
        s_last_raw_y = msg->raw_y;
    }
    else if (msg->event == TOUCH_EVENT_PRESS_HOLD)
    {
        s_last_raw_x = msg->raw_x;
        s_last_raw_y = msg->raw_y;
    }
    else if (msg->event == TOUCH_EVENT_PRESS_UP)
    {
        if (s_pressing)
        {
            s_pressing = 0;
            s_pos[s_step][0] = s_last_raw_x;
            s_pos[s_step][1] = s_last_raw_y;
            s_step++;
            if (s_step >= CALIB_POINT_NUM)
                s_state = CALIB_STATE_DONE;
            else
                Calib_ShowTarget(s_step);
        }
    }
}
