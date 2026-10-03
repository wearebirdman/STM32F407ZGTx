#ifndef __DISPLAY_H
#define __DISPLAY_H


#include <stdint.h>
#include "cmsis_os.h"
#include "lcd.h"          /* 复用 RGB565 颜色宏与 LCD_WIDTH/HEIGHT（SVC 依赖 BSP） */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 队列句柄（CubeMX 生成，定义于 freertos.c）==================== */
extern osMessageQueueId_t q_display_reqHandle;   /* 客户端 -> 服务层 */
extern osMessageQueueId_t q_display_ackHandle;   /* 服务层 -> 客户端 */

/* ==================== 界面枚举 ==================== */
typedef enum {
    DISP_SCREEN_DEFAULT = 0,    /* 开机占位屏（服务层启动时渲染，home 接管后覆盖） */
    DISP_SCREEN_HOME,           /* 主界面：3x4 网格 + 底部导航栏（home_task 常驻属主） */
    DISP_SCREEN_CALIBRATE,      /* 触摸校准界面（calib_task 会话），param: 0=底版 1=成功 2=失败 */
    DISP_SCREEN_DRAW,           /* 画板界面（draw_task 会话），param: 0=空白画布 1=重放历史 */
    DISP_SCREEN_COUNT,
} DispScreen_t;

/* ==================== 客户端枚举（显示服务的身份）==================== */
typedef enum {
    DISP_CLIENT_NONE = 0,       /* 非法/未填身份 */
    DISP_CLIENT_HOME,           /* home_task（主界面属主） */
    DISP_CLIENT_CALIB,          /* calib_task（校准会话属主） */
    DISP_CLIENT_DRAW,           /* draw_task（画板会话属主） */
    DISP_CLIENT_COUNT,
} DispClient_t;

/* ==================== 消息类型 ==================== */
typedef enum {
    DISP_MSG_ACQUIRE = 0,   /* 接管/换屏（空闲=授予；持有者=会话内切换，均按 param 渲染） */
    DISP_MSG_RELEASE,       /* 归还界面（仅持有者，最后一帧保留） */
    DISP_MSG_DRAW,          /* 动态图元：图形种类见消息体 draw 字段（仅持有者） */
    DISP_MSG_ACK,           /* 应答（仅服务层发送） */
} DispMsgType_t;

/* ==================== 绘图类型（DISP_MSG_DRAW 的二级标签）====================
 * type 只区分消息大类，图形种类放消息体 draw 字段——车轴不感知具体图形，
 * 新增图元 = 本枚举加值 + ui.c 的 UI_OnDraw 加 case，display.c 零改动。
 * 注：载荷用公共字段（点/十字用 x/y，线段另用 x2/y2）；当字段浪费面
 * 扩大（3 种以上异构图元）时，演进为带 tag 的联合体（见工程约定）。 */
typedef enum {
    DISP_DRAW_POINT = 0,    /* 大点（单像素不可见，用大点） */
    DISP_DRAW_CROSS,        /* 十字：臂长 8 + 中心点（校准取点标记） */
    DISP_DRAW_LINE,         /* 线段：(x,y)-(x2,y2)，画板笔画连点成线 */
    DISP_DRAW_COUNT,
} DispDraw_t;

/* ==================== 结果码 ==================== */
typedef enum {
    DISP_OK = 0,        /* 命令已执行 */
    DISP_BUSY,          /* 界面被其他任务持有（仅 ACQUIRE 返回） */
    DISP_TIMEOUT,       /* 请求/应答超时（服务层未启动或过载） */
    DISP_ERR,           /* 参数非法（NONE 身份、未知界面） */
    DISP_FORBIDDEN,     /* 无权限：非持有者 RELEASE/DRAW */
} DispResult_t;

