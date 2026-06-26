/**
 * @file    erase_spi.c
 * @brief   W25Q128 全片擦除 — 最小裸机固件
 *
 * 编译: arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
 *         -mfloat-abi=hard -Os -nostdlib -ffreestanding
 *         -T stm32f407_batch.ld erase_spi.c -o erase_spi.elf
 */

#include <stdint.h>

/* ==================================================================
 * 向量表 — 必须放在 0x08000000
 * ================================================================== */
extern uint32_t _estack;
int main(void);
__attribute__((section(".isr_vector")))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,     /* SP */
    (uint32_t)main,         /* Reset_Handler */
};

/* ==================================================================
 * UART1 (PA9=TX, PA10=RX) — 115200 @ 16MHz HSI
 * ================================================================== */
#define RCC_BASE      0x40023800UL
#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIOA_BASE    0x40020000UL
#define UART1_BASE    0x40011000UL
#define UART1_SR       (*(volatile uint32_t *)(UART1_BASE + 0x00))
#define UART1_DR       (*(volatile uint32_t *)(UART1_BASE + 0x04))

static void uart_init(void) {
    RCC_AHB1ENR |= 1;            /* GPIOA */
    RCC_APB2ENR |= (1U << 4);    /* USART1 */
    /* PA9/PA10: AF7 */
    *(volatile uint32_t *)(GPIOA_BASE + 0x00) &= ~((3U<<18)|(3U<<20));
    *(volatile uint32_t *)(GPIOA_BASE + 0x00) |= (2U<<18)|(2U<<20);
    *(volatile uint32_t *)(GPIOA_BASE + 0x24) = (*(volatile uint32_t *)(GPIOA_BASE + 0x24) & ~((0xFU<<4)|(0xFU<<8)))
                                              | (0x7U<<4)|(0x7U<<8);
    *(volatile uint32_t *)(GPIOA_BASE + 0x08) |= (2U<<18)|(2U<<20); /* Medium speed */
    *(volatile uint32_t *)(UART1_BASE + 0x08) = 139;   /* 115200 @ 16MHz */
    *(volatile uint32_t *)(UART1_BASE + 0x0C) = (1U<<13)|(1U<<3)|(1U<<2);
}
static void uart_tx(char c) {
    while (!(UART1_SR & (1U<<7)));
    UART1_DR = c;
}
static void uart_s(const char *s) { while (*s) uart_tx(*s++); }
static void uart_h8(uint8_t v) {
    uart_tx("0123456789ABCDEF"[v>>4]);
    uart_tx("0123456789ABCDEF"[v&0xF]);
}
static void uart_h32(uint32_t v) {
    for (int i=7; i>=0; i--) uart_h8((v>>(i*4))&0xFF);
}

/* ==================================================================
 * LED (PF9) — 进度指示
 * ================================================================== */
#define GPIOF_BASE    0x40021400UL
#define GPIOF_BSRR     (*(volatile uint32_t *)(GPIOF_BASE + 0x18))
static void led_init(void) {
    RCC_AHB1ENR |= (1U<<5);
    *(volatile uint32_t *)(GPIOF_BASE + 0x00) = (*(volatile uint32_t *)(GPIOF_BASE + 0x00) & ~(3U<<18)) | (1U<<18);
}
#define LED_ON()   (GPIOF_BSRR = (1U<<9)<<16)
#define LED_OFF()  (GPIOF_BSRR = (1U<<9))

/* ==================================================================
 * SPI1 + W25Q128 — 纯寄存器
 * ================================================================== */
#define GPIOB_BASE    0x40020400UL
#define GPIOG_BASE    0x40021800UL
#define SPI1_BASE     0x40013000UL
#define SPI1_CR1      (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_SR       (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR       (*(volatile uint8_t  *)(SPI1_BASE + 0x0C))

#define CS_PIN        (1U<<14)   /* PB14 */
#define NRF_PIN       (1U<<7)    /* PG7  */

#define CMD_WREN      0x06
#define CMD_WRDI      0x04
#define CMD_RDSR1     0x05
#define CMD_CE        0xC7       /* Chip Erase (全片擦除, ~80秒) */
#define CMD_RDID      0x9F

static uint8_t spi_xfer(uint8_t tx) {
    while (!(SPI1_SR & (1U<<1)));  /* TXE */
    SPI1_DR = tx;
    while (!(SPI1_SR & (1U<<0)));  /* RXNE */
    return SPI1_DR;
}
static inline void cs_low(void)  { *(volatile uint32_t *)(GPIOB_BASE + 0x18) = CS_PIN<<16; }
static inline void cs_high(void) { *(volatile uint32_t *)(GPIOB_BASE + 0x18) = CS_PIN; }

