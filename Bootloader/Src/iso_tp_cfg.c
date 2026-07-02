#include "iso_tp_cfg.h"
#include "isotp.h"
#include "isotp_porting.h"
#include "canif.h"
#include "inter_flashif.h"
#include "can.h"
#include <stdio.h>
#include <string.h>

#define ISO_BUF_SIZE  4096

static IsoTpLink g_link;
static uint8_t g_send_buf[ISO_BUF_SIZE];
static uint8_t g_recv_buf[ISO_BUF_SIZE];
static uint8_t g_payload[ISO_BUF_SIZE];
volatile uint32_t g_ota_chunks = 0;   /* 已接收数据块数 */
static uint8_t g_erased = 0;          /* APP 扇区已预擦除 */

static void can_start(void)
{
    CAN_FilterTypeDef filt = {0};
    filt.FilterBank = 0;
    filt.FilterMode = CAN_FILTERMODE_IDMASK;
    filt.FilterScale = CAN_FILTERSCALE_32BIT;
    filt.FilterIdHigh = 0x0000;
    filt.FilterIdLow = 0x0000;
    filt.FilterMaskIdHigh = 0x0000;
    filt.FilterMaskIdLow = 0x0000;
    filt.FilterFIFOAssignment = CAN_RX_FIFO0;
    filt.FilterActivation = CAN_FILTER_ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &filt);
    HAL_CAN_Start(&hcan1);
    printf("[CAN] Started\n");
}

void iso_tp_init(void)
{
    can_start();
    isotp_init_link(&g_link, CAN_ID_OTA_FC,
                    g_send_buf, sizeof(g_send_buf),
                    g_recv_buf, sizeof(g_recv_buf));
    printf("[ISO-TP] recv=0x%03X send=0x%03X\n",
           CAN_ID_OTA_DATA, CAN_ID_OTA_FC);
    g_erased = 0;
}

uint8_t iso_tp_poll(void)
{
    uint8_t rx_buf[8];
    uint32_t rx_id;
    uint8_t len = CAN1_Recv_Msg(&rx_id, rx_buf);

    if (len > 0) {
        if (rx_id == CAN_ID_OTA_DATA) {
            isotp_on_can_message(&g_link, rx_buf, len);
        } else if (rx_id == CAN_ID_OTA_END && len >= 5) {
            /* 结束标记: [BE] [AD] [BE] [EF] [02] */
            if (rx_buf[0] == OTA_END_B0 &&
                rx_buf[1] == OTA_END_B1 &&
                rx_buf[2] == OTA_END_B2 &&
                rx_buf[3] == OTA_END_B3 &&
                rx_buf[4] == OTA_END_FLAG) {
                printf("[OTA] Transfer complete!\n");
                return 2;
            }
        }
    }

    isotp_poll(&g_link);

    uint16_t out_size = 0;
    int ret = isotp_receive(&g_link, g_payload, sizeof(g_payload), &out_size);
    if (ret == ISOTP_RET_OK && out_size >= 9) {
        if (g_payload[0] == OTA_PACKET_HEADER) {
            uint32_t offset = ((uint32_t)g_payload[1] << 24) |
                              ((uint32_t)g_payload[2] << 16) |
                              ((uint32_t)g_payload[3] << 8)  |
                              ((uint32_t)g_payload[4]);
            uint32_t size   = ((uint32_t)g_payload[5] << 24) |
                              ((uint32_t)g_payload[6] << 16) |
                              ((uint32_t)g_payload[7] << 8)  |
                              ((uint32_t)g_payload[8]);
            uint32_t addr = INTER_FLASH_APP_ADDR + offset;

            /* 首次收到数据时预擦除整个 APP 区 */
            if (!g_erased) {
                printf("[OTA] Pre-erasing APP area...\n");
                for (uint32_t a = INTER_FLASH_APP_ADDR; a < INTER_FLASH_PARAM_ADDR; ) {
                    inter_flashif_erase_sector(a);
                    if (a < 0x08010000)      a += 0x4000;
                    else if (a < 0x08020000) a += 0x10000;
                    else                     a += 0x20000;
                }
                g_erased = 1;
                printf("[OTA] Erase done\n");
            }

            printf("[OTA] %lu bytes @ 0x%08lX\n", size, addr);
            g_ota_chunks++;

            uint32_t words = size / 4;
            if (words > 0)
                inter_flashif_write(addr, (uint32_t *)&g_payload[9], words);
            uint32_t rem = size % 4;
            if (rem > 0) {
                uint32_t last = 0xFFFFFFFF;
                memcpy(&last, &g_payload[9 + words * 4], rem);
                inter_flashif_write(addr + words * 4, &last, 1);
            }
            return 1;
        }
    }
    return 0;
}
