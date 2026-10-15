/***************************************************************************//**
 * @file
 * @brief AES decryption functionality for Silicon Labs bootloader
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "btl_security_aes.h"
#include "btl_security_types.h"
#include "api/btl_errorcode.h"

#include <string.h>

#if defined(CRYPTOACC_PRESENT)
#include "cryptoacc_management.h"
#include "sx_aes.h"
#include "sx_errors.h"
#endif

/***************************************************************************//**
 * @brief Enable the AES accelerator clock for this device class.
 ******************************************************************************/
static void btl_aes_enable_clock(void)
{
#if defined(_CMU_CLKEN1_MASK) && defined(CRYPTOACC_PRESENT)
  CMU->CLKEN1_SET = CMU_CLKEN1_CRYPTOACC;
  CMU->CRYPTOACCCLKCTRL_SET = CMU_CRYPTOACCCLKCTRL_AESEN;
#elif defined(_CMU_CLKEN1_SEMAILBOXHOST_MASK)
  CMU->CLKEN1_SET = CMU_CLKEN1_SEMAILBOXHOST;
#endif
}

#if defined(SEMAILBOX_PRESENT)

#include "sli_se_manager_mailbox.h"

#if defined(SE_MANAGER_CONFIG_FILE)
#include SE_MANAGER_CONFIG_FILE
#endif

#ifndef SLI_SE_AES_CTR_NUM_BLOCKS_BUFFERED
#if defined(BOOTLOADER_AES_CTR_NUM_BLOCKS_BUFFERED)
#define SLI_SE_AES_CTR_NUM_BLOCKS_BUFFERED BOOTLOADER_AES_CTR_NUM_BLOCKS_BUFFERED
#else
#define SLI_SE_AES_CTR_NUM_BLOCKS_BUFFERED 1U
#endif
#endif

#define BTL_AES_BLOCK_SIZE_BYTES                16U
#define BTL_AES_CTR_NUM_BLOCKS_BUFFERED         SLI_SE_AES_CTR_NUM_BLOCKS_BUFFERED
#define BTL_AES_CTR_STREAM_BYTES                  (BTL_AES_BLOCK_SIZE_BYTES * BTL_AES_CTR_NUM_BLOCKS_BUFFERED)
#define BTL_AES_CTR_STREAM_MASK                   (BTL_AES_CTR_STREAM_BYTES - 1U)

#define BTL_AES_SE_IMMUTABLE_KEY_SLOT           0xFAU

#define BTL_AES_SE_KEYSPEC_INTERNAL \
  ((1UL << 26) | ((uint32_t)BTL_AES_SE_IMMUTABLE_KEY_SLOT << 16) | (1UL << 24))

/***************************************************************************//**
 * @brief Build an SE keyspec for AES operations in the bootloader.
 *
 ******************************************************************************/
static uint32_t btl_aes_se_keyspec(bool useInternalKey, unsigned int keySizeBits)
{
  uint32_t size_bytes = keySizeBits / 8U;

  if (useInternalKey) {
    // INTERNAL_IMMUTABLE slot 0xFA, NON_EXPORTABLE, plus symmetric key size.
    return BTL_AES_SE_KEYSPEC_INTERNAL | (size_bytes & 0x3FFU);
  }

  // External plaintext symmetric key: keyspec is key size in bytes only.
  return size_bytes;
}

/***************************************************************************//**
 * @brief Run one AES-ECB mailbox command.
 ******************************************************************************/
