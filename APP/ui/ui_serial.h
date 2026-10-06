#ifndef __UI_SERIAL_H
#define __UI_SERIAL_H

#include "main.h"
#include "ui_page.h"
#include "lvgl.h"

/* 串口助手参数 */
#define UI_SERIAL_LINE_MAX   40 // 文本槽字节数（含 [Tx]/[Rx] 前缀）
#define UI_SERIAL_HIST_LINES 24 // 历史保留行数（手指拖动浏览，超出丢最旧）

/* 函数接口 */
lv_obj_t *Ui_SerialCreate(UiPageId_t id); // 串口页工厂（ui_serial.c）
void Ui_SerialPostLine(const char *line); // 任意任务：写环形槽 + 投 UI 行消息（uart 任务调用）
void Ui_SerialPush(const char *line);     // lcd_task：追加历史并刷新（Ui_ApplyMsg 调用）

#endif /* __UI_SERIAL_H */
