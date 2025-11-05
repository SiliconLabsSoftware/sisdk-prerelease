/***************************************************************************//**
 * @file
 * @brief The BTC-specific header file for the RAIL library.
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

#ifndef SL_RAIL_BTC_H
#define SL_RAIL_BTC_H

// Get the standard include types
#include <stdint.h>
#include <stdbool.h>

#include "sl_rail_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DOXYGEN_UNDOCUMENTED

#define SL_RAIL_BTC_DV_PKT_VOICE_FIELD_BYTES   (10U)

typedef struct sl_rail_btc_packet_config {
  uint8_t unused : 4;
  uint8_t packet_type : 4;
} sl_rail_btc_packet_config_t;

typedef struct sl_rail_btc_access_address {
  uint32_t sync_word[2U];
} sl_rail_btc_access_address_t;

typedef struct sl_rail_btc_init_config {
  sl_rail_btc_access_address_t access_addr;
  uint8_t bd_addr[6U];
} sl_rail_btc_init_config_t;

typedef struct sl_rail_btc_rx_pkt_type_esco_pl_lnth {
  uint16_t payload_bytes : 11; // RXESCOPLLNTHFRMREG field, needed for EV packets and ACL packets
  uint16_t enhanced_rate : 1;
  uint16_t sco : 1;
  uint16_t esco : 1;
  uint16_t acl : 1;
  uint16_t unused : 1;
} sl_rail_btc_rx_pkt_type_esco_pl_lnth_t;

SLI_RAIL_ENUM(sl_rail_btc_special_pkt_t) {
  SL_RAIL_BTC_NORMAL_PKT = 0,
  SL_RAIL_BTC_ID_PKT = 1,
  SL_RAIL_BTC_DV_PKT = 2,
};
#ifndef DOXYGEN_SHOULD_SKIP_THIS
// Self-referencing defines minimize compiler complaints when using SLI_RAIL_ENUM
#define SL_RAIL_BTC_NORMAL_PKT    ((sl_rail_btc_special_pkt_t) SL_RAIL_BTC_NORMAL_PKT)
#define SL_RAIL_BTC_ID_PKT        ((sl_rail_btc_special_pkt_t) SL_RAIL_BTC_ID_PKT)
#define SL_RAIL_BTC_DV_PKT        ((sl_rail_btc_special_pkt_t) SL_RAIL_BTC_DV_PKT)
#endif //DOXYGEN_SHOULD_SKIP_THIS

typedef struct sl_rail_btc_trx_config {
  bool is_rx;
  sl_rail_btc_special_pkt_t special_pkt_type;  // replaces is_id_pkt and is_dv_pkt
  uint8_t tx_power;         // Transmit Power variable for rail both connected and non_connected
  sl_rail_btc_packet_config_t packet_config;
  uint32_t clk;             // BD Clock value at which TXRX is expected
  uint32_t tsf;             // protimer TSF value for TXRX activity
  uint16_t tx_packet_bytes; // needed for EV packets and ACL packets
  sl_rail_btc_rx_pkt_type_esco_pl_lnth_t link_config;
  uint32_t rx_timeout_us;   // timeout used in rx only
  uint8_t encrypt_link;     // 0: no encryption, 1: E0 encryption, 2: AES encryption
} sl_rail_btc_trx_config_t;

typedef struct sl_rail_btc_e0_encryption_config {
  uint32_t master_clock;
  uint32_t enc_key_0;
  uint32_t enc_key_1;
  uint32_t enc_key_2;
  uint32_t enc_key_3;
  uint32_t bd_addr_0;
  uint16_t bd_addr_1;
  uint8_t key_length;
} sl_rail_btc_e0_encryption_config_t;

typedef struct sl_rail_btc_aes_encryption_config {
  uint32_t aes_key_bytes_0_3;
  uint32_t aes_key_bytes_4_7;
  uint32_t aes_key_bytes_8_11;
  uint32_t aes_key_bytes_12_15;
  uint32_t aes_iv_1;
  uint32_t aes_iv_2;
  uint16_t day_count_and_dir;
  uint16_t zero_len_acl_w;
  uint32_t aes_pld_cntr_1;
  uint8_t aes_pld_cntr_2;
} sl_rail_btc_aes_encryption_config_t;

SLI_RAIL_ENUM(sl_rail_btc_packet_type_t) {
  SL_RAIL_BTC_PKT_TYPE_ID     = 0,
  SL_RAIL_BTC_PKT_TYPE_NULL   = 1,
  SL_RAIL_BTC_PKT_TYPE_POLL   = 2,
  SL_RAIL_BTC_PKT_TYPE_FHS    = 3,
  SL_RAIL_BTC_PKT_TYPE_DH_1   = 4,
  SL_RAIL_BTC_PKT_TYPE_DH_3   = 5,
  SL_RAIL_BTC_PKT_TYPE_DH_5   = 6,
  SL_RAIL_BTC_PKT_TYPE_DM_1   = 7,
  SL_RAIL_BTC_PKT_TYPE_DM_3   = 8,
  SL_RAIL_BTC_PKT_TYPE_DM_5   = 9,
  SL_RAIL_BTC_PKT_TYPE_2_DH_1 = 10,
  SL_RAIL_BTC_PKT_TYPE_2_DH_3 = 11,
  SL_RAIL_BTC_PKT_TYPE_2_DH_5 = 12,
  SL_RAIL_BTC_PKT_TYPE_3_DH_1 = 13,
  SL_RAIL_BTC_PKT_TYPE_3_DH_3 = 14,
  SL_RAIL_BTC_PKT_TYPE_3_DH_5 = 15,
  SL_RAIL_BTC_PKT_TYPE_HV_1   = 16, // 10 bytes with 1/3 fec
  SL_RAIL_BTC_PKT_TYPE_HV_2   = 17, // 20 bytes with 2/3 fec
  SL_RAIL_BTC_PKT_TYPE_HV_3   = 18, // 30 bytes with no fec
  SL_RAIL_BTC_PKT_TYPE_DV     = 19, //TODO special pkt structure
  SL_RAIL_BTC_PKT_TYPE_EV_3   = 20, // 1-30 bytes with no FEC
  SL_RAIL_BTC_PKT_TYPE_EV_4   = 21, // 1-120 bytes 2/3 FEC
  SL_RAIL_BTC_PKT_TYPE_EV_5   = 22, // 1-180 bytes with no FEC
  SL_RAIL_BTC_PKT_TYPE_2_EV_3 = 23, // 1-60 bytes with no FEC
  SL_RAIL_BTC_PKT_TYPE_2_EV_5 = 24, // 1-360 bytes with no FEC
  SL_RAIL_BTC_PKT_TYPE_3_EV_3 = 25, // 1-90 bytes with no FEC
  SL_RAIL_BTC_PKT_TYPE_3_EV_5 = 26, // 1-540 bytes with no FEC
  SL_RAIL_BTC_PKT_TYPE_AUX_1  = 27,
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// Self-referencing defines minimize compiler complaints when using SLI_RAIL_ENUM
#define SL_RAIL_BTC_PKT_TYPE_ID        ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_ID)
#define SL_RAIL_BTC_PKT_TYPE_NULL      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_NULL)
#define SL_RAIL_BTC_PKT_TYPE_POLL      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_POLL)
#define SL_RAIL_BTC_PKT_TYPE_FHS       ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_FHS)
#define SL_RAIL_BTC_PKT_TYPE_DH_1      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_DH_1)
#define SL_RAIL_BTC_PKT_TYPE_DH_3      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_DH_3)
#define SL_RAIL_BTC_PKT_TYPE_DH_5      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_DH_5)
#define SL_RAIL_BTC_PKT_TYPE_DM_1      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_DM_1)
#define SL_RAIL_BTC_PKT_TYPE_DM_3      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_DM_3)
#define SL_RAIL_BTC_PKT_TYPE_DM_5      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_DM_5)
#define SL_RAIL_BTC_PKT_TYPE_2_DH_1    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_2_DH_1)
#define SL_RAIL_BTC_PKT_TYPE_2_DH_3    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_2_DH_3)
#define SL_RAIL_BTC_PKT_TYPE_2_DH_5    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_2_DH_5)
#define SL_RAIL_BTC_PKT_TYPE_3_DH_1    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_3_DH_1)
#define SL_RAIL_BTC_PKT_TYPE_3_DH_3    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_3_DH_3)
#define SL_RAIL_BTC_PKT_TYPE_3_DH_5    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_3_DH_5)
#define SL_RAIL_BTC_PKT_TYPE_HV_1      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_HV_1)
#define SL_RAIL_BTC_PKT_TYPE_HV_2      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_HV_2)
#define SL_RAIL_BTC_PKT_TYPE_HV_3      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_HV_3)
#define SL_RAIL_BTC_PKT_TYPE_DV        ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_DV)
#define SL_RAIL_BTC_PKT_TYPE_EV_3      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_EV_3)
#define SL_RAIL_BTC_PKT_TYPE_EV_4      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_EV_4)
#define SL_RAIL_BTC_PKT_TYPE_EV_5      ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_EV_5)
#define SL_RAIL_BTC_PKT_TYPE_2_EV_3    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_2_EV_3)
#define SL_RAIL_BTC_PKT_TYPE_2_EV_5    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_2_EV_5)
#define SL_RAIL_BTC_PKT_TYPE_3_EV_3    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_3_EV_3)
#define SL_RAIL_BTC_PKT_TYPE_3_EV_5    ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_3_EV_5)
#define SL_RAIL_BTC_PKT_TYPE_AUX_1     ((sl_rail_btc_packet_type_t) SL_RAIL_BTC_PKT_TYPE_AUX_1)
#endif //DOXYGEN_SHOULD_SKIP_THIS

SLI_RAIL_ENUM(sl_rail_btc_continuous_rate_t) {
  SL_RAIL_BTC_CONTINUOUS_DISABLED = 0,
  SL_RAIL_BTC_CONTINUOUS_BDR     = 1,
  SL_RAIL_BTC_CONTINUOUS_EDR2    = 2,
  SL_RAIL_BTC_CONTINUOUS_EDR3    = 3,
  SL_RAIL_BTC_CONTINUOUS_COUNT   = 4,
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// Self-referencing defines minimize compiler complaints when using SLI_RAIL_ENUM
#define SL_RAIL_BTC_CONTINUOUS_DISABLED    ((sl_rail_btc_continuous_rate_t) SL_RAIL_BTC_CONTINUOUS_DISABLED)
#define SL_RAIL_BTC_CONTINUOUS_BDR         ((sl_rail_btc_continuous_rate_t) SL_RAIL_BTC_CONTINUOUS_BDR)
#define SL_RAIL_BTC_CONTINUOUS_EDR2        ((sl_rail_btc_continuous_rate_t) SL_RAIL_BTC_CONTINUOUS_EDR2)
#define SL_RAIL_BTC_CONTINUOUS_EDR3        ((sl_rail_btc_continuous_rate_t) SL_RAIL_BTC_CONTINUOUS_EDR3)
#define SL_RAIL_BTC_CONTINUOUS_COUNT       ((sl_rail_btc_continuous_rate_t) SL_RAIL_BTC_CONTINUOUS_COUNT)
#endif //DOXYGEN_SHOULD_SKIP_THIS
/**
 * Default BTC PHY
 */