static int btl_aes_se_mailbox_ecb(uint32_t keyspec,
                                  const uint8_t *key,
                                  unsigned int keySizeBytes,
                                  bool encryptNotDecrypt,
                                  const uint8_t *input,
                                  uint8_t *output,
                                  size_t length)
{
  sli_se_mailbox_command_t command = SLI_SE_MAILBOX_COMMAND_DEFAULT(
    (encryptNotDecrypt ? SLI_SE_COMMAND_AES_ENCRYPT : SLI_SE_COMMAND_AES_DECRYPT)
    | SLI_SE_COMMAND_OPTION_MODE_ECB
    | SLI_SE_COMMAND_OPTION_CONTEXT_WHOLE);

  sli_se_mailbox_command_add_parameter(&command, keyspec);
  sli_se_mailbox_command_add_parameter(&command, (uint32_t)length);

  sli_se_datatransfer_t auth = SLI_SE_DATATRANSFER_DEFAULT(NULL, 0);
  sli_se_datatransfer_t key_in = SLI_SE_DATATRANSFER_DEFAULT(
    (key != NULL) ? (uint8_t *)key : NULL,
    (key != NULL) ? keySizeBytes : 0U);
  sli_se_mailbox_command_add_input(&command, &auth);
  sli_se_mailbox_command_add_input(&command, &key_in);

  sli_se_datatransfer_t in = SLI_SE_DATATRANSFER_DEFAULT((uint8_t *)input, length);
  sli_se_mailbox_command_add_input(&command, &in);

  sli_se_datatransfer_t out = SLI_SE_DATATRANSFER_DEFAULT(output, length);
  sli_se_mailbox_command_add_output(&command, &out);

  sli_se_mailbox_execute_command(&command);

  volatile sli_se_mailbox_response_t response = sli_se_mailbox_read_response();
  return (response == SLI_SE_RESPONSE_OK) ? BOOTLOADER_OK : BOOTLOADER_ERROR_SECURITY_REJECTED;
}

/***************************************************************************//**
 * @brief Increment a big-endian counter ending at block_end (inclusive).
 ******************************************************************************/
static void btl_aes_se_increment_counter_at(uint8_t block_end,
                                            uint8_t nonce_counter[])
{
  for (size_t i = 0U; i < BTL_AES_BLOCK_SIZE_BYTES; i++) {
    nonce_counter[block_end - i]
      = (uint8_t)(nonce_counter[block_end - i] + 1U);
    if (nonce_counter[block_end - i] != 0U) {
      break;
    }
  }
}

#if (BTL_AES_CTR_NUM_BLOCKS_BUFFERED > 1U)
/***************************************************************************//**
 * @brief Prepare multi-block nonce counter values for buffered CTR keystream.
 *
 ******************************************************************************/
static void btl_aes_se_prepare_nonce_counter(uint8_t nonce_counter[BTL_AES_BLOCK_SIZE_BYTES],
                                             uint8_t stream_block[BTL_AES_CTR_STREAM_BYTES])
{
  uint8_t no_of_blocks = (uint8_t)((BTL_AES_CTR_STREAM_BYTES / BTL_AES_BLOCK_SIZE_BYTES));

  memcpy(stream_block, nonce_counter, BTL_AES_BLOCK_SIZE_BYTES);

  for (size_t i = 0U; i < (size_t)no_of_blocks - 1U; i++) {
    memcpy(&stream_block[(i * BTL_AES_BLOCK_SIZE_BYTES) + BTL_AES_BLOCK_SIZE_BYTES],
           &stream_block[i * BTL_AES_BLOCK_SIZE_BYTES],
           BTL_AES_BLOCK_SIZE_BYTES);
    btl_aes_se_increment_counter_at((uint8_t)(((i + 2U) * BTL_AES_BLOCK_SIZE_BYTES) - 1U),
                                    stream_block);
  }

  memcpy(nonce_counter,
         &stream_block[((size_t)no_of_blocks - 1U) * BTL_AES_BLOCK_SIZE_BYTES],
         BTL_AES_BLOCK_SIZE_BYTES);
}
#endif // BTL_AES_CTR_NUM_BLOCKS_BUFFERED > 1U

/***************************************************************************//**
 * @brief Run AES-CTR via direct SE mailbox commands.
 *
 ******************************************************************************/
