#ifndef __UI_H
#define __UI_H

#include "ui_msg.h"

/* 函数接口 */
void Ui_CreateHome(void);            // 创建主屏并加载
void Ui_ApplyMsg(const UiMsg_t *msg); // 在 lcd_task 上下文执行界面变更

#endif /* __UI_H */
