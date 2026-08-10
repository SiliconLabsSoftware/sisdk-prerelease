/***************************************************************************//**
 * @file
 * @brief PA power conversion curves used by Silicon Labs PA power conversion
 *   functions.
 * @details This file contains the curves needed convert PA power levels to
 *   dBm powers.
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

#ifndef __PA_DBM_POWERSETTING_MAPPING_SIWX353_H_
#define __PA_DBM_POWERSETTING_MAPPING_SIWX353_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SL_RAIL_PA_TABLE_0_NUM_VALUES       (49U)
#define SL_RAIL_PA_TABLE_0_STEP_DDBM        (10U)
#define SL_RAIL_PA_TABLE_0_MAX_POWER_DDBM   (160U)
#define SL_RAIL_PA_TABLE_0_MIN_POWER_DDBM   (-320)

#define SL_RAIL_PA_TABLE_0  \
  {                         \
    0x4000 /* -32.0 dBm */, \
    0x4000 /* -31.0 dBm */, \
    0x4001 /* -30.0 dBm */, \
    0x4001 /* -29.0 dBm */, \
    0x4001 /* -28.0 dBm */, \
    0x4001 /* -27.0 dBm */, \
    0x4001 /* -26.0 dBm */, \
    0x4001 /* -25.0 dBm */, \
    0x4001 /* -24.0 dBm */, \
    0x4001 /* -23.0 dBm */, \
    0x4001 /* -22.0 dBm */, \
    0x4001 /* -21.0 dBm */, \
    0x4001 /* -20.0 dBm */, \
    0x4001 /* -19.0 dBm */, \
    0x4002 /* -18.0 dBm */, \
    0x4002 /* -17.0 dBm */, \
    0x4002 /* -16.0 dBm */, \
    0x4002 /* -15.0 dBm */, \
    0x4002 /* -14.0 dBm */, \
    0x4003 /* -13.0 dBm */, \
    0x4003 /* -12.0 dBm */, \
    0x4003 /* -11.0 dBm */, \
    0x4003 /* -10.0 dBm */, \
    0x4003 /* -9.0 dBm */,  \
    0x4004 /* -8.0 dBm */,  \
    0x4004 /* -7.0 dBm */,  \
    0x4005 /* -6.0 dBm */,  \
    0x4005 /* -5.0 dBm */,  \
    0x4008 /* -4.0 dBm */,  \
    0x4009 /* -3.0 dBm */,  \
    0x400b /* -2.0 dBm */,  \
    0x400c /* -1.0 dBm */,  \
    0x400e /* 0.0 dBm */,   \
    0x400f /* 1.0 dBm */,   \
    0x4012 /* 2.0 dBm */,   \
    0x4015 /* 3.0 dBm */,   \
    0x4019 /* 4.0 dBm */,   \
    0x401f /* 5.0 dBm */,   \
    0x4026 /* 6.0 dBm */,   \
    0x4034 /* 7.0 dBm */,   \
    0x4059 /* 8.0 dBm */,   \
    0x405f /* 9.0 dBm */,   \
    0x405f /* 10.0 dBm */,  \
    0x415f /* 11.0 dBm */,  \
    0x415f /* 12.0 dBm */,  \
    0x415f /* 13.0 dBm */,  \
    0x425f /* 14.0 dBm */,  \
    0x425f /* 15.0 dBm */,  \
    0x425f /* 16.0 dBm */,  \
  }

  #define SL_RAIL_PA_TABLE_1_NUM_VALUES       (29U)
  #define SL_RAIL_PA_TABLE_1_STEP_DDBM        (5U)
  #define SL_RAIL_PA_TABLE_1_MAX_POWER_DDBM   (120U)
  #define SL_RAIL_PA_TABLE_1_MIN_POWER_DDBM   (-20)

  #define SL_RAIL_PA_TABLE_1   \
  {                            \
    0xFFFFFFEC /* -2.0 dBm */, \
    0xFFFFFFF1 /* -1.5 dBm */, \
    0xFFFFFFF6 /* -1.0 dBm */, \
    0xFFFFFFFB /* -0.5 dBm */, \
    0x00000000 /* 0.0 dBm */,  \
    0x00000005 /* 0.5 dBm */,  \
    0x0000000a /* 1.0 dBm */,  \
    0x0000000f /* 1.5 dBm */,  \
    0x00000014 /* 2.0 dBm */,  \
    0x00000019 /* 2.5 dBm */,  \
    0x0000001e /* 3.0 dBm */,  \
    0x00000023 /* 3.5 dBm */,  \
    0x00000028 /* 4.0 dBm */,  \
    0x0000002d /* 4.5 dBm */,  \
    0x00000032 /* 5.0 dBm */,  \
    0x00000037 /* 5.5 dBm */,  \
    0x0000003c /* 6.0 dBm */,  \
    0x00000041 /* 6.5 dBm */,  \
    0x00000046 /* 7.0 dBm */,  \
    0x0000004b /* 7.5 dBm */,  \
    0x00000050 /* 8.0 dBm */,  \
    0x00000055 /* 8.5 dBm */,  \
    0x0000005a /* 9.0 dBm */,  \
    0x0000005f /* 9.5 dBm */,  \
    0x00000064 /* 10.0 dBm */, \
    0x00000069 /* 10.5 dBm */, \
    0x0000006e /* 11.0 dBm */, \
    0x00000073 /* 11.5 dBm */, \
    0x00000078 /* 12.0 dBm */, \
  }

#endif

#ifdef __cplusplus
}
#endif
