#include "ui_draw.h"
#include "ui_chrome.h"

#include <stdlib.h>

/* 画板参数定义 */
#define DRAW_POOL_MAX   512 /* 点池总点数（4KB .bss，bump 分配 LIFO 回收） */
#define DRAW_STROKE_MAX 16  /* 同时存在的笔画上限 */
#define DRAW_TOOLBAR_H  28  /* 工具栏高度 */
#define DRAW_MIN_DX     2   /* 相邻点最小间距（px，过近跳过省池） */
#define DRAW_LINE_W     3   /* 笔宽 */
#define DRAW_COLOR_NUM  4   /* 画笔颜色数 */

static const uint32_t s_PenColor[DRAW_COLOR_NUM] = {
    0x212121, /* 黑 */
    0xE53935, /* 红 */
    0x1E88E5, /* 蓝 */
    0x43A047, /* 绿 */
};

/* 笔画记录：线对象 + 点池区间 */
typedef struct {
    lv_obj_t *line;  /* lv_line 对象 */
    uint16_t  start; /* 点池起始下标 */
    uint16_t  cnt;   /* 点数 */
} DrawStroke_t;

static uint8_t s_Active;                        /* 页面激活标志（事件喂入开关） */
static uint8_t s_Drawing;                       /* 行笔中 */
static uint8_t s_ColorIdx;                      /* 当前画笔颜色 */
static uint16_t s_PoolUsed;                     /* 点池 bump 指针 */
static uint8_t s_StrokeNum;                     /* 当前笔画数 */
static int16_t s_OffX;                          /* 行笔开始时锁定的区域原点偏移 */
static int16_t s_OffY;                          /* 同上（Y 轴） */
static lv_point_precise_t s_Pool[DRAW_POOL_MAX]; /* 点池（笔画共享，静态存储） */
static DrawStroke_t s_Stroke[DRAW_STROKE_MAX];   /* 笔画记录表 */
static lv_obj_t *s_Area;                        /* 画板区（白底） */
static lv_obj_t *s_Scr;                         /* 本页屏对象（用于过滤滞后到达的 DELETE） */
static lv_obj_t *s_Dot[DRAW_COLOR_NUM];         /* 工具栏颜色圆点 */

/* 高亮选中色圆点 */
static void Draw_UpdateDotStyle(void)
{
    for (uint8_t i = 0; i < DRAW_COLOR_NUM; i++)
        lv_obj_set_style_outline_width(s_Dot[i], (i == s_ColorIdx) ? 2 : 0, 0);
}

/* 建一条新笔画的线对象（样式随当前颜色） */
static lv_obj_t *Draw_NewLine(void)
{
    lv_obj_t *line = lv_line_create(s_Area);

    lv_obj_set_style_line_width(line, DRAW_LINE_W, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(s_PenColor[s_ColorIdx]), 0);
    lv_obj_set_style_line_rounded(line, true, 0); /* 端点/拐角圆润（v9.6 单一开关） */
    return line;
}

/* 撤销最近一笔（行笔中禁止） */
static void Draw_UndoLast(void)
{
    if (s_Drawing || s_StrokeNum == 0)
        return;
    s_StrokeNum--;
    DrawStroke_t *st = &s_Stroke[s_StrokeNum];
    lv_obj_delete(st->line);
    s_PoolUsed = st->start; /* LIFO 回收池区间 */
}

/* 清空全部笔画（行笔中禁止） */
static void Draw_ClearAll(void)
{
    if (s_Drawing)
        return;
    for (uint8_t i = 0; i < s_StrokeNum; i++)
        lv_obj_delete(s_Stroke[i].line);
    s_StrokeNum = 0;
    s_PoolUsed = 0;
}

/* 颜色圆点点击：切换当前画笔颜色 */
static void Draw_OnColorClicked(lv_event_t *e)
{
    s_ColorIdx = (uint8_t)(lv_uintptr_t)lv_event_get_user_data(e);
    Draw_UpdateDotStyle();
}

/* Undo 按钮点击：撤销最近一笔 */
static void Draw_OnUndoClicked(lv_event_t *e)
{
    (void)e;
    Draw_UndoLast();
}

/* Clear 按钮点击：清空全部笔画 */
static void Draw_OnClearClicked(lv_event_t *e)
{
    (void)e;
    Draw_ClearAll();
}

/* 建工具栏按钮（文字居中） */
static lv_obj_t *Draw_ToolButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);

    lv_obj_set_size(btn, 44, 24);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_center(lab);
    return btn;
}

/* 页面销毁：停喂事件（仅当销毁的是本页，避免旧页动画滞后 DELETE 关掉新页） */
static void Draw_OnDelete(lv_event_t *e)
{
    if (lv_event_get_target(e) != s_Scr)
        return;
    s_Scr = NULL;
    s_Active = 0;
    s_Drawing = 0;
}

