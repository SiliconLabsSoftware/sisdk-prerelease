/***************************************************************************//**
 * @file
 * @brief Device Manager Portal.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 ******************************************************************************
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
 *****************************************************************************/

#ifndef SLI_DEVICE_PORTAL_H
#define SLI_DEVICE_PORTAL_H

#include "sl_enum.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup device_portal Device Manager Portal
 * @details
 * ## Overview
 *
 * The Device Manager Portal component defines the enums and macros that are
 * used commonly across the Portal HAL and Portal service.
 *
 * @{
 ******************************************************************************/

// ----------------------------------------------------------------------------
// ENUMS

/// PORTAL power domain selector.
SL_ENUM(sli_portal_domain_t) {
  SLI_PORTAL_DOMAIN_HOSTBASE = 0,   ///< PD1HOSTBASE power domain
  SLI_PORTAL_DOMAIN_HOSTNPU,        ///< PD1HOSTNPU power domain
  SLI_PORTAL_DOMAIN_LPW0,           ///< PD1LPW0 power domain
  SLI_PORTAL_DOMAIN_WIFI0BASE,      ///< PD1WIFI0BASE power domain
  SLI_PORTAL_DOMAIN_WIFI0MODEM11B,  ///< PD1WIFI0MODEM11B power domain
  SLI_PORTAL_DOMAIN_WIFI0OFDM,      ///< PD1WIFI0OFDM power domain
  SLI_PORTAL_DOMAIN_MAX             ///< Maximum value used to detect invalid domains
};

/** @} (end addtogroup device_portal) */

#ifdef __cplusplus
}
#endif

#endif // SLI_DEVICE_PORTAL_H
