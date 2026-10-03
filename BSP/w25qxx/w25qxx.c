#include "w25qxx.h"
#include "spi.h"

#include <stdio.h>
#include <string.h>

/* ==================== 内部宏定义 ==================== */
#define W25Q_CS_LOW() HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET)
#define W25Q_CS_HIGH() HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET)

/* 超时时间配置（单位：ms） */
#define W25Q_TIMEOUT_GENERIC 100      // 通用操作超时（读、写）
#define W25Q_TIMEOUT_SECTOR_ERASE 500 // 扇区擦除超时（典型 40~400ms）
#define W25Q_TIMEOUT_BLOCK_ERASE 1000 // 块擦除超时
#define W25Q_TIMEOUT_CHIP_ERASE 90000 // 整片擦除超时（典型 40~80 秒）
#define W25Q_SPI_TIMEOUT 100          // SPI 单字节超时（ms）

/* ==================== 内部函数声明 ==================== */
static uint8_t W25Q_SPI_RW(uint8_t tx);
static uint8_t W25Q_CheckAddr(uint32_t addr, uint32_t len);
static uint8_t W25Q_CheckProtected(void);
static void W25Q_WaitBusyTimeout(uint32_t timeout_ms);

/* ==================== SPI 底层操作 ==================== */
/**
 * @brief  SPI 单字节收发（内部使用）
 * @param  tx: 发送的字节
 * @retval 接收的字节
 */

static uint8_t W25Q_SPI_RW(uint8_t tx)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&W25Q_SPI, &tx, &rx, 1, W25Q_SPI_TIMEOUT);
    return rx;
}

/* ==================== 内部辅助函数 ==================== */
/**
 * @brief  检查地址是否越界
 * @param  addr: 起始地址
 * @param  len:  数据长度
 * @retval 0 = 越界, 1 = 合法
 */

static uint8_t W25Q_CheckAddr(uint32_t addr, uint32_t len)
{
    if (len == 0)
        return 1;
    if (addr >= W25Q_CHIP_SIZE)
        return 0;
    if (addr + len > W25Q_CHIP_SIZE)
        return 0;
    return 1;
}

/**
 * @brief  检查芯片是否处于写保护状态（BP 位非零）
 * @retval 0 = 未保护, 1 = 已保护
 */

static uint8_t W25Q_CheckProtected(void)
{
    uint8_t sr1 = W25Q_ReadStatus1();
    /* 检查 BP0~BP2 是否全为 0 */
    if (sr1 & (W25Q_SR1_BP0 | W25Q_SR1_BP1 | W25Q_SR1_BP2))
        return 1;
    return 0;
}

/**
 * @brief  等待芯片空闲（带超时参数）
 * @param  timeout_ms: 超时时间（毫秒）
 */

static void W25Q_WaitBusyTimeout(uint32_t timeout_ms)
{
    uint32_t timeout = timeout_ms;
    while (W25Q_IsBusy())
    {
        if (--timeout == 0)
            break; // 超时退出
        HAL_Delay(1);
    }
}

/* ==================== 初始化与信息读取 ==================== */
/**
 * @brief  初始化 W25Qxx，验证芯片 ID
 * @retval 错误码
 * @note   会验证制造商 ID、存储类型、容量 ID 是否匹配
 */

W25Q_Error_t W25Q_Init(void)
{
    uint32_t id = W25Q_ReadID();

    /* 检查制造商 ID 是否为 Winbond (0xEF) */
    uint8_t manufacturer = (id >> 16) & 0xFF;
    if (manufacturer != 0xEF)
    {
        return W25Q_ERR_ID;
    }

    /* 检查存储类型是否为 W25Q 系列 (0x40) */
    uint8_t memory_type = (id >> 8) & 0xFF;
    if (memory_type != 0x40)
    {
        return W25Q_ERR_ID;
    }

    /* 检查容量 ID 是否匹配当前定义的芯片型号 */
    uint8_t capacity = id & 0xFF;
    uint8_t expected_capacity = W25Q_EXPECTED_ID & 0xFF;
    if (capacity != expected_capacity)
    {
        /* 容量 ID 不匹配，可能是芯片型号选择错误 */
        return W25Q_ERR_ID;
    }

    /* 退出掉电模式（如果进入） */
    W25Q_WakeUp();

    /* 清除写使能 */
    W25Q_WriteDisable();

    return W25Q_OK;
}

