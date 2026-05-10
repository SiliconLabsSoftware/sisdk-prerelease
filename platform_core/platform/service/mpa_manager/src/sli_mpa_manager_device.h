/***************************************************************************//**
 * @file
 * @brief Memory Protection and Attribute Manager Device-Specific Interface.
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

#ifndef _SLI_MPA_MANAGER_DEVICE_H
#define _SLI_MPA_MANAGER_DEVICE_H

#include "em_device.h"
#include "sl_slist.h"
#include "sl_status.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Detect whether the device has a unified L2 cache (instructions and data
// share a single L2 cache memory). Series 3 parts expose the cache topology
// through the L2ICACHE_PRESENT / L2DCACHE_PRESENT macros from em_device.h:
//   - Unified L2 (e.g. SIMG301, SIMG302): L2ICACHE_PRESENT defined,
//     L2DCACHE_PRESENT NOT defined. The "L2ICACHE" peripheral is in fact a
//     unified instruction+data cache on those parts.
//   - Split L2 (e.g. SIWG353, SIWN353, SIWX353): both L2ICACHE_PRESENT and
//     L2DCACHE_PRESENT defined. The two caches are independent peripherals.
// Devices without an L2 cache (e.g. Series 2) define neither macro and are
// correctly treated as not having a unified cache.
#if defined(L2ICACHE_PRESENT) && !defined(L2DCACHE_PRESENT)
#define SL_DEVICE_HAS_UNIFIED_CACHE
#endif

/*******************************************************************************
 *******************************   STRUCTS   ***********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *  Boundaries are the intermediate representation between MPA Manager
 *  regions and hardware regions (e.g. MPU regions for ARM CortexM).
 ******************************************************************************/
typedef struct sli_mpa_manager_boundary {
  /// List node for the boundary.
  sl_slist_node_t           node;
  /// The MPA Region handle that owns this boundary.
  void*                     region;
  /// The address of the boundary.
  uint32_t                  address;
  /// True if this boundary is the end boundary of the MPA Region.
  bool                      is_end;
} sli_mpa_manager_boundary_t;

/*******************************************************************************
 ******************************** PROTOTYPES ***********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *  Initializes the device-specific portions of the MPA Manager.
 *
 * @description
 *  This function will reset and enable the memory protection hardware.
 *
 ******************************************************************************/
void sli_mpa_manager_device_init(void);

/***************************************************************************//**
 * @brief
 *   Attempts to re-configure the memory protection hardware in order to
 *   apply all currently configured mpa manager regions.
 *
 * @param[in] boundary_list
 *   Pointer to the MPA Manager Boundary List
 *
 * @return
 *   SL_STATUS_OK:
 *     - the memory protection hardware was re-configured successfully.
 *   SL_STATUS_ALLOCATION_FAILED:
 *     - the memory protection hardware was not able to allocate enough regions
 *       to fulfill the total mpa manager region configuration.
 *
 ******************************************************************************/
sl_status_t sli_mpa_manager_device_configure(sl_slist_node_t* boundary_list);

/***************************************************************************//**
 * @brief
 *  Validates the base address and size of an MPA region to ensure that they
 *  respect the limitations that are imposed by the device's architecture.
 *
 * @param[in] base_address
 *  The base address of the MPA region.
 * @param[in] size
 *  The size of the MPA region.
 *
 * @return
 *  True if the base address and size are valid for the device. False otherwise.
 ******************************************************************************/
bool sli_mpa_manager_device_region_validate(uint32_t base_address, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* _SLI_MPA_MANAGER_H */
