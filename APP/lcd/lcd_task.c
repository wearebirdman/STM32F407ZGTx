#include "lcd_task.h"
#include "lcd.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_msg.h"
#include "ui_port.h"
#include "uart_task.h"

#define LCD_DISP_WAIT_MIN_MS 5    /* LVGL 心跳最小等待，防空转忙等 */
#define LCD_DISP_WAIT_MAX_MS 33   /* 上限=LV_DEF_REFR_PERIOD，保动画不掉帧 */

/* LCD 显示任务入口 */
void Lcd_Disp(void *argument)
{
    (void)argument;

    UiMsg_t msg;

    Lcd_Init();        /* LCD 硬件初始化 */
    Ui_PortInit();     /* LVGL 显示/输入适配 + 建界面 */
    Uart_Printf("LCD + LVGL Init Done!\r\n");

    for (;;)
    {
        /* 先处理跨任务界面请求，再跑 LVGL 心跳 */
        while (osMessageQueueGet(q_UiMsgHandle, &msg, NULL, 0) == osOK)
            Ui_ApplyMsg(&msg);

        uint32_t wait = lv_timer_handler();    /* 返回距下次处理还需多少 ms */
        if (wait < LCD_DISP_WAIT_MIN_MS)
            wait = LCD_DISP_WAIT_MIN_MS;
        if (wait > LCD_DISP_WAIT_MAX_MS)
            wait = LCD_DISP_WAIT_MAX_MS;
        osDelay(wait);
    }
}
