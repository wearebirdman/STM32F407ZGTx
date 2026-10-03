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
                }
                break;
            case KEY_ID_2:
                if (key_msg.event == KEY_EVENT_SHORT_PRESS) 
                {
                    printf("Key_2 Pressed\r\n");
                    led_toggle(LED2);
                }
                break;
            case KEY_ID_3:
                if (key_msg.event == KEY_EVENT_SHORT_PRESS) 
                {
                    printf("Key_3 Pressed\r\n");
                    led_toggle(LED3);
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
