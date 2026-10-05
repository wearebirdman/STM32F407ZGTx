#ifndef __UI_MSG_H
#define __UI_MSG_H

#include "main.h"
#include "cmsis_os.h"

/* 消息类型定义 */
typedef enum {
    UI_MSG_NONE = 0,   // 空消息
    UI_MSG_SET_TEXT,   // 更新主屏标签文本
    UI_MSG_REQ_CALIB,  // 请求进入触摸校准
    UI_MSG_COUNT,      // 哨兵：越界校验与数组长度
} UiMsgType_t;

/* UI 消息结构体（8 字节内，值拷贝入队） */
typedef struct {
    uint8_t type;      // UiMsgType_t，压缩到 1 字节
    union {
        const char *text; // UI_MSG_SET_TEXT：须指向静态存储字符串
        uint32_t   param; // 通用数值参数
    } data;
} UiMsg_t;

extern osMessageQueueId_t q_UiMsgHandle; // UI 消息队列句柄（freertos.c 创建）

/* 函数接口 */
uint8_t Ui_PostMsg(const UiMsg_t *msg); // 投递消息，队列满返回 0（满则丢）

#endif /* __UI_MSG_H */
