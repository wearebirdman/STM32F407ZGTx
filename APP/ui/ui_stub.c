#include "ui.h"
#include "ui_chrome.h"
#include "lvgl.h"

/* 占位页标题表：下标 = UiPageId_t */
static const char *const s_stub_title[UI_PAGE_COUNT] = {
    "",             // UI_PAGE_HOME（不走占位）
    "Touch Calib",  // UI_PAGE_CALIB
    "Draw Pad",     // UI_PAGE_DRAW
    "Oscilloscope", // UI_PAGE_OSC
    "Serial",       // UI_PAGE_SERIAL（真实页已接管，此槽仅保持下标对齐）
    "Settings",     // UI_PAGE_SET
    "About",        // UI_PAGE_ABOUT
};

/* 占位页工厂：居中标题 + Coming Soon */
lv_obj_t *Ui_StubCreate(UiPageId_t id)
{
    lv_obj_t *scr;
    lv_obj_t *content;
    lv_obj_t *lab;

    if (id >= UI_PAGE_COUNT)
        return NULL;

    scr = lv_obj_create(NULL);
    Ui_ChromeBuild(scr, &content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lab = lv_label_create(content);
    lv_label_set_text(lab, s_stub_title[id]);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);

    lab = lv_label_create(content);
    lv_label_set_text(lab, "Coming Soon...");
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lab, lv_color_hex(0x9E9E9E), 0);
    return scr;
}
