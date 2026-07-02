#ifndef __ISO_TP_CFG_H__
#define __ISO_TP_CFG_H__

#include <stdint.h>

#define CAN_ID_OTA_DATA    0x0B1U
#define CAN_ID_OTA_FC      0x0B2U
#define CAN_ID_OTA_END     0x0B3U

#define OTA_PACKET_HEADER  0xBF
/* 结束标记 CAN 帧: [BE] [AD] [BE] [EF] [02] */
#define OTA_END_B0  0xBE
#define OTA_END_B1  0xAD
#define OTA_END_B2  0xBE
#define OTA_END_B3  0xEF
#define OTA_END_FLAG 0x02

void iso_tp_init(void);
uint8_t iso_tp_poll(void);

/* OTA 进度：已接收的数据块数（供 LED 闪烁指示进度） */
extern volatile uint32_t g_ota_chunks;

#endif
