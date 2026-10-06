#ifndef __UI_DRAW_H
#define __UI_DRAW_H

#include "main.h"
#include "ui_page.h"
#include "touch.h"
#include "lvgl.h"

/* 函数接口 */
lv_obj_t *Ui_DrawCreate(UiPageId_t id);     // 画板页工厂（ui_draw.c）
void Ui_DrawBack(void);                      // BACK：撤销最近一笔
void Ui_DrawNext(void);                      // NEXT：循环切换画笔颜色
void Ui_DrawFeed(const TouchMsg_t *msg);     // 触摸事件喂入（ui_port indev 回调转发，页面未激活时空操作）

#endif /* __UI_DRAW_H */
