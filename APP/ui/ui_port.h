#ifndef __UI_PORT_H
#define __UI_PORT_H

#include "main.h"
#include "cmsis_os.h"

/* 函数接口 */
void Ui_PortInit(void); /* 初始化 LVGL 显示/输入适配层并建界面，须在 Lcd_Disp 任务内调用 */

#endif /* __UI_PORT_H */
