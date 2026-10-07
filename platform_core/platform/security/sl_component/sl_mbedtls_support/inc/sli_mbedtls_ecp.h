/***************************************************************************//**
 * @file
 * @brief Internal secp224r1 group load/copy for Mbed TLS software ECP.
 *******************************************************************************
 * The implementation is derived from the secp224r1 support formerly in
 * tf-psa-crypto's drivers/builtin/src/ecp_curves.c and
 * drivers/builtin/src/ecp_curves_new.c. Some applications rely on this legacy
 * curve, so the domain parameters remain unchanged. The SLI APIs keep the curve
 * outside tf-psa-crypto's mbedtls_ecp_group_load() dispatch and PSA Crypto
 * support.
 *
 * After a successful load, grp->id is MBEDTLS_ECP_DP_NONE:
 *   - mbedtls_ecp_muladd / point_read_binary / check_pubkey / group_free
 *     may be used unchanged
 *   - mbedtls_ecp_group_copy() cannot be used; call
 *     sli_mbedtls_ecp_secp224r1_group_copy() instead
 * There is no mbedtls_ecp_group_id for this curve; load via
 * sli_mbedtls_ecp_secp224r1_group_load() only.
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

#ifndef SLI_MBEDTLS_ECP_H
#define SLI_MBEDTLS_ECP_H

#include "mbedtls/private/ecp.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @brief
 *   Load secp224r1 domain parameters into an Mbed TLS group.
 *
 * @param[in,out] grp
 *   Group to initialize with secp224r1 domain parameters.
 *   On success, MPIs alias ROM tables (`h = 1`) and `id` is
 *   MBEDTLS_ECP_DP_NONE in order to prevent mbedtls function that depend on
 *   the group id from being called on a deprecated secp224r1 group.
 *
 * @return
 *   0 on success, or an Mbed TLS ECP error code.
 ******************************************************************************/
int sli_mbedtls_ecp_secp224r1_group_load(mbedtls_ecp_group *grp);

/***************************************************************************//**
 * @brief
 *   Copy a secp224r1 group by reloading its domain parameters.
 *
 * @param[out] dst
 *   Destination group.
 * @param[in] src
 *   Source group. Must be a secp224r1 group loaded by this API. Its contents
 *   are not read: secp224r1 domain parameters are constants, and the group
 *   carries no id identifying the curve, so no validation is possible or
 *   needed.
 *
 * @return
 *   0 on success, or an Mbed TLS ECP error code.
 ******************************************************************************/
int sli_mbedtls_ecp_secp224r1_group_copy(mbedtls_ecp_group *dst,
                                         const mbedtls_ecp_group *src);

#ifdef __cplusplus
}
#endif

#endif // SLI_MBEDTLS_ECP_H
