#include "at24c02.h"
#include "i2c.h"

#include <stdio.h>
#include <string.h>

/*
 * ==================== 内部宏定义 ====================
 * 设备地址左移1位（8位写地址），用于I2C发送
 */

#define AT24C02_WRITE_ADDR ((AT24C02_DEV_ADDR << 1) & 0xFE)         // 写地址0xA0
#define AT24C02_READ_ADDR (((AT24C02_DEV_ADDR << 1) | 0x01) & 0xFF) // 读地址0xA1

/* I2C超时时间（ms） */
#define AT24C02_I2C_TIMEOUT 100
/* 写周期等待超时（AT24C02最大5ms，留余量） */
#define AT24C02_WRITE_TIMEOUT 10

/* ==================== 内部辅助函数 ==================== */
/**
 * @brief  检测设备是否存在（发送设备地址，检查ACK）
 * @retval 1：存在，0：不存在
 */

static uint8_t AT24C02_CheckDevice(void)
{
    HAL_StatusTypeDef status;
    /* 尝试发送写地址，检测是否应答 */
    status = HAL_I2C_IsDeviceReady(&AT24C02_I2C, AT24C02_WRITE_ADDR, 3, AT24C02_I2C_TIMEOUT);
    return (status == HAL_OK) ? 1 : 0;
}

/**
 * @brief  等待内部写周期完成（通过轮询设备应答）
 * @note   每次写操作后，芯片内部会进入写周期（最大5ms），期间不响应I2C
 */

void AT24C02_WaitWriteComplete(void)
{
    uint32_t timeout = AT24C02_WRITE_TIMEOUT;
    while (timeout--)
    {
        if (AT24C02_CheckDevice())
            break;
        HAL_Delay(1);
    }
}

/**
 * @brief  检查地址是否越界
 * @param  addr: 起始地址
 * @param  len:  数据长度
 * @retval 0 = 越界, 1 = 合法
 */

static uint8_t AT24C02_CheckAddr(uint16_t addr, uint16_t len)
{
    if (len == 0)
        return 1;
    if (addr >= AT24C02_CHIP_SIZE)
        return 0;
    if (addr + len > AT24C02_CHIP_SIZE)
        return 0;
    return 1;
}

/* ==================== 初始化与信息读取 ==================== */
/**
 * @brief  初始化AT24C02，检测设备是否存在
 * @retval 错误码
 */

AT24C02_Error_t AT24C02_Init(void)
{
    if (!AT24C02_CheckDevice())
        return AT24C02_ERR_DEVICE;
    return AT24C02_OK;
}

/**
 * @brief  获取芯片信息
 * @param  info: 输出参数，芯片信息结构体指针
 */

void AT24C02_GetInfo(AT24C02_Info_t *info)
{
    if (info == NULL)
        return;
    info->size_bytes = AT24C02_CHIP_SIZE;
    info->page_size = AT24C02_PAGE_SIZE;
    info->page_count = AT24C02_PAGE_COUNT;
    info->dev_addr = AT24C02_DEV_ADDR;
}

/* ==================== 数据读取 ==================== */
/**
 * @brief  从指定地址读取数据
 * @param  addr: 起始地址（0 ~ AT24C02_CHIP_SIZE-1）
 * @param  buf:  接收缓冲区
 * @param  len:  读取长度（字节）
 * @retval 错误码
 */

AT24C02_Error_t AT24C02_Read(uint16_t addr, uint8_t *buf, uint16_t len)
{
    /* 参数检查 */
    if (buf == NULL || len == 0)
        return AT24C02_ERR_PARAM;

    /* 地址越界检查 */
    if (!AT24C02_CheckAddr(addr, len))
        return AT24C02_ERR_ADDR;

    /* 使用HAL库的Mem_Read函数（内部地址为16位，但AT24C02只用低8位，高8位忽略） */
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&AT24C02_I2C,
                                                AT24C02_WRITE_ADDR,
                                                addr,
                                                I2C_MEMADD_SIZE_8BIT,
                                                buf,
                                                len,
                                                AT24C02_I2C_TIMEOUT);
    if (status != HAL_OK)
        return AT24C02_ERR_DEVICE;

    return AT24C02_OK;
}

/* ==================== 页写入 ==================== */
/**
 * @brief  页写入（单次最多16字节）
 * @param  addr: 写入起始地址（页内偏移）
 * @param  buf:  数据缓冲区
 * @param  len:  写入长度（1~16字节）
 * @retval 错误码
 * @note   如果数据跨页，硬件会自动回绕到页首，可能导致数据覆盖
 *         建议使用 AT24C02_Write() 处理跨页写入
 */

AT24C02_Error_t AT24C02_PageWrite(uint16_t addr, uint8_t *buf, uint16_t len)
{
    /* 参数检查 */
    if (buf == NULL || len == 0)
        return AT24C02_ERR_PARAM;

    if (len > AT24C02_PAGE_SIZE)
        return AT24C02_ERR_PARAM;

    /* 地址越界检查 */
    if (!AT24C02_CheckAddr(addr, len))
        return AT24C02_ERR_ADDR;

    /* 等待前一次写操作完成 */
    AT24C02_WaitWriteComplete();

    /* 使用HAL库的Mem_Write函数 */
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&AT24C02_I2C,
                                                 AT24C02_WRITE_ADDR,
                                                 addr,
                                                 I2C_MEMADD_SIZE_8BIT,
                                                 buf,
                                                 len,
                                                 AT24C02_I2C_TIMEOUT);
    if (status != HAL_OK)
        return AT24C02_ERR_DEVICE;

    /* 写操作完成后，芯片进入内部写周期，不需要立即等待，但后续操作前会等待 */
    return AT24C02_OK;
}