/**
 * @brief  获取芯片信息
 * @param  info: 输出参数，芯片信息结构体指针
 */

void W25Q_GetInfo(W25Q_Info_t *info)
{
    if (info == NULL)
        return;

    info->jedec_id = W25Q_ReadID();
    info->manufacturer = (info->jedec_id >> 16) & 0xFF;
    info->memory_type = (info->jedec_id >> 8) & 0xFF;
    info->capacity = info->jedec_id & 0xFF;

    /* 使用宏定义的容量参数（编译时计算） */
    info->size_kb = W25Q_CHIP_SIZE / 1024;
    info->size_mb = W25Q_CHIP_SIZE / 1024 / 1024;
    info->sector_count = W25Q_SECTOR_COUNT;
    info->page_count = W25Q_PAGE_COUNT;
    info->block_count = W25Q_BLOCK_COUNT;
}

/**
 * @brief  读取芯片 JEDEC ID
 * @retval 24 位 ID（制造商 | 存储类型 | 容量）
 */

uint32_t W25Q_ReadID(void)
{
    uint8_t id[3];
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_JEDEC_ID);
    id[0] = W25Q_SPI_RW(0xFF);
    id[1] = W25Q_SPI_RW(0xFF);
    id[2] = W25Q_SPI_RW(0xFF);
    W25Q_CS_HIGH();
    return (id[0] << 16) | (id[1] << 8) | id[2];
}

/* ==================== 状态寄存器操作 ==================== */
/**
 * @brief  读取状态寄存器 1
 * @retval 状态寄存器 1 的值
 */

uint8_t W25Q_ReadStatus1(void)
{
    uint8_t sta;
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_READ_STATUS1);
    sta = W25Q_SPI_RW(0xFF);
    W25Q_CS_HIGH();
    return sta;
}

/**
 * @brief  读取状态寄存器 2
 * @retval 状态寄存器 2 的值
 */

uint8_t W25Q_ReadStatus2(void)
{
    uint8_t sta;
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_READ_STATUS2);
    sta = W25Q_SPI_RW(0xFF);
    W25Q_CS_HIGH();
    return sta;
}

/**
 * @brief  读取状态寄存器 3
 * @retval 状态寄存器 3 的值
 */

uint8_t W25Q_ReadStatus3(void)
{
    uint8_t sta;
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_READ_STATUS3);
    sta = W25Q_SPI_RW(0xFF);
    W25Q_CS_HIGH();
    return sta;
}

/**
 * @brief  检查芯片是否忙碌
 * @retval 0 = 空闲, 1 = 忙碌
 */

uint8_t W25Q_IsBusy(void)
{
    return W25Q_ReadStatus1() & W25Q_SR1_BUSY;
}

/**
 * @brief  检查写使能状态
 * @retval 0 = 未使能, 1 = 已使能
 */

uint8_t W25Q_IsWriteEnabled(void)
{
    return (W25Q_ReadStatus1() & W25Q_SR1_WEL) ? 1 : 0;
}

/* ==================== 写使能与等待 ==================== */
/**
 * @brief  写使能（任何写入/擦除操作前必须调用）
 */

void W25Q_WriteEnable(void)
{
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_WRITE_EN);
    W25Q_CS_HIGH();
}

/**
 * @brief  写禁止
 */

void W25Q_WriteDisable(void)
{
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_WRITE_DIS);
    W25Q_CS_HIGH();
}

/**
 * @brief  等待芯片空闲（使用默认超时时间）
 * @note   超时时间由 W25Q_TIMEOUT_GENERIC 宏定义
 */

void W25Q_WaitBusy(void)
{
    W25Q_WaitBusyTimeout(W25Q_TIMEOUT_GENERIC);
}

/* ==================== 数据读取 ==================== */
/**
 * @brief  从指定地址读取数据
 * @param  addr: 起始地址（0 ~ W25Q_CHIP_SIZE-1）
 * @param  buf:  接收缓冲区
 * @param  len:  读取长度（字节）
 * @retval 错误码
 */

