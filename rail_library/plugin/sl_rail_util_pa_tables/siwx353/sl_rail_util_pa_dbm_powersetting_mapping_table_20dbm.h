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

#define SL_RAIL_PA_TABLE_0_NUM_VALUES       (55U)
#define SL_RAIL_PA_TABLE_0_STEP_DDBM        (10U)
#define SL_RAIL_PA_TABLE_0_MAX_POWER_DDBM   (220U)
#define SL_RAIL_PA_TABLE_0_MIN_POWER_DDBM   (-320)

#define SL_RAIL_PA_TABLE_0  \
  {                         \
    0x0000 /* -32.0 dBm */, \
    0x0000 /* -31.0 dBm */, \
    0x0000 /* -30.0 dBm */, \
    0x0000 /* -29.0 dBm */, \
    0x0000 /* -28.0 dBm */, \
    0x0000 /* -27.0 dBm */, \
    0x0000 /* -26.0 dBm */, \
    0x0000 /* -25.0 dBm */, \
    0x0000 /* -24.0 dBm */, \
    0x0000 /* -23.0 dBm */, \
    0x0000 /* -22.0 dBm */, \
    0x0000 /* -21.0 dBm */, \
    0x0000 /* -20.0 dBm */, \
    0x0000 /* -19.0 dBm */, \
    0x0000 /* -18.0 dBm */, \
    0x0000 /* -17.0 dBm */, \
    0x0000 /* -16.0 dBm */, \
    0x0000 /* -15.0 dBm */, \
    0x0001 /* -14.0 dBm */, \
    0x0001 /* -13.0 dBm */, \
    0x0001 /* -12.0 dBm */, \
    0x0001 /* -11.0 dBm */, \
    0x0001 /* -10.0 dBm */, \
    0x0001 /* -9.0 dBm */,  \
    0x0002 /* -8.0 dBm */,  \
    0x0002 /* -7.0 dBm */,  \
    0x0002 /* -6.0 dBm */,  \
    0x0003 /* -5.0 dBm */,  \
    0x0003 /* -4.0 dBm */,  \
    0x0003 /* -3.0 dBm */,  \
    0x0004 /* -2.0 dBm */,  \
    0x0005 /* -1.0 dBm */,  \
    0x0005 /* 0.0 dBm */,   \
    0x0006 /* 1.0 dBm */,   \
    0x0007 /* 2.0 dBm */,   \
    0x0008 /* 3.0 dBm */,   \
    0x0009 /* 4.0 dBm */,   \
    0x000b /* 5.0 dBm */,   \
    0x000d /* 6.0 dBm */,   \
    0x0010 /* 7.0 dBm */,   \
    0x0013 /* 8.0 dBm */,   \
    0x0019 /* 9.0 dBm */,   \
    0x0022 /* 10.0 dBm */,  \
    0x0037 /* 11.0 dBm */,  \
    0x0039 /* 12.0 dBm */,  \
    0x003a /* 13.0 dBm */,  \
    0x003b /* 14.0 dBm */,  \
    0x003c /* 15.0 dBm */,  \
    0x003d /* 16.0 dBm */,  \
    0x003f /* 17.0 dBm */,  \
    0x0042 /* 18.0 dBm */,  \
    0x0046 /* 19.0 dBm */,  \
    0x004b /* 20.0 dBm */,  \
    0x005f /* 21.0 dBm */,  \
    0x005f /* 22.0 dBm */,  \
  }

#endif

#ifdef __cplusplus
}
#endif
