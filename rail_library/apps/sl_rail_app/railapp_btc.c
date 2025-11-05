/***************************************************************************//**
 * @file
 * @brief Header file for RAIL error rate functionality
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include "railapp_btc.h"
#include "rail.h"
#include "railapp_tx.h" // Access pktData and txDataLen from TX module
#define txData pktData

// -----------------------------------------------------------------------------
//                                Local Defines
// -----------------------------------------------------------------------------

#define BUFC_HEADER_TX 2U
#define BUFC_HEADER_RX 3U

// -----------------------------------------------------------------------------
//                            Externs / Weak Symbols
// -----------------------------------------------------------------------------

extern RAIL_Handle_t railHandle;

// Persistent AES/E0 encryption configuration used by the BTC helpers.
// These defaults mirror the railtest reference implementation so that
// encrypted-packet tests succeed out-of-the-box.

sl_rail_btc_aes_encryption_config_t g_aes_config = {
  .aes_key_bytes_0_3  = 0x89678967UL,
  .aes_key_bytes_4_7  = 0x89678967UL,
  .aes_key_bytes_8_11 = 0x45234523UL,
  .aes_key_bytes_12_15 = 0x45234523UL,
  .aes_iv_1           = 0x66778899UL,
  .aes_iv_2           = 0xAABBCCDDUL,
  .day_count_and_dir   = 0U,
  .zero_len_acl_w      = 0U,
  .aes_pld_cntr_1      = 0x00BC614EUL,
  .aes_pld_cntr_2      = 0U
};

sl_rail_btc_e0_encryption_config_t g_e0_enc_config = {
  .master_clock = 0x040034BEUL, /* 64-bit master clock value */
  .enc_key_0     = 0x4AF08722UL,
  .enc_key_1     = 0xD03190BAUL,
  .enc_key_2     = 0x534C0D78UL,
  .enc_key_3     = 0x633A15E0UL,
  .bd_addr_0     = 0x56947F2CUL,
  .bd_addr_1     = 0x1B0FUL,
  .key_length   = 12U
};

// -----------------------------------------------------------------------------
//                           Static Helper Functions
// -----------------------------------------------------------------------------

static uint32_t btcAddHeader(uint8_t packet_type,
                             uint16_t payload_bytes,
                             sl_rail_btc_trx_config_t *p_btc_config)
{
  /* This function sets the PHR for BTC frames and fills the global txData
   * buffer so that the packet can be transmitted with RAIL_WriteTxFifo().
   * It closely mirrors the implementation in railtest/app_ci/btc_ci.c. */

  uint8_t flow          = 1U;   // RX buffer ready flag for ACL transport
  uint8_t lt_addr       = 2U;   // 3-bit LT_ADDR
  uint8_t llid          = 2U;   // 2-bit LLID in payload header
  uint8_t payload_hdr_bytes = 0U;
  uint8_t btc_packet_type = 0U;
  uint8_t is_crc_needed   = 0U;
  uint16_t hdr          = 0U;
  uint16_t payload_hdr   = 0U;
  uint32_t payload_start = 0U;

  p_btc_config->special_pkt_type = SL_RAIL_BTC_NORMAL_PKT;

  switch (packet_type) {
    case SL_RAIL_BTC_PKT_TYPE_NULL:
      btc_packet_type                = 0U;
      payload_hdr                   = 0U;
      payload_hdr_bytes                = 0U;
      payload_bytes                   = 0U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      is_crc_needed                 = 0U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_POLL:
      btc_packet_type                = 1U;
      payload_hdr                   = 0U;
      payload_hdr_bytes                = 0U;
      payload_bytes                   = 0U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      is_crc_needed                 = 0U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_FHS:
      btc_packet_type                = 2U;
      payload_hdr                   = 0U;
      payload_hdr_bytes                = 0U;
      payload_bytes                   = 18U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      is_crc_needed                 = 1U;
      break;

    /* BR data packets */
    case SL_RAIL_BTC_PKT_TYPE_DH_1:
      btc_packet_type = 4U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 1U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DH_3:
      btc_packet_type = 11U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DH_5:
      btc_packet_type = 15U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;

    /* FEC-protected packets */
    case SL_RAIL_BTC_PKT_TYPE_DM_1:
      btc_packet_type = 3U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 1U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DM_3:
      btc_packet_type = 10U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DM_5:
      btc_packet_type = 14U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;

    /* EDR data packets */
    case SL_RAIL_BTC_PKT_TYPE_2_DH_1:
      btc_packet_type = 4U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_2_DH_3:
      btc_packet_type = 10U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_2_DH_5:
      btc_packet_type = 14U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_DH_1:
      btc_packet_type = 8U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_DH_3:
      btc_packet_type = 11U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_DH_5:
      btc_packet_type = 15U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;

    /* Voice packets – SCO */
    case SL_RAIL_BTC_PKT_TYPE_HV_1: /* 10 bytes, 1/3 FEC */
      btc_packet_type = 5U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      payload_bytes    = 10U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 1U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = 10U;
      is_crc_needed = 0U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_HV_2: /* 20 bytes, 2/3 FEC */
      btc_packet_type = 6U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      payload_bytes    = 20U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 1U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = 20U;
      is_crc_needed = 0U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_HV_3: /* 30 bytes, no FEC */
      btc_packet_type = 7U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      payload_bytes    = 30U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 1U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = 30U;
      is_crc_needed = 0U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DV:
      btc_packet_type = 8U;
      payload_hdr    = llid | (flow << 2) | ((payload_bytes - 10U) << 3);
      payload_hdr_bytes = 1U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 1U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes - SL_RAIL_BTC_DV_PKT_VOICE_FIELD_BYTES /* 10 */;
      is_crc_needed = 1U;
      p_btc_config->special_pkt_type = SL_RAIL_BTC_DV_PKT;
      break;

    /* Voice packets – eSCO, BR */
    case SL_RAIL_BTC_PKT_TYPE_EV_3: /* 1–30 bytes, no FEC */
      btc_packet_type = 7U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 1U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_EV_4: /* 1–120 bytes, 2/3 FEC */
      btc_packet_type = 12U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 1U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_EV_5: /* 1–180 bytes, no FEC */
      btc_packet_type = 13U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 1U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;

    /* Voice packets – eSCO, EDR */
    case SL_RAIL_BTC_PKT_TYPE_2_EV_3: /* 1–60 bytes, no FEC */
      btc_packet_type = 6U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 1U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_2_EV_5: /* 1–360 bytes, no FEC */
      btc_packet_type = 12U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 1U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_EV_3: /* 1–90 bytes, no FEC */
      btc_packet_type = 7U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 1U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_EV_5: /* 1–540 bytes, no FEC */
      btc_packet_type = 13U;
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      p_btc_config->link_config.enhanced_rate = 1U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 1U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 1U;
      break;

    /* Test packet */
    case SL_RAIL_BTC_PKT_TYPE_AUX_1:
      btc_packet_type = 9U;
      payload_hdr    = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 1U;
      p_btc_config->link_config.enhanced_rate = 0U; /* TODO: could also be EDR */
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 1U;
      p_btc_config->link_config.payload_bytes   = payload_bytes;
      is_crc_needed = 0U;
      break;

    /* ID packet */
    case SL_RAIL_BTC_PKT_TYPE_ID:
      payload_hdr    = 0U;
      payload_hdr_bytes = 0U;
      payload_bytes    = 0U;
      p_btc_config->link_config.enhanced_rate = 0U;
      p_btc_config->link_config.sco          = 0U;
      p_btc_config->link_config.esco         = 0U;
      p_btc_config->link_config.acl          = 0U;
      p_btc_config->link_config.payload_bytes   = 0U;
      p_btc_config->special_pkt_type          = SL_RAIL_BTC_ID_PKT;
      break;

    default:
      /* Unsupported/unknown packet type – leave p_btc_config untouched so that the
       * higher-level application can detect the error condition.               */
      return 0U;
  }

  /* Compute total length (PHR + payload + optional CRC) */
  p_btc_config->tx_packet_bytes = BUFC_HEADER_TX + payload_hdr_bytes + payload_bytes;

  if (packet_type == SL_RAIL_BTC_PKT_TYPE_ID) {
    hdr = 0U; // No header
  } else {
    p_btc_config->packet_config.packet_type = btc_packet_type;
    hdr = lt_addr | (btc_packet_type << 3) | (flow << 7);
  }

  /* Build packet into txData buffer */
  uint16_t *txData16 = (uint16_t *)txData;
  txData16[0] = hdr;

  if ((p_btc_config->link_config.sco == 1U) && (((hdr >> 3) & 0xF) == 8U)) {
    /* DV packet type – insert 10 bytes of voice data */
    for (uint32_t i = 0; i < 10U; i++) {
      txData[2U + i] = (uint8_t)i;
    }
    payload_start   = 12U;
    txData16[6]    = payload_hdr;
  } else {
    payload_start   = 2U;
    txData16[1]    = payload_hdr;
  }

  /* Leave payload and CRC bytes to be populated by higher-level tooling
   * (e.g., RTA's SetTestPacket Python helper). */
  return payload_start;
}

