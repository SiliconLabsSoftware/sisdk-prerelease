/***************************************************************************//**
 * @brief RAIL Configuration
 * @details
 *   WARNING: Auto-Generated Radio Config  -  DO NOT EDIT
 *   Radio Configurator Version: 2602.2.0
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
#include "sl_rail_btc_config_39MHz.h"
#include "sl_code_classification.h"

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
static const uint8_t irCalConfig[] = {
  25, 63, 1, 6, 4, 16, 1, 0, 0, 1, 1, 6, 0, 16, 39, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0
};

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
static const int32_t timingConfig[] = {
  2654, 2654, 1000, 0
};

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
__ALIGNED(4) static const uint8_t hfxoRetimingConfigEntries[] = {
  2, 0, 0, 0, 0xc0, 0x17, 0x53, 0x02, 6, 20, 0, 0, 0x80, 0x2f, 0xa6, 0x04, 5, 56, 0, 0, 0xa0, 0x08, 0, 0, 0, 0, 0x58, 0x09, 1, 4, 7, 6, 0x10, 0x0a, 1, 4, 7, 7, 0xc8, 0x0a, 0, 4, 8, 7, 0x80, 0x0b, 0, 4, 8, 8, 0x38, 0x0c, 0, 4, 9, 8, 0x61, 0x08, 0, 0, 0, 0, 0x8a, 0x08, 0, 0, 0, 0, 0xc7, 0x09, 1, 4, 4, 3, 0x2c, 0x0b, 1, 4, 4, 4, 0x92, 0x0c, 1, 4, 5, 4
};

#ifdef RADIO_CONFIG_ENABLE_STACK_INFO
SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
__ALIGNED(4) static const uint8_t stackInfo_0[2] = { 0x0a, 0x00 };
#endif // RADIO_CONFIG_ENABLE_STACK_INFO

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
static RAIL_ChannelConfigEntryAttr_t channelConfigEntryAttr = {
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

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
static const uint32_t phyInfo[] = {
  20UL,
  0x00200000UL, // 32.0
  (uint32_t) NULL,
  (uint32_t) irCalConfig,
  (uint32_t) timingConfig,
  0x00000000UL,
  0UL,
  0UL,
  999998UL,
  0x00F00101UL,
  0x02503483UL,
  (uint32_t) NULL,
  (uint32_t) hfxoRetimingConfigEntries,
  (uint32_t) NULL,
  0UL,
  0UL,
  1000000UL,
  (uint32_t) NULL,
  (uint32_t) NULL,
  (uint32_t) NULL,
  (uint32_t) 0UL,
};

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
const uint32_t sl_rail_btc_phy_39MHz_modemConfigBase[] = {
  0x03080000UL, 0x0000803FUL,
  /*    0004 */ 0x0001007EUL,
  0x030C0014UL, 0x00000201UL,
  /*    0018 */ 0x00000000UL,
  /*    001C */ (uint32_t) &phyInfo,
  0x00040010UL, 0x0B008700UL,
  0x0004001CUL, 0x00000000UL,
  0x0008002CUL, 0x0000000FUL,
  /*    0030 */ 0x00000000UL,
  0x00080038UL, 0x00000000UL,
  /*    003C */ 0x00000000UL,
  0x00040044UL, 0x00040000UL,
  0x00042048UL, 0xFFFFFF80UL,
  0x0004004CUL, 0x00000000UL,
  0x00042050UL, 0xFFFFFF80UL,
  0x0004006CUL, 0x00000007UL,
  0x00080074UL, 0x00000000UL,
  /*    0078 */ 0x00000000UL,
  0x00200080UL, 0x00800000UL,
  /*    0084 */ 0x00000000UL,
  /*    0088 */ 0x00000000UL,
  /*    008C */ 0x00000000UL,
  /*    0090 */ 0x00000000UL,
  /*    0094 */ 0xA000E000UL,
  /*    0098 */ 0x00000000UL,
  /*    009C */ 0x00000000UL,
  0x00100110UL, 0x00101FFFUL,
  /*    0114 */ 0x00000000UL,
  /*    0118 */ 0x00105FFFUL,
  /*    011C */ 0x00000000UL,
  0x00080184UL, 0x00000000UL,
  /*    0188 */ 0x00000024UL,
  0x0004E020UL, 0xFFF807FFUL,
  0x0004D020UL, 0x012801FEUL,
  0x0004E024UL, 0xFFFFFF00UL,
  0x0004D024UL, 0x04001300UL,
  0x0020C028UL, 0x03B380ECUL,
  /*    C02C */ 0x51407543UL,
  /*    C030 */ 0xB8000FA0UL,
  /*    C034 */ 0x00004000UL,
  /*    C038 */ 0x0007AAA8UL,
  /*    C03C */ 0x01000800UL,
  /*    C040 */ 0x00010700UL,
  /*    C044 */ 0x00000000UL,
  0x0040C054UL, 0x00604255UL,
  /*    C058 */ 0xE6130614UL,
  /*    C05C */ 0x0000003CUL,
  /*    C060 */ 0x12110F0BUL,
  /*    C064 */ 0x00000013UL,
  /*    C068 */ 0x0002C688UL,
  /*    C06C */ 0x000005A0UL,
  /*    C070 */ 0x0001DEB9UL,
  /*    C074 */ 0x00003000UL,
  /*    C078 */ 0x0000A006UL,
  /*    C07C */ 0x00015010UL,
  /*    C080 */ 0x0002101DUL,
  /*    C084 */ 0x0003F02FUL,
  /*    C088 */ 0x0009F071UL,
  /*    C08C */ 0x003FF0FFUL,
  /*    C090 */ 0x007FF7FFUL,
  0x0014C0A8UL, 0x083198AEUL,
  /*    C0AC */ 0x000420C6UL,
  /*    C0B0 */ 0x76543210UL,
  /*    C0B4 */ 0x00000A98UL,
  /*    C0B8 */ 0x00000000UL,
  0x000CC0D0UL, 0x00000000UL,
  /*    C0D4 */ 0x000A0001UL,
  /*    C0D8 */ 0x00280001UL,
  0x000CC0ECUL, 0x00200000UL,
  /*    C0F0 */ 0x10044222UL,
  /*    C0F4 */ 0x000001F8UL,
  0x001CC10CUL, 0x02010026UL,
  /*    C110 */ 0x0202011EUL,
  /*    C114 */ 0x00010202UL,
  /*    C118 */ 0x1F1F1F1FUL,
  /*    C11C */ 0x00000024UL,
  /*    C120 */ 0x1E1F1F00UL,
  /*    C124 */ 0x00001F1FUL,
  0x0004E130UL, 0xFF000000UL,
  0x0004D130UL, 0x01000000UL,
  0x0010C134UL, 0x00000088UL,
  /*    C138 */ 0x00000000UL,
  /*    C13C */ 0x00000000UL,
  /*    C140 */ 0x00000000UL,
  0x0014C160UL, 0x00000000UL,
  /*    C164 */ 0x00000000UL,
  /*    C168 */ 0x00000000UL,
  /*    C16C */ 0x01010101UL,
  /*    C170 */ 0x01010101UL,
  0x000CC180UL, 0x01010101UL,
  /*    C184 */ 0x09249240UL,
  /*    C188 */ 0xFDA75310UL,
  0x00050008UL, 0x00000744UL,
  0x00050018UL, 0x00000000UL,
  0x00050020UL, 0x0000A001UL,
  0x000D0028UL, 0x00000000UL,
  /*    002C */ 0x00000000UL,
  /*    0030 */ 0x00000000UL,
  0x00094050UL, 0x00000000UL,
  /*    4054 */ 0x00000000UL,
  0x001D405CUL, 0x00000000UL,
  /*    4060 */ 0x0082C20FUL,
  /*    4064 */ 0x20000000UL,
  /*    4068 */ 0x000A1000UL,
  /*    406C */ 0x03000000UL,
  /*    4070 */ 0x20000000UL,
  /*    4074 */ 0x00000000UL,
  0x0021407CUL, 0x00001040UL,
  /*    4080 */ 0x00000000UL,
  /*    4084 */ 0x00000011UL,
  /*    4088 */ 0x000C0037UL,
  /*    408C */ 0x00000000UL,
  /*    4090 */ 0x00040000UL,
  /*    4094 */ 0x60000000UL,
  /*    4098 */ 0x00000000UL,
  0x00094124UL, 0x00000000UL,
  /*    4128 */ 0x00000000UL,
  0x0005413CUL, 0x00000000UL,
  0x00094158UL, 0x00000000UL,
  /*    415C */ 0x00000000UL,
  0x00314168UL, 0x00FA53E8UL,
  /*    416C */ 0x40000000UL,
  /*    4170 */ 0x00000000UL,
  /*    4174 */ 0x00000000UL,
  /*    4178 */ 0x00000000UL,
  /*    417C */ 0x00000000UL,
  /*    4180 */ 0x00000000UL,
  /*    4184 */ 0x0007D101UL,
  /*    4188 */ 0x00000000UL,
  /*    418C */ 0x00000000UL,
  /*    4190 */ 0x00000000UL,
  /*    4194 */ 0x00000000UL,
  0x000541A4UL, 0x00000000UL,
  0x000D41BCUL, 0x00000000UL,
  /*    41C0 */ 0x003C0000UL,
  /*    41C4 */ 0x00069069UL,
  0x000541E0UL, 0x00000004UL,
  0x0005421CUL, 0x0000AA00UL,
  0x00154224UL, 0x0010001CUL,
  /*    4228 */ 0x00000000UL,
  /*    422C */ 0x40001860UL,
  /*    4230 */ 0x00000000UL,
  /*    4234 */ 0x00000000UL,
  0x0005423CUL, 0x00000000UL,
  0x00054244UL, 0x00000014UL,
  0x0005424CUL, 0x04000008UL,
  0x00054268UL, 0x00000000UL,
  0x000942A0UL, 0x00000000UL,
  /*    42A4 */ 0x00000081UL,
  0x001542B8UL, 0x0200003FUL,
  /*    42BC */ 0x0000FFFFUL,
  /*    42C0 */ 0x0000FFFFUL,
  /*    42C4 */ 0x000003FFUL,
  /*    42C8 */ 0x0000FFFFUL,
  0x000542D4UL, 0x00000000UL,
  0x00294350UL, 0x01200040UL,
  /*    4354 */ 0x000000A0UL,
  /*    4358 */ 0x01005008UL,
  /*    435C */ 0x1F1F1F1FUL,
  /*    4360 */ 0x1B1F1F1FUL,
  /*    4364 */ 0x11131518UL,
  /*    4368 */ 0x0C0D0E10UL,
  /*    436C */ 0x2F87C145UL,
  /*    4370 */ 0x00000000UL,
  /*    4374 */ 0x00000000UL,
  0x001143C0UL, 0x00082220UL,
  /*    43C4 */ 0x0000007AUL,
  /*    43C8 */ 0x2BC32080UL,
  /*    43CC */ 0x3155E258UL,
  0x00154420UL, 0x0000B16FUL,
  /*    4424 */ 0x00000000UL,
  /*    4428 */ 0x00000000UL,
  /*    442C */ 0x00000000UL,
  /*    4430 */ 0x00000000UL,
  0x002544A0UL, 0x00000000UL,
  /*    44A4 */ 0x123556B7UL,
  /*    44A8 */ 0x50000000UL,
  /*    44AC */ 0x00003B80UL,
  /*    44B0 */ 0x00000000UL,
  /*    44B4 */ 0x00000168UL,
  /*    44B8 */ 0x00000000UL,
  /*    44BC */ 0x00000240UL,
  /*    44C0 */ 0x03010000UL,
  0x000944C8UL, 0x00000000UL,
  /*    44CC */ 0x123556B7UL,
  0x00058030UL, 0x03000000UL,
  0x00058038UL, 0x04789BC0UL,
  0x000D8044UL, 0x00103483UL,
  /*    8048 */ 0x0000803FUL,
  /*    804C */ 0x00000001UL,
  0x00098068UL, 0x44455500UL,
  /*    806C */ 0x00823344UL,
  0x00158078UL, 0x246D2240UL,
  /*    807C */ 0x00B22489UL,
  /*    8080 */ 0x3FC69018UL,
  /*    8084 */ 0x3FC00018UL,
  /*    8088 */ 0x00800005UL,
  0x00098094UL, 0x31D2A507UL,
  /*    8098 */ 0x3212A587UL,
  0x0005A0A0UL, 0xFFFE0000UL,
  0x000D80A4UL, 0x9AAACCCFUL,
  /*    80A8 */ 0x6777BBBFUL,
  /*    80AC */ 0x0000000BUL,
  0x000580B4UL, 0x00800E03UL,
  0x00060084UL, 0x000001FFUL,
  0x00060090UL, 0x00000000UL,
  0x00060098UL, 0x00000001UL,
  0x01060014UL, 0x34000028UL,
  0x010A005CUL, 0x8DC5A104UL,
  /*    0060 */ 0x100090BEUL,
  0x0106C034UL, 0x30830006UL,
  0x010F8018UL, 0x00000040UL,
  /*    801C */ 0x18CE2404UL,
  /*    8020 */ 0x00000000UL,
  0x01078028UL, 0x0034521CUL,
  0x0107810CUL, 0x01200000UL,
  0x01078114UL, 0x00000080UL,
  0x01078180UL, 0x00000148UL,
  0x0107818CUL, 0x06270000UL,
  0x0107825CUL, 0x00000031UL,
  0x01138280UL, 0x004BF32EUL,
  /*    8284 */ 0x195871D8UL,
  /*    8288 */ 0x00004140UL,
  /*    828C */ 0x00002448UL,
  0x01078400UL, 0x00020106UL,
  0x0113C004UL, 0x00000000UL,
  /*    C008 */ 0x00000002UL,
  /*    C00C */ 0x00000000UL,
  /*    C010 */ 0x00145463UL,
  0x014BC018UL, 0x0021A418UL,
  /*    C01C */ 0x00000080UL,
  /*    C020 */ 0x00000004UL,
  /*    C024 */ 0x01282481UL,
  /*    C028 */ 0x02A94455UL,
  /*    C02C */ 0x28A49402UL,
  /*    C030 */ 0x00402494UL,
  /*    C034 */ 0x01282481UL,
  /*    C038 */ 0x02A94455UL,
  /*    C03C */ 0x28A49402UL,
  /*    C040 */ 0x00402494UL,
  /*    C044 */ 0xE7BC0410UL,
  /*    C048 */ 0x00080C7FUL,
  /*    C04C */ 0xE7BC0410UL,
  /*    C050 */ 0x00080C7FUL,
  /*    C054 */ 0x0809C000UL,
  /*    C058 */ 0x00000000UL,
  /*    C05C */ 0x00000010UL,
  0x011BC070UL, 0x00000000UL,
  /*    C074 */ 0x00082807UL,
  /*    C078 */ 0x06304F17UL,
  /*    C07C */ 0x0C4640C5UL,
  /*    C080 */ 0x76543210UL,
  /*    C084 */ 0xCBA98765UL,
  0x0107C09CUL, 0x00000000UL,
  0x010FC0A4UL, 0x00000000UL,
  /*    C0A8 */ 0x000007FFUL,
  /*    C0AC */ 0x00000000UL,
  0x010BC0B4UL, 0x00000000UL,
  /*    C0B8 */ 0x000007FFUL,
  0x011BC0C4UL, 0x00000000UL,
  /*    C0C8 */ 0x00000000UL,
  /*    C0CC */ 0x00000000UL,
  /*    C0D0 */ 0x00801000UL,
  /*    C0D4 */ 0x00400000UL,
  /*    C0D8 */ 0x00000000UL,
  0x0214005CUL, 0x14000000UL,
  /*    0060 */ 0x0000080FUL,
  /*    0064 */ 0x00000000UL,
  /*    0068 */ 0x00000000UL,
  /*    006C */ 0x00000000UL,
  0x02080074UL, 0x00000000UL,
  /*    0078 */ 0x0000D20DUL,
  0x02040084UL, 0x00020011UL,
  0x0208008CUL, 0x00000000UL,
  /*    0090 */ 0x00001E11UL,
  0x0240009CUL, 0x331E0F06UL,
  /*    00A0 */ 0x7770614CUL,
  /*    00A4 */ 0x00000000UL,
  /*    00A8 */ 0x00000000UL,
  /*    00AC */ 0x00000000UL,
  /*    00B0 */ 0x00000000UL,
  /*    00B4 */ 0x00000000UL,
  /*    00B8 */ 0x00000000UL,
  /*    00BC */ 0x00000000UL,
  /*    00C0 */ 0x00000000UL,
  /*    00C4 */ 0x00000000UL,
  /*    00C8 */ 0x00000000UL,
  /*    00CC */ 0x00000000UL,
  /*    00D0 */ 0x00000000UL,
  /*    00D4 */ 0x00000000UL,
  /*    00D8 */ 0x00000000UL,
  0x021001C0UL, 0x003C0000UL,
  /*    01C4 */ 0x00069069UL,
  /*    01C8 */ 0x00000000UL,
  /*    01CC */ 0x00000000UL,
  0x020401DCUL, 0x00000000UL,
  0x02080420UL, 0x0000B16FUL,
  /*    0424 */ 0x00000000UL,
  0x02040440UL, 0x001E1489UL,
  0x02040448UL, 0x00000D53UL,
  0x02080454UL, 0x00500003UL,
  /*    0458 */ 0x000D20D2UL,
  0x020C0460UL, 0x070600A0UL,
  /*    0464 */ 0xF6ECF300UL,
  /*    0468 */ 0x7F6E4516UL,
  0xFFFFFFFFUL,
};

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
const RAIL_ChannelConfigEntry_t sl_rail_btc_phy_39MHz_channels[] = {
  {
    .phyConfigDeltaAdd = NULL,
    .baseFrequency = 2402000000,
    .channelSpacing = 1000000,
    .physicalChannelOffset = 0,
    .channelNumberStart = 0,
    .channelNumberEnd = 78,
    .maxPower = RAIL_TX_POWER_MAX,
    .attr = &channelConfigEntryAttr,
#ifdef RADIO_CONFIG_ENABLE_CONC_PHY
    .entryType = 0,
#endif
#ifdef RADIO_CONFIG_ENABLE_STACK_INFO
    .stackInfo = stackInfo_0,
#endif
    .alternatePhy = NULL,
  },
};

SL_CODE_CLASSIFY(rail_lib, SL_CODE_CLASS_TIME_CRITICAL)
const RAIL_ChannelConfig_t sl_rail_btc_phy_39MHz_channelConfig = {
  .phyConfigBase = sl_rail_btc_phy_39MHz_modemConfigBase,
  .phyConfigDeltaSubtract = NULL,
  .configs = sl_rail_btc_phy_39MHz_channels,
  .length = 1U,
  .signature = 0UL,
  .xtalFrequencyHz = 39000000UL,
};
