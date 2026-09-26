/*
 * ui.c —— 简单 UI 渲染层实现（基于 lcd.c 驱动的直接绘制）
 *
 * 本文件实现 DispRenderer_t 接口的四个回调，供 display 服务层派发调用。
 * 所有 LCD_* 驱动调用集中在此，与 display 车轴解耦。
 *
 * 渲染知识（坐标、文案、布局）全部收在下方 static 私有函数中，
 * 新增界面 = ui.h 加枚举值（display.h 中）+ 本文件加排版函数 + UI_OnAcquire 加分支。
 */

#include "ui.h"
#include "lcd.h"
#include <string.h>

/* ====================================================================
 *                      界面渲染（static 私有）
 * ==================================================================== */

/* 开机占位屏：仅上电瞬间可见，home_task 启动后立即 Acquire HOME 覆盖 */
static void Screen_ShowDefault(uint8_t param)
{
    (void)param;
    LCD_Clear(BLACK);
    LCD_ShowString(20, 20, 100, 20, 16, "hello world", RED, BLACK);
}

/* 主界面：上方 3x4 网格 + 底部 3 键导航栏（几何宏见 ui.h，
 * home_task 用同一组宏做命中测试，此处只管画） */
static void Screen_ShowHome(uint8_t param)
{
    static const char *cell_label[HOME_COLS * HOME_ROWS] = {
        "CAL", "2", "3", "4", "5", "6",
        "7", "8", "9", "10", "11", "12",
    };
    (void)param;

    LCD_Clear(BLACK);

    /* 网格外框 + 内部格线 */
    LCD_DrawRectangle(0, 0, LCD_WIDTH - 1, HOME_NAV_Y - 1, WHITE);
    for (uint8_t c = 1; c < HOME_COLS; c++)
        LCD_DrawLine(c * HOME_CELL_W, 1, c * HOME_CELL_W, HOME_NAV_Y - 2, WHITE);
    for (uint8_t r = 1; r < HOME_ROWS; r++)
        LCD_DrawLine(1, r * HOME_CELL_H, LCD_WIDTH - 2, r * HOME_CELL_H, WHITE);

    /* 格标签（16 号字：字符宽 8 高 16，居中放置） */
    for (uint8_t i = 0; i < HOME_COLS * HOME_ROWS; i++)
    {
        uint8_t col = i % HOME_COLS;
        uint8_t row = i / HOME_COLS;
        uint8_t len = (uint8_t)strlen(cell_label[i]);
        LCD_ShowString(col * HOME_CELL_W + (HOME_CELL_W - len * 8) / 2,
                       row * HOME_CELL_H + (HOME_CELL_H - 16) / 2,
                       HOME_CELL_W, HOME_CELL_H, 16,
                       cell_label[i], (i == 0) ? GREEN : GRAY, BLACK);
    }

    /* 底部导航栏：填充 + 分隔线 + 三键标签 */
    LCD_Fill(0, HOME_NAV_Y, LCD_WIDTH - 1, LCD_HEIGHT - 1, DARKBLUE);
    LCD_DrawLine(HOME_NAV_W, HOME_NAV_Y, HOME_NAV_W, LCD_HEIGHT - 1, WHITE);
    LCD_DrawLine(2 * HOME_NAV_W, HOME_NAV_Y, 2 * HOME_NAV_W, LCD_HEIGHT - 1, WHITE);
    LCD_ShowString(0 * HOME_NAV_W + (HOME_NAV_W - 4 * 8) / 2, HOME_NAV_Y + (HOME_NAV_H - 16) / 2,
                   HOME_NAV_W, HOME_NAV_H, 16, "HOME", WHITE, DARKBLUE);
    LCD_ShowString(1 * HOME_NAV_W + (HOME_NAV_W - 8) / 2, HOME_NAV_Y + (HOME_NAV_H - 16) / 2,
                   HOME_NAV_W, HOME_NAV_H, 16, "<", WHITE, DARKBLUE);
    LCD_ShowString(2 * HOME_NAV_W + (HOME_NAV_W - 8) / 2, HOME_NAV_Y + (HOME_NAV_H - 16) / 2,
                   HOME_NAV_W, HOME_NAV_H, 16, ">", WHITE, DARKBLUE);
}

/* 触摸校准界面（三态）。param: 0=底版（十字由持有者经 DRAW
 * 动态请求）；1=校准成功 2=校准失败——结果态清屏重写：无帧缓冲，
 * 取点十字遍布全屏，无法在其上干净叠加文字，整屏重画即"换状态" */
static void Screen_ShowCalibrate(uint8_t param)
{
    LCD_Clear(BLACK);
    switch (param)
    {
        case 1:
            LCD_ShowString(60, 150, 120, 16, 16, "Calibration OK", GREEN, BLACK);
            break;
        case 2:
            LCD_ShowString(30, 150, 180, 16, 16, "Calibration Failed", RED, BLACK);
            break;
        default:
            LCD_ShowString(30, 150, 180, 16, 16, "Touch Calibration", WHITE, BLACK);
            break;
    }
}

/* 界面分发：根据 screen 调用对应的排版函数 */
static void Screen_Render(DispScreen_t screen, uint8_t param)
{
    switch (screen)
    {
        case DISP_SCREEN_DEFAULT:      Screen_ShowDefault(param);   break;
        case DISP_SCREEN_HOME:         Screen_ShowHome(param);      break;
        case DISP_SCREEN_CALIBRATE:    Screen_ShowCalibrate(param); break;
        default: break;                /* 调用方已校验越界，双保险 */
    }
}

/* 绘制十字：臂长 8 像素 + 中心大点（调用点保证 x,y >= 8，校准 margin=20 满足） */
static void Screen_Draw(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_DrawLine(x - 8, y, x + 8, y, color);
    LCD_DrawLine(x, y - 8, x, y + 8, color);
    LCD_DrawPoint(x, y, color);
}

/* ====================================================================
 *                      DispRenderer_t 接口实现
 * ==================================================================== */

/* on_acquire：渲染指定界面底版 */
void UI_OnAcquire(DispScreen_t screen, uint8_t param)
{
    Screen_Render(screen, param);
}

/* on_release：释放屏幕（当前实现无需动作，最后一帧保留） */
void UI_OnRelease(void)
{
    /* 无帧缓冲场景下，释放时不渲染，保留最后一帧至下一任属主接管 */
}

/* on_draw：处理动态绘图消息（如 DRAW） */
void UI_OnDraw(const DispMsg_t *msg)
{
    if (msg->type == DISP_MSG_DRAW)
    {
        Screen_Draw(msg->x, msg->y, msg->color);
    }
    /* 未来扩展其他绘图消息类型 */
}

/* on_tick：周期性回调（LVGL 场景下调用 lv_timer_handler） */
void UI_OnTick(void)
{
    /* 简单 UI 模式下无需周期性动作；LVGL 适配器会在此调用 lv_timer_handler() */
}

/* ====================================================================
 *                      UI 初始化
 * ==================================================================== */

/* 渲染器实例（供 Display_RegisterRenderer 注册） */
static const DispRenderer_t s_ui_renderer = {
    .on_acquire = UI_OnAcquire,
    .on_release = UI_OnRelease,
    .on_draw    = UI_OnDraw,
    .on_tick    = UI_OnTick,
};

void UI_Init(void)
{
    Display_RegisterRenderer(&s_ui_renderer);
}
