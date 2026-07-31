/**
 * Minimalist HMAC-SHA256 implementation in C.
 * Public Domain / CC0.
 * Adapted for ClaVel framework.
 */

#ifndef CLAVEL_HMAC_SHA256_H
#define CLAVEL_HMAC_SHA256_H

#include <stddef.h>
#include <stdint.h>

/* SHA-256 context */
typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[]);

/* HMAC-SHA256 */
void hmac_sha256(const uint8_t *key, size_t keylen,
                 const uint8_t *data, size_t datalen,
                 uint8_t out[32]);

/* Convierte 32 bytes en 64 caracteres hex + nulo */
void hmac_sha256_hex(const uint8_t *key, size_t keylen,
                     const uint8_t *data, size_t datalen,
                     char out_hex[65]);

#endif /* CLAVEL_HMAC_SHA256_H */
