#include "ui.h"
#include "lvgl.h"

#include <stdio.h>

static lv_obj_t *s_home_label; // 主屏演示标签，接收跨任务文本消息

/* 校准按钮点击回调：投递消息，演示单向数据流 */
static void Ui_OnCalibClicked(lv_event_t *e)
{
    UiMsg_t msg = { .type = UI_MSG_REQ_CALIB };

    (void)e;
    Ui_PostMsg(&msg);
}

/* 创建主屏：标题标签 + 校准按钮，并加载为当前屏 */
void Ui_CreateHome(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);

    s_home_label = lv_label_create(scr);
    lv_label_set_text(s_home_label, "STM32F407 + LVGL 9.6");
    lv_obj_set_style_text_font(s_home_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_home_label, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 140, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn, Ui_OnCalibClicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Touch Calib");
    lv_obj_center(btn_label);

    lv_screen_load(scr);
}

/* 应用 UI 消息：只允许在 lcd_task 上下文调用（LVGL 非线程安全） */
void Ui_ApplyMsg(const UiMsg_t *msg)
{
    if (msg->type >= UI_MSG_COUNT)
        return;

    switch (msg->type)
    {
        case UI_MSG_SET_TEXT:
            lv_label_set_text(s_home_label, msg->data.text);
            break;

        case UI_MSG_REQ_CALIB:
            printf("calib requested\r\n"); // TODO: 校准流程（建议留在 LVGL 外用 LCD 直绘）
            break;

        default:
            break;
    }
}
