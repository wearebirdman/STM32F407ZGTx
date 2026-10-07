#ifndef __UI_H
#define __UI_H

#include "ui_msg.h"
#include "ui_page.h"

/* 函数接口 */
void Ui_ApplyMsg(const UiMsg_t *msg); /* 在 Lcd_Disp 任务上下文执行界面变更（ui_chrome.c） */

#endif /* __UI_H */
