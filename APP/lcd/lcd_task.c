#include "lcd_task.h"
#include "lcd.h"
#include "display.h"
#include "ui.h"

#include <stdio.h>

/**
 * @brief  LCD 属主任务：初始化硬件后进入显示服务主循环（不返回）。
 *         其他任务不再直接操作 LCD，一律通过 display.h 的客户端 API 请求。
 *
 * 初始化序列：
 *   1. LCD_Init()：硬件初始化（含约 220ms 复位/唤醒延时）；
 *   2. UI_Init()：注册简单 UI 渲染器（内部调用 Display_RegisterRenderer）；
 *   3. Display_ServiceLoop()：进入服务主循环，永不返回。
 *
 * 移植 LVGL 时，将 UI_Init() 替换为 LVGL_Adapter_Init() 即可，
 * display.c 一行不改。
 */
void lcd_proc(void *argument)
{
    LCD_Init();   // LCD 硬件初始化
    printf("LCD Init Done!\r\n");

    UI_Init();    // 注册简单 UI 渲染器（或 LVGL_Adapter_Init()）

    Display_ServiceLoop();   // 显示服务主循环，永不返回
}
