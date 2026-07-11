/**
 * @file    main_batch.c
 * @brief   批烧固件 — 独立, 零依赖, SPI Flash 寄存器操作内联
 *
 * v2: 添加中断向量表 + 启动代码 (修复 MCU 无法启动的根本原因)
 *
 * 编译: arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
 *         -mfloat-abi=hard -Os -nostdlib -ffreestanding
 *         -T stm32f407_batch.ld main_batch.c -o batch_XX.elf
 */

#include <stdint.h>

/* batch.h 由 copy 命令生成: copy batch_00.h batch.h */
#include "batch.h"

/* ==================================================================
 * 链接器符号 (来自 stm32f407_batch.ld)
 * ================================================================== */
extern uint32_t _estack;     /* 栈顶 = ORIGIN(RAM) + LENGTH(RAM) */
extern uint32_t _sidata;     /* .data 段在 Flash 中的加载地址 */
extern uint32_t _sdata;      /* .data 段在 RAM 中的起始地址 */
extern uint32_t _edata;      /* .data 段在 RAM 中的结束地址 */
extern uint32_t _sbss;       /* .bss 段在 RAM 中的起始地址 */
extern uint32_t _ebss;       /* .bss 段在 RAM 中的结束地址 */

/* ==================================================================
 * 异常/中断向量表 (必需 — 否则 MCU 不会启动)
 * ================================================================== */
__attribute__((naked, noreturn)) void Reset_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void NMI_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void HardFault_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void MemManage_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void BusFault_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void UsageFault_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void SVC_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void DebugMon_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void PendSV_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void SysTick_Handler(void);

/* 简化的 IRQ 弱定义 — 用宏批量生成 */
#define WEAK_IRQ(n) __attribute__((weak, alias("Default_Handler"))) void IRQ##n##_Handler(void)

/* STM32F407 常用外设 IRQ (仅占位, 不会被触发 — 我们不用中断) */
WEAK_IRQ(0);   /* WWDG */
WEAK_IRQ(1);   /* PVD */
WEAK_IRQ(2);   /* TAMP_STAMP */
WEAK_IRQ(3);   /* RTC_WKUP */
WEAK_IRQ(4);   /* FLASH */
WEAK_IRQ(5);   /* RCC */
WEAK_IRQ(6);   /* EXTI0 */
WEAK_IRQ(7);   /* EXTI1 */
WEAK_IRQ(8);   /* EXTI2 */
WEAK_IRQ(9);   /* EXTI3 */
WEAK_IRQ(10);  /* EXTI4 */
WEAK_IRQ(11);  /* DMA1_Stream0 */
WEAK_IRQ(12);  /* DMA1_Stream1 */
WEAK_IRQ(13);  /* DMA1_Stream2 */
WEAK_IRQ(14);  /* DMA1_Stream3 */
WEAK_IRQ(15);  /* DMA1_Stream4 */

/* 向量表 — 必须放在 .isr_vector 段 (链接脚本保证在 0x08000000) */
__attribute__((section(".isr_vector"), used, aligned(256)))
const uint32_t g_pfnVectors[] = {
    (uint32_t)&_estack,          /* [0]  初始 SP — 硬件自动加载 */
    (uint32_t)Reset_Handler,     /* [1]  Reset */
    (uint32_t)NMI_Handler,       /* [2]  NMI */
    (uint32_t)HardFault_Handler, /* [3]  HardFault */
    (uint32_t)MemManage_Handler, /* [4]  MemManage */
    (uint32_t)BusFault_Handler,  /* [5]  BusFault */
    (uint32_t)UsageFault_Handler,/* [6]  UsageFault */
    0,                           /* [7]  Reserved */
    0,                           /* [8]  Reserved */
    0,                           /* [9]  Reserved */
    0,                           /* [10] Reserved */
    (uint32_t)SVC_Handler,       /* [11] SVCall */
    (uint32_t)DebugMon_Handler,  /* [12] Debug Monitor */
    0,                           /* [13] Reserved */
    (uint32_t)PendSV_Handler,    /* [14] PendSV */
    (uint32_t)SysTick_Handler,   /* [15] SysTick */
    /* IRQ 0..15 */
    (uint32_t)IRQ0_Handler,
    (uint32_t)IRQ1_Handler,
    (uint32_t)IRQ2_Handler,
    (uint32_t)IRQ3_Handler,
    (uint32_t)IRQ4_Handler,
    (uint32_t)IRQ5_Handler,
    (uint32_t)IRQ6_Handler,
    (uint32_t)IRQ7_Handler,
    (uint32_t)IRQ8_Handler,
    (uint32_t)IRQ9_Handler,
    (uint32_t)IRQ10_Handler,
    (uint32_t)IRQ11_Handler,
    (uint32_t)IRQ12_Handler,
    (uint32_t)IRQ13_Handler,
    (uint32_t)IRQ14_Handler,
    (uint32_t)IRQ15_Handler,
};

/* ==================================================================
 * 默认中断处理 — 死循环 (不被触发的保险)
 * ================================================================== */
