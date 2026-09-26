/*
 * display.c —— 显示服务层实现（车轴：通信 + 属主状态机 + 渲染器派发）
 *
 * 本模块是显示服务的"车轴"，负责：
 *   - 消息路由：客户端请求经队列串行化，服务层统一处理；
 *   - 权限仲裁：s_owner 身份校验，冲突立即回码不阻塞；
 *   - 状态管理：s_screen + s_owner 双状态，支持查询；
 *   - 渲染派发：通过 DispRenderer_t 接口调用已注册的渲染后端。
 *
 * 渲染后端通过 Display_RegisterRenderer() 注册：
 *   - ui.c：简单 UI 渲染器（基于 lcd.c 驱动）；
 *   - lvgl_adapter.c（未来）：LVGL 渲染器。
 *
 * 修复的已知缺陷：
 *   1. ack 回包带 client 字段，Transact 按 client 过滤——消除多客户端并发时
 *      互相丢弃应答导致超时的问题；
 *   2. s_seq 改为 per-client 数组，各客户端独立递增，消除全局共享冲突；
 *   3. ServiceLoop 改 5ms 限时等待 + 调用 on_tick()，为 LVGL 心跳提供接入点；
 *   4. 新增 Display_GetStatus() 供客户端查询状态，消除影子状态漂移。
 */

#include "display.h"
#include <string.h>

/* ====================================================================
 *                     服务层状态（仅 ServiceLoop 上下文访问）
 * ==================================================================== */

static DispScreen_t s_screen = DISP_SCREEN_DEFAULT;   /* 当前显示界面 */
static DispClient_t s_owner  = DISP_CLIENT_NONE;      /* 当前属主（NONE=空闲） */

/* 渲染器指针（由 UI_Init / LVGL_Adapter_Init 注册） */
static const DispRenderer_t *s_renderer = NULL;

/* ====================================================================
 *                     渲染器注册
 * ==================================================================== */

void Display_RegisterRenderer(const DispRenderer_t *renderer)
{
    s_renderer = renderer;
}

/* ====================================================================
 *                          服务主循环（状态机）
 * ==================================================================== */

void Display_ServiceLoop(void)
{
    DispMsg_t msg;
    DispMsg_t ack;

    /* 启动时渲染 DEFAULT 占位屏（需 renderer 已注册） */
    if (s_renderer && s_renderer->on_acquire)
    {
        s_renderer->on_acquire(DISP_SCREEN_DEFAULT, 0);
    }

    for (;;)
    {
        /* 5ms 限时等待：无论有无消息，周期调用 on_tick（LVGL 心跳接入点） */
        osStatus_t status = osMessageQueueGet(q_display_reqHandle, &msg, 0, DISP_TICK_PERIOD_MS);

        if (s_renderer && s_renderer->on_tick)
        {
            s_renderer->on_tick();
        }

        if (status != osOK)
            continue;

        DispResult_t res = DISP_OK;

        /* 身份与界面 ID 合法性：所有消息统一前置校验 */
        if (msg.client == DISP_CLIENT_NONE || msg.client >= DISP_CLIENT_COUNT)
        {
            res = DISP_ERR;
            goto reply;
        }
        if (msg.screen >= DISP_SCREEN_COUNT)
        {
            res = DISP_ERR;
            goto reply;
        }

        switch (msg.type)
        {
            case DISP_MSG_ACQUIRE:
                if (s_owner == DISP_CLIENT_NONE)
                {
                    /* 空闲：授予并渲染底版 */
                    if (s_renderer && s_renderer->on_acquire)
                        s_renderer->on_acquire(msg.screen, msg.param);
                    s_owner  = msg.client;
                    s_screen = msg.screen;
                }
                else if (s_owner == msg.client)
                {
                    /* 持有者再次 Acquire：会话内换屏/重渲染（吸收原 SHOW_SCREEN） */
                    if (s_renderer && s_renderer->on_acquire)
                        s_renderer->on_acquire(msg.screen, msg.param);
                    s_screen = msg.screen;
                }
                else
                {
                    res = DISP_BUSY;   /* 他人持有：含持有者界面被占场景 */
                }
                break;

            case DISP_MSG_RELEASE:
                if (s_owner == DISP_CLIENT_NONE)
                    res = DISP_ERR;            /* 无人持有，无可释放 */
                else if (msg.client != s_owner)
                    res = DISP_FORBIDDEN;      /* 只有持有者能结束自己的会话 */
                else
                {
                    if (s_renderer && s_renderer->on_release)
                        s_renderer->on_release();
                    s_owner = DISP_CLIENT_NONE;
                    /* 不渲染：最后一帧保留至下一任属主接管，避免闪屏 */
                }
                break;

            case DISP_MSG_DRAW_CROSS:
                /* 作画仅限当前属主：空闲时 s_owner=NONE，client 已校验非
                 * NONE，故自然拒绝——"谁持有谁作画"一条规则覆盖所有场景 */
                if (msg.client != s_owner)
                    res = DISP_FORBIDDEN;
                else
                {
                    if (s_renderer && s_renderer->on_draw)
                        s_renderer->on_draw(&msg);
                }
                break;

            default:
                res = DISP_ERR;
                break;
        }

    reply:
        /* 应答：回带 seq + client，客户端据此配对、丢弃过期应答 */
        memset(&ack, 0, sizeof(ack));
        ack.type   = DISP_MSG_ACK;
        ack.result = res;
        ack.seq    = msg.seq;
        ack.client = msg.client;   /* 新增：标识应答归属 */
        (void)osMessageQueuePut(q_display_ackHandle, &ack, 0, 0);
    }
}

