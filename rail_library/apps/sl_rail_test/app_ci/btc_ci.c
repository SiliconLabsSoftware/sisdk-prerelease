/***************************************************************************//**
 * @file
 * @brief This file implements commands for configuring BTC RAIL options
 *   relevant to receiving packets
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#include <stdio.h>
#include <string.h>

#include "response_print.h"

#include "sl_rail.h"
#include "app_common.h"
#include "sl_rail_btc.h"

#define BUFC_HEADER_TX 2
#define BUFC_HEADER_RX 3
#define BTC_DV_PKT_VOICE_FIELD_BYTES   (10U)

// Default BTC AES encryption config
// from BTC Core spec Sample Data, Vol.2 Part G 1.2
static sl_rail_btc_aes_encryption_config_t aes_enc_config = {
  .aes_key_bytes_0_3 = 0x89678967,
  .aes_key_bytes_4_7 = 0x89678967,
  .aes_key_bytes_8_11 = 0x45234523,
  .aes_key_bytes_12_15 = 0x45234523,
  .aes_iv_1 = 0x66778899,
  .aes_iv_2 = 0xaabbccdd,
  .day_count_and_dir = 0,
  .zero_len_acl_w = 0,
  .aes_pld_cntr_1 = 0x00bc614e,
  .aes_pld_cntr_2 = 0
};

void addBTCHeader(uint8_t packet_type, uint16_t payload_bytes, sl_rail_btc_trx_config_t *p_btc_config)
{
  /*
     This function sets the PHR for BTC frames. It must be used with the Tx buffer size
     (set by setTxLength command). It does NOT take into account the PHY loaded.
   */
  // uint8_t packet_type = sl_cli_get_argument_uint8(args, 0);
  // uint8_t payload_bytes = sl_cli_get_argument_uint8(args, 1);

  uint8_t flow = 1;     // RX buffer ready flag for ACL transport
  uint8_t lt_addr = 2;  // 3 bit-LT_ADDR
  uint8_t llid = 2;     // 2 bit-LLID in payload Hdr
  uint8_t payload_hdr_bytes = 0;
  uint8_t btc_packet_type = 0;
  uint8_t is_crc_needed = 0;
  uint16_t hdr = 0;
  uint16_t payload_hdr = 0;

  extern uint16_t txDataLen;

  p_btc_config->special_pkt_type = SL_RAIL_BTC_NORMAL_PKT;

  switch (packet_type) {
    case SL_RAIL_BTC_PKT_TYPE_NULL:
      btc_packet_type = 0;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      payload_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      is_crc_needed = 0;
      break;
    case SL_RAIL_BTC_PKT_TYPE_POLL:
      btc_packet_type = 1;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      payload_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      is_crc_needed = 0;
      break;
    case SL_RAIL_BTC_PKT_TYPE_FHS:
      btc_packet_type = 2;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      payload_bytes = 18;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      is_crc_needed = 1;
      break;

    /*BR data pkts*/
    case SL_RAIL_BTC_PKT_TYPE_DH_1:
      btc_packet_type = 4;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 1;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DH_3:
      btc_packet_type = 11;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DH_5:
      btc_packet_type = 15;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;

    /*FEC protected pkts*/
    case SL_RAIL_BTC_PKT_TYPE_DM_1:
      btc_packet_type = 3;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 1;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DM_3:
      btc_packet_type = 10;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DM_5:
      btc_packet_type = 14;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;

    /*EDR data pkts*/
    case SL_RAIL_BTC_PKT_TYPE_2_DH_1:
      btc_packet_type = 4;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_2_DH_3:
      btc_packet_type = 10;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_2_DH_5:
      btc_packet_type = 14;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_DH_1:
      btc_packet_type = 8;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_DH_3:
      btc_packet_type = 11;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_DH_5:
      btc_packet_type = 15;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 2;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;

    /*Voice pkts SCO*/
    case SL_RAIL_BTC_PKT_TYPE_HV_1: // 10 bytes with 1/3 fec
      btc_packet_type = 5;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      payload_bytes = 10;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 1;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = 10;
      is_crc_needed = 0;
      break;
    case SL_RAIL_BTC_PKT_TYPE_HV_2: // 20 bytes with 2/3 fec
      btc_packet_type = 6;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      payload_bytes = 20;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 1;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = 20;
      is_crc_needed = 0;
      break;
    case SL_RAIL_BTC_PKT_TYPE_HV_3: // 30 bytes with no fec
      btc_packet_type = 7;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      payload_bytes = 30;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 1;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = 30;
      is_crc_needed = 0;
      break;
    case SL_RAIL_BTC_PKT_TYPE_DV:
      btc_packet_type = 8;
      payload_hdr = llid | (flow << 2) | ((payload_bytes - 10) << 3);
      payload_hdr_bytes = 1;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 1;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes - BTC_DV_PKT_VOICE_FIELD_BYTES; /* payload_bytes includes 10B voice field */
      is_crc_needed = 1;
      p_btc_config->special_pkt_type = SL_RAIL_BTC_DV_PKT;
      break;

    /*Voice pkts enhanced SCO BR*/
    case SL_RAIL_BTC_PKT_TYPE_EV_3: // 1-30 bytes with no FEC
      btc_packet_type = 7;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 1;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_EV_4: // 1-120 bytes 2/3 FEC
      btc_packet_type = 12;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 1;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_EV_5: // 1-180 bytes with no FEC
      btc_packet_type = 13;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 1;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;

    /*Voice pkts enhanced SCO EDR*/
    case SL_RAIL_BTC_PKT_TYPE_2_EV_3: // 1-60 bytes with no FEC
      btc_packet_type = 6;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 1;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_2_EV_5: // 1-360 bytes with no FEC
      btc_packet_type = 12;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 1;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_EV_3: // 1-90 bytes with no FEC
      btc_packet_type = 7;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 1;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;
    case SL_RAIL_BTC_PKT_TYPE_3_EV_5: // 1-540 bytes with no FEC
      btc_packet_type = 13;
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 1;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 1;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 1;
      break;

    /*test pkt*/
    case SL_RAIL_BTC_PKT_TYPE_AUX_1:
      btc_packet_type = 9;
      payload_hdr = llid | (flow << 2) | (payload_bytes << 3);
      payload_hdr_bytes = 1;
      p_btc_config->link_config.enhanced_rate = 0; // FIXME: can also be EDR
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 1;
      p_btc_config->link_config.payload_bytes = payload_bytes;
      is_crc_needed = 0;
      break;

    /*ID pkt*/
    case SL_RAIL_BTC_PKT_TYPE_ID:
      payload_hdr = 0;
      payload_hdr_bytes = 0;
      payload_bytes = 0;
      p_btc_config->link_config.enhanced_rate = 0;
      p_btc_config->link_config.sco = 0;
      p_btc_config->link_config.esco = 0;
      p_btc_config->link_config.acl = 0;
      p_btc_config->link_config.payload_bytes = 0;
      p_btc_config->special_pkt_type = SL_RAIL_BTC_ID_PKT;
      break;
  }

  p_btc_config->tx_packet_bytes = BUFC_HEADER_TX + payload_hdr_bytes + payload_bytes;
  if (packet_type == SL_RAIL_BTC_PKT_TYPE_ID) {
    hdr = 0; // no Hdr
  } else {
    p_btc_config->packet_config.packet_type = btc_packet_type;
    hdr = lt_addr | (btc_packet_type << 3) | (flow << 7);
  }
  uint32_t i, payload_start;
  uint16_t *tx_data_16 = (uint16_t *)txData;
  // Write the hdr in the payload
  tx_data_16[0] = hdr;

  if ((p_btc_config->link_config.sco == 1) && (((hdr >> 3) & 0xF) == 8)) {
    // DV packet type
    for (i = 0; i < 10; i++) { // insert 10 bytes of voice data
      txData[2 + i] = (i & 0xFF);
    }
    payload_start = 12;
    // insert data portion payload hdr
    tx_data_16[6] = payload_hdr;
  } else {
    // other packet types
    payload_start = 2;
    // insert payload hdr
    tx_data_16[1] = payload_hdr;
  }
  // responsePrint("txData0: 0x%08x", txData[0]);
  // responsePrint("txData1: 0x%08x", txData[1]);

  for (i = 0; i < (uint32_t) payload_bytes + 2 * is_crc_needed; i++) { //+2 to fill CRC
    txData[payload_start + payload_hdr_bytes + i] = (i & 0xFF);
    // responsePrint("txData: 0x%08x", txData[payload_start + payload_hdr_bytes + i]);
  }
  (void)sl_rail_write_tx_fifo(railHandle, txData, txDataLen, true);
}