W25Q_Error_t W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 参数检查 */
    if (buf == NULL || len == 0)
        return W25Q_ERR_PARAM;

    /* 地址越界检查 */
    if (!W25Q_CheckAddr(addr, len))
        return W25Q_ERR_ADDR;

    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_READ);
    W25Q_SPI_RW((addr >> 16) & 0xFF);
    W25Q_SPI_RW((addr >> 8) & 0xFF);
    W25Q_SPI_RW(addr & 0xFF);

    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = W25Q_SPI_RW(0xFF);
    }

    W25Q_CS_HIGH();
    return W25Q_OK;
}

/* ==================== 页写入 ==================== */
/**
 * @brief  页写入（单次最多 256 字节）
 * @param  addr: 写入地址（建议页对齐）
 * @param  buf:  数据缓冲区
 * @param  len:  写入长度（1~256 字节）
 * @retval 错误码
 * @note   如果数据跨页，硬件会自动回绕到页首，可能导致数据覆盖
 *         建议使用 W25Q_Write() 处理跨页写入
 */

W25Q_Error_t W25Q_PageWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    /* 参数检查 */
    if (buf == NULL || len == 0)
        return W25Q_ERR_PARAM;

    if (len > W25Q_PAGE_SIZE)
        return W25Q_ERR_PARAM;

    /* 地址越界检查 */
    if (!W25Q_CheckAddr(addr, len))
        return W25Q_ERR_ADDR;

    /* 检查写保护 */
    if (W25Q_CheckProtected())
        return W25Q_ERR_PROTECTED;

    /* 检测跨页 */
    uint32_t page_start = addr & ~(W25Q_PAGE_SIZE - 1);
    if (addr + len > page_start + W25Q_PAGE_SIZE)
    {
        /*
         * 跨页警告，硬件会自动回绕到页首，可能导致数据覆盖
         * 调用者应使用 W25Q_Write() 处理跨页
         */
    }

    W25Q_WriteEnable();

    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_PAGE_PROGRAM);
    W25Q_SPI_RW((addr >> 16) & 0xFF);
    W25Q_SPI_RW((addr >> 8) & 0xFF);
    W25Q_SPI_RW(addr & 0xFF);

    for (uint16_t i = 0; i < len; i++)
    {
        W25Q_SPI_RW(buf[i]);
    }

    W25Q_CS_HIGH();
    W25Q_WaitBusy();

    return W25Q_OK;
}

/* ==================== 跨页写入 ==================== */
/**
 * @brief  任意地址写入任意长度数据（自动处理跨页）
 * @param  addr: 起始地址（0 ~ W25Q_CHIP_SIZE-1）
 * @param  buf:  数据缓冲区
 * @param  len:  写入长度（字节）
 * @retval 错误码
 * @note   此函数会自动将数据拆分为多次页写入，处理跨页边界。
 *         注意：写入前需要确保目标地址所在的扇区已被擦除。
 */

W25Q_Error_t W25Q_Write(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25Q_Error_t ret;
    uint32_t remaining = len;
    uint32_t current_addr = addr;
    uint8_t *current_buf = buf;

    /* 参数检查 */
    if (buf == NULL || len == 0)
        return W25Q_ERR_PARAM;

    /* 地址越界检查 */
    if (!W25Q_CheckAddr(addr, len))
        return W25Q_ERR_ADDR;

    /* 检查写保护 */
    if (W25Q_CheckProtected())
        return W25Q_ERR_PROTECTED;

    while (remaining > 0)
    {
        /* 计算当前页剩余空间 */
        uint16_t page_offset = current_addr & (W25Q_PAGE_SIZE - 1);
        uint16_t page_remain = W25Q_PAGE_SIZE - page_offset;

        /* 本次可写入的长度 */
        uint16_t chunk = (remaining > page_remain) ? page_remain : remaining;

        /* 执行页写入 */
        ret = W25Q_PageWrite(current_addr, current_buf, chunk);
        if (ret != W25Q_OK)
            return ret;

        /* 更新指针和剩余长度 */
        current_addr += chunk;
        current_buf += chunk;
        remaining -= chunk;
    }

    return W25Q_OK;
}

