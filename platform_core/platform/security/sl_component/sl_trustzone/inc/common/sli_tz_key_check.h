/***************************************************************************//**
 * @file
 * @brief Internal metadata types for TrustZone PSA key checks.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef SLI_TZ_KEY_CHECK_H
#define SLI_TZ_KEY_CHECK_H

#include <stdint.h>

// Describes where a crypto function carries a key ID in its input.
enum {
  SLI_TZ_KEY_ID_SOURCE_NONE = 0,
  SLI_TZ_KEY_ID_SOURCE_PACKED_KEY_ID,
  SLI_TZ_KEY_ID_SOURCE_IN_VEC_KEY_ID,
  SLI_TZ_KEY_ID_SOURCE_KEY_ATTRIBUTES
};

// One generated key-check row. A SID can have multiple rows when an operation
// consumes more than one key ID.
typedef struct {
  uint16_t function_id;
  uint8_t source;
  uint8_t invec_index;
} sli_tz_key_check_rule_t;

#endif // SLI_TZ_KEY_CHECK_H