void BTC_Init(sl_cli_command_arg_t *args)
{
  uint32_t access_code_1 = sl_cli_get_argument_uint32(args, 0);
  uint32_t access_code_2 = sl_cli_get_argument_uint32(args, 1);
  uint8_t bd_addr_5 = sl_cli_get_argument_uint8(args, 2);
  sl_rail_btc_init_config_t btc_init_config = {
    .access_addr.sync_word[0] = access_code_1,
    .access_addr.sync_word[1] = access_code_2,
    .bd_addr[5] = bd_addr_5,
  };
  (void) sl_rail_btc_hw_init(railHandle, &btc_init_config);
  responsePrint(sl_cli_get_command_string(args, 0), "Access code:0x%x 0x%x,bd_addr_5:0x%x", access_code_1, access_code_2, bd_addr_5);
}

void BTC_SetWhitening(sl_cli_command_arg_t *args)
{
  uint8_t scrambler_seed = sl_cli_get_argument_uint8(args, 0);
  (void) sl_rail_btc_set_whitening(railHandle, scrambler_seed);
  responsePrint(sl_cli_get_command_string(args, 0), "Scrambler_seed:0x%x", scrambler_seed);
}

void BTC_Prepare(sl_cli_command_arg_t *args)
{
  CHECK_RAIL_HANDLE(sl_cli_get_command_string(args, 0));
  uint8_t tx_or_rx = sl_cli_get_argument_uint8(args, 0);
  uint8_t packet_type = sl_cli_get_argument_uint8(args, 1);
  uint16_t payload_bytes = sl_cli_get_argument_uint16(args, 2);
  uint8_t encryption = sl_cli_get_argument_uint8(args, 3);
  sl_rail_btc_trx_config_t btc_tx_rx_config = { 0 };

  addBTCHeader(packet_type, payload_bytes, &btc_tx_rx_config);
  btc_tx_rx_config.is_rx = (bool) tx_or_rx;
  if (btc_tx_rx_config.is_rx) {
    btc_tx_rx_config.rx_timeout_us = 5000; // us rx timeout value (to start of payload)
  }
  btc_tx_rx_config.encrypt_link = encryption;

  (void) sl_rail_btc_prepare_tx_rx(railHandle, &btc_tx_rx_config);
  responsePrint(sl_cli_get_command_string(args, 0), "txOrRx:%s,packet_type:%d,payload_bytes:%d,encryption:%d",
                (tx_or_rx ? "RX" : "TX"), packet_type, payload_bytes, encryption);
}