/* ==================== 擦除操作 ==================== */
/**
 * @brief  扇区擦除（4KB）
 * @param  addr: 扇区内任意地址（自动对齐到扇区首地址）
 * @retval 错误码
 */

W25Q_Error_t W25Q_SectorErase(uint32_t addr)
{
    /* 自动对齐到扇区首地址 */
    uint32_t sector_addr = W25Q_SECTOR_ALIGN(addr);

    /* 地址越界检查 */
    if (sector_addr >= W25Q_CHIP_SIZE)
        return W25Q_ERR_ADDR;

    /* 检查写保护 */
    if (W25Q_CheckProtected())
        return W25Q_ERR_PROTECTED;

    W25Q_WriteEnable();

    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_SECTOR_ERASE);
    W25Q_SPI_RW((sector_addr >> 16) & 0xFF);
    W25Q_SPI_RW((sector_addr >> 8) & 0xFF);
    W25Q_SPI_RW(sector_addr & 0xFF);
    W25Q_CS_HIGH();

    /* 扇区擦除典型时间：40~400ms，使用 500ms 超时 */
    W25Q_WaitBusyTimeout(W25Q_TIMEOUT_SECTOR_ERASE);

    return W25Q_OK;
}

/**
 * @brief  块擦除（32KB）
 * @param  addr: 块内任意地址（自动对齐到 32KB 块首地址）
 * @retval 错误码
 */

W25Q_Error_t W25Q_BlockErase32K(uint32_t addr)
{
    /* 自动对齐到 32KB 块首地址 */
    uint32_t block_addr = W25Q_BLOCK_32K_ALIGN(addr);

    /* 地址越界检查 */
    if (block_addr >= W25Q_CHIP_SIZE)
        return W25Q_ERR_ADDR;

    /* 检查写保护 */
    if (W25Q_CheckProtected())
        return W25Q_ERR_PROTECTED;

    W25Q_WriteEnable();

    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_BLOCK_ERASE_32K);
    W25Q_SPI_RW((block_addr >> 16) & 0xFF);
    W25Q_SPI_RW((block_addr >> 8) & 0xFF);
    W25Q_SPI_RW(block_addr & 0xFF);
    W25Q_CS_HIGH();

    /* 块擦除典型时间：200~2000ms，使用 1000ms 超时 */
    W25Q_WaitBusyTimeout(W25Q_TIMEOUT_BLOCK_ERASE);

    return W25Q_OK;
}

/**
 * @brief  块擦除（64KB）
 * @param  addr: 块内任意地址（自动对齐到 64KB 块首地址）
 * @retval 错误码
 */

W25Q_Error_t W25Q_BlockErase64K(uint32_t addr)
{
    /* 自动对齐到 64KB 块首地址 */
    uint32_t block_addr = W25Q_BLOCK_ALIGN(addr);

    /* 地址越界检查 */
    if (block_addr >= W25Q_CHIP_SIZE)
        return W25Q_ERR_ADDR;

    /* 检查写保护 */
    if (W25Q_CheckProtected())
        return W25Q_ERR_PROTECTED;

    W25Q_WriteEnable();

    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_BLOCK_ERASE_64K);
    W25Q_SPI_RW((block_addr >> 16) & 0xFF);
    W25Q_SPI_RW((block_addr >> 8) & 0xFF);
    W25Q_SPI_RW(block_addr & 0xFF);
    W25Q_CS_HIGH();

    /* 块擦除典型时间：200~2000ms，使用 1000ms 超时 */
    W25Q_WaitBusyTimeout(W25Q_TIMEOUT_BLOCK_ERASE);

    return W25Q_OK;
}

/**
 * @brief  整片擦除
 * @retval 错误码
 * @note   整片擦除需要较长时间（典型值 40~80 秒）
 */

W25Q_Error_t W25Q_ChipErase(void)
{
    /* 检查写保护 */
    if (W25Q_CheckProtected())
        return W25Q_ERR_PROTECTED;

    W25Q_WriteEnable();

    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_CHIP_ERASE);
    W25Q_CS_HIGH();

    /* 整片擦除超时时间：90 秒 */
    W25Q_WaitBusyTimeout(W25Q_TIMEOUT_CHIP_ERASE);

    return W25Q_OK;
}

