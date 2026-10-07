#ifndef __UART_TASK_H__
#define __UART_TASK_H__

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* 串口消息定义宏 */
#define UART_MSG_DATA_MAX 31 /* 单条消息最大数据字节数（含 1B 长度头共 32B 定长值拷贝） */

/* 串口消息结构体（q_Usart1RxMsg / q_Usart1TxMsg 共用） */
typedef struct {
    uint8_t len;                     /* 有效数据字节数 1..31 */
    uint8_t data[UART_MSG_DATA_MAX]; /* 数据字节 */
} UartMsg_t;

/* 跨模块共享消息队列句柄 */
extern osMessageQueueId_t q_Usart1RxMsgHandle; /* 串口 RX 消息队列，freertos.c 创建 */
extern osMessageQueueId_t q_Usart1TxMsgHandle; /* 串口 TX 消息队列，freertos.c 创建 */

/* 函数接口 */
void Uart_Send(void *argument);         /* TX 属主任务：出队→发硬件→镜像 [Tx] 上屏（uart_task.c） */
void Uart_Recv(void *argument);         /* RX 任务：出队→按 \n 组行→镜像 [Rx] 上屏（uart_task.c） */
void Uart_Printf(const char *fmt, ...); /* printf 风格发送：格式化入队，由 Uart_Send 统一发送（满则丢；收发任务内禁调） */

#endif /* __UART_TASK_H__ */
