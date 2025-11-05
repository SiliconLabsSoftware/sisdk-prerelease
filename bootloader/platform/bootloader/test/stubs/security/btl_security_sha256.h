#ifndef BTL_SECURITY_SHA256_H
#define BTL_SECURITY_SHA256_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/// Number of bytes in a SHA-256 digest
#define BTL_SECURITY_SHA256_DIGEST_LENGTH   32

void btl_initSha256(void *ctx);
void btl_updateSha256(void *ctx, const void *data, size_t length);
void btl_finalizeSha256(void *ctx);
int32_t btl_verifySha256(void *ctx, const void *sha256);
#endif // BTL_SECURITY_SHA256_H
