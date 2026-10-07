#include "ui_msg.h"

/* 投递 UI 消息：由 Lcd_Disp 任务串行消费，发送方任意任务可用 */
uint8_t Ui_PostMsg(const UiMsg_t *msg)
{
    return (osMessageQueuePut(q_UiMsgHandle, msg, 0, 0) == osOK) ? 1 : 0;
}
