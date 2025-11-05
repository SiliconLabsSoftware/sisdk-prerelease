#ifndef BTL_SHA256_H
#define BTL_SHA256_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint32_t total[2];              /*!< number of bytes processed  */
  uint32_t state[8];              /*!< intermediate digest state  */
  unsigned char buffer[64];       /*!< data block being processed */
}
btl_sha256_context;

typedef enum {
  SHA256                          /*!< SHA-256 type */
} SHA_Type_t;

void btl_sha256_init(btl_sha256_context *ctx);

int btl_sha256_starts_ret(btl_sha256_context *ctx, int is224);

int btl_sha256_update_ret(btl_sha256_context *ctx, const unsigned char *input, size_t ilen);

int btl_sha256_finish_ret(btl_sha256_context *ctx, unsigned char output[32]);

int sha_x_process(SHA_Type_t algo,
                  uint8_t* state_in,
                  const unsigned char *blockdata,
                  uint8_t* state_out,
                  uint32_t num_blocks);
int sha_x_update(SHA_Type_t algo,
                 const unsigned char *data,
                 size_t data_len,
                 uint8_t* state,
                 unsigned char *buffer,
                 uint32_t* counter);

int sha_x_finish(SHA_Type_t algo,
                 uint8_t* state,
                 uint8_t* buffer,
                 uint32_t* counter,
                 uint8_t* output);
#endif // BTL_SHA256_H
