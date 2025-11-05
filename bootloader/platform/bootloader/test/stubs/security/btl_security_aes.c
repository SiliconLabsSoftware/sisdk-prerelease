#include "btl_security_aes.h"
#include "btl_security_types.h"

#include <string.h> // For memory copy functions
#include <stdio.h>

// Initialize AES context variable
void btl_initAesContext(void *ctx)
{
  (void) *ctx;
  return;
}

// Set initial key
void btl_setAesKey(void          *ctx,
                   const uint8_t *key,
                   unsigned int  keySize,
                   bool          encryptNotDecrypt)
{
  (void) *ctx;
  (void) *key;
  (void) keySize;
  (void) encryptNotDecrypt;
  return;
}

// Process AES block. Block size is 16 bytes == 128 bit.
void btl_processAesBlock(void    *ctx,
                         uint8_t *inputBlock,
                         uint8_t *outputBlock,
                         bool    encryptNotDecrypt)
{
  (void) *ctx;
  (void) *inputBlock;
  (void) *outputBlock;
  (void) encryptNotDecrypt;
  return;
}

// Initialize AES-CTR algorithm in CCM mode as used by GBL
void btl_initAesCcm(void          *ctx,
                    uint8_t       flags,
                    uint8_t       *nonce,
                    uint32_t      counter,
                    const uint8_t *key,
                    unsigned int  keySize)
{
  (void) *ctx;
  (void) flags;
  (void) *nonce;
  (void) counter;
  (void) *key;
  (void) keySize;
  return;
}

// Process AES-CTR data.
void btl_processAesCtrData(void          *ctx,
                           const uint8_t *input,
                           uint8_t       *output,
                           size_t        length)
{
  (void) *ctx;
  (void) *input;
  (void) *output;
  (void) length;
  return;
}
