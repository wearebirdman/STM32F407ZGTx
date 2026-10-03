#ifndef __KEY_H
#define __KEY_H

#include "main.h"

/* 按键数量 */
#define KEY_NUM             4 // 始终为最大按键ID+1
#define KEY_WK_UNUSED       1 // 未使用WakeUp按键为1,按键扫描中从1开始扫描
#define KEY_WK_PORT         GPIOA
#define KEY_WK_PIN          GPIO_PIN_0
#define KEY_WK_ACTIVE_LEVEL GPIO_PIN_SET
#define KEY_1_PORT          GPIOE
#define KEY_1_PIN           GPIO_PIN_4
#define KEY_1_ACTIVE_LEVEL  GPIO_PIN_RESET
#define KEY_2_PORT          GPIOE
#define KEY_2_PIN           GPIO_PIN_3
#define KEY_2_ACTIVE_LEVEL  GPIO_PIN_RESET
#define KEY_3_PORT          GPIOE
#define KEY_3_PIN           GPIO_PIN_2
#define KEY_3_ACTIVE_LEVEL  GPIO_PIN_RESET

/* 按键事件时间定义 */
#define KEY_DEBOUNCE_MS     10   // 消抖时间
#define KEY_LONG_PRESS_MS   2000 // 长按时间
#define KEY_DOUBLE_CLICK_MS 200  // 双击间隔时间

/* 按键ID定义 */
typedef enum {
    KEY_ID_NONE = -1, // 无效按键，用于无事件返回
    KEY_ID_WK   = 0,
    KEY_ID_1,
    KEY_ID_2,
    KEY_ID_3
} KeyID_t;

/* 按键事件类型定义 */
typedef enum {
    KEY_EVENT_NONE = 0,    // 无事件
    KEY_EVENT_SHORT_PRESS, // 短按事件
    KEY_EVENT_LONG_PRESS,  // 长按事件
    KEY_EVENT_DOUBLE_CLICK // 双击事件
} KeyEvent_t;

/* 按键消息结构体 */
typedef struct {
    KeyID_t key_id;
    KeyEvent_t event;
} KeyMsg_t;

/* 函数接口 */
void Key_Init(void);
KeyMsg_t Key_Scan(void);

#endif