__attribute__((naked)) void Default_Handler(void) {
    __asm volatile (
        "b ."  /* 无限自跳 — 触发后 LED 不闪, 可被调试器捕获 */
    );
}

/* ==================================================================
 * 复位处理 — 初始化 C 运行时 → 调用 main
 * ================================================================== */
__attribute__((naked, noreturn)) void Reset_Handler(void) {
    __asm volatile (
        /* 1. 复制 .data 段: Flash → RAM */
        "  ldr r0, =_sidata      \n"
        "  ldr r1, =_sdata       \n"
        "  ldr r2, =_edata       \n"
        "  cmp r1, r2            \n"
        "  beq 2f                \n"
        "1:                      \n"
        "  ldr r3, [r0], #4      \n"
        "  str r3, [r1], #4      \n"
        "  cmp r1, r2            \n"
        "  bne 1b                \n"

        /* 2. 清零 .bss 段 */
        "2:                      \n"
        "  ldr r0, =_sbss        \n"
        "  ldr r1, =_ebss        \n"
        "  mov r2, #0            \n"
        "  cmp r0, r1            \n"
        "  beq 4f                \n"
        "3:                      \n"
        "  str r2, [r0], #4      \n"
        "  cmp r0, r1            \n"
        "  bne 3b                \n"

        /* 3. 调用 main() */
        "4:                      \n"
        "  bl main               \n"

        /* 4. main 返回 → 死循环 */
        "  b .                   \n"
    );
}

/* ==================================================================
 * UART1 最小实现 (PA9=TX, 仅发不收)
 * ================================================================== */
#define RCC_BASE      0x40023800UL
#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIOA_BASE    0x40020000UL
#define GPIO_MODER(b)  (*(volatile uint32_t *)((b) + 0x00))
#define GPIO_AFRH(b)   (*(volatile uint32_t *)((b) + 0x24))
#define GPIO_OSPEEDR(b) (*(volatile uint32_t *)((b) + 0x08))

#define UART1_BASE    0x40011000UL
#define UART1_SR       (*(volatile uint32_t *)(UART1_BASE + 0x00))
#define UART1_DR       (*(volatile uint32_t *)(UART1_BASE + 0x04))

static void uart_init(void) {
    RCC_AHB1ENR |= 1;            /* GPIOA */
    RCC_APB2ENR |= (1U << 4);    /* USART1 */
    GPIO_MODER(GPIOA_BASE) &= ~((3U << 18) | (3U << 20));
    GPIO_MODER(GPIOA_BASE) |= (2U << 18) | (2U << 20);
    GPIO_AFRH(GPIOA_BASE) = (GPIO_AFRH(GPIOA_BASE) & ~((0xFU << 4)|(0xFU << 8)))
                          | (0x7U << 4) | (0x7U << 8);
    GPIO_OSPEEDR(GPIOA_BASE) |= (2U << 18) | (2U << 20); /* Medium */
    *(volatile uint32_t *)(UART1_BASE + 0x08) = 139; /* 115200@16MHz */
    *(volatile uint32_t *)(UART1_BASE + 0x0C) = (1U << 13) | (1U << 3) | (1U << 2);
}
static void uart_tx(char c) {
    while (!(UART1_SR & (1U << 7)));
    UART1_DR = c;
}
static void uart_s(const char *s) { while (*s) uart_tx(*s++); }
static void uart_h8(uint8_t v) {
    uart_tx("0123456789ABCDEF"[v >> 4]);
    uart_tx("0123456789ABCDEF"[v & 0xF]);
}
static void uart_h32(uint32_t v) {
    for (int i = 7; i >= 0; i--) uart_h8((v >> (i * 4)) & 0xFF);
}
static void uart_d(uint32_t v) {
    char b[12]; int p = 0;
    do { b[p++] = '0' + (v % 10); v /= 10; } while (v);
    while (p) uart_tx(b[--p]);
}

/* ==================================================================
 * LED (PF9) — 进度指示
 * ================================================================== */
#define GPIOF_BASE    0x40021400UL
#define GPIOF_ODR     (*(volatile uint32_t *)(GPIOF_BASE + 0x14))
#define GPIOF_BSRR    (*(volatile uint32_t *)(GPIOF_BASE + 0x18))
static void led_init(void) {
    RCC_AHB1ENR |= (1U << 5);
    GPIO_MODER(GPIOF_BASE) = (GPIO_MODER(GPIOF_BASE) & ~(3U << 18)) | (1U << 18);
}
#define LED_ON()   (GPIOF_BSRR = (1U << 9) << 16)
#define LED_OFF()  (GPIOF_BSRR = (1U << 9))

/* ==================================================================
 * SPI1 + W25Q128 — 纯寄存器
 * ================================================================== */
#define GPIOB_BASE    0x40020400UL
#define GPIOG_BASE    0x40021800UL

#define SPI1_BASE     0x40013000UL
#define SPI1_CR1      (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_SR       (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR       (*(volatile uint8_t  *)(SPI1_BASE + 0x0C))

