/**
 * @file    stm32f407_loader.ld
 * @brief   External Loader GCC linker — 全部驻留 SRAM
 *          CubeProgrammer 要求: LOAD 段 4 字节对齐, 无大块填充
 */

ENTRY(Init)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 126K
}

PHDRS
{
    loader PT_LOAD FLAGS(7);   /* R+W+E (7=PF_R|PF_W|PF_X) */
}

SECTIONS
{
    /* Dev_Inf 必须优先, 最小对齐 */
    .Dev_Inf : ALIGN(4) {
        KEEP(*(.Dev_Inf))
    } > SRAM :loader

    .text : ALIGN(4) {
        *(.text .text* .glue_7 .glue_7t)
        *(.rodata .rodata*)
        . = ALIGN(4);
    } > SRAM :loader

    .data : ALIGN(4) {
        *(.data .data*)
        . = ALIGN(4);
    } > SRAM :loader

    .bss (NOLOAD) : ALIGN(4) {
        *(.bss .bss*)
        *(COMMON)
        . = ALIGN(4);
    } > SRAM

    .comment 0 (INFO) : { *(.comment) }

    _estack = ORIGIN(SRAM) + LENGTH(SRAM);
}
