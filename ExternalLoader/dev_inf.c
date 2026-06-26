/**
 * @file    dev_inf.c
 * @brief   W25Q128 External Loader — Device Information
 */

#include <stdint.h>

#define SECTOR_COUNT  4096
#define SECTOR_SIZE   4096

struct StorageInfo {
    char     DeviceName[100];
    uint16_t DeviceType;          /* NOR_FLASH = 4 */
    uint32_t DeviceStartAddress;
    uint32_t DeviceSize;          /* 16 MB */
    uint32_t PageSize;            /* 256 */
    uint32_t EraseValue;          /* 0xFF */
    struct {
        uint32_t Count;
        uint32_t Size;
    } Sectors[2];
};

__attribute__((section(".Dev_Inf"), used))
const struct StorageInfo StorageInfo = {
    .DeviceName        = "W25Q128_STM32F407_SPI1",
    .DeviceType        = 4,
    .DeviceStartAddress = 0x00000000,
    .DeviceSize         = 0x01000000,
    .PageSize           = 256,
    .EraseValue         = 0xFF,
    .Sectors = {
        {4096, 4096},   /* 4096 sectors × 4KB */
        {0, 0}          /* end marker */
    },
};
