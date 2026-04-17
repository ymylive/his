/**
 * @file sha256.h
 * @brief Minimal SHA-256 implementation (public domain)
 *
 * Derived from Brad Conte's public-domain reference implementation
 * (https://github.com/B-Con/crypto-algorithms). See LICENSE in this
 * directory for the full notice. Used here to verify integrity of
 * downloaded update packages without pulling in OpenSSL/libsodium.
 */

#ifndef HIS_THIRD_PARTY_SHA256_H
#define HIS_THIRD_PARTY_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_BLOCK_SIZE 32  /* digest size in bytes */

typedef struct {
    uint8_t  data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[SHA256_BLOCK_SIZE]);

/**
 * @brief Compute the SHA-256 digest of a file identified by path.
 *
 * @param path Path to the file to hash (opened with "rb").
 * @param out  Output buffer that receives the 32-byte digest.
 * @return 1 on success, 0 on failure (e.g. file cannot be opened).
 */
int sha256_file(const char *path, uint8_t out[SHA256_BLOCK_SIZE]);

/**
 * @brief Format a 32-byte digest as a lowercase hex string.
 *
 * @param digest 32-byte SHA-256 digest.
 * @param hex    Output buffer of at least 65 bytes (64 hex chars + NUL).
 */
void sha256_hex(const uint8_t digest[SHA256_BLOCK_SIZE], char hex[65]);

#endif
