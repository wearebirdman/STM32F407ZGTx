#include "key_task.h"
#include "key.h"
#include "led.h"
#include "led_task.h"
#include "uart_task.h"

/* ========== 任务配置 ========== */
#define KEY_SCAN_PERIOD_MS  10  /* 按键扫描周期（ms） */

/* 按键任务入口 */
void Key_Proc(void *argument)
{
    KeyMsg_t key_msg;
    LedReq_t led_req;

    (void)argument;

    Key_Init();
    Uart_Printf("Key Init Done!\r\n");

    for (;;)
    {
        key_msg = Key_Scan();
        switch (key_msg.key_id)
        {
        case KEY_1_ID:
            if (key_msg.event == KEY_EVENT_SHORT_PRESS)
            {
                Uart_Printf("Key_1 Pressed\r\n");
                led_req.led_id = LED_1_ID;
                led_req.state = LED_TOGGLE;
                osMessageQueuePut(q_LedReqHandle, &led_req, 0, 0);
            }
            break;
        case KEY_2_ID:
            if (key_msg.event == KEY_EVENT_SHORT_PRESS)
            {
                Uart_Printf("Key_2 Pressed\r\n");
                led_req.led_id = LED_2_ID;
                led_req.state = LED_TOGGLE;
                osMessageQueuePut(q_LedReqHandle, &led_req, 0, 0);
            }
            break;
        case KEY_3_ID:
            if (key_msg.event == KEY_EVENT_SHORT_PRESS)
            {
                Uart_Printf("Key_3 Pressed\r\n");
                led_req.led_id = LED_3_ID;
                led_req.state = LED_TOGGLE;
                osMessageQueuePut(q_LedReqHandle, &led_req, 0, 0);
            }
            break;
        case KEY_NONE_ID:
            break;
        default:
            break;
        }
        osDelay(KEY_SCAN_PERIOD_MS);  /* 按扫描周期轮询按键 */
    }
}
