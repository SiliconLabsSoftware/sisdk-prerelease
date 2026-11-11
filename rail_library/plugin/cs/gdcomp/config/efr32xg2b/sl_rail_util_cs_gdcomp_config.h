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

#ifndef SL_RAIL_UTIL_CS_GDCOMP_CONFIG_H
#define SL_RAIL_UTIL_CS_GDCOMP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define SL_RAIL_UTIL_CS_GDCOMP_RUNTIME_PA_TABLE_SELECTION 0

// This tracks entreis of AGC_STATUS_0 gainIndex values.
// AGC_STATUS_0 goes from 0 to 33 i.e. 34 values,
// and firmware subtracts 3 to get table index.
// But validation has given us values from 1-33,
// so the table length is 33.
#define SL_RAIL_UTIL_CS_GDCOMP_TABLE_LENGTH 33U

// RTT slope (per channel) and offset are S16.4, 1 LSB = 1/16 ns.
// Converted from Q8 half-nanoseconds by dividing by 32. PBR entries are unchanged.
// Separate tables are provided per PHY (BLE 1M and BLE 2M). The HAL selects the
// variant that matches the PHY applied when CS is configured. PBR phase tables
// remain shared across PHYs.
//
// The 10 dBm tables are the BLE 1M and BLE 2M validation data for AGC index
// 1 through 33. The 0 dBm and 20 dBm tables keep the previous values, and
// entries past the last measured value repeat that value. Their 2M tables
// use the same values as 1M until 2M data is available for those PA settings.
#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_1M \
  {                                                 \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3,                                        \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_0DBM_PA_2M \
  {                                                 \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3, 3, 3,                                  \
    3, 3, 3,                                        \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA_1M \
  {                                                  \
    -76, -86, -92, -85, -104,                        \
    -116, -40, -51, -64, -80,                        \
    -83, -87, -77, -81, -79,                         \
    -77, -90, -78, -93, -85,                         \
    -90, -97, -97, -97, -97,                         \
    -97, -97, -97, -97, -97,                         \
    -97, -97, -97,                                   \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_0DBM_PA_2M \
  {                                                  \
    -76, -86, -92, -85, -104,                        \
    -116, -40, -51, -64, -80,                        \
    -83, -87, -77, -81, -79,                         \
    -77, -90, -78, -93, -85,                         \
    -90, -97, -97, -97, -97,                         \
    -97, -97, -97, -97, -97,                         \
    -97, -97, -97,                                   \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA_1M \
  {                                                  \
    -3, -3, -3, -3, -3,                              \
    -3, -3, -3, -3, -3,                              \
    -3, -3, -3, -3, -3,                              \
    -3, -3, -3, -3, -3,                              \
    -3, -3, -3, -3, -3,                              \
    -3, -3, -3, -3, -3,                              \
    -3, -3, -3,                                      \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_10DBM_PA_2M \
  {                                                  \
    -1, -1, -1, -1, -1,                              \
    -1, -1, -1, -1, -1,                              \
    -1, -1, -1, -1, -1,                              \
    -1, -1, -1, -1, -1,                              \
    -1, -1, -1, -1, -1,                              \
    -1, -1, -1, -1, -1,                              \
    -1, -1, -1,                                      \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA_1M \
  {                                                   \
    -276, -227, -211, -286, -255,                     \
    -260, -261, -257, -269, -258,                     \
    -258, -269, -265, -264, -264,                     \
    -261, -259, -257, -255, -249,                     \
    -253, -249, -249, -252, -244,                     \
    -243, -245, -235, -301, -259,                     \
    -253, -306, -281,                                 \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA_2M \
  {                                                   \
    -342, -302, -282, -361, -328,                     \
    -328, -330, -331, -335, -335,                     \
    -336, -337, -336, -336, -337,                     \
    -330, -329, -329, -325, -322,                     \
    -319, -319, -316, -317, -318,                     \
    -316, -313, -310, -372, -338,                     \
    -329, -384, -355,                                 \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA_1M \
  {                                                  \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3,                                         \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_SLOPE_20DBM_PA_2M \
  {                                                  \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3, 3, 3,                                   \
    3, 3, 3,                                         \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA_1M \
  {                                                   \
    -76, -86, -92, -85, -104,                         \
    -116, -40, -51, -64, -80,                         \
    -83, -87, -77, -81, -79,                          \
    -77, -90, -78, -93, -85,                          \
    -90, -97, -97, -97, -97,                          \
    -97, -97, -97, -97, -97,                          \
    -97, -97, -97,                                    \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_20DBM_PA_2M \
  {                                                   \
    -76, -86, -92, -85, -104,                         \
    -116, -40, -51, -64, -80,                         \
    -83, -87, -77, -81, -79,                          \
    -77, -90, -78, -93, -85,                          \
    -90, -97, -97, -97, -97,                          \
    -97, -97, -97, -97, -97,                          \
    -97, -97, -97,                                    \
  }

// AUTOGENERATED testRunCalibration.m 20230828_2220_c06eaf7f
#define SL_RAIL_UTIL_CS_GDCOMP_PBR_0DBM_PA \
  {                                        \
    4, 4, 5, 4, 4,                         \
    4, 4, 5, 4, 4,                         \
    4, 4, 4, 3, 4,                         \
    5, 4, 5, 4, 4,                         \
    2, 3, 3, 1, 1,                         \
    1, 1, 1, 1, 1,                         \
    1, 1, 1,                               \
  }

// AUTOGENERATED testRunCalibration.m 20230828_2220_c06eaf7f
#define SL_RAIL_UTIL_CS_GDCOMP_PBR_10DBM_PA \
  {                                         \
    63, 64, 63, 64, 64,                     \
    63, 60, 56, 55, 54,                     \
    53, 52, 52, 53, 52,                     \
    52, 51, 51, 53, 53,                     \
    53, 53, 53, 52, 53,                     \
    52, 53, 53, 51, 52,                     \
    52, 52, 52,                             \
  }

// AUTOGENERATED testRunCalibration.m 20230908_2159_9791097d
#define SL_RAIL_UTIL_CS_GDCOMP_PBR_20DBM_PA \
  {                                         \
    33, 33, 31, 31, 30,                     \
    32, 32, 31, 30, 27,                     \
    28, 28, 27, 27, 28,                     \
    26, 27, 26, 27, 30,                     \
    30, 30, 30, 31, 31,                     \
    31, 31, 31, 31, 31,                     \
    31, 31, 31,                             \
  }

#ifdef __cplusplus
}
#endif

#endif // SL_RAIL_UTIL_CS_GDCOMP_CONFIG_H
