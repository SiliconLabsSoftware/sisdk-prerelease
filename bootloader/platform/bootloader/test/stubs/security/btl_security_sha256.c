#include "btl_security_sha256.h"
#include "mbedtls/sha256.h"
#include "btl_security_types.h"
#include "api/btl_errorcode.h"

#if BTL_SECURITY_SHA256_DIGEST_LENGTH % 4 != 0
#error "SHA digest size is not a multiple of native data type"
#endif

void btl_initSha256(void *ctx)
{
  Sha256Context_t *context = (Sha256Context_t *)ctx;
  mbedtls_sha256_init(&(context->shaContext));
  mbedtls_sha256_starts_ret(&(context->shaContext), 0);
}

void btl_updateSha256(void *ctx, const void *data, size_t length)
{
  Sha256Context_t *context = (Sha256Context_t *)ctx;
  mbedtls_sha256_update_ret(&(context->shaContext), data, length);
}

void btl_finalizeSha256(void *ctx)
{
  Sha256Context_t *context = (Sha256Context_t *)ctx;
  mbedtls_sha256_finish_ret(&(context->shaContext), context->sha);
  return;
}

int32_t btl_verifySha256(void *ctx, const void *sha)
{
  Sha256Context_t *context = (Sha256Context_t *)ctx;
  unsigned int* sha_calculated = (unsigned int *)context->sha;
  unsigned int* sha_verifying  = (unsigned int *)sha;

  if (context == NULL || sha == NULL) {
    return BOOTLOADER_ERROR_SECURITY_INVALID_PARAM;
  }

  for (unsigned int word = 0; word < (BTL_SECURITY_SHA256_DIGEST_LENGTH / sizeof(unsigned int)); word++) {
    if (sha_verifying[word] != sha_calculated[word]) {
      return BOOTLOADER_ERROR_SECURITY_REJECTED;
    }
  }

  return BOOTLOADER_OK;
}
