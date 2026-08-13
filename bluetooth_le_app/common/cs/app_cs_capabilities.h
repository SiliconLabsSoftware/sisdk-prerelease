/***************************************************************************//**
 * @file
 * @brief CS supported capabilities types
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef APP_CS_CAPABILITIES_H
#define APP_CS_CAPABILITIES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Aligned local CS capabilities (sl_bt_cs_read_local_supported_capabilities outputs).
typedef struct {
  uint16_t max_consecutive_procedures; // Max consecutive procedures (1-65535); 0 = indefinite
  uint16_t nadm_sounding_capability;   // NADM sounding: bit 0 Phase-based NADM
  uint16_t nadm_random_capability;     // NADM random: bit 0 Phase-based NADM
  uint16_t subfeatures;                // Subfeatures: bit 1 no FAE, bit 2 algo #3c, bit 3 phase RTT
  uint16_t t_ip1_times;                // IP1 durations bitmask (10-80 us)
  uint16_t t_ip2_times;                // IP2 durations bitmask (10-80 us)
  uint16_t t_fcs_times;                // FCS durations bitmask (10-120 us)
  uint16_t t_pm_times;                 // PM durations: bit 0 10 us, bit 1 20 us
  uint8_t  num_config;                 // Number of CS configurations per connection (1 to 4)
  uint8_t  num_antennas;               // Antenna elements for CS tone exchanges (1 to 4)
  uint8_t  max_antenna_paths;          // Maximum antenna paths (1 to 4)
  uint8_t  roles;                      // CS roles: bit 0 Initiator, bit 1 Reflector
  uint8_t  modes;                      // Optional modes: bit 0 Mode 3
  uint8_t  rtt_capability;             // RTT ToF precision bits 0-2 (10 ns if set, else 150 ns)
  uint8_t  rtt_aa_only;                // RTT AA: 0 not supported, else SYNC changes (1-255)
  uint8_t  rtt_sounding;               // RTT Sounding: 0 not supported, else SYNC changes
  uint8_t  rtt_random_payload;         // RTT Random: 0 not supported, else SYNC changes
  uint8_t  t_sw_times;                 // Antenna switch period: 0, 1, 2, 4, or 10 us
  uint8_t  tx_snr_capability;          // TX SNR: bits 0-4 -> 18, 21, 24, 27, 30 dB
  uint8_t  cs_sync_phys;               // CS SYNC PHY: bit 1 LE 2M, bit 2 LE 2M 2BT
} app_cs_capabilities_t;

#ifdef __cplusplus
}
#endif

#endif // APP_CS_CAPABILITIES_H
