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
#ifndef BTL_SECURITY_TYPES_H
#define BTL_SECURITY_TYPES_H

#include "config/btl_config.h"
#include "core/btl_util.h"
#include "em_device.h"
#if defined(SEMAILBOX_PRESENT)
#include <stdbool.h>
#endif
#if defined(SEMAILBOX_PRESENT) && defined(SE_MANAGER_CONFIG_FILE)
#include SE_MANAGER_CONFIG_FILE
#endif
MISRAC_DISABLE
#include "security/sha/btl_sha256.h"
MISRAC_ENABLE

/***************************************************************************//**
 * @addtogroup Components
 * @{
 * @addtogroup Security
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup AES
 * @{
 ******************************************************************************/

/// Context variable type for AES-ECB
typedef struct AesContext {
  unsigned int keybits; ///< Key length in bits
  uint8_t      key[32]; ///< AES key (128/192/256-bit)
} AesContext_t;

/// Context variable type for AES-CTR (and AES-CCM)
typedef struct AesCtrContext {
#if defined(SEMAILBOX_PRESENT)
  bool useInternalSeKey; ///< True when using the immutable SE application AES key
#endif
  unsigned int keybits; ///< Key length in bits
  uint8_t      key[32]; ///< AES key (128/192/256-bit), when not in SE storage
  size_t       offsetInBlock; ///< Position in block of last byte en/decrypted
#if defined(SEMAILBOX_PRESENT) && defined(SE_MANAGER_CONFIG_FILE)
  uint8_t                streamBlock[16U * BOOTLOADER_AES_CTR_NUM_BLOCKS_BUFFERED]; ///< Current CTR encrypted block(s)
#else
  uint8_t                streamBlock[16]; ///< Current CTR encrypted block
#endif
  uint8_t                counter[16]; ///< Current counter/CCM value
} AesCtrContext_t;

/** @} addtogroup AES */

/***************************************************************************//**
 * @addtogroup SHA_256
 * @{
 ******************************************************************************/

/// Context type for SHA algorithm
typedef union Sha256Context {
  btl_sha256_context       shaContext;      ///< SHA-256 context struct
  uint8_t                  sha[32];         ///< resulting SHA hash
} Sha256Context_t;

/** @} addtogroup SHA_256 */

/***************************************************************************//**
 * @addtogroup Decryption
 * @{
 * @brief Generic decryption functionality for bootloader
 * @details
 ******************************************************************************/

/// Generic decryption context
typedef union {
  AesCtrContext_t aesCtr; ///< Context for AES-CTR-128 decryption
} DecryptContext_t;

/// Generic authentication context
typedef union {
  Sha256Context_t sha256; ///< Context for SHA-256 digest
} AuthContext_t;

/** @} addtogroup Decryption */

/** @} addtogroup Security */
/** @} addtogroup Compones */

#endif // BTL_SECURITY_TYPES_H
