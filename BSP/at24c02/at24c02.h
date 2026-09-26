#ifndef __AT24C02_H
#define __AT24C02_H

#include "main.h"

// ==================== 你只需要改这里 ====================
#define AT24C02_I2C        hi2c1          // 使用的I2C句柄
#define AT24C02_DEV_ADDR   0x50           // 7位设备地址（常用0x50，写0xA0/读0xA1）
// ======================================================

// ==================== 芯片容量参数 ====================
#define AT24C02_PAGE_SIZE          16      // 页大小：16字节
#define AT24C02_CHIP_SIZE          256     // 总容量：256字节（2Kbit）
#define AT24C02_PAGE_COUNT         16      // 页数量 (256 / 16)

// ==================== 常用地址宏 ====================
#define AT24C02_TEST_ADDR          0x00    // 测试地址

// ==================== EEPROM 数据分配表 ====================
// 所有模块的持久化数据地址统一由本表管理（256 字节预算小、重叠代价高，
// 集中一张表才能一眼看出冲突）。新增数据区时：
//   1. 在此定义 ADDR / SIZE
//   2. 各外设头文件只引用本表宏，不自行拍地址
// 当前分配：
//   0x00 ~ 0x0F  测试区（AT24C02_Example，16 字节）
//   0x28 ~ 0x38  触摸校准参数（17 字节）
//   空闲起始：   0x39
#define EEPROM_ADDR_TOUCH_CAL      40      // 触摸校准参数起始地址
#define EEPROM_SIZE_TOUCH_CAL      17      // 触摸校准参数长度

// ==================== 错误码枚举 ====================
typedef enum {
    AT24C02_OK = 0,               // 成功
    AT24C02_ERR_DEVICE,           // 设备未应答（未检测到）
    AT24C02_ERR_TIMEOUT,          // 超时（写周期等待超时）
    AT24C02_ERR_ADDR,             // 地址越界
    AT24C02_ERR_PARAM,            // 参数错误
} AT24C02_Error_t;

// ==================== 芯片信息结构体 ====================
typedef struct {
    uint32_t size_bytes;          // 总容量（字节）
    uint32_t page_size;           // 页大小（字节）
    uint32_t page_count;          // 页数量
    uint16_t dev_addr;            // 7位设备地址
} AT24C02_Info_t;

// ==================== 函数声明 ====================
AT24C02_Error_t AT24C02_Init(void);
void AT24C02_GetInfo(AT24C02_Info_t *info);
AT24C02_Error_t AT24C02_Read(uint16_t addr, uint8_t *buf, uint16_t len);
AT24C02_Error_t AT24C02_PageWrite(uint16_t addr, uint8_t *buf, uint16_t len);
AT24C02_Error_t AT24C02_Write(uint16_t addr, uint8_t *buf, uint16_t len);

void AT24C02_Example(void);

#endif /* __AT24C02_H */