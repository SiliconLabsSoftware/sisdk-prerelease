/***************************************************************************//**
 * @brief RAIL Configuration
 * @details
 *   WARNING: Auto-Generated Radio Config  -  DO NOT EDIT
 *   Radio Configurator Version: 2604.3.0rc0
 *   RAIL Adapter Version: 2.4.33
 *   RAIL Compatibility: 2.x
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
#include "em_device.h"
#include "rail_types.h"
#include "rail_config_915_xg28.h"

uint32_t RAILCb_CalcSymbolRate(RAIL_Handle_t railHandle)
{
  (void) railHandle;
  return 0U;
}

uint32_t RAILCb_CalcBitRate(RAIL_Handle_t railHandle)
{
  (void) railHandle;
  return 0U;
}

void RAILCb_ConfigFrameTypeLength(RAIL_Handle_t railHandle,
                                  const RAIL_FrameType_t *frameType)
{
  (void) railHandle;
  (void) frameType;
}

static const uint8_t irCalConfig[] = {
  20, 41, 2, 0, 0, 57, 19, 0, 0, 0, 1, 0, 2, 100, 0, 1, 1, 47, 0, 0, 7
};

static const int32_t timingConfig[] = {
  6183, 6183, 1250, 0
};

__ALIGNED(4) static const uint8_t hfxoRetimingConfigEntries[] = {
  1, 0, 0, 0, 0xc0, 0x17, 0x53, 0x02, 4, 12, 0, 0, 0xe0, 0x02, 0, 0, 0, 0, 0x3c, 0x03, 1, 2, 5, 4, 0x98, 0x03, 1, 2, 5, 5, 0xf4, 0x03, 1, 2, 6, 5
};

static sl_rail_channel_config_entry_attr_t channelConfigEntryAttr = {
#if RAIL_SUPPORTS_OFDM_PA
  {
#ifdef RADIO_CONFIG_ENABLE_IRCAL_MULTIPLE_RF_PATHS
    { 0xFFFFFFFFUL, 0xFFFFFFFFUL, },
#else
    { 0xFFFFFFFFUL },
#endif // RADIO_CONFIG_ENABLE_IRCAL_MULTIPLE_RF_PATHS
    { 0xFFFFFFFFUL, 0xFFFFFFFFUL }
  }
#else // RAIL_SUPPORTS_OFDM_PA
#ifdef RADIO_CONFIG_ENABLE_IRCAL_MULTIPLE_RF_PATHS
  { 0xFFFFFFFFUL, 0xFFFFFFFFUL, },
#else
  { 0xFFFFFFFFUL },
#endif // RADIO_CONFIG_ENABLE_IRCAL_MULTIPLE_RF_PATHS
#endif // RAIL_SUPPORTS_OFDM_PA
};

static const uint32_t phyInfo[] = {
  21UL,
  0x0003E978UL, // 3.911995076923077
  (uint32_t) NULL,
  (uint32_t) irCalConfig,
  (uint32_t) timingConfig,
  0x00000000UL,
  8UL,
  0UL,
  800000UL,
  0x00F42004UL,
  0x07505E85UL,
  (uint32_t) NULL,
  (uint32_t) hfxoRetimingConfigEntries,
  (uint32_t) NULL,
  0UL,
  0UL,
  800000UL,
  (uint32_t) NULL,
  (uint32_t) NULL,
  (uint32_t) NULL,
  (uint32_t) 0UL,
  (uint32_t) 320UL,
  (uint32_t) 80UL,
};

const uint32_t Protocol_Configuration_1_modemConfigBase[] = {
  0x0002400CUL, 0x00148001UL,
  /*    4010 */ 0x0000407FUL,
  0x00024020UL, 0x00000000UL,
  /*    4024 */ 0x00000000UL,
  0x00074030UL, 0x00000000UL,
  /*    4034 */ 0x00000000UL,
  /*    4038 */ 0x00000000UL,
  /*    403C */ 0x00000000UL,
  /*    4040 */ 0x00000000UL,
  /*    4044 */ 0x00006000UL,
  /*    4048 */ 0x000007A0UL,
  0x00014050UL, 0x00000000UL,
  0x0002405CUL, 0x00000000UL,
  /*    4060 */ 0x00000000UL,
  0x000140A8UL, 0x00000007UL,
  0x000440BCUL, 0x00000000UL,
  /*    40C0 */ 0x00000000UL,
  /*    40C4 */ 0x00000000UL,
  /*    40C8 */ 0x00000000UL,
  0x00044108UL, 0x00004000UL,
  /*    410C */ 0x00004CFFUL,
  /*    4110 */ 0x00004100UL,
  /*    4114 */ 0x00004DFFUL,
  0x0001E020UL, 0xFFF807FFUL,
  0x0001D020UL, 0x00280214UL,
  0x0001E024UL, 0xFFFFFF00UL,
  0x0001D024UL, 0x00004300UL,
  0x0008C028UL, 0x03B380ECUL,
  /*    C02C */ 0x51407543UL,
  /*    C030 */ 0xF8000FA0UL,
  /*    C034 */ 0x00004030UL,
  /*    C038 */ 0x0007AAA8UL,
  /*    C03C */ 0x00000000UL,
  /*    C040 */ 0x00000000UL,
  /*    C044 */ 0x0000022EUL,
  0x0014C054UL, 0x00304187UL,
  /*    C058 */ 0xE61F0021UL,
  /*    C05C */ 0x00000063UL,
  /*    C060 */ 0x1E1C1912UL,
  /*    C064 */ 0x0000001FUL,
  /*    C068 */ 0x0002B6D1UL,
  /*    C06C */ 0x00000660UL,
  /*    C070 */ 0x000010BAUL,
  /*    C074 */ 0x003F0000UL,
  /*    C078 */ 0x00EE008DUL,
  /*    C07C */ 0x03AC01F6UL,
  /*    C080 */ 0x079604F5UL,
  /*    C084 */ 0x0D9C09DEUL,
  /*    C088 */ 0x179311C3UL,
  /*    C08C */ 0x26F51DFEUL,
  /*    C090 */ 0x3FFF32BDUL,
  /*    C094 */ 0x1BF815FEUL,
  /*    C098 */ 0x2DB423DCUL,
  /*    C09C */ 0x3FFF39D0UL,
  /*    C0A0 */ 0x00003FFFUL,
  0x0005C0A8UL, 0x15724BBDUL,
  /*    C0AC */ 0x0518A311UL,
  /*    C0B0 */ 0x76543210UL,
  /*    C0B4 */ 0x00000A98UL,
  /*    C0B8 */ 0x00000000UL,
  0x0004C0CCUL, 0x00000000UL,
  /*    C0D0 */ 0x00000000UL,
  /*    C0D4 */ 0x000A0001UL,
  /*    C0D8 */ 0x00280001UL,
  0x01010008UL, 0x00000704UL,
  0x01010018UL, 0x00000000UL,
  0x01010020UL, 0x00008408UL,
  0x01034040UL, 0x00000000UL,
  /*    4044 */ 0x00000000UL,
  /*    4048 */ 0x00000010UL,
  0x0101604CUL, 0xFFDFFFFFUL,
  0x0101504CUL, 0x0413F920UL,
  0x01224050UL, 0x00620007UL,
  /*    4054 */ 0x00007038UL,
  /*    4058 */ 0x00000000UL,
  /*    405C */ 0x03000000UL,
  /*    4060 */ 0x0F016800UL,
  /*    4064 */ 0x4024B840UL,
  /*    4068 */ 0x00E00555UL,
  /*    406C */ 0x00000841UL,
  /*    4070 */ 0x00000002UL,
  /*    4074 */ 0x000807B0UL,
  /*    4078 */ 0x000000A7UL,
  /*    407C */ 0x00000000UL,
  /*    4080 */ 0x00006323UL,
  /*    4084 */ 0x744AC39BUL,
  /*    4088 */ 0x000F0032UL,
  /*    408C */ 0x60008000UL,
  /*    4090 */ 0x00000ABEUL,
  /*    4094 */ 0x22140A04UL,
  /*    4098 */ 0x4F4A4132UL,
  /*    409C */ 0x00000000UL,
  /*    40A0 */ 0x00000000UL,
  /*    40A4 */ 0x00000000UL,
  /*    40A8 */ 0x00000000UL,
  /*    40AC */ 0x00000000UL,
  /*    40B0 */ 0x00000000UL,
  /*    40B4 */ 0x00000000UL,
  /*    40B8 */ 0x00000000UL,
  /*    40BC */ 0x00000000UL,
  /*    40C0 */ 0x00000000UL,
  /*    40C4 */ 0x00000000UL,
  /*    40C8 */ 0x00000000UL,
  /*    40CC */ 0x00000000UL,
  /*    40D0 */ 0x00000000UL,
  /*    40D4 */ 0x00000000UL,
  0x010160E0UL, 0xFFFFFE07UL,
  0x010150E0UL, 0x00000201UL,
  0x01024110UL, 0x00051E33UL,
  /*    4114 */ 0x00000000UL,
  0x0103411CUL, 0x8CE00000UL,
  /*    4120 */ 0x00000B59UL,
  /*    4124 */ 0x078304FFUL,
  0x010A412CUL, 0x0C6606FFUL,
  /*    4130 */ 0x078304FFUL,
  /*    4134 */ 0x03FF1388UL,
  /*    4138 */ 0xF00A2090UL,
  /*    413C */ 0x0051F81BUL,
  /*    4140 */ 0x00000000UL,
  /*    4144 */ 0x123556B7UL,
  /*    4148 */ 0x50000000UL,
  /*    414C */ 0x00003B80UL,
  /*    4150 */ 0x00000000UL,
  0x01024158UL, 0x00000000UL,
  /*    415C */ 0x00001E00UL,
  0x01014164UL, 0x0000010CUL,
  0x010B416CUL, 0xC08C1400UL,
  /*    4170 */ 0x28211A14UL,
  /*    4174 */ 0x423B342EUL,
  /*    4178 */ 0x55435049UL,
  /*    417C */ 0x0CBA9876UL,
  /*    4180 */ 0x00006928UL,
  /*    4184 */ 0x00000101UL,
  /*    4188 */ 0x64282117UL,
  /*    418C */ 0x001A1714UL,
  /*    4190 */ 0x69C80420UL,
  /*    4194 */ 0x093A20A1UL,
  0x010241A4UL, 0x00000000UL,
  /*    41A8 */ 0x00000029UL,
  0x010C41B0UL, 0x00000000UL,
  /*    41B4 */ 0x00200000UL,
  /*    41B8 */ 0x00000000UL,
  /*    41BC */ 0x00000000UL,
  /*    41C0 */ 0x003C0000UL,
  /*    41C4 */ 0x0006AAAAUL,
  /*    41C8 */ 0x00000000UL,
  /*    41CC */ 0x00000000UL,
  /*    41D0 */ 0x00000000UL,
  /*    41D4 */ 0x000000D0UL,
  /*    41D8 */ 0x00020000UL,
  /*    41DC */ 0x00000000UL,
  0x011541E4UL, 0x0C81901EUL,
  /*    41E8 */ 0x0006490CUL,
  /*    41EC */ 0x006DDFA8UL,
  /*    41F0 */ 0x00B10BC0UL,
  /*    41F4 */ 0x00A53D18UL,
  /*    41F8 */ 0x05020AE8UL,
  /*    41FC */ 0x1DD71B27UL,
  /*    4200 */ 0x0C81901EUL,
  /*    4204 */ 0x0006490CUL,
  /*    4208 */ 0x006DDFA8UL,
  /*    420C */ 0x00B10BC0UL,
  /*    4210 */ 0x00A53D18UL,
  /*    4214 */ 0x05020AE8UL,
  /*    4218 */ 0x1DD71B27UL,
  /*    421C */ 0x00000000UL,
  /*    4220 */ 0x00000000UL,
  /*    4224 */ 0x0000002CUL,
  /*    4228 */ 0x3675EE07UL,
  /*    422C */ 0x40001860UL,
  /*    4230 */ 0x00000000UL,
  /*    4234 */ 0x00000000UL,
  0x0101423CUL, 0x00000000UL,
  0x01014244UL, 0x00000014UL,
  0x0101424CUL, 0x04000008UL,
  0x01014254UL, 0x00000080UL,
  0x010F4330UL, 0x00000000UL,
  /*    4334 */ 0x00000000UL,
  /*    4338 */ 0x00002000UL,
  /*    433C */ 0x00000000UL,
  /*    4340 */ 0x00000000UL,
  /*    4344 */ 0x00000000UL,
  /*    4348 */ 0x00000000UL,
  /*    434C */ 0x00000000UL,
  /*    4350 */ 0x00000000UL,
  /*    4354 */ 0x00000000UL,
  /*    4358 */ 0x00000000UL,
  /*    435C */ 0x38000000UL,
  /*    4360 */ 0x00000000UL,
  /*    4364 */ 0x00000000UL,
  /*    4368 */ 0x58FF0000UL,
  0x01018010UL, 0x00000003UL,
  0x01028038UL, 0x00105E85UL,
  /*    803C */ 0x00000003UL,
  0x0103809CUL, 0x000240EBUL,
  /*    80A0 */ 0x0003B870UL,
  /*    80A4 */ 0x00000FB0UL,
  0x0101A0A8UL, 0xFFFFFE09UL,
  0x010190A8UL, 0x01014201UL,
  0x0101A0ACUL, 0xFFFFFE09UL,
  0x010190ACUL, 0x03AF0001UL,
  0x010280B0UL, 0x02000300UL,
  /*    80B4 */ 0x01000037UL,
  0x02030098UL, 0x00000000UL,
  /*    009C */ 0x04000C00UL,
  /*    00A0 */ 0x0000044CUL,
  0x020200D8UL, 0xAA400005UL,
  /*    00DC */ 0x00000188UL,
  0x020120ECUL, 0xFFFFF01FUL,
  0x020110ECUL, 0x5151200DUL,
  0x020100F0UL, 0x0000052BUL,
  0x02010100UL, 0x00000110UL,
  0x02012104UL, 0xFFFFFFFFUL,
  0x02011104UL, 0x00000110UL,
  0x02012110UL, 0xFFF000FFUL,
  0x02011110UL, 0x42000002UL,
  0x0201212CUL, 0xFFE003FFUL,
  0x0201112CUL, 0x008000A9UL,
  0x02010140UL, 0x0000003FUL,
  0x02012150UL, 0xFFFF3FFFUL,
  0x02011150UL, 0x00510060UL,
  0x02020168UL, 0x00060000UL,
  /*    016C */ 0x00012000UL,
  0x02010174UL, 0x0C1B8169UL,
  0x02012178UL, 0xFFE3FFFFUL,
  0x02011178UL, 0xCFE00440UL,
  0x02012180UL, 0xFFFFF886UL,
  0x02011180UL, 0x00000002UL,
  0x02020188UL, 0x00000050UL,
  /*    018C */ 0x00000000UL,
  0x020121ACUL, 0xFFFFFE07UL,
  0x020111ACUL, 0x00010204UL,
  0x020101B0UL, 0x00000000UL,
  0x03027FF8UL, 0x00000000UL,
  /*    7FFC */ (uint32_t) &phyInfo,
  0xFFFFFFFFUL,
};

const sl_rail_channel_config_entry_t Protocol_Configuration_1_channels[] = {
  {
    .phy_config_delta_add = NULL,
    .base_frequency_hz = 903200000,
    .channel_spacing_hz = 1200000,
    .physical_channel_offset = 4,
    .channel_number_start = 4,
    .channel_number_end = 23,
    .max_power_ddbm = SL_RAIL_TX_POWER_MAX,
    .p_attr = &channelConfigEntryAttr,
    .entry_type = SL_RAIL_CH_TYPE_NORMAL,
    .p_stack_info = NULL,
    .p_alternate_phy = NULL,
  },
};

const sl_rail_channel_config_t Protocol_Configuration_1_channelConfig = {
  .phy_config_base = Protocol_Configuration_1_modemConfigBase,
  .phy_config_delta_subtract = NULL,
  .p_entries = Protocol_Configuration_1_channels,
  .number_of_entries = 1U,
  .signature = 0UL,
  .xtal_frequency_hz = 39000000UL,
};

const sl_rail_channel_config_t *channelConfigs[] = {
  &Protocol_Configuration_1_channelConfig,
  NULL
};

uint32_t protocolAccelerationBuffer[471];
