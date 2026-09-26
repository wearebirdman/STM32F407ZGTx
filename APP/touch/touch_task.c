#include "touch_task.h"
#include "home_task.h"      /* UiEvt_t / q_ui_evtHandle（按下广播给主界面） */
#include "touch.h"
#include "display.h"

#if TOUCH_USE_EEPROM_CAL
#include "at24c02.h"
#endif

#include <stdio.h>

/**
 * @brief  触摸任务：初始化后周期扫描，是触摸硬件（XPT2046）唯一属主。
 *
 * 每次 PRESS_DOWN：
 *   1. 把按下瞬间原始 AD 坐标推入 q_touch_evt（供 calib_task 取点消费，
 *      非阻塞、满则丢——无消费者时自动丢弃，无需状态门控）；
 *   2. 演示：打印 + 请求显示服务画十字（校准会话期间持有者是 calib_task，
 *      本请求会被服务层按 s_owner 校验自然拒绝，互不干扰）。
 *
 * 校准业务流程已拆至 calib_task，本任务不再包含任何校准逻辑。
 */
void touch_proc(void *argument)
{
    Touch_Data_t touch_data;

#if TOUCH_USE_EEPROM_CAL
    AT24C02_Init();          // 检测 EEPROM 在位（I2C 外设由 MX_I2C1_Init 初始化）
#endif
    Touch_Init();            // 内部加载 EEPROM 校准参数（未校准过则用原始 AD 值）
    printf("Touch Init Done!\r\n");

    for (;;)
    {
        Touch_Scan(&touch_data);
        switch (touch_data.event)
        {
            case TOUCH_EVENT_PRESS_DOWN:
                printf("Touch DOWN: x=%u y=%u (raw %u,%u)\r\n",
                       touch_data.x, touch_data.y,
                       touch_data.raw_x, touch_data.raw_y);
                {
                    TouchEvt_t evt;
                    evt.raw_x = touch_data.raw_x;
                    evt.raw_y = touch_data.raw_y;
                    (void)osMessageQueuePut(q_touch_evtHandle, &evt, 0, 0);

                    /* 广播进 UI 事件枢纽（home 命中测试用屏幕坐标） */
                    UiEvt_t ue;
                    ue.type  = UIEVT_TOUCH;
                    ue.x     = touch_data.x;
                    ue.y     = touch_data.y;
                    ue.raw_x = touch_data.raw_x;
                    ue.raw_y = touch_data.raw_y;
                    (void)osMessageQueuePut(q_ui_evtHandle, &ue, 0, 0);
                    }
                    break;
            case TOUCH_EVENT_PRESS_UP:
                break;
            case TOUCH_EVENT_PRESS_HOLD:
                break;   // 拖拽需求时再启用，避免高频刷屏
            default:
                break;
        }
        osDelay(15);   // 每 15ms 扫描一次触摸
    }
}
