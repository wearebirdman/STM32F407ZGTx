#ifndef __LED_H
#define __LED_H

#include "main.h"

// ==================== LED 引脚定义 ====================
// 低电平点亮
#define LED1_PIN              GPIO_PIN_8
#define LED1_PORT             GPIOF
#define LED1_ON_LEVEL         GPIO_PIN_RESET
#define LED1_OFF_LEVEL        GPIO_PIN_SET

#define LED2_PIN              GPIO_PIN_9
#define LED2_PORT             GPIOF
#define LED2_ON_LEVEL         GPIO_PIN_RESET
#define LED2_OFF_LEVEL        GPIO_PIN_SET

#define LED3_PIN              GPIO_PIN_10
#define LED3_PORT             GPIOF
#define LED3_ON_LEVEL         GPIO_PIN_RESET
#define LED3_OFF_LEVEL        GPIO_PIN_SET

// ==================== LED 编号枚举 ====================
typedef enum {
    LED1 = 0,
    LED2,
    LED3,
} led_t;

// ==================== 函数声明 ====================
void led_on(led_t led);
void led_off(led_t led);
void led_toggle(led_t led);

#endif /* __LED_H */