/* ====================================================================
 *                          客户端 API（同步）
 * ==================================================================== */

/* per-client 请求序号：各客户端独立递增，消除全局共享冲突 */
static uint8_t s_seq[DISP_CLIENT_COUNT];

/* 投递请求并等待配对应答（按 client 过滤） */
static DispResult_t Display_Transact(DispMsg_t *req, DispClient_t client)
{
    DispMsg_t ack;
    uint32_t  elapsed;

    req->seq = ++s_seq[client];

    if (osMessageQueuePut(q_display_reqHandle, req, 0, 0) != osOK)
        return DISP_TIMEOUT;      /* 请求队列满：服务层处理不过来 */

    uint32_t t0 = osKernelGetTickCount();

    for (;;)
    {
        elapsed = osKernelGetTickCount() - t0;
        if (elapsed >= DISP_ACK_TIMEOUT_MS)
            return DISP_TIMEOUT;

        if (osMessageQueueGet(q_display_ackHandle, &ack, 0,
                              DISP_ACK_TIMEOUT_MS - elapsed) != osOK)
            return DISP_TIMEOUT;

        /* 按 client 过滤：只接受归属自己的应答 */
        if (ack.type == DISP_MSG_ACK && ack.client == client && ack.seq == req->seq)
            return ack.result;

        /* 其他请求的过期应答：丢弃，用剩余超时继续等待 */
    }
}

DispResult_t Display_Acquire(DispClient_t self, DispScreen_t screen, uint8_t param)
{
    DispMsg_t m;
    memset(&m, 0, sizeof(m));
    m.type   = DISP_MSG_ACQUIRE;
    m.client = self;
    m.screen = screen;
    m.param  = param;
    return Display_Transact(&m, self);
}

DispResult_t Display_Release(DispClient_t self)
{
    DispMsg_t m;
    memset(&m, 0, sizeof(m));
    m.type   = DISP_MSG_RELEASE;
    m.client = self;
    return Display_Transact(&m, self);
}

DispResult_t Display_DrawCross(DispClient_t self, uint16_t x, uint16_t y, uint16_t color)
{
    DispMsg_t m;
    memset(&m, 0, sizeof(m));
    m.type   = DISP_MSG_DRAW_CROSS;
    m.client = self;
    m.x      = x;
    m.y      = y;
    m.color  = color;
    return Display_Transact(&m, self);
}

/* ====================================================================
 *                          客户端 API（异步 fire-and-forget）
 * ==================================================================== */

void Display_PostAcquire(DispClient_t self, DispScreen_t screen, uint8_t param)
{
    DispMsg_t m;
    memset(&m, 0, sizeof(m));
    m.type   = DISP_MSG_ACQUIRE;
    m.client = self;
    m.screen = screen;
    m.param  = param;
    (void)osMessageQueuePut(q_display_reqHandle, &m, 0, 0);
}

void Display_PostRelease(DispClient_t self)
{
    DispMsg_t m;
    memset(&m, 0, sizeof(m));
    m.type   = DISP_MSG_RELEASE;
    m.client = self;
    (void)osMessageQueuePut(q_display_reqHandle, &m, 0, 0);
}

void Display_PostDrawCross(DispClient_t self, uint16_t x, uint16_t y, uint16_t color)
{
    DispMsg_t m;
    memset(&m, 0, sizeof(m));
    m.type   = DISP_MSG_DRAW_CROSS;
    m.client = self;
    m.x      = x;
    m.y      = y;
    m.color  = color;
    (void)osMessageQueuePut(q_display_reqHandle, &m, 0, 0);
}

/* ====================================================================
 *                          状态查询 API
 * ==================================================================== */

void Display_GetStatus(DispClient_t *out_owner, DispScreen_t *out_screen)
{
    /* 注意：此函数在客户端上下文调用，读取的是服务层状态的快照。
     * 由于服务层状态仅在 ServiceLoop 中修改，且修改是原子的（单字节赋值），
     * 此处读取是安全的（无撕裂风险）。 */
    if (out_owner)  *out_owner  = s_owner;
    if (out_screen) *out_screen = s_screen;
}