/* 画板页工厂：工具栏 + 白底画板区 */
lv_obj_t *Ui_DrawCreate(UiPageId_t id)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *content;

    (void)id;
    Ui_ChromeBuild(scr, &content);
    /* content 没有布局时子对象会以固定默认尺寸堆叠在同一原点，
     * 必须显式设为纵向 flex，bar 取固定高、s_Area 靠 flex_grow 吃掉剩余高度 */
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    s_Scr = scr;
    s_Active = 1;
    s_Drawing = 0;
    s_PoolUsed = 0;
    s_StrokeNum = 0;
    s_ColorIdx = 0;

    /* 工具栏：4 色圆点 + Undo + Clear */
    lv_obj_t *bar = lv_obj_create(content);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), DRAW_TOOLBAR_H);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < DRAW_COLOR_NUM; i++)
    {
        s_Dot[i] = lv_button_create(bar);
        lv_obj_remove_style_all(s_Dot[i]);
        lv_obj_set_size(s_Dot[i], 22, 22);
        lv_obj_set_style_radius(s_Dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_Dot[i], lv_color_hex(s_PenColor[i]), 0);
        lv_obj_set_style_bg_opa(s_Dot[i], LV_OPA_COVER, 0);
        lv_obj_set_style_outline_color(s_Dot[i], lv_color_hex(0x37474F), 0);
        lv_obj_add_event_cb(s_Dot[i], Draw_OnColorClicked, LV_EVENT_CLICKED, (void *)(lv_uintptr_t)i);
    }

    Draw_ToolButton(bar, "Undo", Draw_OnUndoClicked);
    Draw_ToolButton(bar, "Clear", Draw_OnClearClicked);

    /* 画板区：白底，flex 生长填满剩余 */
    s_Area = lv_obj_create(content);
    lv_obj_remove_style_all(s_Area);
    lv_obj_set_width(s_Area, LV_PCT(100));
    lv_obj_set_flex_grow(s_Area, 1);
    /* 笔画线对象按点集自动撑大尺寸，若画板区可滚动，行笔中会因滚动改变
     * lv_obj_get_coords 原点，锁定的 s_OffX/s_OffY 随之失配，必须禁止滚动 */
    lv_obj_set_scrollable(s_Area, false);
    lv_obj_set_style_bg_color(s_Area, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_Area, LV_OPA_COVER, 0);

    Draw_UpdateDotStyle();
    lv_obj_add_event_cb(scr, Draw_OnDelete, LV_EVENT_DELETE, NULL);
    return scr;
}

/* BACK：撤销最近一笔 */
void Ui_DrawBack(void)
{
    if (s_Active)
        Draw_UndoLast();
}

/* NEXT：循环切换画笔颜色 */
void Ui_DrawNext(void)
{
    if (!s_Active)
        return;
    s_ColorIdx = (uint8_t)((s_ColorIdx + 1) % DRAW_COLOR_NUM);
    Draw_UpdateDotStyle();
}

/* 触摸事件喂入：区域内按下起笔，HOLD 追加点，UP 收笔 */
void Ui_DrawFeed(const TouchMsg_t *msg)
{
    if (!s_Active)
        return;

    if (msg->event == TOUCH_EVENT_PRESS_DOWN)
    {
        lv_area_t rc;
        lv_obj_get_coords(s_Area, &rc);
        int32_t lx = (int32_t)msg->x - rc.x1;
        int32_t ly = (int32_t)msg->y - rc.y1;
        if (lx < 0 || ly < 0 || lx >= lv_area_get_width(&rc) || ly >= lv_area_get_height(&rc))
            return; /* 画板区外按下（工具栏/导航栏等）不起笔 */
        if (s_StrokeNum >= DRAW_STROKE_MAX || s_PoolUsed >= DRAW_POOL_MAX)
            return; /* 资源耗尽 */

        s_OffX = (int16_t)rc.x1; /* 行笔期间布局不变，锁定偏移 */
        s_OffY = (int16_t)rc.y1;
        s_Drawing = 1;
        DrawStroke_t *st = &s_Stroke[s_StrokeNum];
        st->line = Draw_NewLine();
        st->start = s_PoolUsed;
        s_Pool[s_PoolUsed].x = lx;
        s_Pool[s_PoolUsed].y = ly;
        s_PoolUsed++;
        st->cnt = 1;
        lv_line_set_points(st->line, &s_Pool[st->start], st->cnt);
        s_StrokeNum++;
    }
    else if (msg->event == TOUCH_EVENT_PRESS_HOLD)
    {
        if (!s_Drawing || s_StrokeNum == 0)
            return;
        DrawStroke_t *st = &s_Stroke[s_StrokeNum - 1];
        lv_point_precise_t *last = &s_Pool[st->start + st->cnt - 1];
        int32_t lx = (int32_t)msg->x - s_OffX;
        int32_t ly = (int32_t)msg->y - s_OffY;
        if (abs((int32_t)last->x - lx) < DRAW_MIN_DX && abs((int32_t)last->y - ly) < DRAW_MIN_DX)
            return; /* 距上一点过近，跳过省池 */
        if (s_PoolUsed >= DRAW_POOL_MAX)
            return; /* 池满：笔画继续但不再记录 */

        s_Pool[s_PoolUsed].x = lx;
        s_Pool[s_PoolUsed].y = ly;
        s_PoolUsed++;
        st->cnt++;
        lv_line_set_points(st->line, &s_Pool[st->start], st->cnt);
    }
    else if (msg->event == TOUCH_EVENT_PRESS_UP)
    {
        if (s_Drawing && s_StrokeNum > 0)
        {
            DrawStroke_t *st = &s_Stroke[s_StrokeNum - 1];
            /* lv_line 少于 2 个点不绘制，且零长度线段会被底层直接丢弃：
             * 单点笔画（轻点不拖动）补一个相邻点，配合圆端点画成一个圆点 */
            if (st->cnt == 1 && s_PoolUsed < DRAW_POOL_MAX)
            {
                s_Pool[s_PoolUsed].x = s_Pool[st->start].x + 1;
                s_Pool[s_PoolUsed].y = s_Pool[st->start].y;
                s_PoolUsed++;
                st->cnt++;
                lv_line_set_points(st->line, &s_Pool[st->start], st->cnt);
            }
        }
        s_Drawing = 0;
    }
}
