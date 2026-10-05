#include "ui_port.h"
#include "ui.h"
#include "ui_chrome.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl.h"

#define UI_BUF_LINES 80 // 绘制缓冲行数：240 * 80 * 2 = 38400 字节，占 CCMRAM

/* 绘制缓冲放 CCMRAM：主 SRAM 全留给 FreeRTOS 堆与 LVGL 对象堆；
   零初始化数组按 .ccmram 段以 NOBITS 放置，启动无需拷贝 */
static uint8_t s_ui_buf[UI_BUF_LINES * LCD_WIDTH * 2]
    __attribute__((section(".ccmram"), aligned(4)));

extern osMessageQueueId_t q_TouchMsgHandle; // 触摸消息队列（touch_task 生产）

/* LVGL flush 回调：把渲染好的脏区搬运到 GRAM */
static void Ui_FlushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    LCD_FillBitmap((uint16_t)area->x1, (uint16_t)area->y1,
                   (uint16_t)(area->x2 - area->x1 + 1),
                   (uint16_t)(area->y2 - area->y1 + 1),
                   (const uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

/* LVGL 输入回调：非阻塞排空触摸队列，保留最后状态；q_TouchMsg 的唯一消费者 */
static void Ui_TouchReadCb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static uint16_t s_x;    // 最后有效坐标
    static uint16_t s_y;
    static uint8_t  s_down; // 最后按压状态
    TouchMsg_t      msg;

    (void)indev;
    while (osMessageQueueGet(q_TouchMsgHandle, &msg, NULL, 0) == osOK)
    {
        if (msg.event == TOUCH_EVENT_PRESS_UP)
            s_down = 0;
        else
        {
            s_x = msg.x;
            s_y = msg.y;
            s_down = 1;
        }
    }

    data->point.x = s_x;
    data->point.y = s_y;
    data->state = s_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* 初始化 LVGL：时钟源、显示（PARTIAL 单缓冲）、触摸输入，最后建界面 */
void Ui_PortInit(void)
{
    lv_init();
    lv_tick_set_cb(HAL_GetTick); // HAL 时基走 TIM，SysTick 归 FreeRTOS，互不干扰

    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, s_ui_buf, NULL, sizeof(s_ui_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, Ui_FlushCb);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, Ui_TouchReadCb);
    lv_indev_set_display(indev, disp);

    Ui_Home(); // 建主菜单并加载
}
