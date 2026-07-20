/***************************************************************************//**
 * @file
 * @brief Serial Port Profile IO Stream component
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

#ifndef SPP_IOSTREAM_H
#define SPP_IOSTREAM_H

/***************************************************************************//**
 * @addtogroup spp_iostream
 * @{
 ******************************************************************************/

#include <stddef.h>
#include "sl_iostream.h"
#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

extern sl_iostream_t *sl_iostream_spp_handle;
extern sl_iostream_instance_info_t sl_iostream_instance_spp_info;

/***************************************************************************//**
 * SPP IO Stream error events.
 ******************************************************************************/
typedef enum {
  SPP_IOSTREAM_ERROR_RX_OVERFLOW = 0,  ///< Received data exceeded RX buffer capacity.
} spp_iostream_error_t;

/***************************************************************************//**
 * Initialize the SPP IO Stream instance.
 *
 * This component owns the SPP receive callback while it is enabled.
 *
 * @return SL_STATUS_OK on success, or an error code otherwise
 ******************************************************************************/
sl_status_t spp_iostream_init(void);

/***************************************************************************//**
 * Get the SPP-backed IO Stream instance.
 *
 * @return Pointer to the SPP IO Stream instance
 ******************************************************************************/
sl_iostream_t *spp_iostream_get_stream_instance(void);

/***************************************************************************//**
 * Callback invoked when the SPP IO Stream encounters an error.
 *
 * Applications may override this weak function to react to component errors.
 * The default implementation is empty.
 *
 * @param[in] error Error event identifier
 * @param[in] result Error result code
 ******************************************************************************/
void spp_iostream_on_error(spp_iostream_error_t error, sl_status_t result);

#ifdef __cplusplus
}
#endif

/** @} (end addtogroup spp_iostream) */

#endif // SPP_IOSTREAM_H
