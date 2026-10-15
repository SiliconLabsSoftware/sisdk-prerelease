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
    -226, -186, -163, -248, -211,                     \
    -218, -216, -214, -214, -218,                     \
    -222, -217, -213, -220, -218,                     \
    -223, -220, -218, -215, -211,                     \
    -209, -207, -207, -207, -204,                     \
    -204, -203, -204, -263, -231,                     \
    -220, -275, -240,                                 \
  }

#define SL_RAIL_UTIL_CS_GDCOMP_RTT_OFFSET_10DBM_PA_2M \
  {                                                   \
    -340, -301, -280, -360, -326,                     \
    -326, -329, -331, -333, -334,                     \
    -335, -336, -336, -334, -337,                     \
    -334, -333, -337, -330, -329,                     \
    -324, -322, -321, -321, -324,                     \
    -319, -322, -319, -379, -345,                     \
    -336, -383, -361,                                 \
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
