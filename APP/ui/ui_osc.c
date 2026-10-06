#include "ui_osc.h"
#include "ui_chrome.h"
#include "uart_task.h"
#include "adc.h"

#include <stdio.h>
#include <string.h>

/* 示波参数定义 */
#define OSC_RATE_NUM  5    // 采样率档位数
#define OSC_POINTS    200  // 一屏显示点数（< chart 内容宽，避开 crowded 模式的大额临时分配）
#define OSC_TICK_MS   50   // 显示刷新周期（20fps，半屏重绘压力减半）
#define OSC_MV_MAX    3300 // 纵轴满幅（mV，ADC 满量程）

static const uint16_t s_rate_hz[OSC_RATE_NUM] = {50, 100, 200, 500, 1000};
static const char *const s_rate_tag[OSC_RATE_NUM] = {"Rate:50", "Rate:100", "Rate:200", "Rate:500", "Rate:1k"};

static uint8_t s_active;            // 页面激活标志
static uint8_t s_rate_idx;          // 当前采样率档位
static uint8_t s_paused;            // 暂停显示标志（采样不停）
static int32_t s_mv[OSC_POINTS];    // 快照缓冲（仅 lcd_task 访问）
static lv_obj_t *s_chart;           // 波形图
static lv_obj_t *s_readout;         // 读数行
static lv_chart_series_t *s_series; // 波形系列
static lv_timer_t *s_timer;         // 刷新定时器
static lv_obj_t *s_scr;             // 本页屏对象（过滤滞后到达的 DELETE）

/* 刷新读数行：最新电压 + 当前档位窗口时长 */
static void Osc_UpdateReadout(void)
{
    char buf[40];
    uint32_t mv = (uint32_t)s_mv[OSC_POINTS - 1];
    uint32_t win_ms = OSC_POINTS * 1000u / s_rate_hz[s_rate_idx];

    snprintf(buf, sizeof(buf), "%u.%02uV  %uHz %u.%02us",
             (unsigned)(mv / 1000u), (unsigned)((mv % 1000u) / 10u),
             (unsigned)s_rate_hz[s_rate_idx],
             (unsigned)(win_ms / 1000u), (unsigned)((win_ms % 1000u) / 10u));
    lv_label_set_text(s_readout, buf);
}

/* 刷新 tick：快照最新窗口（撕裂重试一次）并更新波形/读数。
   每帧先过采集看门狗（停摆自重启并自带串口分类诊断）；波形直写 chart 内部数组
   后整体 refresh（lv_chart_set_series_values 是逐个 set_next 的推送语义，弃用） */
static void Osc_Tick(lv_timer_t *timer)
{
    int32_t *arr;

    (void)timer;
    if (s_paused)
        return;
    Adc_CaptureHealth();
    if (Adc_ReadWindow(s_mv, OSC_POINTS))
        Adc_ReadWindow(s_mv, OSC_POINTS);
    arr = lv_chart_get_series_y_array(s_chart, s_series);
    memcpy(arr, s_mv, sizeof(s_mv));
    lv_chart_refresh(s_chart);
    Osc_UpdateReadout();
}

/* 页面销毁：停刷新停采集（仅当销毁的是本页，避免旧页滞后 DELETE 关掉新页） */
static void Osc_OnDelete(lv_event_t *e)
{
    if (lv_event_get_target(e) != s_scr)
        return;
    s_scr = NULL;
    s_active = 0;
    if (s_timer != NULL)
    {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    Adc_CaptureStop();
}

/* 示波页工厂：读数行 + lv_chart，建页即启动采集（本页为 ADC 临时属主） */
lv_obj_t *Ui_OscCreate(UiPageId_t id)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *content;

    (void)id;
    Ui_ChromeBuild(scr, &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    s_scr = scr;
    s_active = 1;
    s_paused = 0;
    s_rate_idx = 0;

    s_readout = lv_label_create(content);
    lv_obj_set_style_text_font(s_readout, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_readout, "--");

    s_chart = lv_chart_create(content);
    lv_obj_set_width(s_chart, LV_PCT(100));
    lv_obj_set_flex_grow(s_chart, 1);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, OSC_POINTS);
    lv_chart_set_axis_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, OSC_MV_MAX);
    lv_chart_set_div_line_count(s_chart, 4, 6);
    /* v9 chart 无轴线/刻度标签：用边框模拟坐标框；关主题圆点；线宽显式固定 */
    lv_obj_set_style_border_width(s_chart, 1, 0);
    lv_obj_set_style_border_color(s_chart, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_pad_all(s_chart, 2, 0);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(s_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(s_chart, 0, LV_PART_INDICATOR);
    s_series = lv_chart_add_series(s_chart, lv_color_hex(0x1E88E5), LV_CHART_AXIS_PRIMARY_Y);

    if (!Adc_CaptureStart(s_rate_hz[s_rate_idx]))
        Uart_Printf("OSC: ADC capture start FAIL\r\n"); // 串口助手页可见的诊断
    if (s_timer != NULL) // 上一实例滞后 DELETE 未回收时兜底，避免双定时器
    {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    s_timer = lv_timer_create(Osc_Tick, OSC_TICK_MS, NULL);
    Osc_Tick(NULL); // 立即填充一次

    lv_obj_add_event_cb(scr, Osc_OnDelete, LV_EVENT_DELETE, NULL);
    return scr;
}

/* BACK：循环切采样率档（幂等重启采集流），档位实时上钮 */
void Ui_OscBack(void)
{
    if (!s_active)
        return;
    s_rate_idx = (uint8_t)((s_rate_idx + 1) % OSC_RATE_NUM);
    if (!Adc_CaptureStart(s_rate_hz[s_rate_idx]))
        Uart_Printf("OSC: ADC capture restart FAIL\r\n");
    Ui_ChromeSetNavLabels(s_rate_tag[s_rate_idx], NULL);
    if (!s_paused)
        Osc_Tick(NULL);
}

/* NEXT：暂停/恢复显示；恢复即跳到最新窗口 */
void Ui_OscNext(void)
{
    if (!s_active)
        return;
    s_paused = (uint8_t)(!s_paused);
    Ui_ChromeSetNavLabels(NULL, s_paused ? "Play" : "Pause");
    if (!s_paused)
        Osc_Tick(NULL);
}
