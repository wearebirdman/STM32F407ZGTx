#ifndef __UI_CALIB_H
#define __UI_CALIB_H

#include "main.h"
#include "ui_page.h"
#include "touch.h"
#include "lvgl.h"

/* 函数接口 */
lv_obj_t *Ui_CalibCreate(UiPageId_t id);   /* 校准页工厂（ui_calib.c） */
void Ui_CalibBack(void);                   /* BACK：回到上一个校准点 */
void Ui_CalibNext(void);                   /* NEXT：重试当前校准点 */
void Ui_CalibFeed(const TouchMsg_t *msg);  /* 触摸事件喂入（ui_port indev 回调转发，页面未激活时空操作） */

#endif /* __UI_CALIB_H */