/* ==================== 消息体 ==================== */
typedef struct {
    uint8_t type;       /* 消息类型 DispMsgType_t */
    uint8_t result;     /* ACK: DispResult_t */
    uint8_t screen;     /* ACQUIRE: 目标界面 DispScreen_t */
    uint8_t client;     /* 请求方身份 DispClient_t（服务层权限校验依据） */
    uint8_t seq;        /* 请求序号：ACK 配对，用于丢弃过期应答 */
    uint8_t param;      /* ACQUIRE: 界面状态参数 */
    uint8_t draw;       /* DRAW: 绘图类型 DispDraw_t */
    uint16_t x;         /* DRAW: 起点/点坐标 */
    uint16_t y;
    uint16_t color;     /* DRAW: 颜色 */
    uint16_t x2;        /* DRAW_LINE: 终点坐标（其余图元忽略） */
    uint16_t y2;
} DispMsg_t;

_Static_assert(sizeof(DispMsg_t) <= 32, "DispMsg_t must stay <= 32 bytes (queue pass-by-value)");

/* 应答超时（ms）：须大于 LCD 初始化耗时（约 220ms）+ 最长单次绘制时间。
   客户端可能在 lcd_task 进入服务循环之前发请求，请求会排队，故超时需覆盖初始化。 */
#define DISP_ACK_TIMEOUT_MS     1000u

/* 服务层心跳周期（ms）：用于 LVGL 适配器的 lv_timer_handler 调用间隔。
   简单 UI 模式下此值无实际影响，但 ServiceLoop 仍按此周期唤醒。 */
#define DISP_TICK_PERIOD_MS     5u

/* ==================== 渲染器接口（DispRenderer_t）====================
 * display 服务层通过此接口派发渲染动作，不直接调用 LCD_*。
 * ui.c 提供简单 UI 渲染器；移植 LVGL 时，lvgl_adapter.c 提供另一组回调。 */
typedef struct {
    void (*on_acquire)(DispScreen_t screen, uint8_t param);  /* 接管/换屏时渲染底版 */
    void (*on_release)(void);                                /* 释放屏幕（可选动作） */
    void (*on_draw)(const DispMsg_t *msg);                   /* 处理动态绘图消息 */
    void (*on_tick)(void);                                   /* 周期性回调（LVGL 心跳） */
} DispRenderer_t;

/* ==================== 渲染器注册（供 ui.c / lvgl_adapter.c 调用）====================
 * 调用时机：lcd_task 启动后、Display_ServiceLoop 之前。
 * 注册后，ServiceLoop 通过 renderer 派发所有渲染调用。 */
void Display_RegisterRenderer(const DispRenderer_t *renderer);

/* ==================== 客户端 API（同步带超时）====================
 * self：调用方身份（编译期常量，如 DISP_CLIENT_CALIB），服务层据此校验权限 */
DispResult_t Display_Acquire(DispClient_t self, DispScreen_t screen, uint8_t param);
DispResult_t Display_Release(DispClient_t self);
DispResult_t Display_Draw(DispClient_t self, DispDraw_t kind, uint16_t x, uint16_t y, uint16_t color);

/* ==================== 客户端 API（异步 fire-and-forget）====================
 * 投递请求后立即返回，不等待应答。适用于高频更新、不关心结果的场景。
 * 注意：无法获知执行结果，仅保证请求已入队。 */
void Display_PostAcquire(DispClient_t self, DispScreen_t screen, uint8_t param);
void Display_PostRelease(DispClient_t self);
void Display_PostDraw(DispClient_t self, DispDraw_t kind, uint16_t x, uint16_t y, uint16_t color);
void Display_PostDrawLine(DispClient_t self, uint16_t x1, uint16_t y1,
                          uint16_t x2, uint16_t y2, uint16_t color);

/* ==================== 状态查询 API ====================
 * 获取当前服务层状态：持有者 + 当前界面。
 * 用于客户端判断是否需要重新接管，消除影子状态漂移。 */
void Display_GetStatus(DispClient_t *out_owner, DispScreen_t *out_screen);

/* ==================== 服务 API（仅 LCD 属主任务调用，内部死循环不返回）==================== */
void Display_ServiceLoop(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_H */
