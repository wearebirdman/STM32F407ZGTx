#include "key_task.h"
#include "key.h"
#include "display.h"
#include "home_task.h"
#include "led.h"

#include <stdio.h>

void key_proc(void *argument)
{
    KeyMsg_t key_msg;

    Key_Init();
    printf("Key Init Done!\r\n");

    for (;;)
    {
        key_msg = Key_Scan();
        switch (key_msg.key_id) 
        {
            case KEY_ID_1:
                if (key_msg.event == KEY_EVENT_SHORT_PRESS) 
                {
                    printf("Key_1 Pressed -> kick calibration\r\n");
                    led_toggle(LED1);
                    /* 校准入口统一经 home 仲裁（避免与主界面让屏竞态） */
                    UiEvt_t ev = { .type = UIEVT_KICK_CALIB };
                    (void)osMessageQueuePut(q_ui_evtHandle, &ev, 0, 0);
                }
                break;
            case KEY_ID_2:
                if (key_msg.event == KEY_EVENT_SHORT_PRESS) 
                {
                    printf("Key_2 Pressed\r\n");
                    led_toggle(LED2);
                    /* 权限测试：home 持有主界面时应回 DISP_BUSY(1) */
                    DispResult_t r2 = Display_Acquire(DISP_CLIENT_KEY, DISP_SCREEN_CALIBRATE, 0);
                    printf("  Acquire(CALIBRATE) -> %d (expect BUSY=1)\r\n", r2);
                }
                break;
            case KEY_ID_3:
                if (key_msg.event == KEY_EVENT_SHORT_PRESS) 
                {
                    printf("Key_3 Pressed\r\n");
                    led_toggle(LED3);
                    DispResult_t r3 = Display_Release(DISP_CLIENT_KEY);
                    printf("  Release -> %d (expect FORBIDDEN=4)\r\n", r3);   /* 非持有者 */
                }
                break;
            case KEY_ID_NONE:
                break;
            default:
                break;
        }
        osDelay(10);   // 每 10ms 扫描一次按键
    }
}
