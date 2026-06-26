/**
 * @file    loader.c
 * @brief   W25Q128 External Loader — SPI Flash 操作
 *
 * 全部寄存器级, 无 HAL 依赖。由 STM32CubeProgrammer 下载到 SRAM 执行。
 *
 * 引脚: PB3=SCK(AF5), PB4=MISO(AF5), PB5=MOSI(AF5), PB14=CS(GPIO), PG7=NRF_CS(GPIO)
 */

#include <stdint.h>

/* ------------------------------------------------------------------
 * 寄存器基址 (STM32F407)
 * ------------------------------------------------------------------ */
#define RCC_BASE        0x40023800UL
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))
#define RCC_AHB1ENR_GPIOB   (1U << 1)
#define RCC_AHB1ENR_GPIOG   (1U << 6)
#define RCC_APB2ENR_SPI1    (1U << 12)

#define GPIOB_BASE      0x40020400UL
#define GPIOG_BASE      0x40021800UL

#define GPIO_MODER(base)    (*(volatile uint32_t *)((base) + 0x00))
#define GPIO_OTYPER(base)   (*(volatile uint32_t *)((base) + 0x04))
#define GPIO_OSPEEDR(base)  (*(volatile uint32_t *)((base) + 0x08))
#define GPIO_PUPDR(base)    (*(volatile uint32_t *)((base) + 0x0C))
#define GPIO_ODR(base)      (*(volatile uint32_t *)((base) + 0x14))
#define GPIO_BSRR(base)     (*(volatile uint32_t *)((base) + 0x18))
#define GPIO_AFRL(base)     (*(volatile uint32_t *)((base) + 0x20))
#define GPIO_AFRH(base)     (*(volatile uint32_t *)((base) + 0x24))

#define SPI1_BASE       0x40013000UL
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_CR2        (*(volatile uint32_t *)(SPI1_BASE + 0x04))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR         (*(volatile uint8_t  *)(SPI1_BASE + 0x0C))

/* SPI1 CR1 bits */
#define SPI_CR1_SPE     (1U << 6)
#define SPI_CR1_BR_DIV256   (7U << 3)
#define SPI_CR1_BR_DIV2     (0U << 3)
#define SPI_CR1_MSTR    (1U << 2)
#define SPI_CR1_CPOL    (1U << 1)
#define SPI_CR1_CPHA    (1U << 0)
#define SPI_CR1_SSM     (1U << 9)
#define SPI_CR1_SSI     (1U << 8)

/* SPI1 SR bits */
#define SPI_SR_TXE      (1U << 1)
#define SPI_SR_RXNE     (1U << 0)
#define SPI_SR_BSY      (1U << 7)

/* ------------------------------------------------------------------
 * 引脚定义
 * ------------------------------------------------------------------ */
#define CS_PIN      (1U << 14)   /* PB14 */
#define NRF_CS_PIN  (1U << 7)    /* PG7  */

/* ------------------------------------------------------------------
 * 命令
 * ------------------------------------------------------------------ */
#define CMD_WREN    0x06
#define CMD_SE      0x20
#define CMD_PP      0x02
#define CMD_READ    0x03
#define CMD_RDSR1   0x05
#define CMD_CE      0xC7

/* ------------------------------------------------------------------
 * SPI 字节交换
 * ------------------------------------------------------------------ */
static uint8_t spi1_xfer(uint8_t tx)
{
    while (!(SPI1_SR & SPI_SR_TXE));
    SPI1_DR = tx;
    while (!(SPI1_SR & SPI_SR_RXNE));
    return SPI1_DR;
}

/* ------------------------------------------------------------------
 * CS 控制
 * ------------------------------------------------------------------ */
static inline void cs_low(void)  { GPIO_BSRR(GPIOB_BASE) = CS_PIN << 16; }
static inline void cs_high(void) { GPIO_BSRR(GPIOB_BASE) = CS_PIN; }

/* ------------------------------------------------------------------
 * 忙等待
 * ------------------------------------------------------------------ */
static void wait_busy(void)
{
    cs_low();
    spi1_xfer(CMD_RDSR1);
    while (spi1_xfer(0xFF) & 0x01);
    cs_high();
}

/* ------------------------------------------------------------------
 * 写使能
 * ------------------------------------------------------------------ */
static void write_enable(void)
{
    cs_low();
    spi1_xfer(CMD_WREN);
    cs_high();
}

/* ==================================================================
 * External Loader API
 * ================================================================== */

/**
 * @brief  初始化 SPI1 + GPIO (由 CubeProgrammer 调用)
 */
