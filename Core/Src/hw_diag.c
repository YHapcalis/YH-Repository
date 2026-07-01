/*
 * hw_diag.c — 硬件诊断实现
 *
 * 从 freertos.c 拆出的独立模块，内容与原 static void hw_diag() 完全一致。
 * 在 StartGUITask 中调用一次，输出诊断结果后不再使用。
 *
 * 诊断内容：
 *   1. 外部 SRAM 16-bit Walking-1/0 测试
 *   2. SPI Flash JEDEC ID + 状态寄存器
 *   3. SPI Flash 全部图片头部检查
 *   4. Moto 像素验证 (S: 路径)
 *   5. SPI Flash 擦写回路测试
 */

#include "hw_diag.h"
#include <stdio.h>
#include "en25q128.h"
#include "spiflash_offset_table.h"

void hw_diag(void)
{
    int pass = 0, fail = 0;

    /* ===== 1. 外部 SRAM 16-bit 读写 ===== */
    printf("\r\n=== SRAM 16-bit Diag @ 0x68000000 ===\r\n");
    volatile uint16_t *sram = (volatile uint16_t *)0x68000000;
    uint16_t orig0 = sram[0];
    uint16_t orig1 = sram[1000];

    /* 0xAA55 / 0x55AA — 每个 bit 都翻转 */
    uint16_t pat;
    pat = 0xAA55; sram[0] = pat;
    printf("  wr=0x%04X rd=0x%04X %s\r\n", pat, sram[0], sram[0] == pat ? "[PASS]" : "[FAIL]");
    if (sram[0] == pat) pass++; else fail++;

    pat = 0x55AA; sram[0] = pat;
    printf("  wr=0x%04X rd=0x%04X %s\r\n", pat, sram[0], sram[0] == pat ? "[PASS]" : "[FAIL]");
    if (sram[0] == pat) pass++; else fail++;

    /* Walking-1: 16 个单 bit 逐一走一遍 */
    uint16_t w1 = 1;
    int wp = 0;
    for (int i = 0; i < 16; i++) {
        sram[0] = w1;
        if (sram[0] == w1) wp++;
        w1 <<= 1;
    }
    printf("  Walking-1: %d/16 %s\r\n", wp, wp == 16 ? "[PASS]" : "[FAIL]");
    if (wp == 16) pass++; else fail++;

    /* Walking-0: 16 个单 bit 清零 */
    uint16_t w0 = 0xFFFE;
    wp = 0;
    for (int i = 0; i < 16; i++) {
        sram[0] = w0;
        if (sram[0] == w0) wp++;
        w0 = (w0 << 1) | 1;
    }
    printf("  Walking-0: %d/16 %s\r\n", wp, wp == 16 ? "[PASS]" : "[FAIL]");
    if (wp == 16) pass++; else fail++;

    /* 另一地址验证不是总线电容残留 */
    pat = 0x1234; sram[1000] = pat;
    printf("  addr+2000: wr=0x%04X rd=0x%04X %s\r\n", pat, sram[1000], sram[1000] == pat ? "[PASS]" : "[FAIL]");
    if (sram[1000] == pat) pass++; else fail++;
    sram[1000] = orig1;
    sram[0]    = orig0;

    printf("  SRAM: %d pass, %d fail\r\n", pass, fail);

    /* ===== 2. SPI Flash ===== */
    printf("\r\n=== SPI Flash Diag ===\r\n");

    /* JEDEC ID */
    uint32_t id = EN25Q128_ReadID();
    printf("  JEDEC ID: 0x%06lX (%s)\r\n", id,
           id == EN25Q128_EXPECTED_ID ? "OK" : "MISMATCH");

    /* Status */
    uint8_t sr1 = EN25Q128_ReadSR1();
    uint8_t sr2 = EN25Q128_ReadSR2();
    printf("  SR1=0x%02X (BUSY=%d) SR2=0x%02X\r\n", sr1, (sr1 & 0x01) ? 1 : 0, sr2);

    /* ---- 诊断: 遍历全部图片检查 LVGL header (magic=0x19) ---- */
    printf("\r\n  SPI Flash header check (all images):\r\n");

    #define CHK_ENTRY(name) { OFFSET_##name, #name }
    static const struct { uint32_t off; const char *label; } img_tbl[] = {
        CHK_ENTRY(_BATTERY_BAK_RGB565A8_43X24_MAP),
        CHK_ENTRY(_BATTERY_IND_RGB565A8_43X24_MAP),
        CHK_ENTRY(_DET_BAK_RGB565A8_800X480_MAP),
        CHK_ENTRY(_DIRECTION_RGB565A8_50X42_MAP),
        CHK_ENTRY(_HIGH_BEAM_RGB565A8_46X46_MAP),
        CHK_ENTRY(_HOME_RGB565A8_43X43_MAP),
        CHK_ENTRY(_IMAGE_GREEN_RGB565A8_350X350_MAP),
        CHK_ENTRY(_IMAGE_RED_RGB565A8_350X350_MAP),
        CHK_ENTRY(_IMG_NAV_1_RGB565A8_116X116_MAP),
        CHK_ENTRY(_KMBG_RGB565A8_166X166_MAP),
        CHK_ENTRY(_LIGHT_RGB565A8_46X46_MAP),
        CHK_ENTRY(_LOGO_RGB565A8_100X35_MAP),
        CHK_ENTRY(_MONITOR_RGB565A8_41X37_MAP),
        CHK_ENTRY(_MOTO_RGB565A8_800X480_MAP),
        CHK_ENTRY(_MUSIC_COVER_1_RGB565A8_183X183_MAP),
        CHK_ENTRY(_MUSIC_COVER_2_RGB565A8_183X183_MAP),
        CHK_ENTRY(_MUSIC_COVER_3_RGB565A8_183X183_MAP),
        CHK_ENTRY(_SUNNY_RGB565A8_66X66_MAP),
        CHK_ENTRY(_TEM_RGB565A8_91X123_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_0_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_01_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_02_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_03_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_04_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_05_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_06_MAP),
    };
    #undef CHK_ENTRY

    int hdr_ok = 0, hdr_bad = 0, hdr_empty = 0;
    uint8_t hdr[12];
    for (int i = 0; i < (int)(sizeof(img_tbl)/sizeof(img_tbl[0])); i++) {
        EN25Q128_Read(hdr, img_tbl[i].off, 12);
        if (hdr[0] == 0x19) {
            hdr_ok++;
        } else if (hdr[0] == 0xFF) {
            hdr_empty++;
            printf("  [ERASED] @ 0x%08lX %s\r\n", img_tbl[i].off, img_tbl[i].label);
        } else {
            hdr_bad++;
            printf("  [BAD 0x%02X] @ 0x%08lX %s\r\n", hdr[0], img_tbl[i].off, img_tbl[i].label);
        }
    }
    printf("  Header result: %d OK, %d EMPTY, %d BAD (total %d)\r\n",
           hdr_ok, hdr_empty, hdr_bad, hdr_ok + hdr_empty + hdr_bad);

    /* ---- moto 背景深度验证 (头部 + 像素首尾 + 随机采样) ---- */
    printf("\r\n  moto pixel verify (512B head + 512B mid + 512B tail):\r\n");
    EN25Q128_Read(hdr, OFFSET__MOTO_RGB565A8_800X480_MAP, 12);
    if (hdr[0] != 0x19) {
        printf("  SKIP — moto header missing\r\n");
    } else {
        uint16_t mw = hdr[4] | ((uint16_t)hdr[5] << 8);
        uint16_t mh = hdr[6] | ((uint16_t)hdr[7] << 8);
        uint16_t ms = hdr[8] | ((uint16_t)hdr[9] << 8);
        printf("  Header: w=%u h=%u stride=%u\r\n", mw, mh, ms);

        /* moto 已走 S: 路径, 无 SRAM 副本可对比 */
        printf("  SKIP — SRAM moto data not loaded\r\n");
    }

    /* Erase+Write+Read 回路测试 (最后扇区 0x00FFF000) */
    uint32_t test_sec = 0x00FFF000;
    uint8_t wbuf[256], rbuf[256];
    for (int i = 0; i < 256; i++) wbuf[i] = (uint8_t)i;

    printf("  EraseWrite+Read @ 0x%08lX ... ", test_sec);
    EN25Q128_EraseWrite(wbuf, test_sec, 256);
    EN25Q128_Read(rbuf, test_sec, 256);

    int r_ok = 1;
    for (int i = 0; i < 256; i++) { if (wbuf[i] != rbuf[i]) { r_ok = 0; break; } }
    printf("%s\r\n", r_ok ? "PASS" : "FAIL (first mismatch below)");
    if (!r_ok) {
        for (int i = 0; i < 256; i++) {
            if (wbuf[i] != rbuf[i]) {
                printf("  @[%d] wr=0x%02X rd=0x%02X\r\n", i, wbuf[i], rbuf[i]);
                break;
            }
        }
    }
    printf("=== HW Diag Done ===\r\n\r\n");
}