typedef enum BtcSeqSelect {
  PGSN,  // 0x0 page scan
  IPGSN, // 0x1 generalized interlaced page scan
  IQSN,  // 0x2 inquiry scan
  IIQSN, // 0x3 generalized interlaced inquiry scan
  PAGE,  // 0x4 page
  PGRSM, // 0x5 central page response
  PGRSS, // 0x6 peripheral page response
  IQRY,  // 0x7 inquiry
  IQRS,  // 0x8 inquiry response
  BASIC, // 0x9 connection (basic)
  ADAPT  // 0xA connection (adaptive)
} BtcSeqSelect_t;

#define setBtcFhKosAndN(seqselect, nval, kos)                                        \
  ((((seqselect) << _BTCFH_KOSANDN_SEQSELECT_SHIFT) & _BTCFH_KOSANDN_SEQSELECT_MASK) \
   | (((nval) << _BTCFH_KOSANDN_NVAL_SHIFT) & _BTCFH_KOSANDN_NVAL_MASK)              \
   | (((kos) << _BTCFH_KOSANDN_KOS_SHIFT) & _BTCFH_KOSANDN_KOS_MASK))

#define setNinBtcFhKosAndN(nval) \
  (((nval) << _BTCFH_KOSANDN_NVAL_SHIFT) & _BTCFH_KOSANDN_NVAL_MASK)

#define setBtcFhKnudgeAndIlos(ilos, knudge)                                       \
  ((((ilos) << _BTCFH_KNUDGEANDILOS_ILOS_SHIFT) & _BTCFH_KNUDGEANDILOS_ILOS_MASK) \
   | (((knudge) << _BTCFH_KNUDGEANDILOS_KNUDGE_SHIFT) & _BTCFH_KNUDGEANDILOS_KNUDGE_MASK))

