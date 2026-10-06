#ifndef __UI_OSC_H
#define __UI_OSC_H

#include "main.h"
#include "ui_page.h"
#include "lvgl.h"

/* 函数接口 */
lv_obj_t *Ui_OscCreate(UiPageId_t id); // 示波页工厂（ui_osc.c）
void Ui_OscBack(void);                 // BACK：循环切换采样率档位（按钮文字同步）
void Ui_OscNext(void);                 // NEXT：暂停/恢复显示（采样不停，恢复即最新）

#endif /* __UI_OSC_H */