static int btl_aes_se_mailbox_ctr(uint32_t keyspec,
                                  const uint8_t *key,
                                  unsigned int keySizeBytes,
                                  size_t length,
                                  size_t *nc_off,
                                  uint8_t nonce_counter[BTL_AES_BLOCK_SIZE_BYTES],
                                  uint8_t stream_block[BTL_AES_CTR_STREAM_BYTES],
                                  const uint8_t *input,
                                  uint8_t *output)
{
  size_t n = (nc_off != NULL) ? *nc_off : 0U;
  size_t processed = 0U;

  while (processed < length) {
    if (n > 0U) {
      output[processed] = (uint8_t)(input[processed] ^ stream_block[n]);
      n = (n + 1U) & BTL_AES_CTR_STREAM_MASK;
      processed++;
    } else {
      size_t iterations = (length - processed) / BTL_AES_BLOCK_SIZE_BYTES;

      if (iterations > 0U) {
        sli_se_mailbox_command_t command = SLI_SE_MAILBOX_COMMAND_DEFAULT(
          SLI_SE_COMMAND_AES_ENCRYPT
          | SLI_SE_COMMAND_OPTION_MODE_CTR
          | SLI_SE_COMMAND_OPTION_CONTEXT_ADD);

        sli_se_mailbox_command_add_parameter(&command, keyspec);
        sli_se_mailbox_command_add_parameter(&command,
                                             (uint32_t)(iterations * BTL_AES_BLOCK_SIZE_BYTES));

        sli_se_datatransfer_t auth = SLI_SE_DATATRANSFER_DEFAULT(NULL, 0);
        sli_se_datatransfer_t key_in = SLI_SE_DATATRANSFER_DEFAULT(
          (key != NULL) ? (uint8_t *)key : NULL,
          (key != NULL) ? keySizeBytes : 0U);
        sli_se_mailbox_command_add_input(&command, &auth);
        sli_se_mailbox_command_add_input(&command, &key_in);

        sli_se_datatransfer_t iv_in = SLI_SE_DATATRANSFER_DEFAULT(nonce_counter,
                                                                  BTL_AES_BLOCK_SIZE_BYTES);
        sli_se_datatransfer_t in = SLI_SE_DATATRANSFER_DEFAULT(
          (uint8_t *)&input[processed],
          iterations * BTL_AES_BLOCK_SIZE_BYTES);
        sli_se_mailbox_command_add_input(&command, &iv_in);
        sli_se_mailbox_command_add_input(&command, &in);

        sli_se_datatransfer_t out = SLI_SE_DATATRANSFER_DEFAULT(
          &output[processed],
          iterations * BTL_AES_BLOCK_SIZE_BYTES);
        sli_se_datatransfer_t iv_out = SLI_SE_DATATRANSFER_DEFAULT(nonce_counter,
                                                                   BTL_AES_BLOCK_SIZE_BYTES);
        sli_se_mailbox_command_add_output(&command, &out);
        sli_se_mailbox_command_add_output(&command, &iv_out);

        sli_se_mailbox_execute_command(&command);

        volatile sli_se_mailbox_response_t response = sli_se_mailbox_read_response();
        if (response != SLI_SE_RESPONSE_OK) {
          return BOOTLOADER_ERROR_SECURITY_REJECTED;
        }

        processed += iterations * BTL_AES_BLOCK_SIZE_BYTES;
      }

      while ((length - processed) > 0U) {
        if (n == 0U) {
          const uint8_t *counter_ptr;

#if (BTL_AES_CTR_NUM_BLOCKS_BUFFERED > 1U)
          btl_aes_se_prepare_nonce_counter(nonce_counter, stream_block);
          counter_ptr = stream_block;
#else
          counter_ptr = nonce_counter;
#endif

          if (btl_aes_se_mailbox_ecb(keyspec,
                                     key,
                                     keySizeBytes,
                                     true,
                                     counter_ptr,
                                     stream_block,
                                     BTL_AES_CTR_STREAM_BYTES) != BOOTLOADER_OK) {
            return BOOTLOADER_ERROR_SECURITY_REJECTED;
          }
          btl_aes_se_increment_counter_at(BTL_AES_BLOCK_SIZE_BYTES - 1U, nonce_counter);
        }
        output[processed] = (uint8_t)(input[processed] ^ stream_block[n]);
        n = (n + 1U) & BTL_AES_CTR_STREAM_MASK;
        processed++;
      }
    }
  }

  if (nc_off != NULL) {
    *nc_off = n;
  }

  return BOOTLOADER_OK;
}

#endif // SEMAILBOX_PRESENT

#if defined(CRYPTOACC_PRESENT)
/***************************************************************************//**
 * @brief AES-ECB one-block encrypt/decrypt via CryptoAcc.
 ******************************************************************************/