void BTC_FHCoproTest(sl_cli_command_arg_t *args)
{
  uint32_t i, increment, j = 0;
  uint8_t kNudge, IlOs;
  uint8_t seqSelect = 0, NVal = 0, kOffset = 0;
  uint8_t setIndex = sl_cli_get_argument_uint8(args, 0);
  uint8_t substateIndex = sl_cli_get_argument_uint8(args, 1);
  uint8_t hopN = sl_cli_get_argument_uint8(args, 2);
  uint8_t channelNumTab[16] = { 0 };
  sl_rail_btc_fh_config_t btcFhData = { 0 };
  sl_rail_btc_fh_result_t btcFhResult = { 0 };

  if ((setIndex == 0) || (setIndex > 3)) {
    responsePrint(sl_cli_get_command_string(args, 0), "Invalid setIndex:%d", setIndex);
    return;
  }

  kNudge = 0;
  IlOs = 0x10;
  btcFhData.frozen_clk = 0;
  if (setIndex == 1) {
    btcFhData.ulap = 0;
  } else if (setIndex == 2) {
    btcFhData.ulap = 0x2a96ef25;
  } else if (setIndex == 3) {
    btcFhData.ulap = 0x6587cba9;
  }

  increment = 4;
  if (substateIndex == 0) {
    kOffset = 0;
    kNudge = 16;
    seqSelect = PGSN;  // page scan
    NVal = 0;
    btcFhData.chl_map0_init = 0;
    btcFhData.chl_map1_init = 0;
    btcFhData.chl_map2_init = 0;
    btcFhData.master_clk = 0;
    increment = 4096;
  } else if (substateIndex == 1) {
    kOffset = 1;
    seqSelect = PAGE;
    NVal = 0;
    btcFhData.chl_map0_init = 0;
    btcFhData.chl_map1_init = 0;
    btcFhData.chl_map2_init = 0;
    btcFhData.master_clk = 0;
    increment = 1;
  } else if (substateIndex == 2) {
    kOffset = 0;
    seqSelect = PGRSS;  //peripheral page response
    NVal = 0;
    btcFhData.chl_map0_init = 0;
    btcFhData.chl_map1_init = 0;
    btcFhData.chl_map2_init = 0;
    btcFhData.master_clk = 0x12;
    increment = 2;
    btcFhData.frozen_clk = 0x10;
  } else if (substateIndex == 3) {
    kOffset = 1;
    seqSelect = PGRSM;  //central page response
    NVal = 1;
    btcFhData.chl_map0_init = 0;
    btcFhData.chl_map1_init = 0;
    btcFhData.chl_map2_init = 0;
    btcFhData.master_clk = 0x14;
    increment = 2;
    btcFhData.frozen_clk = 0x12;
  } else if (substateIndex == 4) {
    kOffset = 0;
    seqSelect = BASIC;
    NVal = 79;
    btcFhData.chl_map0_init = 0xffffffff;
    btcFhData.chl_map1_init = 0xffffffff;
    btcFhData.chl_map2_init = 0x7fff;
    btcFhData.master_clk = 0x10;
    increment = 2;
  } else if (substateIndex == 5) {
    kOffset = 0;
    seqSelect = ADAPT;
    NVal = 79;
    btcFhData.chl_map0_init = 0xffffffff;
    btcFhData.chl_map1_init = 0xffffffff;
    btcFhData.chl_map2_init = 0x7fff;
    btcFhData.master_clk = 0x10;
  } else if (substateIndex == 6) {
    kOffset = 0;
    seqSelect = ADAPT;
    NVal = 57;
    btcFhData.chl_map0_init = 0xffc00000;
    btcFhData.chl_map1_init = 0xffffffff;
    btcFhData.chl_map2_init = 0x7fff;
    btcFhData.master_clk = 0x10;
  } else if (substateIndex == 7) {
    kOffset = 0;
    seqSelect = ADAPT;
    NVal = 40;
    btcFhData.chl_map0_init = 0x55555555;
    btcFhData.chl_map1_init = 0x55555555;
    btcFhData.chl_map2_init = 0x5555;
    btcFhData.master_clk = 0x10;
  } else if (substateIndex == 8) {
    kOffset = 0;
    seqSelect = ADAPT;
    NVal = 39;
    btcFhData.chl_map0_init = 0xaaaaaaaa;
    btcFhData.chl_map1_init = 0xaaaaaaaa;
    btcFhData.chl_map2_init = 0x2aaa;
    btcFhData.master_clk = 0x10;
  }

  btcFhData.knudge_and_ilos = setBtcFhKnudgeAndIlos(IlOs, kNudge);
  btcFhData.kos_and_N = setBtcFhKosAndN(seqSelect, NVal, kOffset);

  for (i = 0; i < hopN; i++) {
    sl_rail_btc_fhcopro(railHandle, &btcFhData, &btcFhResult);
    channelNumTab[i % 16] = btcFhResult.channel_num;
    if ((i % 16) == 15) {
      responsePrint(sl_cli_get_command_string(args, 0), "Hop channels:%02d %02d %02d %02d %02d %02d %02d %02d %02d %02d %02d %02d %02d %02d %02d %02d",
                    channelNumTab[j * 16 + 0], channelNumTab[j * 16 + 1], channelNumTab[j * 16 + 2], channelNumTab[j * 16 + 3],
                    channelNumTab[j * 16 + 4], channelNumTab[j * 16 + 5], channelNumTab[j * 16 + 6], channelNumTab[j * 16 + 7],
                    channelNumTab[j * 16 + 8], channelNumTab[j * 16 + 9], channelNumTab[j * 16 + 10], channelNumTab[j * 16 + 11],
                    channelNumTab[j * 16 + 12], channelNumTab[j * 16 + 13], channelNumTab[j * 16 + 14], channelNumTab[j * 16 + 15]);
      j++;
    }

    btcFhData.master_clk += increment;
    if ((seqSelect == PGRSS) || (seqSelect == PGRSM)) {
      /* For peripheral page response and central page response hopping sequences,
         the value of the counter N shall be increased by one each time CLK1 is set to 0*/
      if ((btcFhData.master_clk & 2) == 0) {
        NVal++;
        btcFhData.kos_and_N = setBtcFhKosAndN(seqSelect, NVal, kOffset);
      }
    }
  }
}

