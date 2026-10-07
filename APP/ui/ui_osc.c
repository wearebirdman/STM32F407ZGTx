#include "ui_osc.h"
#include "adc.h"
#include "uart_task.h"
#include "ui_chrome.h"

#include <stdio.h>
#include <string.h>

/* 示波参数定义 */
#define OSC_RATE_NUM  5    /* 采样率档位数 */
#define OSC_POINTS    200  /* 一屏显示点数（< chart 内容宽，避开 crowded 模式的大额临时分配） */
#define OSC_TICK_MS   50   /* 显示刷新周期（20fps，半屏重绘压力减半） */
#define OSC_MV_MAX    3300 /* 纵轴满幅（mV，ADC 满量程） */

/* 采样率档位表：Hz 数值 + 对应按钮文字 */
static const uint16_t s_RateHz[OSC_RATE_NUM] = {50, 100, 200, 500, 1000};
static const char *const s_RateTag[OSC_RATE_NUM] = {"Rate:50", "Rate:100", "Rate:200", "Rate:500", "Rate:1k"};

static uint8_t s_Active;            /* 页面激活标志 */
static uint8_t s_RateIdx;           /* 当前采样率档位 */
static uint8_t s_Paused;            /* 暂停显示标志（采样不停） */
static int32_t s_Mv[OSC_POINTS];    /* 快照缓冲（仅 Lcd_Disp 任务访问） */
static lv_obj_t *s_Chart;           /* 波形图 */
static lv_obj_t *s_Readout;         /* 读数行 */
static lv_chart_series_t *s_Series; /* 波形系列 */
static lv_timer_t *s_Timer;         /* 刷新定时器 */
static lv_obj_t *s_Scr;             /* 本页屏对象（过滤滞后到达的 DELETE） */

/* 刷新读数行：最新电压 + 当前档位窗口时长 */
static void Osc_UpdateReadout(void)
{
    char buf[40];
    uint32_t mv = (uint32_t)s_Mv[OSC_POINTS - 1];
    uint32_t win_ms = OSC_POINTS * 1000u / s_RateHz[s_RateIdx];

    snprintf(buf, sizeof(buf), "%u.%02uV  %uHz %u.%02us",
             (unsigned)(mv / 1000u), (unsigned)((mv % 1000u) / 10u),
             (unsigned)s_RateHz[s_RateIdx],
             (unsigned)(win_ms / 1000u), (unsigned)((win_ms % 1000u) / 10u));
    lv_label_set_text(s_Readout, buf);
}

/* 刷新 tick：快照最新窗口（撕裂重试一次）并更新波形/读数
 * 每帧先过采集看门狗（停摆自重启并自带串口分类诊断）；波形直写 chart 内部数组
 * 后整体 refresh（lv_chart_set_series_values 是逐个 set_next 的推送语义，弃用） */
static void Osc_Tick(lv_timer_t *timer)
{
    (void)timer;
    if (s_Paused)
        return;
    Adc_CaptureHealth();
    if (Adc_ReadWindow(s_Mv, OSC_POINTS))
        Adc_ReadWindow(s_Mv, OSC_POINTS);
    int32_t *arr = lv_chart_get_series_y_array(s_Chart, s_Series);
    memcpy(arr, s_Mv, sizeof(s_Mv));
    lv_chart_refresh(s_Chart);
    Osc_UpdateReadout();
}

/* 页面销毁：停刷新停采集（仅当销毁的是本页，避免旧页滞后 DELETE 关掉新页） */
static void Osc_OnDelete(lv_event_t *e)
{
    if (lv_event_get_target(e) != s_Scr)
        return;
    s_Scr = NULL;
    s_Active = 0;
    if (s_Timer != NULL)
    {
        lv_timer_delete(s_Timer);
        s_Timer = NULL;
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

    s_Scr = scr;
    s_Active = 1;
    s_Paused = 0;
    s_RateIdx = 0;

    s_Readout = lv_label_create(content);
    lv_obj_set_style_text_font(s_Readout, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_Readout, "--");

    s_Chart = lv_chart_create(content);
    lv_obj_set_width(s_Chart, LV_PCT(100));
    lv_obj_set_flex_grow(s_Chart, 1);
    lv_chart_set_type(s_Chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_Chart, OSC_POINTS);
    lv_chart_set_axis_range(s_Chart, LV_CHART_AXIS_PRIMARY_Y, 0, OSC_MV_MAX);
    lv_chart_set_div_line_count(s_Chart, 4, 6);
    /* v9 chart 无轴线/刻度标签：用边框模拟坐标框；关主题圆点；线宽显式固定 */
    lv_obj_set_style_border_width(s_Chart, 1, 0);
    lv_obj_set_style_border_color(s_Chart, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_pad_all(s_Chart, 2, 0);
    lv_obj_set_style_line_width(s_Chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(s_Chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(s_Chart, 0, LV_PART_INDICATOR);
    s_Series = lv_chart_add_series(s_Chart, lv_color_hex(0x1E88E5), LV_CHART_AXIS_PRIMARY_Y);

    if (!Adc_CaptureStart(s_RateHz[s_RateIdx]))
        Uart_Printf("OSC: ADC capture start FAIL\r\n"); /* 串口助手页可见的诊断 */
    if (s_Timer != NULL) /* 上一实例滞后 DELETE 未回收时兜底，避免双定时器 */
    {
        lv_timer_delete(s_Timer);
        s_Timer = NULL;
    }
    s_Timer = lv_timer_create(Osc_Tick, OSC_TICK_MS, NULL);
    Osc_Tick(NULL); /* 立即填充一次 */

    lv_obj_add_event_cb(scr, Osc_OnDelete, LV_EVENT_DELETE, NULL);
    return scr;
}

/* BACK：循环切采样率档（幂等重启采集流），档位实时上钮 */
void Ui_OscBack(void)
{
    if (!s_Active)
        return;
    s_RateIdx = (uint8_t)((s_RateIdx + 1) % OSC_RATE_NUM);
    if (!Adc_CaptureStart(s_RateHz[s_RateIdx]))
        Uart_Printf("OSC: ADC capture restart FAIL\r\n");
    Ui_ChromeSetNavLabels(s_RateTag[s_RateIdx], NULL);
    if (!s_Paused)
        Osc_Tick(NULL);
}

/* NEXT：暂停/恢复显示；恢复即跳到最新窗口 */
void Ui_OscNext(void)
{
    if (!s_Active)
        return;
    s_Paused = (uint8_t)(!s_Paused);
    Ui_ChromeSetNavLabels(NULL, s_Paused ? "Play" : "Pause");
    if (!s_Paused)
        Osc_Tick(NULL);
}