static int btl_aes_cryptoacc_ecb(const uint8_t *key,
                                 unsigned int   keybits,
                                 bool           encryptNotDecrypt,
                                 const uint8_t  input[16],
                                 uint8_t        output[16])
{
  int status;
  uint32_t sx_ret;
  block_t key_block;
  block_t data_in;
  block_t data_out;

  if ((keybits != 128U) && (keybits != 192U) && (keybits != 256U)) {
    return BOOTLOADER_ERROR_SECURITY_INVALID_PARAM;
  }

  key_block = block_t_convert(key, keybits / 8U);
  data_in = block_t_convert(input, 16);
  data_out = block_t_convert(output, 16);

  status = cryptoacc_management_acquire();
  if (status != 0) {
    return BOOTLOADER_ERROR_SECURITY_REJECTED;
  }

  if (encryptNotDecrypt) {
    sx_ret = sx_aes_ecb_encrypt((const block_t *)&key_block,
                                (const block_t *)&data_in,
                                &data_out);
  } else {
    sx_ret = sx_aes_ecb_decrypt((const block_t *)&key_block,
                                (const block_t *)&data_in,
                                &data_out);
  }

  cryptoacc_management_release();

  return (sx_ret == CRYPTOLIB_SUCCESS) ? BOOTLOADER_OK : BOOTLOADER_ERROR_SECURITY_REJECTED;
}

/***************************************************************************//**
 * @brief AES-CTR encrypt/decrypt via CryptoAcc (same algorithm for both).
 ******************************************************************************/
static int btl_aes_cryptoacc_ctr(const uint8_t *key,
                                 unsigned int   keybits,
                                 size_t         length,
                                 size_t        *nc_off,
                                 uint8_t        nonce_counter[16],
                                 uint8_t        stream_block[16],
                                 const uint8_t *input,
                                 uint8_t       *output)
{
  int status;
  size_t n = (nc_off != NULL) ? *nc_off : 0U;
  size_t processed = 0;
  uint32_t sx_ret;
  block_t key_block;
  block_t iv_block;
  block_t data_in;
  block_t data_out;

  if ((keybits != 128U) && (keybits != 192U) && (keybits != 256U)) {
    return BOOTLOADER_ERROR_SECURITY_INVALID_PARAM;
  }

  key_block = block_t_convert(key, keybits / 8U);
  iv_block = block_t_convert(nonce_counter, 16);

  while (processed < length) {
    if (n > 0U) {
      output[processed] = (uint8_t)(input[processed] ^ stream_block[n]);
      n = (n + 1U) & 0x0FU;
      processed++;
    } else {
      size_t iterations = (length - processed) / 16U;

      if (iterations > 0U) {
        data_in = block_t_convert(&input[processed], iterations * 16U);
        data_out = block_t_convert(&output[processed], iterations * 16U);

        status = cryptoacc_management_acquire();
        if (status != 0) {
          return BOOTLOADER_ERROR_SECURITY_REJECTED;
        }

        // CTR uses encrypt for both directions
        sx_ret = sx_aes_ctr_encrypt_update((const block_t *)&key_block,
                                           (const block_t *)&data_in,
                                           &data_out,
                                           (const block_t *)&iv_block,
                                           &iv_block);

        cryptoacc_management_release();

        if (sx_ret != CRYPTOLIB_SUCCESS) {
          return BOOTLOADER_ERROR_SECURITY_REJECTED;
        }

        processed += iterations * 16U;
      }

      while ((length - processed) > 0U) {
        if (n == 0U) {
          if (btl_aes_cryptoacc_ecb(key, keybits, true,
                                    nonce_counter, stream_block) != BOOTLOADER_OK) {
            return BOOTLOADER_ERROR_SECURITY_REJECTED;
          }
          for (size_t i = 0; i < 16U; i++) {
            nonce_counter[15U - i] = (uint8_t)(nonce_counter[15U - i] + 1U);
            if (nonce_counter[15U - i] != 0U) {
              break;
            }
          }
        }
        output[processed] = (uint8_t)(input[processed] ^ stream_block[n]);
        n = (n + 1U) & 0x0FU;
        processed++;
      }
    }
  }

  if (nc_off != NULL) {
    *nc_off = n;
  }

  return BOOTLOADER_OK;
}
#endif // CRYPTOACC_PRESENT