void BTC_setE0Encryption(sl_cli_command_arg_t *args)
{
  // Numbers coming from design.
  sl_rail_btc_e0_encryption_config_t e0_enc_config = {
    .master_clock = 0x40034be,
    .enc_key_0 = 0x4af08722,
    .enc_key_1 = 0xd03190ba,
    .enc_key_2 = 0x534c0d78,
    .enc_key_3 = 0x633a15e0,
    .bd_addr_0 = 0x56947f2c,
    .bd_addr_1 = 0x1b0f,
    .key_length = 12
  };

  (void) sl_rail_btc_set_e0_encryption(railHandle, &e0_enc_config);
  responsePrint(sl_cli_get_command_string(args, 0), "master_clock:%x,enc_key:%x %x %x %x,bd_addr:%x %x,key_length:%d",
                e0_enc_config.master_clock, e0_enc_config.enc_key_0, e0_enc_config.enc_key_1, e0_enc_config.enc_key_2, e0_enc_config.enc_key_3,
                e0_enc_config.bd_addr_0, e0_enc_config.bd_addr_1, e0_enc_config.key_length);
}

void BTC_setE0MasterClock(sl_cli_command_arg_t *args)
{
  uint32_t masterClock = 0x40034be;
  sl_rail_btc_set_e0_master_clock(railHandle, masterClock);
  responsePrint(sl_cli_get_command_string(args, 0), "master_clock:%x", masterClock);
}

void BTC_setAESEncryption(sl_cli_command_arg_t *args)
{
  (void) sl_rail_btc_set_aes_encryption(railHandle, &aes_enc_config);
  responsePrint(sl_cli_get_command_string(args, 0),
                "aes_key_bytes:%x %x %x %x,aes_iv:%x %x,day_count_and_dir:%x,zero_len_acl_w:%d,aes_pld_cntr:%x %x",
                aes_enc_config.aes_key_bytes_0_3, aes_enc_config.aes_key_bytes_4_7, aes_enc_config.aes_key_bytes_8_11,
                aes_enc_config.aes_key_bytes_12_15, aes_enc_config.aes_iv_1, aes_enc_config.aes_iv_2,
                aes_enc_config.day_count_and_dir, aes_enc_config.zero_len_acl_w, aes_enc_config.aes_pld_cntr_1,
                aes_enc_config.aes_pld_cntr_2);
}

void BTC_setAESEncryptionNonce(sl_cli_command_arg_t *args)
{
  (void) sl_rail_btc_set_aes_encryption_nonce(railHandle, &aes_enc_config);
  responsePrint(sl_cli_get_command_string(args, 0), "aes_iv: % x % x, day_count_and_dir: % x, aes_pld_cntr: % x % x ",
                aes_enc_config.aes_iv_1, aes_enc_config.aes_iv_2,
                aes_enc_config.day_count_and_dir,
                aes_enc_config.aes_pld_cntr_1, aes_enc_config.aes_pld_cntr_2);
}

