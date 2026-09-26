#include "led_task.h"
#include "led.h"

void led_proc(void *argument)
{
    for(;;)
    {
        //led_toggle(LED1);
        //led_toggle(LED2);
        //led_toggle(LED3);
        osDelay(1000);
    }
}