static void wait_busy(void) {
    cs_low(); spi_xfer(CMD_RDSR1);
    while (spi_xfer(0xFF) & 1); /* BUSY bit */
    cs_high();
}

static uint32_t read_jedec_id(void) {
    uint32_t id = 0;
    cs_low();
    spi_xfer(CMD_RDID);
    id  = (uint32_t)spi_xfer(0xFF) << 16;
    id |= (uint32_t)spi_xfer(0xFF) << 8;
    id |= (uint32_t)spi_xfer(0xFF);
    cs_high();
    return id;
}

/* ==================================================================
 * 主入口
 * ================================================================== */
int main(void) {
    uart_init();
    uart_s("\r\n>>> SPI FLASH ERASER (W25Q128)\r\n");

    /* GPIO + SPI1 初始化 */
    RCC_AHB1ENR |= (1U<<1)|(1U<<6);  /* GPIOB, GPIOG */
    RCC_APB2ENR |= (1U<<12);         /* SPI1 */

    /* PB3/4/5: AF5, PB14: OUT, PG7: OUT */
    *(volatile uint32_t *)(GPIOB_BASE + 0x00) &= ~((3U<<6)|(3U<<8)|(3U<<10)|(3U<<28));
    *(volatile uint32_t *)(GPIOB_BASE + 0x00) |= (2U<<6)|(2U<<8)|(2U<<10)|(1U<<28);
    *(volatile uint32_t *)(GPIOB_BASE + 0x20) &= ~((0xFU<<12)|(0xFU<<16)|(0xFU<<20));
    *(volatile uint32_t *)(GPIOB_BASE + 0x20) |= (0x5U<<12)|(0x5U<<16)|(0x5U<<20);
    /* PB14, PG7 上拉 */
    *(volatile uint32_t *)(GPIOB_BASE + 0x0C) |= (1U<<28);
    *(volatile uint32_t *)(GPIOG_BASE + 0x0C) |= (1U<<14);
    *(volatile uint32_t *)(GPIOG_BASE + 0x00) = (*(volatile uint32_t *)(GPIOG_BASE + 0x00) & ~(3U<<14)) | (1U<<14);
    cs_high();
    *(volatile uint32_t *)(GPIOG_BASE + 0x18) = NRF_PIN;  /* NRF_CS HIGH */

    /* SPI1: Master, Mode3, MSB, SSM, BR=fPCLK/4=4MHz */
    SPI1_CR1 = (1U<<2)|(1U<<1)|(1U<<0)|(1U<<9)|(1U<<8)|(1U<<6)|(1U<<3);

    led_init();
    LED_OFF();

    /* ---- 读 ID ---- */
    uint32_t id = read_jedec_id();
    uart_s("  JEDEC ID: 0x"); uart_h32(id);
    uart_s(id == 0xEF6018 ? " (W25Q128JV OK)\r\n" : " (UNKNOWN!)\r\n");

    /* ---- 全片擦除 ---- */
    uart_s("  Full chip erase started (may take ~80s)...\r\n");
    LED_ON();

    cs_low();
    spi_xfer(CMD_WREN);   /* 写使能 */
    cs_high();
    cs_low();
    spi_xfer(CMD_CE);     /* 全片擦除命令 */
    cs_high();

    /* 轮询等待 (最长 80 秒) */
    uint32_t start = 0;
    for (volatile uint32_t t = 0; t < 5000000; t++) { asm volatile("nop"); } /* ~3秒延时让擦除开始 */

    /* 轮询 BUSY */
    int elapsed = 3;
    cs_low();
    spi_xfer(CMD_RDSR1);
    while (spi_xfer(0xFF) & 1) {
        cs_high();
        for (volatile uint32_t d = 0; d < 1000000; d++) { asm volatile("nop"); } /* ~1秒 */
        elapsed++;
        uart_s("  .");
        cs_low();
        spi_xfer(CMD_RDSR1);
    }
    cs_high();

    LED_OFF();
    uart_s("\r\n  ERASE DONE (");
    /* print elapsed as number */
    uart_tx('0'+elapsed/10); uart_tx('0'+elapsed%10);
    uart_s("s)\r\n\r\n");

    uart_s(">>> FINISHED — SPI Flash is now all 0xFF\r\n");
    while (1) { LED_ON(); for (volatile int i=0;i<500000;i++); LED_OFF(); for (volatile int i=0;i<500000;i++); }
}
