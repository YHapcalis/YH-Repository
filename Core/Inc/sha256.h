/*
 * sha256.h — SHA-256 + HMAC-SHA256 固件签名
 *
 * 提供标准 SHA-256 哈希和 HMAC-SHA256 认证。
 * 用于 Bootloader 在跳转 APP 前验证固件签名。
 *
 * HMAC-SHA256(k, data) = SHA256((k ⊕ opad) || SHA256((k ⊕ ipad) || data))
 */

#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SHA-256 上下文 (56 bytes) */
typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

/* SHA-256 API */
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[32]);

/* HMAC-SHA256: 一步完成 HMAC 计算 */
void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t out[32]);

/* ── 固件签名常量 ── */

#define SIG_MAGIC       0xA5A5A5A5U
#define SIG_SIZE        40U          /* 签名块总大小 */
#define SIG_HMAC_OFF    4U           /* HMAC 在签名块中的偏移 */
#define SIG_MAGIC_OFF   36U          /* Magic 在签名块中的偏移 */

#pragma pack(1)
typedef struct {
    uint32_t fw_size;       /* 固件原始大小 (不含签名) */
    uint8_t  hmac[32];      /* HMAC-SHA256 签名 */
    uint32_t magic;         /* SIG_MAGIC */
} fw_signature_t;
#pragma pack()

/* 签名块固定地址 (APP 区域尾部, 参数扇区之前) */
#define SIG_ADDR        ((volatile fw_signature_t *)0x080DFF80UL)

/* 验证固件签名: 0=有效, 1=无效/未签名 */
int verify_firmware_sig(void);

#ifdef __cplusplus
}
#endif

#endif /* SHA256_H */
