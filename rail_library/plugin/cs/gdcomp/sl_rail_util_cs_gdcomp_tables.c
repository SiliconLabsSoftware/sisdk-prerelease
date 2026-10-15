/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_rail.h"
#include "sl_rail_ble.h"

#if SL_RAIL_BLE_SUPPORTS_CS
#include "sl_rail_util_cs_gdcomp_config.h"

// Backward-compatibility shim for chips whose config header still provides a
// single RTT slope/offset table per PA (i.e. no per-PHY _1M/_2M variants). In
// that case the same table is used for both the BLE 1M and BLE 2M PHYs. Chips
// that provide per-PHY tables (e.g. efr32xg2b) define the _1M/_2M macros and
// this shim is skipped.
#ifndef SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_1M
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_1M   SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_2M   SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA_1M  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA_2M  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA_1M  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA_2M  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA_1M SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA_2M SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA_1M  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA_2M  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA_1M SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA_2M SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA
#endif // SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_1M

// 0dBm PA LUTs. The PBR phase table is shared across PHYs; the RTT slope and
// offset tables are provided per PHY (BLE 1M and BLE 2M).
static const int16_t gdcompPbrPhaseLsb0Dbm[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_PBR_0DBM_PA;
static const int16_t gdcompRttSlope0Dbm1m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_1M;
static const int16_t gdcompRttSlope0Dbm2m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_2M;
static const int16_t gdcompRttOffset0Dbm1m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA_1M;
static const int16_t gdcompRttOffset0Dbm2m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA_2M;

SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_0_dbm_tables_1m = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb0Dbm,
  .p_rtt_slope = gdcompRttSlope0Dbm1m,
  .p_rtt_offset = gdcompRttOffset0Dbm1m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_0_dbm_tables_2m = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb0Dbm,
  .p_rtt_slope = gdcompRttSlope0Dbm2m,
  .p_rtt_offset = gdcompRttOffset0Dbm2m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

// Legacy single-PHY symbol retained for backward compatibility (BLE 1M).
SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_0_dbm_tables = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb0Dbm,
  .p_rtt_slope = gdcompRttSlope0Dbm1m,
  .p_rtt_offset = gdcompRttOffset0Dbm1m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

// 10dBm PA LUTs
static const int16_t gdcompPbrPhaseLsb10Dbm[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_PBR_10DBM_PA;
static const int16_t gdcompRttSlope10Dbm1m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA_1M;
static const int16_t gdcompRttSlope10Dbm2m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA_2M;
static const int16_t gdcompRttOffset10Dbm1m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA_1M;
static const int16_t gdcompRttOffset10Dbm2m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA_2M;

SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_10_dbm_tables_1m = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb10Dbm,
  .p_rtt_slope = gdcompRttSlope10Dbm1m,
  .p_rtt_offset = gdcompRttOffset10Dbm1m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_10_dbm_tables_2m = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb10Dbm,
  .p_rtt_slope = gdcompRttSlope10Dbm2m,
  .p_rtt_offset = gdcompRttOffset10Dbm2m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

// Legacy single-PHY symbol retained for backward compatibility (BLE 1M).
SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_10_dbm_tables = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb10Dbm,
  .p_rtt_slope = gdcompRttSlope10Dbm1m,
  .p_rtt_offset = gdcompRttOffset10Dbm1m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

// 20dBm PA LUTs
static const int16_t gdcompPbrPhaseLsb20Dbm[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_PBR_20DBM_PA;
static const int16_t gdcompRttSlope20Dbm1m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA_1M;
static const int16_t gdcompRttSlope20Dbm2m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA_2M;
static const int16_t gdcompRttOffset20Dbm1m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA_1M;
static const int16_t gdcompRttOffset20Dbm2m[SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH] =
  SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA_2M;

SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_20_dbm_tables_1m = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb20Dbm,
  .p_rtt_slope = gdcompRttSlope20Dbm1m,
  .p_rtt_offset = gdcompRttOffset20Dbm1m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_20_dbm_tables_2m = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb20Dbm,
  .p_rtt_slope = gdcompRttSlope20Dbm2m,
  .p_rtt_offset = gdcompRttOffset20Dbm2m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

// Legacy single-PHY symbol retained for backward compatibility (BLE 1M).
SLI_LIBRAIL_WEAK
const sl_rail_ble_cs_gd_comp_tables_t sl_rail_ble_cs_gd_comp_20_dbm_tables = {
  .p_pbr_phase_lsb = gdcompPbrPhaseLsb20Dbm,
  .p_rtt_slope = gdcompRttSlope20Dbm1m,
  .p_rtt_offset = gdcompRttOffset20Dbm1m,
  .table_length_halfwords = SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH,
};

#endif // SL_RAIL_BLE_SUPPORTS_CS