extern const sl_rail_channel_config_t *const sl_rail_btc_phy;

/**
 * Init BTC.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_init(sl_rail_handle_t rail_handle);

/**
 * Take RAIL out of BTC mode.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @return Status code indicating success of the function call.
 *
 * This function will undo some of the configuration that happens when you call
 * \ref sl_rail_btc_init(). After this you can safely run your normal radio
 * initialization code to use a non-BTC configuration. This function does \b
 * not change back your radio or channel configurations so you must do this by
 * manually reinitializing. This also resets the protocol output on PTI to \ref
 * SL_RAIL_PTI_PROTOCOL_CUSTOM.
 */
sl_rail_status_t sl_rail_btc_deinit(sl_rail_handle_t rail_handle);

/**
 * Init BTC radio hardware.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] p_btc_init_config A pointer to the BTC init configuration.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_hw_init(sl_rail_handle_t rail_handle,
                                     const sl_rail_btc_init_config_t *p_btc_init_config);

/**
 * Prepare BTC transmit and receive.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] p_btc_tx_rx_config A pointer to the BTC transmit and receive
 *   configuration.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_prepare_tx_rx(sl_rail_handle_t rail_handle,
                                           const sl_rail_btc_trx_config_t *p_btc_tx_rx_config);

/**
 * Set BTC whitening seed.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] scrambler_seed The whitening seed (0 means no whitening).
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_set_whitening(sl_rail_handle_t rail_handle,
                                           uint8_t scrambler_seed);

/**
 * Set AES CCM Encryption.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] p_aes_enc_config A non-NULL pointer to AES config structure.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_set_aes_encryption(sl_rail_handle_t rail_handle,
                                                const sl_rail_btc_aes_encryption_config_t *p_aes_enc_config);

/**
 * Set AES encryption parameters that are required per packet.
 *   This function is also called in \ref sl_rail_btc_set_aes_encryption.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] p_aes_enc_config A non-NULL pointer to AES config structure.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_set_aes_encryption_nonce(sl_rail_handle_t rail_handle,
                                                      const sl_rail_btc_aes_encryption_config_t *p_aes_enc_config);

/**
 * Set E0 Encryption.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] p_e0_enc_config A non-NULL pointer to E0 config structure.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_set_e0_encryption(sl_rail_handle_t rail_handle,
                                               const sl_rail_btc_e0_encryption_config_t *p_e0_enc_config);

/**
 * Set E0 master clock. This function is also called in
 *   \ref sl_rail_btc_set_e0_encryption.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] master_clock The value to set as master clock.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_set_e0_master_clock(sl_rail_handle_t rail_handle, uint32_t master_clock);

/**
 * Configure continuous Tx.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @param[in] rate The rate to use for the continuous Tx (0 means disabled).
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_btc_configure_continuous_tx(sl_rail_handle_t rail_handle,
                                                     sl_rail_btc_continuous_rate_t rate);

/**
 * Determine whether BTC mode is enabled or not.
 *
 * @param[in] rail_handle A real RAIL instance handle.
 * @return true if BTC mode is enabled and false otherwise.
 *
 * This function returns the current status of RAIL's BTC mode. It is enabled by
 * a call to \ref sl_rail_btc_init() and disabled by a call to \ref sl_rail_btc_deinit().
 */
bool sl_rail_btc_is_enabled(sl_rail_handle_t rail_handle);

#endif//DOXYGEN_UNDOCUMENTED

#ifdef __cplusplus
}
#endif

#endif//SL_RAIL_BTC_H