// -----------------------------------------------------------------------------
//                         Public Generic API (RailApp)
// -----------------------------------------------------------------------------

void RAILAPP_BTC_HW_Init(uint32_t access_word_0,
                         uint32_t access_word_1,
                         uint8_t  bd_addr_5)
{
  sl_rail_btc_init_config_t init_cfg = { 0 };
  init_cfg.access_addr.sync_word[0] = access_word_0;
  init_cfg.access_addr.sync_word[1] = access_word_1;
  init_cfg.bd_addr[5]              = bd_addr_5;
  sl_rail_btc_hw_init(railHandle, &init_cfg);
}

void RAILAPP_BTC_SetWhitening(uint8_t seed)
{
  sl_rail_btc_set_whitening(railHandle, seed);
}

uint32_t RAILAPP_BTC_Prepare(bool     is_rx,
                             uint8_t  packet_type,
                             uint16_t payload_bytes,
                             uint8_t  encryption)
{
  sl_rail_btc_trx_config_t cfg = { 0 };
  uint32_t payload_start = btcAddHeader(packet_type, payload_bytes, &cfg);
  cfg.is_rx        = is_rx;
  cfg.encrypt_link = encryption;
  if (is_rx) {
    cfg.rx_timeout_us = 5000U; // us
  }
  sl_rail_btc_prepare_tx_rx(railHandle, &cfg);
  return payload_start;
}

void RAILAPP_BTC_SetE0Encryption(void)
{
  sl_rail_btc_set_e0_encryption(railHandle, &g_e0_enc_config);
}

void RAILAPP_BTC_SetE0EncryptionMasterClock(void)
{
  sl_rail_btc_set_e0_master_clock(railHandle, g_e0_enc_config.master_clock);
}

void RAILAPP_BTC_SetAESEncryption(void)
{
  sl_rail_btc_set_aes_encryption(railHandle, &g_aes_config);
}

void RAILAPP_BTC_SetAESEncryptionNonce(void)
{
  sl_rail_btc_set_aes_encryption_nonce(railHandle, &g_aes_config);
}

void RAILAPP_BTC_configureContinuousTx(sl_rail_btc_continuous_rate_t rate)
{
  sl_rail_btc_configure_continuous_tx(railHandle, rate);
}
