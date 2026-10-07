#include "uart_task.h"
#include "ui_serial.h"
#include "usart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 接收组行参数 */
#define UART_RECV_LINE_MAX 64 /* 行组装缓冲上限（满则强制结行，防残行永挂） */

/* 去掉行尾 CR/LF（仅供上屏显示，发送内容原样不动） */
static void Uart_TrimEol(char *s)
{
    size_t len = strlen(s);

    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n'))
    {
        len--;
        s[len] = '\0';
    }
}

/* printf 风格发送：格式化进 32B 消息入队，Uart_Send 统一发硬件并镜像上屏。
 * 注意：Uart_Send / Uart_Recv 任务内部禁止调用本函数（自锁吞消息） */
void Uart_Printf(const char *fmt, ...)
{
    UartMsg_t msg;
    char buf[UART_MSG_DATA_MAX + 1]; /* vsnprintf 需预留结尾 NUL */
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0)
        return;
    if (n > UART_MSG_DATA_MAX)
        n = UART_MSG_DATA_MAX; /* 超长截断 */
    msg.len = (uint8_t)n;
    memcpy(msg.data, buf, (size_t)n);
    osMessageQueuePut(q_Usart1TxMsgHandle, &msg, 0, 0); /* 满则丢 */
}

/* TX 属主任务：全工程唯一串口写者（printf 重定向已删除） */
void Uart_Send(void *argument)
{
    UartMsg_t msg;
    char line[UI_SERIAL_LINE_MAX];

    (void)argument;
    for (;;)
    {
        if (osMessageQueueGet(q_Usart1TxMsgHandle, &msg, NULL, osWaitForever) != osOK)
            continue;
        HAL_UART_Transmit(&huart1, msg.data, msg.len, HAL_MAX_DELAY);

        /* 镜像上屏：[Tx] 前缀 + 内容（行尾换行符剥除，一行一条） */
        snprintf(line, sizeof(line), "[Tx] %.*s", (int)msg.len, (const char *)msg.data);
        Uart_TrimEol(line);
        Ui_SerialPostLine(line);
    }
}

/* RX 任务：排空 ISR 分块消息，按 \n 组行后镜像上屏。
 * 指令解析分发点：后续要支持命令时在此处结行后判断前缀 */
void Uart_Recv(void *argument)
{
    UartMsg_t msg;
    char line[UART_RECV_LINE_MAX];
    char out[UI_SERIAL_LINE_MAX];
    uint8_t len = 0; /* 当前残行长度（任务栈上跨轮次保持） */

    (void)argument;
    for (;;)
    {
        if (osMessageQueueGet(q_Usart1RxMsgHandle, &msg, NULL, osWaitForever) != osOK)
            continue;
        for (uint8_t i = 0; i < msg.len; i++)
        {
            char c = (char)msg.data[i];
            if (c == '\r')
                continue;
            if (c == '\n' || len >= sizeof(line) - 1)
            {
                if (len > 0)
                {
                    line[len] = '\0';
                    snprintf(out, sizeof(out), "[Rx] %.*s",
                             (int)(UI_SERIAL_LINE_MAX - 6), line); /* 显式限长：截断即设计，消警告 */
                    Ui_SerialPostLine(out);
                    len = 0;
                }
                continue;
            }
            line[len++] = c;
        }
    }
}