void btl_initAesContext(void *ctx)
{
  btl_aes_enable_clock();
  AesContext_t *context = (AesContext_t *)ctx;
  memset(context, 0, sizeof(AesContext_t));
}

void btl_setAesKey(void          *ctx,
                   const uint8_t *key,
                   unsigned int  keySize,
                   bool          encryptNotDecrypt)
{
  (void)encryptNotDecrypt;
  AesContext_t *context = (AesContext_t *)ctx;

  context->keybits = keySize;
  memcpy(context->key, key, keySize / 8U);
}

void btl_processAesBlock(void    *ctx,
                         uint8_t *inputBlock,
                         uint8_t *outputBlock,
                         bool    encryptNotDecrypt)
{
  btl_aes_enable_clock();
  AesContext_t *context = (AesContext_t *)ctx;

#if defined(SEMAILBOX_PRESENT)
  uint32_t keyspec = btl_aes_se_keyspec(false, context->keybits);

  (void)btl_aes_se_mailbox_ecb(keyspec,
                               context->key,
                               context->keybits / 8U,
                               encryptNotDecrypt,
                               inputBlock,
                               outputBlock,
                               BTL_AES_BLOCK_SIZE_BYTES);
#elif defined(CRYPTOACC_PRESENT)
  (void)btl_aes_cryptoacc_ecb(context->key,
                              context->keybits,
                              encryptNotDecrypt,
                              inputBlock,
                              outputBlock);
#else
  (void)context;
  (void)inputBlock;
  (void)outputBlock;
  (void)encryptNotDecrypt;
#endif
}

void btl_initAesCcm(void          *ctx,
                    uint8_t       flags,
                    uint8_t       *nonce,
                    uint32_t      counter,
                    const uint8_t *key,
                    unsigned int  keySize)
{
  btl_aes_enable_clock();

  AesCtrContext_t *context = (AesCtrContext_t *)ctx;
  memset(context, 0, sizeof(AesCtrContext_t));

  context->keybits = keySize;
  context->offsetInBlock = 0;

#if defined(SEMAILBOX_PRESENT)
#if defined(BOOTLOADER_USE_SYMMETRIC_KEY_FROM_SE_STORAGE) \
  && (BOOTLOADER_USE_SYMMETRIC_KEY_FROM_SE_STORAGE == 1)
  (void)key;
  context->useInternalSeKey = true;
#else
  memcpy(context->key, key, keySize / 8U);
  context->useInternalSeKey = false;
#endif
#elif defined(CRYPTOACC_PRESENT)
  memcpy(context->key, key, keySize / 8U);
#else
  (void)key;
#endif

  // CCM uses counter mode with:
  //  * flags   (1  byte)
  //  * nonce   (12 bytes)
  //  * counter (3  bytes)
  context->counter[0] = flags;
  memcpy(&(context->counter[1]), nonce, 12);
  context->counter[13] = (uint8_t)((counter & 0x00FF0000U) >> 16);
  context->counter[14] = (uint8_t)((counter & 0x0000FF00U) >> 8);
  context->counter[15] = (uint8_t)(counter & 0x000000FFU);
}

void btl_processAesCtrData(void          *ctx,
                           const uint8_t *input,
                           uint8_t       *output,
                           size_t        length)
{
  btl_aes_enable_clock();
  AesCtrContext_t *context = (AesCtrContext_t *)ctx;

#if defined(SEMAILBOX_PRESENT)
  uint32_t keyspec = btl_aes_se_keyspec(context->useInternalSeKey, context->keybits);
  const uint8_t *key = context->useInternalSeKey ? NULL : context->key;

  (void)btl_aes_se_mailbox_ctr(keyspec,
                               key,
                               context->keybits / 8U,
                               length,
                               &context->offsetInBlock,
                               context->counter,
                               context->streamBlock,
                               input,
                               output);
#elif defined(CRYPTOACC_PRESENT)
  (void)btl_aes_cryptoacc_ctr(context->key,
                              context->keybits,
                              length,
                              &context->offsetInBlock,
                              context->counter,
                              context->streamBlock,
                              input,
                              output);
#else
  (void)context;
  (void)input;
  (void)output;
  (void)length;
#endif
}
