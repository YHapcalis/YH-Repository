ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    /* 中断向量表 — 必须在 0x08000000 */
    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* 代码 + 只读数据 (包括 batch 数组) */
    .text : {
        . = ALIGN(4);
        *(.text .text*)
        *(.rodata .rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* .data 段初始化变量 — 加载地址 = _sidata, 运行时地址 = _sdata.._edata */
    _sidata = LOADADDR(.data);

    .data : {
        . = ALIGN(4);
        _sdata = .;
        *(.data .data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH

    /* .bss 段 — 零初始化 */
    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss .bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* 栈顶 */
    _estack = ORIGIN(RAM) + LENGTH(RAM);
}
