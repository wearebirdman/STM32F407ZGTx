#include "led.h"

// ==================== LED 基本操作 ====================

/**
 * @brief  点亮指定 LED
 * @param  led: LED 编号
 */
void led_on(led_t led)
{
    switch (led)
    {
        case LED1:
            HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, LED1_ON_LEVEL);
            break;
        case LED2:
            HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, LED2_ON_LEVEL);
            break;
        case LED3:
            HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, LED3_ON_LEVEL);
            break;
        default:
            break;
    }
}

/**
 * @brief  熄灭指定 LED
 * @param  led: LED 编号
 */
void led_off(led_t led)
{
    switch (led)
    {
        case LED1:
            HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, LED1_OFF_LEVEL);
            break;
        case LED2:
            HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, LED2_OFF_LEVEL);
            break;
        case LED3:
            HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, LED3_OFF_LEVEL);
            break;
        default:
            break;
    }
}

/**
 * @brief  翻转指定 LED 状态
 * @param  led: LED 编号
 */
void led_toggle(led_t led)
{
    switch (led)
    {
        case LED1:
            HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
            break;
        case LED2:
            HAL_GPIO_TogglePin(LED2_PORT, LED2_PIN);
            break;
        case LED3:
            HAL_GPIO_TogglePin(LED3_PORT, LED3_PIN);
            break;
        default:
            break;
    }
}
