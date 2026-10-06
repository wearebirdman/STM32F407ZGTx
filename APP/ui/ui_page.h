#ifndef __UI_PAGE_H
#define __UI_PAGE_H

#include "main.h"
#include "lvgl.h"

/* 页面 ID 定义 */
typedef enum {
    UI_PAGE_HOME = 0,  // 主菜单
    UI_PAGE_CALIB,     // 触摸校准
    UI_PAGE_DRAW,      // 画板
    UI_PAGE_OSC,       // 示波器（ADC 波形，待实现）
    UI_PAGE_SERIAL,    // 串口助手（[Tx]/[Rx] 双向合流显示）
    UI_PAGE_SET,       // 系统设置（校时等）
    UI_PAGE_ABOUT,     // 关于
    UI_PAGE_COUNT,     // 哨兵：越界校验与表长
} UiPageId_t;

/* 页面工厂函数类型：构建一个未加载的屏幕对象 */
typedef lv_obj_t *(*UiPageCreateFn)(UiPageId_t id);

/* 页面导航回调类型：BACK/NEXT 在当前页的语义动作 */
typedef void (*UiPageNavFn)(void);

/* 页面表项：工厂 + 当前页 BACK/NEXT 语义 + 按钮文字定制 */
typedef struct {
    UiPageCreateFn create;     // 页面工厂
    UiPageNavFn    on_back;    // BACK 动作（NULL=无）
    UiPageNavFn    on_next;    // NEXT 动作（NULL=无）
    const char *   back_label; // BACK 按钮文字（NULL=默认 "Back"）
    const char *   next_label; // NEXT 按钮文字（NULL=默认 "Next"）
} UiPageCbs_t;

/* 函数接口 */
lv_obj_t *Ui_CreateHome(UiPageId_t id); // 主菜单页工厂（ui_home.c）
lv_obj_t *Ui_StubCreate(UiPageId_t id); // 占位页工厂（ui_stub.c）
void Ui_HomeBack(void);                // 主菜单：翻上一页菜单（ui_home.c）
void Ui_HomeNext(void);                // 主菜单：翻下一页菜单（ui_home.c）

#endif /* __UI_PAGE_H */
