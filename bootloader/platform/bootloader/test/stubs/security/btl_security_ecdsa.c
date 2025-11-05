#include "btl_security_ecdsa.h"

#include <stddef.h>
#include <string.h> // For memset

int32_t btl_verifyEcdsaP256r1(const uint8_t *sha256,
                              const uint8_t *signatureR,
                              const uint8_t *signatureS,
                              const uint8_t *keyX,
                              const uint8_t *keyY)
{
  (void) *sha256;
  (void) *signatureR;
  (void) *signatureS;
  (void) *keyX;
  (void) *keyY;
  return 0;
}