void BTC_configureContinuousTx(sl_cli_command_arg_t *args)
{
  sl_rail_btc_continuous_rate_t rate = (sl_rail_btc_continuous_rate_t)sl_cli_get_argument_uint8(args, 0);
  sl_rail_status_t status = sl_rail_btc_configure_continuous_tx(railHandle, rate);

  if (status == SL_RAIL_STATUS_NO_ERROR) {
    responsePrint(sl_cli_get_command_string(args, 0), "BTC continuous Tx configured with rate: %d", rate);
  } else {
    responsePrintError(sl_cli_get_command_string(args, 0), status, "Failed to configure BTC continuous Tx");
  }
}

void btcStatus(sl_cli_command_arg_t *args)
{
  CHECK_RAIL_HANDLE(sl_cli_get_command_string(args, 0));
  bool enabled = sl_rail_btc_is_enabled(railHandle);

  // Report the current enabled status for BTC
  responsePrint(sl_cli_get_command_string(args, 0), "BTC:%s", enabled ? "Enabled" : "Disabled");
}

void btcEnable(sl_cli_command_arg_t *args)
{
  CHECK_RAIL_HANDLE(sl_cli_get_command_string(args, 0));
  bool enable = !!sl_cli_get_argument_uint8(args, 0);

  // Turn BTC mode on or off as requested
  if (enable) {
    if (disableIncompatibleProtocols(SL_RAIL_PTI_PROTOCOL_BTC) != SL_RAIL_STATUS_NO_ERROR) {
      responsePrintError(sl_cli_get_command_string(args, 0), 0x22, "Current protocol deinit failed");
      return;
    }
    sl_rail_btc_init(railHandle);
  } else {
    sl_rail_btc_deinit(railHandle);
  }

  // Report the current status of BLE mode
  args->argc = sl_cli_get_command_count(args); /* only reference cmd str */
  btcStatus(args);
}

void getBtcStats(sl_cli_command_arg_t *args)
{
  uint32_t *stats = (uint32_t *)sl_cli_get_argument_uint32(args, 0);

  if (stats == NULL) {
    responsePrintError(sl_cli_get_command_string(args, 0), 0x08,
                       "Invalid stats pointer. Must provide valid address.");
    return;
  }

  responsePrintStart(sl_cli_get_command_string(args, 0));
  responsePrintContinue("Schedules:%u,"
                        "ScheduleAbort:%u,"
                        "TransmitAborts:%u,"
                        "TransmitsDone:%u,"
                        "ReceivesDone:%u,"
                        "ReceivesTimeout:%u,"
                        "ReceivesAbort:%u,"
                        "ScheduleIdTx:%u,"
                        "ScheduleIdRx:%u,"
                        "ScheduleTx:%u",
                        stats[0],   // schedules
                        stats[1],   // schedule_abort
                        stats[2],   // transmit_aborts
                        stats[3],   // transmits_done
                        stats[4],   // receives_done
                        stats[5],   // receives_timeout
                        stats[6],   // receives_abort
                        stats[7],   // schedule_id_tx
                        stats[8],   // schedule_id_rx
                        stats[9]    // schedule_tx
                        );
  responsePrintContinue("ScheduleRx:%u,"
                        "CrcPass:%u,"
                        "CrcFail:%u,"
                        "HecError:%u,"
                        "RxPacketReceived:%u,"
                        "RxTimeout:%u,"
                        "IdPktRx:%u,"
                        "TxSent:%u,"
                        "TxAborted:%u,"
                        "RxPacketDone:%u",
                        stats[10],  // schedule_rx
                        stats[11],  // crc_pass
                        stats[12],  // crc_fail
                        stats[13],  // hec_error
                        stats[14],  // rx_packet_received
                        stats[15],  // rx_timeout
                        stats[16],  // id_pkt_rx
                        stats[17],  // tx_sent
                        stats[18],  // tx_aborted
                        stats[19]   // rx_packet_done
                        );
  responsePrintContinue("RxPacketAborted:%u,"
                        "TxPrepareCb:%u,"
                        "TxPrepareCancel:%u",
                        stats[20],  // rx_packet_aborted
                        stats[21],  // tx_prepare_cb
                        stats[22]   // tx_prepare_cancel
                        );
  responsePrintEnd("TimerCb:%u",
                   stats[23]);  // timer_cb
}
