#include "key.h"

// ==================== 按键状态定义 ====================
typedef enum {
    KEY_STATE_IDLE = 0,          // 空闲
    KEY_STATE_DEBOUNCE,          // 消抖中
    KEY_STATE_PRESSED,           // 已确认按下
    KEY_STATE_WAIT_RELEASE,      // 等待释放/双击窗口
    KEY_STATE_LONG_PRESS_HOLD,   // 长按保持
} KeyState_t;

// ==================== 按键上下文结构体 ====================
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_level;
    KeyState_t state;       // 按键状态
    uint32_t tick_start;    // 当前状态进入时间戳
    uint8_t click_count;    // 已完成的短按次数（用于双击判定）
} KeyContext_t;

static KeyContext_t s_keyCtx[KEY_NUM];    // 按键上下文数组
static uint8_t s_initialized = 0;         // 初始化标志

/**
 * @brief  读取按键当前电平状态
 * @param  key_id: 按键ID
 * @retval 0 = 未按下, 1 = 按下
 */
static uint8_t Key_GetState(uint8_t key_id)
{
    uint8_t pin_state = HAL_GPIO_ReadPin(s_keyCtx[key_id].port, s_keyCtx[key_id].pin);
    return (pin_state == s_keyCtx[key_id].active_level) ? 1 : 0;
}

/**
 * @brief  初始化按键
 * @note   配置各按键端口参数并复位状态机，需在调用 Key_Scan() 前执行
 */
void Key_Init(void)
{
    s_keyCtx[KEY_ID_WK].port = KEY_WK_PORT;
    s_keyCtx[KEY_ID_WK].pin = KEY_WK_PIN;
    s_keyCtx[KEY_ID_WK].active_level = KEY_WK_ACTIVE_LEVEL;

    s_keyCtx[KEY_ID_1].port = KEY_1_PORT;
    s_keyCtx[KEY_ID_1].pin = KEY_1_PIN;
    s_keyCtx[KEY_ID_1].active_level = KEY_1_ACTIVE_LEVEL;

    s_keyCtx[KEY_ID_2].port = KEY_2_PORT;
    s_keyCtx[KEY_ID_2].pin = KEY_2_PIN;
    s_keyCtx[KEY_ID_2].active_level = KEY_2_ACTIVE_LEVEL;

    s_keyCtx[KEY_ID_3].port = KEY_3_PORT;
    s_keyCtx[KEY_ID_3].pin = KEY_3_PIN;
    s_keyCtx[KEY_ID_3].active_level = KEY_3_ACTIVE_LEVEL;

    for (uint8_t i = KEY_WK_UNUSED; i < KEY_NUM; i++)
    {
        s_keyCtx[i].state = KEY_STATE_IDLE;
        s_keyCtx[i].tick_start = 0;
        s_keyCtx[i].click_count = 0;
    }

    s_initialized = 1;
}

/**
 * @brief  扫描按键状态机（需周期性调用）
 * @retval 按键消息，无事件时 event 为 KEY_EVENT_NONE
 */
KeyMsg_t Key_Scan(void)
{
    KeyMsg_t msg = {KEY_ID_NONE, KEY_EVENT_NONE};

    if (!s_initialized)
        return msg;

    uint32_t current_tick = HAL_GetTick();

    for (uint8_t i = KEY_WK_UNUSED; i < KEY_NUM; i++)
    {
        uint8_t pin_state = Key_GetState(i);
        KeyContext_t *ctx = &s_keyCtx[i];

        switch (ctx->state)
        {
            case KEY_STATE_IDLE:
                if (pin_state == 1)
                {
                    ctx->state = KEY_STATE_DEBOUNCE;
                    ctx->tick_start = current_tick;
                }
                break;

            case KEY_STATE_DEBOUNCE:
                if (pin_state == 1)
                {
                    if (current_tick - ctx->tick_start >= KEY_DEBOUNCE_MS)
                    {
                        // 消抖通过
                        if (ctx->click_count >= 1)
                        {
                            // 第二次按下 → 双击
                            msg.key_id = (KeyID_t)i;
                            msg.event = KEY_EVENT_DOUBLE_CLICK;
                            ctx->state = KEY_STATE_LONG_PRESS_HOLD;
                            ctx->click_count = 0;
                            return msg;
                        }
                        else
                        {
                            // 第一次按下
                            ctx->state = KEY_STATE_PRESSED;
                            ctx->tick_start = current_tick;
                        }
                    }
                    // 消抖未通过，继续等待
                }
                else
                {
                    // 抖动恢复
                    if (ctx->click_count > 0)
                    {
                        // 属于双击窗口内的第二次按下抖动，回到等待状态继续等超时
                        ctx->state = KEY_STATE_WAIT_RELEASE;
                        ctx->tick_start = current_tick;
                    }
                    else
                    {
                        ctx->state = KEY_STATE_IDLE;
                    }
                }
                break;

            case KEY_STATE_PRESSED:
                if (pin_state == 0)
                {
                    // 短按释放
                    ctx->click_count++;
                    ctx->state = KEY_STATE_WAIT_RELEASE;
                    ctx->tick_start = current_tick;
                }
                else if (current_tick - ctx->tick_start >= KEY_LONG_PRESS_MS)
                {
                    // 长按触发
                    msg.key_id = (KeyID_t)i;
                    msg.event = KEY_EVENT_LONG_PRESS;
                    ctx->state = KEY_STATE_LONG_PRESS_HOLD;
                    ctx->click_count = 0;
                    return msg;
                }
                break;

            case KEY_STATE_WAIT_RELEASE:
                if (pin_state == 1)
                {
                    // 双击窗口内又按下
                    ctx->state = KEY_STATE_DEBOUNCE;
                    ctx->tick_start = current_tick;
                }
                else if (current_tick - ctx->tick_start >= KEY_DOUBLE_CLICK_MS)
                {
                    // 双击窗口超时 → 判定为短按
                    msg.key_id = (KeyID_t)i;
                    msg.event = KEY_EVENT_SHORT_PRESS;
                    ctx->state = KEY_STATE_IDLE;
                    ctx->click_count = 0;
                    return msg;
                }
                break;

            case KEY_STATE_LONG_PRESS_HOLD:
                if (pin_state == 0)
                {
                    ctx->state = KEY_STATE_IDLE;
                    ctx->click_count = 0;
                }
                break;

            default:
                ctx->state = KEY_STATE_IDLE;
                ctx->click_count = 0;
                break;
        }
    }

    return msg;
}