/* ==================== 跨页写入 ==================== */
/**
 * @brief  任意地址写入任意长度数据（自动处理跨页）
 * @param  addr: 起始地址（0 ~ AT24C02_CHIP_SIZE-1）
 * @param  buf:  数据缓冲区
 * @param  len:  写入长度（字节）
 * @retval 错误码
 * @note   自动将数据拆分为多次页写入，处理页边界。
 *         每次页写入后等待写周期完成。
 */

AT24C02_Error_t AT24C02_Write(uint16_t addr, uint8_t *buf, uint16_t len)
{
    AT24C02_Error_t ret;
    uint16_t remaining = len;
    uint16_t current_addr = addr;
    uint8_t *current_buf = buf;

    /* 参数检查 */
    if (buf == NULL || len == 0)
        return AT24C02_ERR_PARAM;

    /* 地址越界检查 */
    if (!AT24C02_CheckAddr(addr, len))
        return AT24C02_ERR_ADDR;

    while (remaining > 0)
    {
        /* 计算当前页剩余空间 */
        uint16_t page_offset = current_addr & (AT24C02_PAGE_SIZE - 1);
        uint16_t page_remain = AT24C02_PAGE_SIZE - page_offset;

        /* 本次可写入的长度 */
        uint16_t chunk = (remaining > page_remain) ? page_remain : remaining;

        /* 执行页写入 */
        ret = AT24C02_PageWrite(current_addr, current_buf, chunk);
        if (ret != AT24C02_OK)
            return ret;

        /*
         * 等待写周期完成（PageWrite内部已经等待了前一次，但本次写入后需要等待）
         * 实际上PageWrite内部调用WaitWriteComplete是在写入前，所以写入后需要等待
         * 这里为了可靠，在每次PageWrite后主动等待
         */

        AT24C02_WaitWriteComplete();

        /* 更新指针和剩余长度 */
        current_addr += chunk;
        current_buf += chunk;
        remaining -= chunk;
    }

    return AT24C02_OK;
}

/* ==================== 使用示例 ==================== */
#if 1 // 设置为1启用示例代码

/**
 * @brief  AT24C02 使用示例
 * @note   展示完整的读写流程：
 *         1. 初始化芯片
 *         2. 获取芯片信息
 *         3. 写入数据（自动跨页）
 *         4. 读取数据
 *         5. 验证数据
 */

void AT24C02_Example(void)
{
    /* 测试数据 */
    uint8_t write_buf[] = "Hello AT24C02!";
    uint8_t read_buf[sizeof(write_buf)];
    AT24C02_Info_t info;
    AT24C02_Error_t ret;

    /* 1. 初始化 */
    ret = AT24C02_Init();
    if (ret != AT24C02_OK)
    {
        printf("AT24C02 Init Failed! Error: %d\r\n", ret);
        return;
    }

    /* 2. 获取芯片信息 */
    AT24C02_GetInfo(&info);
    printf("========== AT24C02 Info ==========\r\n");
    printf("Device Address: 0x%02X\r\n", info.dev_addr);
    printf("Capacity:       %lu bytes\r\n", info.size_bytes);
    printf("Page Size:      %lu bytes\r\n", info.page_size);
    printf("Page Count:     %lu\r\n", info.page_count);
    printf("==================================\r\n\r\n");

    /* 3. 写入数据（使用跨页写入函数） */
    printf("Writing %zu bytes to address 0x%04X...\r\n", sizeof(write_buf), AT24C02_TEST_ADDR);
    ret = AT24C02_Write(AT24C02_TEST_ADDR, write_buf, sizeof(write_buf));
    if (ret != AT24C02_OK)
    {
        printf("Write Failed! Error: %d\r\n", ret);
        return;
    }
    printf("Write OK!\r\n\r\n");

    /* 4. 读取数据 */
    memset(read_buf, 0, sizeof(read_buf));
    printf("Reading %zu bytes from address 0x%04X...\r\n", sizeof(read_buf), AT24C02_TEST_ADDR);
    ret = AT24C02_Read(AT24C02_TEST_ADDR, read_buf, sizeof(read_buf));
    if (ret != AT24C02_OK)
    {
        printf("Read Failed! Error: %d\r\n", ret);
        return;
    }
    printf("Read OK!\r\n\r\n");

    /* 5. 验证数据 */
    if (memcmp(write_buf, read_buf, sizeof(write_buf)) == 0)
    {
        printf("Data verified OK!\r\n");
        printf("Written: \"%s\"\r\n", write_buf);
        printf("Read:    \"%s\"\r\n", read_buf);
    }
    else
    {
        printf("Data verification FAILED!\r\n");
        printf("Written: \"%s\"\r\n", write_buf);
        printf("Read:    \"%s\"\r\n", read_buf);
    }
}

#endif /* AT24C02_EXAMPLE_ENABLE */
