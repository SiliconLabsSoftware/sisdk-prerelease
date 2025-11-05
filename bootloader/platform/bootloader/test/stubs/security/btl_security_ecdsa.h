#ifndef BTL_SECURITY_ECDSA_H
#define BTL_SECURITY_ECDSA_H

#include <stdint.h>
#include <stdbool.h>
#include "api/btl_errorcode.h"

/// Number of bytes of data to verify the signature against
#define BTL_SECURITY_ECDSA_SHA256_LENGTH    32
/// Number of bytes in the EC points that the signature consists of
#define BTL_SECURITY_ECDSA_POINT_LENGTH     32

int32_t btl_verifyEcdsaP256r1(const uint8_t *sha256,
                              const uint8_t *signatureR,
                              const uint8_t *signatureS,
                              const uint8_t *keyX,
                              const uint8_t *keyY);
#endif // BTL_SECURITY_ECDSA_H