int Init(void)
{
    /* 使能时钟 */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOB | RCC_AHB1ENR_GPIOG;
    RCC_APB2ENR |= RCC_APB2ENR_SPI1;

    /* ---- PB3/4/5: SPI1 AF5, 推挽, 无上下拉 ---- */
    /* Moder: AF */
    GPIO_MODER(GPIOB_BASE) &= ~((3U << 6) | (3U << 8) | (3U << 10));
    GPIO_MODER(GPIOB_BASE) |=  (2U << 6) | (2U << 8) | (2U << 10);
    /* AFRL/AFRH: AF5 */
    GPIO_AFRL(GPIOB_BASE) &= ~(0xFU << 12);
    GPIO_AFRL(GPIOB_BASE) |=  (0x5U << 12);  /* PB3 → AF5 */
    GPIO_AFRL(GPIOB_BASE) &= ~(0xFU << 16);
    GPIO_AFRL(GPIOB_BASE) |=  (0x5U << 16);  /* PB4 → AF5 */
    GPIO_AFRL(GPIOB_BASE) &= ~(0xFU << 20);
    GPIO_AFRL(GPIOB_BASE) |=  (0x5U << 20);  /* PB5 → AF5 */
    /* OSPEEDR: Medium */
    GPIO_OSPEEDR(GPIOB_BASE) &= ~((3U << 6) | (3U << 8) | (3U << 10));
    GPIO_OSPEEDR(GPIOB_BASE) |=  (1U << 6) | (1U << 8) | (1U << 10);
    /* No pull-up/down */
    GPIO_PUPDR(GPIOB_BASE) &= ~((3U << 6) | (3U << 8) | (3U << 10));

    /* ---- PB14: CS, 推挽输出, 上拉, IDLE=HIGH ---- */
    GPIO_MODER(GPIOB_BASE) &= ~(3U << 28);
    GPIO_MODER(GPIOB_BASE) |=  (1U << 28);     /* Output */
    GPIO_OSPEEDR(GPIOB_BASE) &= ~(3U << 28);
    GPIO_PUPDR(GPIOB_BASE) |= (1U << 28);       /* Pull-up */
    GPIO_BSRR(GPIOB_BASE) = CS_PIN;             /* CS HIGH (idle) */

    /* ---- PG7: NRF_CS, 推挽输出, 上拉, IDLE=HIGH ---- */
    GPIO_MODER(GPIOG_BASE) &= ~(3U << 14);
    GPIO_MODER(GPIOG_BASE) |=  (1U << 14);      /* Output */
    GPIO_PUPDR(GPIOG_BASE) |= (1U << 14);        /* Pull-up */
    GPIO_BSRR(GPIOG_BASE) = NRF_CS_PIN;          /* HIGH (disable NRF) */

    /* ---- SPI1: Master, Mode3, MSB, 8-bit, SSM=1, BR=FAST ---- */
    SPI1_CR1 = SPI_CR1_MSTR
             | SPI_CR1_CPOL
             | SPI_CR1_CPHA
             | SPI_CR1_SSM
             | SPI_CR1_SSI
             | SPI_CR1_BR_DIV2      /* fPCLK/2 = 42MHz: fast for loader */
             | SPI_CR1_SPE;

    /* 空读清 RX */
    (void)SPI1_DR;
    return 1;  /* 非零 = 成功 */
}

/**
 * @brief  整片擦除
 */
int MassErase(void)
{
    write_enable();
    cs_low();
    spi1_xfer(CMD_CE);
    cs_high();
    wait_busy();  /* ~80s for 16MB */
    return 1;
}

/**
 * @brief  扇区擦除 (4KB steps)
 * @param  start  起始地址
 * @param  end    结束地址 (不含)
 */
int SectorErase(uint32_t start, uint32_t end)
{
    while (start < end) {
        write_enable();
        cs_low();
        spi1_xfer(CMD_SE);
        spi1_xfer((uint8_t)(start >> 16));
        spi1_xfer((uint8_t)(start >> 8));
        spi1_xfer((uint8_t)(start));
        cs_high();
        wait_busy();
        start += 4096;  /* 4KB per sector */
    }
    return 1;
}

/**
 * @brief  页写入 (最大 256 字节/次)
 * @param  addr    目标地址
 * @param  size    数据大小 (≤256)
 * @param  buffer  数据指针
 */
int Write(uint32_t addr, uint32_t size, const uint8_t *buffer)
{
    if (size == 0) return 1;
    if (size > 256) return 0;  /* 调用者负责分页 */

    write_enable();
    cs_low();
    spi1_xfer(CMD_PP);
    spi1_xfer((uint8_t)(addr >> 16));
    spi1_xfer((uint8_t)(addr >> 8));
    spi1_xfer((uint8_t)(addr));
    for (uint32_t i = 0; i < size; i++) {
        spi1_xfer(buffer[i]);
    }
    cs_high();
    wait_busy();
    return 1;
}

/**
 * @brief  读数据 (验证用)
 * @param  addr    源地址
 * @param  size    数据大小
 * @param  buffer  输出缓冲
 */
static void read_data(uint32_t addr, uint32_t size, uint8_t *buffer)
{
    cs_low();
    spi1_xfer(CMD_READ);
    spi1_xfer((uint8_t)(addr >> 16));
    spi1_xfer((uint8_t)(addr >> 8));
    spi1_xfer((uint8_t)(addr));
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = spi1_xfer(0xFF);
    }
    cs_high();
}

/**
 * @brief  校验写入 (CubeProgrammer 调用)
 * @return 0 = 全部匹配, >0 = 不匹配的字节数
 */
uint64_t Verify(uint32_t addr, uint32_t size, const uint8_t *expected)
{
    /* 使用少量栈上缓冲 — 外部 Loader 有 2KB+ SRAM 可用 */
    uint8_t buf[256];
    uint64_t mismatches = 0;

    while (size > 0) {
        uint32_t chunk = (size > 256) ? 256 : size;
        read_data(addr, chunk, buf);
        for (uint32_t i = 0; i < chunk; i++) {
            if (buf[i] != expected[i]) mismatches++;
        }
        addr += chunk;
        expected += chunk;
        size -= chunk;
    }
    return mismatches;
}

/**
 * @brief  状态检查 (空实现 — CubeProgrammer 兼容)
 */
int CheckStatus(void)
{
    return 0;
}

/**
 * @brief  反初始化 (空实现)
 */
int DeInit(void)
{
    SPI1_CR1 &= ~SPI_CR1_SPE;
    return 1;
}
