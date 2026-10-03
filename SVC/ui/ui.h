#ifndef __UI_H
#define __UI_H

/*
 * ==================== 简单 UI 渲染层（SVC/ui）====================
 *
 * 本模块是 display 服务层的"渲染后端"之一，提供基于 lcd.c 驱动的直接绘制。
 * 职责：接收 display 派发的渲染动作（on_acquire/on_release/on_draw/on_tick），
 *       调用 LCD_* 驱动完成实际绘制。
 *
 * 设计意图：
 *   - display 服务层（车轴）只负责消息路由、权限仲裁、状态管理；
 *   - 渲染知识（坐标、文案、布局）集中在此模块，与车轴解耦；
 *   - 移植 LVGL 时，只需替换为 LVGL 适配器（lvgl_adapter.c），display 一行不改。
 *
 * 当前提供的界面：
 *   - DISP_SCREEN_DEFAULT：开机占位屏（hello world）
 *   - DISP_SCREEN_HOME：主界面（3x4 网格）
 *   - DISP_SCREEN_CALIBRATE：校准界面（三态：底版/成功/失败）
 *   - DISP_SCREEN_DRAW：画板（白底画布）
 * 底部导航栏（HOME/< / >）由 Screen_Render 在所有界面绘制后统一追加，
 * 常驻显示；子界面会话任务用 HOME_* 宏对触摸流做导航命中测试。
 *
 * 命中测试：HOME_* 宏导出供 home_task 做触摸命中测试，与渲染共用同一事实源。
 */

#include <stdint.h>
#include "display.h"      /* DispScreen_t / DispMsg_t / DispRenderer_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 主界面几何（渲染与命中测试的单一事实源）====================
 * 布局：上方 HOME_COLS x HOME_ROWS 网格 + 底部 HOME_NAV_H 高导航栏（3 键）。
 * home_task 用这些宏做触摸命中测试，ui.c 用同一组宏渲染——改布局只改这里。 */
#define HOME_NAV_H      40u                                     /* 导航栏高度 */
#define HOME_COLS       3u                                      /* 网格列数 */
#define HOME_ROWS       4u                                      /* 网格行数 */
#define HOME_CELL_W     (LCD_WIDTH  / HOME_COLS)                /* 网格单元宽（80） */
#define HOME_CELL_H     ((LCD_HEIGHT - HOME_NAV_H) / HOME_ROWS) /* 网格单元高（70） */
#define HOME_NAV_Y      (LCD_HEIGHT - HOME_NAV_H)               /* 导航栏上沿 y（280） */
#define HOME_NAV_W      (LCD_WIDTH  / 3u)                       /* 导航键宽（80） */

/* ==================== UI 初始化 ====================
 * 调用时机：lcd_task 启动后、Display_ServiceLoop 之前。
 * 内部调用 Display_RegisterRenderer() 注册本模块的渲染回调。 */
void UI_Init(void);

/* ==================== 渲染器回调（供 Display_RegisterRenderer 使用）====================
 * 以下函数由 display 服务层在 ServiceLoop 上下文中调用，
 * 实现 DispRenderer_t 接口的四个回调。 */
void UI_OnAcquire(DispScreen_t screen, uint8_t param);
void UI_OnRelease(void);
void UI_OnDraw(const DispMsg_t *msg);
void UI_OnTick(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_H */
