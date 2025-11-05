#include <stdint.h>
#include "btl_sha256.h"

int sha_x_process(SHA_Type_t algo,
                  uint8_t* state_in,
                  const unsigned char *blockdata,
                  uint8_t* state_out,
                  uint32_t num_blocks)
{
  (void) algo;
  (void) *state_in;
  (void) blockdata;
  (void) *state_out;
  (void) num_blocks;
  return 0;
}
