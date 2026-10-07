#include "ui.h"
#include "ui_chrome.h"
#include "lvgl.h"

/* 图块描述：标签 + 目标页 + 主题色 */
typedef struct {
    const char *name;  /* 图块文字 */
    uint8_t     page;  /* 目标页（UiPageId_t） */
    uint32_t    color; /* 图块底色（RGB888） */
} HomeTile_t;

#define HOME_TILE_NUM   6    /* 2 列 × 3 行 */
#define HOME_MENU_PAGES 1    /* 菜单页数；加页时扩展 s_TileList 并改此值 */
#define HOME_ANIM_MS    250  /* 菜单翻页滑入动画时长 */

static const HomeTile_t s_TileList[HOME_TILE_NUM] = {
    { "Calib",  UI_PAGE_CALIB,  0xE53935 }, /* 触摸校准 */
    { "Draw",   UI_PAGE_DRAW,   0x1E88E5 }, /* 画板 */
    { "Osc",    UI_PAGE_OSC,    0x43A047 }, /* 示波器 */
    { "Serial", UI_PAGE_SERIAL, 0xFB8C00 }, /* 串口助手 */
    { "Set",    UI_PAGE_SET,    0x8E24AA }, /* 系统设置 */
    { "About",  UI_PAGE_ABOUT,  0x757575 }, /* 关于 */
};

static uint8_t s_MenuPage; /* 当前菜单页（0 起） */

/* 图块点击：跳转到对应 App 页 */
static void Home_OnTileClicked(lv_event_t *e)
{
    const HomeTile_t *tile = lv_event_get_user_data(e);

    Ui_Push((UiPageId_t)tile->page);
}

/* 重建主菜单并按方向滑入（菜单翻页用） */
static void Home_SwitchAnim(lv_scr_load_anim_t dir)
{
    lv_obj_t *scr = Ui_CreateHome(UI_PAGE_HOME);

    lv_screen_load_anim(scr, dir, HOME_ANIM_MS, 0, true);
}

/* 主菜单 BACK：翻到上一页菜单 */
void Ui_HomeBack(void)
{
    if (s_MenuPage == 0)
        return;
    s_MenuPage--;
    Home_SwitchAnim(LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

/* 主菜单 NEXT：翻到下一页菜单 */
void Ui_HomeNext(void)
{
    if (s_MenuPage + 1 >= HOME_MENU_PAGES)
        return;
    s_MenuPage++;
    Home_SwitchAnim(LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

/* 主菜单页工厂：2×3 彩色图块网格 */
lv_obj_t *Ui_CreateHome(UiPageId_t id)
{
    static lv_coord_t col_dsc[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t row_dsc[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *content;
    uint32_t col = 0;
    uint32_t row = 0;

    (void)id;
    Ui_ChromeBuild(scr, &content);
    lv_obj_set_grid_dsc_array(content, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(content, 6, 0); /* 图块横向间隙 */
    lv_obj_set_style_pad_row(content, 6, 0);    /* 图块纵向间隙 */

    for (uint32_t i = 0; i < HOME_TILE_NUM; i++)
    {
        lv_obj_t *tile = lv_button_create(content);
        lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_set_style_bg_color(tile, lv_color_hex(s_TileList[i].color), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(tile, 8, 0);
        lv_obj_add_event_cb(tile, Home_OnTileClicked, LV_EVENT_CLICKED, (void *)&s_TileList[i]);
        col++;
        if (col >= 2)
        {
            col = 0;
            row++;
        }

        lv_obj_t *lab = lv_label_create(tile);
        lv_label_set_text(lab, s_TileList[i].name);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lab, lv_color_white(), 0);
        lv_obj_center(lab);
    }
    return scr;
}
