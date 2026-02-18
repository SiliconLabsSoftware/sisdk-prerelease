/***************************************************************************//**
 * @file sl_wisun_rf_test_tools.c
 * @brief Wi-SUN RF test helper utilities
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

#include <string.h>
#include <stdio.h>
#include "sl_wisun_rf_test_tools.h"
#include "sl_wisun_trace_api.h"

#define RF_SUN_FSK_PHR_FCS_TYPE_MASK 0x0800
#define RF_SUN_FSK_PHR_FCS_TYPE_SHIFT 11
#define RF_SUN_FSK_PHR_DATA_WHITENING_MASK 0x1000
#define RF_SUN_FSK_PHR_DATA_WHITENING_SHIFT 12
#define RF_SUN_FSK_SET_PHR_FCS_TYPE(__value__) (((__value__) << RF_SUN_FSK_PHR_FCS_TYPE_SHIFT) & RF_SUN_FSK_PHR_FCS_TYPE_MASK)
#define RF_SUN_FSK_SET_PHR_DATA_WHITENING(__value__) (((__value__) << RF_SUN_FSK_PHR_DATA_WHITENING_SHIFT) & RF_SUN_FSK_PHR_DATA_WHITENING_MASK)

#define LEGACY_FSK_PHR_LENGTH 2

extern const sl_rail_channel_config_t * channelConfigs[];

static uint8_t rf_test_reverse_bits8(uint8_t value)
{
  value = (uint8_t)((value >> 4) | (value << 4));
  value = (uint8_t)(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
  value = (uint8_t)(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
  return value;
}

static uint16_t rf_test_fsk_phr_data_length(uint16_t length)
{
  uint16_t lo = rf_test_reverse_bits8((uint8_t)(length & 0xFF));
  uint16_t hi = rf_test_reverse_bits8((uint8_t)((length >> 8) & 0xFF));
  return (uint16_t)(lo | (hi << 8));
}

uint8_t rf_test_op_mode_to_phy_mode(uint8_t op_mode, uint8_t fec)
{
  uint8_t phy_mode = 0;

  switch (op_mode) {
    case 0x1a: phy_mode = 1; break;
    case 0x1b: phy_mode = 2; break;
    case 0x2a: phy_mode = 3; break;
    case 0x2b: phy_mode = 4; break;
    case 0x3:  phy_mode = 5; break;
    case 0x4a: phy_mode = 6; break;
    case 0x4b: phy_mode = 7; break;
    case 0x5:  phy_mode = 8; break;
    default: return 0;
  }

  uint8_t phy_type = fec ? 1 : 0;
  return (phy_type << 4) | phy_mode;
}

bool rf_test_entry_matches_phy(const sl_rail_channel_config_entry_t *entry,
                               uint8_t phy_mode_id,
                               uint8_t reg_domain)
{
  if (!entry->p_stack_info) {
    return false;
  }
  return (entry->p_stack_info[STACK_INFO_FIELD_PHY_MODE_ID] == phy_mode_id) &&
         (entry->p_stack_info[STACK_INFO_FIELD_REG_DOMAIN] == reg_domain);
}

uint16_t rf_test_get_buffer_len(uint16_t length)
{
  for (uint8_t i = 6; i < 14; i++) {
    if ((uint16_t)(1 << i) > length) {
      return (uint16_t)(1 << i);
    }
  }

  return 0;
}

uint8_t rf_test_build_fsk_phr(uint8_t *dst, uint16_t frame_length, uint8_t crc_length)
{
  uint16_t phr = rf_test_fsk_phr_data_length(frame_length);

  phr |= RF_SUN_FSK_SET_PHR_DATA_WHITENING(1);
  phr |= RF_SUN_FSK_SET_PHR_FCS_TYPE(crc_length == 2 ? 1 : 0);

  dst[0] = (uint8_t)(phr >> 8);
  dst[1] = (uint8_t)(phr & 0xFF);
  return LEGACY_FSK_PHR_LENGTH;
}

bool rf_test_prepare_tx_buffer(uint8_t phy_mode_id,
                               uint16_t data_length,
                               const uint8_t *data,
                               bool use_phr,
                               uint8_t crc_length,
                               uint8_t *tx_fifo,
                               uint16_t fifo_capacity,
                               uint16_t *fifo_size_bytes,
                               uint8_t *phr_length,
                               uint16_t *init_bytes)
{
  if (data_length == 0) {
    return false;
  }

  uint16_t frame_length = data_length + crc_length;
  uint8_t local_phr_length = 0;

  if (use_phr) {
    if (IS_OFDM(phy_mode_id) || IS_OQPSK(phy_mode_id)) {
      sl_wisun_trace_error("rf_test: PHR for OFDM/OQPSK not supported\r\n");
      return false;
    }
    local_phr_length = rf_test_build_fsk_phr(tx_fifo, frame_length, crc_length);
  }

  uint16_t total_length = (uint16_t)(data_length + local_phr_length);
  uint16_t fifo_size = rf_test_get_buffer_len(total_length);
  if (fifo_size == 0 || fifo_size > fifo_capacity) {
    return false;
  }

  uint8_t *dst = tx_fifo + local_phr_length;
  if (data != NULL) {
    memcpy(dst, data, data_length);
  } else {
    for (uint16_t i = 0; i < data_length; i++) {
      dst[i] = (uint8_t)i;
    }
  }

  *fifo_size_bytes = fifo_size;
  *phr_length = local_phr_length;
  *init_bytes = total_length;
  return true;
}


sl_status_t rf_test_phy_config_to_chan_config(sl_wisun_phy_config_t *phy_config,
                                              sl_rail_channel_config_entry_t *chan_config,
                                              uint8_t *phy_mode_id,
                                              uint8_t *reg_domain)
{
  sl_status_t status = SL_STATUS_OK;
  sl_rail_handle_t rail_handle;
  const sl_rail_channel_config_t *iter;
  int index = 0;
  bool found = false;

  switch (phy_config->type) {
  case SL_WISUN_PHY_CONFIG_FAN10:
    *phy_mode_id = rf_test_op_mode_to_phy_mode(phy_config->config.fan10.op_mode,
                                       phy_config->config.fan10.fec);
    if (*phy_mode_id == 0) {
      sl_wisun_trace_error("rf_test: invalid FAN10 configuration");
      return SL_STATUS_INVALID_PARAMETER;
    }
    *reg_domain = phy_config->config.fan10.reg_domain;
    break;

  case SL_WISUN_PHY_CONFIG_FAN11:
    *phy_mode_id = phy_config->config.fan11.phy_mode_id;
    *reg_domain = phy_config->config.fan11.reg_domain;
    break;

  case SL_WISUN_PHY_CONFIG_EXPLICIT:
    *phy_mode_id = phy_config->config.explicit_plan.phy_mode_id;
    *reg_domain = STACK_INFO_REG_DOMAIN_UNUSED;
    break;

  case SL_WISUN_PHY_CONFIG_IDS:
    status = sli_wisun_get_rail_handle(&rail_handle);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if (phy_config->config.ids.protocol_id == 0 && channelConfigs[0] != NULL) {
      iter = channelConfigs[0];
    } else {
      int proto_index = 0;
      while (channelConfigs[proto_index] != NULL && proto_index < phy_config->config.ids.protocol_id) {
        proto_index++;
      }
      if (channelConfigs[proto_index] == NULL) {
        sl_wisun_trace_error("rf_test: IDS protocol_id not found\r\n");
        return SL_STATUS_INVALID_PARAMETER;
      }
      iter = channelConfigs[proto_index];
    }

    sl_rail_config_channels(rail_handle, iter, NULL);

    if (phy_config->config.ids.channel_id < iter->number_of_entries) {
      const sl_rail_channel_config_entry_t *entry = &iter->p_entries[phy_config->config.ids.channel_id];
      if (entry->p_stack_info) {
        *reg_domain = entry->p_stack_info[STACK_INFO_FIELD_REG_DOMAIN];
        *phy_mode_id = phy_config->config.ids.phy_mode_id;
        if (entry->p_stack_info[STACK_INFO_FIELD_PHY_MODE_ID] == *phy_mode_id) {
          memcpy(chan_config, entry, sizeof(sl_rail_channel_config_entry_t));
          return SL_STATUS_OK;
        }
      }
    }
    sl_wisun_trace_error("rf_test: IDS config did not match entry");
    return SL_STATUS_INVALID_PARAMETER;

  default:
    sl_wisun_trace_error("rf_test: unknown phy_config type %u",
           (unsigned int)phy_config->type);
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sli_wisun_get_rail_handle(&rail_handle);
  if (status != SL_STATUS_OK) {
    return status;
  }

  iter = channelConfigs[index];
  while (iter && !found) {
    sl_rail_config_channels(rail_handle, iter, NULL);

    for (uint32_t entry_num = 0; entry_num < iter->number_of_entries; entry_num++) {
      const sl_rail_channel_config_entry_t *entry = &iter->p_entries[entry_num];

      if (!entry->p_stack_info) {
        continue;
      }

      if (rf_test_entry_matches_phy(entry, *phy_mode_id, *reg_domain)) {
        memcpy(chan_config, entry, sizeof(sl_rail_channel_config_entry_t));
        found = true;
        break;
      }
    }

    if (!found) {
      iter = channelConfigs[++index];
    }
  }

  if (!found) {
    sl_wisun_trace_error("rf_test: no matching entry found");
    status = SL_STATUS_NOT_FOUND;
  }
  return status;
}
