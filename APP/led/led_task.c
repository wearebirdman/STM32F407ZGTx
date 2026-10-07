#include "led_task.h"
#include "led.h"

/* LED显示任务入口 */
void Led_Disp(void *argument)
{
    LedReq_t led_req;

    (void)argument;

    for (;;)
    {
        osMessageQueueGet(q_LedReqHandle, &led_req, NULL, osWaitForever);  /* 阻塞等待 LED 请求 */
        Led_Refresh(led_req);
    }
}