/* ==================== 电源管理 ==================== */
/**
 * @brief  进入掉电模式（降低功耗）
 */

void W25Q_PowerDown(void)
{
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_POWER_DOWN);
    W25Q_CS_HIGH();
}

/**
 * @brief  退出掉电模式（唤醒芯片）
 * @note   唤醒后需要等待至少 3μs 才能发送命令
 */

void W25Q_WakeUp(void)
{
    W25Q_CS_LOW();
    W25Q_SPI_RW(W25Q_RELEASE_POWER_DOWN);
    W25Q_CS_HIGH();

    /* 等待芯片唤醒（典型 3μs，给一个安全延时 1ms） */
    HAL_Delay(1);
}

/* ==================== 使用示例 ==================== */
#if 0 // 设置为 1 启用示例代码

/**
 * @brief  W25Qxx 使用示例
 * @note   展示完整的读写流程：
 *         1. 初始化芯片
 *         2. 获取芯片信息
 *         3. 擦除扇区
 *         4. 写入数据
 *         5. 读取数据
 *         6. 验证数据
 */

void W25Q_Example(void)
{
    /* 测试数据 */
    uint8_t write_buf[] = "Hello W25Qxx!";
    uint8_t read_buf[sizeof(write_buf)];
    W25Q_Info_t info;
    W25Q_Error_t ret;

    /* 1. 初始化 */
    ret = W25Q_Init();
    if (ret != W25Q_OK)
    {
        printf("W25Q Init Failed! Error: %d\r\n", ret);
        return;
    }

    /* 2. 获取芯片信息 */
    W25Q_GetInfo(&info);
    printf("========== W25Qxx Info ==========\r\n");
    printf("JEDEC ID:     0x%06X\r\n", info.jedec_id);
    printf("Manufacturer: 0x%02X\r\n", info.manufacturer);
    printf("Memory Type:  0x%02X\r\n", info.memory_type);
    printf("Capacity ID:  0x%02X\r\n", info.capacity);
    printf("Capacity:     %u MB (%lu bytes)\r\n", info.size_mb, W25Q_CHIP_SIZE);
    printf("Sector Size:  %u KB\r\n", W25Q_SECTOR_SIZE / 1024);
    printf("Sector Count: %u\r\n", info.sector_count);
    printf("Block Size:   %u KB\r\n", W25Q_BLOCK_SIZE / 1024);
    printf("Block Count:  %u\r\n", info.block_count);
    printf("Page Size:    %u B\r\n", W25Q_PAGE_SIZE);
    printf("Page Count:   %u\r\n", info.page_count);
    printf("==================================\r\n\r\n");

    /* 3. 擦除扇区（使用第一个扇区） */
    printf("Erasing sector at address 0x%06X...\r\n", W25Q_TEST_ADDR);
    ret = W25Q_SectorErase(W25Q_TEST_ADDR);
    if (ret != W25Q_OK)
    {
        printf("Erase Failed! Error: %d\r\n", ret);
        return;
    }
    printf("Sector erased OK!\r\n\r\n");

    /* 4. 写入数据（使用跨页写入函数） */
    printf("Writing %zu bytes to address 0x%06X...\r\n", sizeof(write_buf), W25Q_TEST_ADDR);
    ret = W25Q_Write(W25Q_TEST_ADDR, write_buf, sizeof(write_buf));
    if (ret != W25Q_OK)
    {
        printf("Write Failed! Error: %d\r\n", ret);
        return;
    }
    printf("Write OK!\r\n\r\n");

    /* 5. 读取数据 */
    memset(read_buf, 0, sizeof(read_buf));  // 清零缓冲区
    printf("Reading %zu bytes from address 0x%06X...\r\n", sizeof(read_buf), W25Q_TEST_ADDR);
    ret = W25Q_Read(W25Q_TEST_ADDR, read_buf, sizeof(read_buf));
    if (ret != W25Q_OK)
    {
        printf("Read Failed! Error: %d\r\n", ret);
        return;
    }
    printf("Read OK!\r\n\r\n");

    /* 6. 验证数据 */
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

#endif /* W25Q_EXAMPLE_ENABLE */
