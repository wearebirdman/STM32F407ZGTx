#ifndef __TOUCH_H
#define __TOUCH_H

#include "main.h"

// ==================== 你只需要改这里 ====================
// SPI 模式选择：0 = 软件 SPI（GPIO 模拟，仅需 5 个普通 IO）
//               1 = 硬件 SPI（需额外配置一个 SPI 外设，模式 0：CPOL=0, CPHA=0）
#define TOUCH_SPI_MODE            0     // 0: 软件 SPI  1: 硬件 SPI

// 引脚配置（软件 SPI 与硬件 SPI 共用 CS / PEN 引脚）
#define TOUCH_CS_PORT             GPIOC // T_CS   片选
#define TOUCH_CS_PIN              GPIO_PIN_13
#define TOUCH_CLK_PORT            GPIOB // T_SCK  时钟（硬件 SPI 模式下忽略）
#define TOUCH_CLK_PIN             GPIO_PIN_0
#define TOUCH_MOSI_PORT           GPIOF // T_MOSI 写数据线（硬件 SPI 模式下忽略）
#define TOUCH_MOSI_PIN            GPIO_PIN_11
#define TOUCH_MISO_PORT           GPIOB // T_MISO 读数据线（硬件 SPI 模式下忽略）
#define TOUCH_MISO_PIN            GPIO_PIN_2
#define TOUCH_PEN_PORT            GPIOB // T_PEN  按下检测，低电平有效
#define TOUCH_PEN_PIN             GPIO_PIN_1

// 硬件 SPI 模式使用的 SPI 句柄（注意：XPT2046 要求 SPI 模式 0，
// 不能与 W25Qxx 等使用模式 3 的器件共用同一句柄，除非每次读写前重新初始化）
#if TOUCH_SPI_MODE == 1
#define TOUCH_SPI                 hspi1
#include "spi.h"
#endif

// 校准参数是否存入 EEPROM（1 = 启用，需工程中存在 EEPROM 模块）
#define TOUCH_USE_EEPROM_CAL      1

// EEPROM 访问抽象层：移植到其他存储介质（如 W25Qxx、内部 Flash）时，
// 只需替换下面 4 个宏的实现，touch.c 中的 Save/Load 无需改动。
// 注意：存储地址不在此拍板，统一引用 at24c02.h 中的 EEPROM 数据分配表。
#if TOUCH_USE_EEPROM_CAL
#include "at24c02.h"
#define TOUCH_EEPROM_OK               AT24C02_OK
#define TOUCH_EEPROM_WRITE(a, b, l)   AT24C02_Write((a), (b), (l))
#define TOUCH_EEPROM_READ(a, b, l)    AT24C02_Read((a), (b), (l))
#define TOUCH_CAL_EEPROM_ADDR         EEPROM_ADDR_TOUCH_CAL   // 校准参数起始地址
#endif

// PEN 引脚按下时的有效电平（四线电阻屏固定为低电平）
#define TOUCH_PEN_ACTIVE_LEVEL    GPIO_PIN_RESET
// ======================================================

// ==================== 滤波与精度参数 ====================
#define TOUCH_READ_TIMES          5     // 单轴去极值均值滤波的采样次数
#define TOUCH_LOST_VAL            1     // 滤波时去掉的最大/最小值个数
#define TOUCH_ERR_RANGE           50    // 两次读取允许的最大偏差（原始 AD 值）
#define TOUCH_SPI_TIMEOUT         100   // 硬件 SPI 单字节超时（ms）
// ======================================================

// ==================== XPT2046/ADS7846 命令字 ====================
#define TOUCH_CMD_X               0xD0  // 读 X 轴 AD 值
#define TOUCH_CMD_Y               0x90  // 读 Y 轴 AD 值
// ======================================================

// ==================== 触摸事件定义 ====================
#define TOUCH_EVENT_NONE          0x00  // 无事件
#define TOUCH_EVENT_PRESS_DOWN    0x01  // 按下瞬间
#define TOUCH_EVENT_PRESS_HOLD    0x02  // 持续按住
#define TOUCH_EVENT_PRESS_UP      0x04  // 释放瞬间
// ======================================================

// ==================== 错误码枚举 ====================
typedef enum {
    TOUCH_OK = 0,               // 成功
    TOUCH_ERR_PARAM,            // 参数错误
    TOUCH_ERR_NO_TOUCH,         // 未检测到触摸（读取超时）
    TOUCH_ERR_READ,             // 坐标读取失败（滤波校验不通过）
    TOUCH_ERR_CAL,              // 校准数据无效 / 校准采样不合格
    TOUCH_ERR_EEPROM,           // EEPROM 读写失败
} Touch_Error_t;

// ==================== 校准参数结构体 ====================
typedef struct {
    float   xfac;               // X 轴比例系数
    float   yfac;               // Y 轴比例系数
    int16_t xoff;               // X 轴偏移量
    int16_t yoff;               // Y 轴偏移量
    uint8_t swap;               // X/Y 轴交换标志（0/1）
    uint8_t valid;              // 校准数据是否有效（内部使用）
} Touch_Cal_t;

// ==================== 触摸数据结构体 ====================
typedef struct {
    uint16_t x;                 // 屏幕坐标 X（未校准时为原始 AD 值）
    uint16_t y;                 // 屏幕坐标 Y
    uint16_t raw_x;             // 原始 AD 值 X
    uint16_t raw_y;             // 原始 AD 值 Y
    uint8_t  pressed;           // 当前是否按下：0/1
    uint8_t  event;             // 本次扫描产生的事件（TOUCH_EVENT_xxx）
} Touch_Data_t;

// ==================== 校准取点回调类型 ====================
// 由使用者实现：在屏幕 (x, y) 处绘制十字并等待用户点击，
// 点击完成后将原始 AD 值写入 *raw_x / *raw_y。
// 返回 1 = 取点成功；返回 0 = 超时/取消（Touch_Adjust 将中止）
typedef uint8_t (*Touch_GetPointFn)(uint16_t x, uint16_t y,
                                    uint16_t *raw_x, uint16_t *raw_y);

// ==================== 函数声明 ====================
void          Touch_Init(void);                       // 初始化（配置 IO，加载校准）
uint8_t       Touch_IsPressed(void);                  // 查询当前是否按下
uint8_t       Touch_Scan(Touch_Data_t *data);         // 扫描一次，返回当前按下状态
Touch_Error_t Touch_ReadRawXY(uint16_t *raw_x, uint16_t *raw_y);  // 双重滤波读取原始坐标

void          Touch_SetCalibration(const Touch_Cal_t *cal);      // 手动设置校准参数
void          Touch_GetCalibration(Touch_Cal_t *cal);            // 获取当前校准参数
Touch_Error_t Touch_CalcCalibration(const uint16_t pos[4][2],
                                    uint16_t screen_w, uint16_t screen_h,
                                    uint16_t margin, Touch_Cal_t *cal); // 由 4 点原始坐标计算校准参数
Touch_Error_t Touch_Adjust(uint16_t screen_w, uint16_t screen_h,
                           uint16_t margin, Touch_GetPointFn get_point); // 交互式 4 点校准
#if TOUCH_USE_EEPROM_CAL
Touch_Error_t Touch_SaveCalibration(void);            // 校准参数存入 EEPROM
Touch_Error_t Touch_LoadCalibration(void);            // 从 EEPROM 加载校准参数
#endif

void          Touch_Example(void);                    // 使用示例

#endif /* __TOUCH_H */
