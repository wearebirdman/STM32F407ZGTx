#include "lcd_task.h"
#include "lcd.h"
#include "ui.h"
#include "ui_port.h"
#include "uart_task.h"
#include "lvgl.h"

extern osMessageQueueId_t q_UiMsgHandle;

void lcd_disp(void *argument)
{
    UiMsg_t msg;
    uint32_t wait;

    LCD_Init();      // LCD 硬件初始化
    Ui_PortInit();   // LVGL 显示/输入适配 + 建界面
    Uart_Printf("LCD + LVGL Init Done!\r\n");

    for (;;)
    {
        /* 先处理跨任务界面请求，再跑 LVGL 心跳 */
        while (osMessageQueueGet(q_UiMsgHandle, &msg, NULL, 0) == osOK)
            Ui_ApplyMsg(&msg);

        wait = lv_timer_handler(); // 返回距下次处理还需多少 ms
        if (wait < 5)
            wait = 5;   // 下限：避免空转忙等
        if (wait > 33)
            wait = 33;  // 上限 = LV_DEF_REFR_PERIOD，保证动画不掉帧
        osDelay(wait);
    }
}
