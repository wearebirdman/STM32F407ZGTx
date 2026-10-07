#ifndef __UI_CHROME_H
#define __UI_CHROME_H

#include "main.h"
#include "ui_page.h"
#include "lvgl.h"

/* 外壳尺寸定义（供页面布局参考） */
#define CHROME_STATUS_H 28 /* 状态栏高度 */
#define CHROME_NAV_H    40 /* 导航栏高度 */

/* 函数接口 */
void Ui_ChromeBuild(lv_obj_t *scr, lv_obj_t **content_out);     /* 在屏对象上建状态栏+导航栏外壳，输出内容容器（按钮文字按当前页表项定制） */
void Ui_ChromeSetNavLabels(const char *back, const char *next); /* 运行时改导航按钮文字（NULL=保持；仅 Lcd_Disp 任务调用） */
void Ui_Home(void);                                             /* 清场回主菜单 */
void Ui_Back(void);                                             /* 当前页语义：BACK（菜单=翻页，App=上一步） */
void Ui_Next(void);                                             /* 当前页语义：NEXT（菜单=翻页，App=下一步） */
void Ui_Push(UiPageId_t id);                                    /* 建 App 页并滑入加载 */

#endif /* __UI_CHROME_H */
