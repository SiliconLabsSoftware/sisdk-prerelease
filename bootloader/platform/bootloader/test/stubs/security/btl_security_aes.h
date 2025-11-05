#ifndef BTL_SECURITY_AES_H
#define BTL_SECURITY_AES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void btl_initAesContext(void *ctx);

void btl_setAesKey(void          *ctx,
                   const uint8_t *key,
                   unsigned int  keySize,
                   bool          encryptNotDecrypt);
void btl_processAesBlock(void    *ctx,
                         uint8_t *inputBlock,
                         uint8_t *outputBlock,
                         bool    encryptNotDecrypt);
void btl_initAesCcm(void          *ctx,
                    uint8_t       flags,
                    uint8_t       *nonce,
                    uint32_t      counter,
                    const uint8_t *key,
                    unsigned int  keySize);
void btl_processAesCtrData(void          *ctx,
                           const uint8_t *input,
                           uint8_t       *output,
                           size_t        length);

#endif // BTL_SECURITY_AES_H