#define CS_PIN        (1U << 14)   /* PB14 */
#define NRF_PIN       (1U << 7)    /* PG7  */

#define CMD_WREN      0x06
#define CMD_SE        0x20
#define CMD_PP        0x02
#define CMD_RDSR1     0x05

static uint8_t spi_xfer(uint8_t tx) {
    while (!(SPI1_SR & (1U << 1)));  /* TXE */
    SPI1_DR = tx;
    while (!(SPI1_SR & (1U << 0)));  /* RXNE */
    return SPI1_DR;
}
static inline void cs_low(void)  { *(volatile uint32_t *)(GPIOB_BASE + 0x18) = CS_PIN << 16; }
static inline void cs_high(void) { *(volatile uint32_t *)(GPIOB_BASE + 0x18) = CS_PIN; }
static void wren(void)     { cs_low(); spi_xfer(CMD_WREN);  cs_high(); }
static void wait_busy(void) {
    cs_low(); spi_xfer(CMD_RDSR1); while (spi_xfer(0xFF) & 1); cs_high();
}
static void erase_sector(uint32_t addr) {
    wren(); cs_low();
    spi_xfer(CMD_SE);
    spi_xfer((uint8_t)(addr >> 16));
    spi_xfer((uint8_t)(addr >> 8));
    spi_xfer((uint8_t)(addr));
    cs_high(); wait_busy();
}
static void page_write(const uint8_t *d, uint32_t addr, uint32_t len) {
    wren(); cs_low();
    spi_xfer(CMD_PP);
    spi_xfer((uint8_t)(addr >> 16));
    spi_xfer((uint8_t)(addr >> 8));
    spi_xfer((uint8_t)(addr));
    while (len--) spi_xfer(*d++);
    cs_high(); wait_busy();
}
static void write_data(const uint8_t *d, uint32_t addr, uint32_t len) {
    while (len) {
        uint32_t off = addr & 0xFF;
        uint32_t spc = 256 - off;
        uint32_t chk = (len < spc) ? len : spc;
        page_write(d, addr, chk);
        addr += chk; d += chk; len -= chk;
    }
}

/* ==================================================================
 * 主入口
 * ================================================================== */
int main(void) {
    uart_init();
    uart_s("\r\n>>> BATCH FLASHER v2\r\n");

    /* SPI1 + GPIO Init */
    RCC_AHB1ENR |= (1U << 1) | (1U << 6);  /* GPIOB, GPIOG */
    RCC_APB2ENR |= (1U << 12);             /* SPI1 */

    /* PB3/4/5: AF5, PB14: OUT, PG7: OUT */
    GPIO_MODER(GPIOB_BASE) &= ~((3U<<6)|(3U<<8)|(3U<<10)|(3U<<28));
    GPIO_MODER(GPIOB_BASE) |= (2U<<6)|(2U<<8)|(2U<<10)|(1U<<28);
    *(volatile uint32_t *)(GPIOB_BASE + 0x20) &= ~((0xFU<<12)|(0xFU<<16)|(0xFU<<20));
    *(volatile uint32_t *)(GPIOB_BASE + 0x20) |= (0x5U<<12)|(0x5U<<16)|(0x5U<<20);
    /* Pull-up PB14 + PG7 */
    *(volatile uint32_t *)(GPIOB_BASE + 0x0C) |= (1U << 28);
    *(volatile uint32_t *)(GPIOG_BASE + 0x0C) |= (1U << 14);
    GPIO_MODER(GPIOG_BASE) = (GPIO_MODER(GPIOG_BASE) & ~(3U<<14)) | (1U<<14);
    /* CS + NRF HIGH */
    cs_high();
    *(volatile uint32_t *)(GPIOG_BASE + 0x18) = NRF_PIN;

    /* SPI1: Master, Mode3, MSB, SSM, BR=fPCLK/4=4MHz */
    SPI1_CR1 = (1U << 2) | (1U << 1) | (1U << 0) | (1U << 9) | (1U << 8)
             | (1U << 6) | (1U << 3);  /* BR=001=div4 */

    led_init();
    LED_OFF();

    uart_s("  Batch offset=0x"); uart_h32(batch_0_offset);
    uart_s(" size="); uart_d(batch_0_size); uart_s("\r\n");

    /* 擦除 */
    uint32_t es = batch_0_offset & ~0xFFF;
    uint32_t ee = (batch_0_offset + batch_0_size + 0xFFF) & ~0xFFF;
    uart_s("  Erasing...\r\n");
    LED_ON();
    for (uint32_t s = es; s < ee; s += 4096) erase_sector(s);
    LED_OFF();

    /* 写入 */
    uart_s("  Writing...\r\n");
    LED_ON();
    write_data(batch_0_data, batch_0_offset, batch_0_size);
    LED_OFF();

    uart_s(">>> DONE\r\n");
    while (1) { LED_ON(); for (volatile int i=0;i<500000;i++); LED_OFF(); for (volatile int i=0;i<500000;i++); }
}
