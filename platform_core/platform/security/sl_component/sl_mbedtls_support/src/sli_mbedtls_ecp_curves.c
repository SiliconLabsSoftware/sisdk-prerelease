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
 * Domain parameters are shared; the NIST modp reduction is selected by
 * MBEDTLS_ECP_WITH_MPI_UINT.
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
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "sli_mbedtls_ecp.h"

#include "tf-psa-crypto/build_info.h"

#include "mbedtls/private/bignum.h"
#include "mbedtls/private/error_common.h"

#include "bn_mul.h"
#include "bignum_core.h"
#include <stdint.h>

/*
 * SLI ECP MPI and point initialization macros based on
 * tf-psa-crypto/drivers/builtin/src/ecp_curves.c macros.
 */
#define SLI_ECP_MPI_INIT(_p, _n) { .p = (mbedtls_mpi_uint *) (_p), .s = 1, .n = (_n) }

#define SLI_ECP_MPI_INIT_ARRAY(x) \
  SLI_ECP_MPI_INIT(x, sizeof(x) / sizeof(mbedtls_mpi_uint))

#define SLI_ECP_POINT_INIT_XY_Z0(x, y) { \
    SLI_ECP_MPI_INIT_ARRAY(x), SLI_ECP_MPI_INIT_ARRAY(y), SLI_ECP_MPI_INIT(NULL, 0) }
#define SLI_ECP_POINT_INIT_XY_Z1(x, y) { \
    SLI_ECP_MPI_INIT_ARRAY(x), SLI_ECP_MPI_INIT_ARRAY(y), SLI_ECP_MPI_INIT(mpi_one, 1) }

static const mbedtls_mpi_uint mpi_one[] = { 1 };

/* Domain parameters for secp224r1 */
static const mbedtls_mpi_uint secp224r1_p[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
  MBEDTLS_BYTES_TO_T_UINT_8(0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF),
  MBEDTLS_BYTES_TO_T_UINT_8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF),
  MBEDTLS_BYTES_TO_T_UINT_8(0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_b[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xB4, 0xFF, 0x55, 0x23, 0x43, 0x39, 0x0B, 0x27),
  MBEDTLS_BYTES_TO_T_UINT_8(0xBA, 0xD8, 0xBF, 0xD7, 0xB7, 0xB0, 0x44, 0x50),
  MBEDTLS_BYTES_TO_T_UINT_8(0x56, 0x32, 0x41, 0xF5, 0xAB, 0xB3, 0x04, 0x0C),
  MBEDTLS_BYTES_TO_T_UINT_4(0x85, 0x0A, 0x05, 0xB4),
};
static const mbedtls_mpi_uint secp224r1_gx[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x21, 0x1D, 0x5C, 0x11, 0xD6, 0x80, 0x32, 0x34),
  MBEDTLS_BYTES_TO_T_UINT_8(0x22, 0x11, 0xC2, 0x56, 0xD3, 0xC1, 0x03, 0x4A),
  MBEDTLS_BYTES_TO_T_UINT_8(0xB9, 0x90, 0x13, 0x32, 0x7F, 0xBF, 0xB4, 0x6B),
  MBEDTLS_BYTES_TO_T_UINT_4(0xBD, 0x0C, 0x0E, 0xB7),
};
static const mbedtls_mpi_uint secp224r1_gy[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x34, 0x7E, 0x00, 0x85, 0x99, 0x81, 0xD5, 0x44),
  MBEDTLS_BYTES_TO_T_UINT_8(0x64, 0x47, 0x07, 0x5A, 0xA0, 0x75, 0x43, 0xCD),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE6, 0xDF, 0x22, 0x4C, 0xFB, 0x23, 0xF7, 0xB5),
  MBEDTLS_BYTES_TO_T_UINT_4(0x88, 0x63, 0x37, 0xBD),
};
static const mbedtls_mpi_uint secp224r1_n[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x3D, 0x2A, 0x5C, 0x5C, 0x45, 0x29, 0xDD, 0x13),
  MBEDTLS_BYTES_TO_T_UINT_8(0x3E, 0xF0, 0xB8, 0xE0, 0xA2, 0x16, 0xFF, 0xFF),
  MBEDTLS_BYTES_TO_T_UINT_8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF),
  MBEDTLS_BYTES_TO_T_UINT_4(0xFF, 0xFF, 0xFF, 0xFF),
};
#if MBEDTLS_ECP_FIXED_POINT_OPTIM == 1
static const mbedtls_mpi_uint secp224r1_T_0_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x21, 0x1D, 0x5C, 0x11, 0xD6, 0x80, 0x32, 0x34),
  MBEDTLS_BYTES_TO_T_UINT_8(0x22, 0x11, 0xC2, 0x56, 0xD3, 0xC1, 0x03, 0x4A),
  MBEDTLS_BYTES_TO_T_UINT_8(0xB9, 0x90, 0x13, 0x32, 0x7F, 0xBF, 0xB4, 0x6B),
  MBEDTLS_BYTES_TO_T_UINT_8(0xBD, 0x0C, 0x0E, 0xB7, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_0_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x34, 0x7E, 0x00, 0x85, 0x99, 0x81, 0xD5, 0x44),
  MBEDTLS_BYTES_TO_T_UINT_8(0x64, 0x47, 0x07, 0x5A, 0xA0, 0x75, 0x43, 0xCD),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE6, 0xDF, 0x22, 0x4C, 0xFB, 0x23, 0xF7, 0xB5),
  MBEDTLS_BYTES_TO_T_UINT_8(0x88, 0x63, 0x37, 0xBD, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_1_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xE0, 0xF9, 0xB8, 0xD0, 0x3D, 0xD2, 0xD3, 0xFA),
  MBEDTLS_BYTES_TO_T_UINT_8(0x1E, 0xFD, 0x99, 0x26, 0x19, 0xFE, 0x13, 0x6E),
  MBEDTLS_BYTES_TO_T_UINT_8(0x1C, 0x0E, 0x4C, 0x48, 0x7C, 0xA2, 0x17, 0x01),
  MBEDTLS_BYTES_TO_T_UINT_8(0x3D, 0xA3, 0x13, 0x57, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_1_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x9F, 0x16, 0x5C, 0x8F, 0xAA, 0xED, 0x0F, 0x58),
  MBEDTLS_BYTES_TO_T_UINT_8(0xBF, 0xC5, 0x43, 0x34, 0x93, 0x05, 0x2A, 0x4C),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE4, 0xE3, 0x6C, 0xCA, 0xC6, 0x14, 0xC2, 0x25),
  MBEDTLS_BYTES_TO_T_UINT_8(0xD3, 0x43, 0x6C, 0xD7, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_2_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xC3, 0x5A, 0x98, 0x1E, 0xC8, 0xA5, 0x42, 0xA3),
  MBEDTLS_BYTES_TO_T_UINT_8(0x98, 0x49, 0x56, 0x78, 0xF8, 0xEF, 0xED, 0x65),
  MBEDTLS_BYTES_TO_T_UINT_8(0x1B, 0xBB, 0x64, 0xB6, 0x4C, 0x54, 0x5F, 0xD1),
  MBEDTLS_BYTES_TO_T_UINT_8(0x2F, 0x0C, 0x33, 0xCC, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_2_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xFA, 0x79, 0xCB, 0x2E, 0x08, 0xFF, 0xD8, 0xE6),
  MBEDTLS_BYTES_TO_T_UINT_8(0x2E, 0x1F, 0xD4, 0xD7, 0x57, 0xE9, 0x39, 0x45),
  MBEDTLS_BYTES_TO_T_UINT_8(0xD8, 0xD6, 0x3B, 0x0A, 0x1C, 0x87, 0xB7, 0x6A),
  MBEDTLS_BYTES_TO_T_UINT_8(0xEB, 0x30, 0xD8, 0x05, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_3_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xAD, 0x79, 0x74, 0x9A, 0xE6, 0xBB, 0xC2, 0xC2),
  MBEDTLS_BYTES_TO_T_UINT_8(0xB4, 0x5B, 0xA6, 0x67, 0xC1, 0x91, 0xE7, 0x64),
  MBEDTLS_BYTES_TO_T_UINT_8(0xF0, 0xDF, 0x38, 0x82, 0x19, 0x2C, 0x4C, 0xCA),
  MBEDTLS_BYTES_TO_T_UINT_8(0xD1, 0x2E, 0x39, 0xC5, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_3_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x99, 0x36, 0x78, 0x4E, 0xAE, 0x5B, 0x02, 0x76),
  MBEDTLS_BYTES_TO_T_UINT_8(0x14, 0xF6, 0x8B, 0xF8, 0xF4, 0x92, 0x6B, 0x42),
  MBEDTLS_BYTES_TO_T_UINT_8(0xBA, 0x4D, 0x71, 0x35, 0xE7, 0x0C, 0x2C, 0x98),
  MBEDTLS_BYTES_TO_T_UINT_8(0x9B, 0xA5, 0x1F, 0xAE, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_4_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xAF, 0x1C, 0x4B, 0xDF, 0x5B, 0xF2, 0x51, 0xB7),
  MBEDTLS_BYTES_TO_T_UINT_8(0x05, 0x74, 0xB1, 0x5A, 0xC6, 0x0F, 0x0E, 0x61),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE8, 0x24, 0x09, 0x62, 0xAF, 0xFC, 0xDB, 0x45),
  MBEDTLS_BYTES_TO_T_UINT_8(0x43, 0xE1, 0x80, 0x55, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_4_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x3C, 0x82, 0xFE, 0xAD, 0xC3, 0xE5, 0xCF, 0xD8),
  MBEDTLS_BYTES_TO_T_UINT_8(0x24, 0xA2, 0x62, 0x17, 0x76, 0xF0, 0x5A, 0xFA),
  MBEDTLS_BYTES_TO_T_UINT_8(0x3E, 0xB8, 0xE5, 0xAC, 0xB7, 0x66, 0x38, 0xAA),
  MBEDTLS_BYTES_TO_T_UINT_8(0x97, 0xFD, 0x86, 0x05, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_5_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x59, 0xD3, 0x0C, 0x3C, 0xD1, 0x66, 0xB0, 0xF1),
  MBEDTLS_BYTES_TO_T_UINT_8(0xBC, 0x59, 0xB4, 0x8D, 0x90, 0x10, 0xB7, 0xA2),
  MBEDTLS_BYTES_TO_T_UINT_8(0x96, 0x47, 0x9B, 0xE6, 0x55, 0x8A, 0xE4, 0xEE),
  MBEDTLS_BYTES_TO_T_UINT_8(0xB1, 0x49, 0xDB, 0x78, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_5_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x41, 0x97, 0xED, 0xDE, 0xFF, 0xB3, 0xDF, 0x48),
  MBEDTLS_BYTES_TO_T_UINT_8(0x10, 0xB9, 0x83, 0xB7, 0xEB, 0xBE, 0x40, 0x8D),
  MBEDTLS_BYTES_TO_T_UINT_8(0xAF, 0xD3, 0xD3, 0xCD, 0x0E, 0x82, 0x79, 0x3D),
  MBEDTLS_BYTES_TO_T_UINT_8(0x9B, 0x83, 0x1B, 0xF0, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_6_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x3F, 0x22, 0xBB, 0x54, 0xD3, 0x31, 0x56, 0xFC),
  MBEDTLS_BYTES_TO_T_UINT_8(0x80, 0x36, 0xE5, 0xE0, 0x89, 0x96, 0x8E, 0x71),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE1, 0xEF, 0x0A, 0xED, 0xD0, 0x11, 0x4A, 0xFF),
  MBEDTLS_BYTES_TO_T_UINT_8(0x15, 0x00, 0x57, 0x27, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_6_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x13, 0xCA, 0x3D, 0xF7, 0x64, 0x9B, 0x6E, 0x85),
  MBEDTLS_BYTES_TO_T_UINT_8(0x90, 0xE3, 0x70, 0x6B, 0x41, 0xD7, 0xED, 0x8F),
  MBEDTLS_BYTES_TO_T_UINT_8(0x02, 0x44, 0x44, 0x80, 0xCE, 0x13, 0x37, 0x92),
  MBEDTLS_BYTES_TO_T_UINT_8(0x94, 0x73, 0x80, 0x79, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_7_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xB7, 0x4D, 0x70, 0x7D, 0x31, 0x0F, 0x1C, 0x58),
  MBEDTLS_BYTES_TO_T_UINT_8(0x6D, 0x35, 0x88, 0x47, 0xC4, 0x24, 0x78, 0x3F),
  MBEDTLS_BYTES_TO_T_UINT_8(0xBA, 0xF0, 0xCD, 0x91, 0x81, 0xB3, 0xDE, 0xB6),
  MBEDTLS_BYTES_TO_T_UINT_8(0x04, 0xCE, 0xC6, 0xF7, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_7_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xE9, 0x9C, 0x2D, 0xE8, 0xD2, 0x00, 0x8F, 0x10),
  MBEDTLS_BYTES_TO_T_UINT_8(0xD5, 0x5E, 0x7C, 0x0E, 0x0C, 0x6E, 0x58, 0x02),
  MBEDTLS_BYTES_TO_T_UINT_8(0xAE, 0x81, 0x21, 0xCE, 0x43, 0xF4, 0x24, 0x3D),
  MBEDTLS_BYTES_TO_T_UINT_8(0x9E, 0xBC, 0xF0, 0xF4, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_8_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xD6, 0x10, 0xC2, 0x74, 0x4A, 0x8F, 0x8A, 0xCF),
  MBEDTLS_BYTES_TO_T_UINT_8(0x89, 0x67, 0xF4, 0x2B, 0x38, 0x2B, 0x35, 0x17),
  MBEDTLS_BYTES_TO_T_UINT_8(0xF5, 0xE7, 0x0C, 0xA9, 0xFA, 0x77, 0x5C, 0xBD),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE0, 0x33, 0x19, 0x2B, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_8_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xE7, 0x3E, 0x96, 0x22, 0x53, 0xE1, 0xE9, 0xBE),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE0, 0x13, 0xBC, 0xA1, 0x16, 0xEC, 0x01, 0x1A),
  MBEDTLS_BYTES_TO_T_UINT_8(0x9A, 0x00, 0xC9, 0x7A, 0xC3, 0x73, 0xA5, 0x45),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE1, 0xF4, 0x5E, 0xC1, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_9_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xA8, 0x95, 0xD6, 0xD9, 0x32, 0x30, 0x2B, 0xD0),
  MBEDTLS_BYTES_TO_T_UINT_8(0x77, 0x42, 0x09, 0x05, 0x61, 0x2A, 0x7E, 0x82),
  MBEDTLS_BYTES_TO_T_UINT_8(0x73, 0x84, 0xA2, 0x05, 0x88, 0x64, 0x65, 0xF9),
  MBEDTLS_BYTES_TO_T_UINT_8(0x03, 0x2D, 0x90, 0xB3, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_9_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x0A, 0xE7, 0x2E, 0x85, 0x55, 0x80, 0x7C, 0x79),
  MBEDTLS_BYTES_TO_T_UINT_8(0x0F, 0xC1, 0xAC, 0x78, 0xB4, 0xAF, 0xFB, 0x6E),
  MBEDTLS_BYTES_TO_T_UINT_8(0xD3, 0xC3, 0x28, 0x8E, 0x79, 0x18, 0x1F, 0x58),
  MBEDTLS_BYTES_TO_T_UINT_8(0x34, 0x46, 0xCF, 0x49, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_10_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x63, 0x5F, 0xA8, 0x6C, 0x46, 0x83, 0x43, 0xFA),
  MBEDTLS_BYTES_TO_T_UINT_8(0xFA, 0xA9, 0x93, 0x11, 0xB6, 0x07, 0x57, 0x74),
  MBEDTLS_BYTES_TO_T_UINT_8(0x77, 0x2A, 0x9D, 0x03, 0x89, 0x7E, 0xD7, 0x3C),
  MBEDTLS_BYTES_TO_T_UINT_8(0x7B, 0x8C, 0x62, 0xCF, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_10_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x44, 0x2C, 0x13, 0x59, 0xCC, 0xFA, 0x84, 0x9E),
  MBEDTLS_BYTES_TO_T_UINT_8(0x51, 0xB9, 0x48, 0xBC, 0x57, 0xC7, 0xB3, 0x7C),
  MBEDTLS_BYTES_TO_T_UINT_8(0xFC, 0x0A, 0x38, 0x24, 0x2E, 0x3A, 0x28, 0x25),
  MBEDTLS_BYTES_TO_T_UINT_8(0xBC, 0x0A, 0x43, 0xB8, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_11_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x59, 0x25, 0xAB, 0xC1, 0xEE, 0x70, 0x3C, 0xE1),
  MBEDTLS_BYTES_TO_T_UINT_8(0xF3, 0xDB, 0x45, 0x1D, 0x4A, 0x80, 0x75, 0x35),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE8, 0x1F, 0x4D, 0x2D, 0x9A, 0x05, 0xF4, 0xCB),
  MBEDTLS_BYTES_TO_T_UINT_8(0x6B, 0x10, 0xF0, 0x5A, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_11_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x35, 0x95, 0xE1, 0xDC, 0x15, 0x86, 0xC3, 0x7B),
  MBEDTLS_BYTES_TO_T_UINT_8(0xEC, 0xDC, 0x27, 0xD1, 0x56, 0xA1, 0x14, 0x0D),
  MBEDTLS_BYTES_TO_T_UINT_8(0x59, 0x0B, 0xD6, 0x77, 0x4E, 0x44, 0xA2, 0xF8),
  MBEDTLS_BYTES_TO_T_UINT_8(0x94, 0x42, 0x71, 0x1F, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_12_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x30, 0x86, 0xB2, 0xB0, 0xC8, 0x2F, 0x7B, 0xFE),
  MBEDTLS_BYTES_TO_T_UINT_8(0x96, 0xEF, 0xCB, 0xDB, 0xBC, 0x9E, 0x3B, 0xC5),
  MBEDTLS_BYTES_TO_T_UINT_8(0x1B, 0x03, 0x86, 0xDD, 0x5B, 0xF5, 0x8D, 0x46),
  MBEDTLS_BYTES_TO_T_UINT_8(0x58, 0x95, 0x79, 0xD6, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_12_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x84, 0x32, 0x14, 0xDA, 0x9B, 0x4F, 0x07, 0x39),
  MBEDTLS_BYTES_TO_T_UINT_8(0xB5, 0x3E, 0xFB, 0x06, 0xEE, 0xA7, 0x40, 0x40),
  MBEDTLS_BYTES_TO_T_UINT_8(0x76, 0x1F, 0xDF, 0x71, 0x61, 0xFD, 0x8B, 0xBE),
  MBEDTLS_BYTES_TO_T_UINT_8(0x80, 0x8B, 0xAB, 0x8B, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_13_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xC9, 0x34, 0xB3, 0xB4, 0xBC, 0x9F, 0xB0, 0x5E),
  MBEDTLS_BYTES_TO_T_UINT_8(0xE6, 0x58, 0x48, 0xA8, 0x77, 0xBB, 0x13, 0x2F),
  MBEDTLS_BYTES_TO_T_UINT_8(0x41, 0xC6, 0xF7, 0x34, 0xCC, 0x89, 0x21, 0x0A),
  MBEDTLS_BYTES_TO_T_UINT_8(0xCA, 0x33, 0xDD, 0x1F, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_13_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xCC, 0x81, 0xEF, 0xA4, 0xF2, 0x10, 0x0B, 0xCD),
  MBEDTLS_BYTES_TO_T_UINT_8(0x83, 0xF7, 0x6E, 0x72, 0x4A, 0xDF, 0xDD, 0xE8),
  MBEDTLS_BYTES_TO_T_UINT_8(0x67, 0x23, 0x0A, 0x53, 0x03, 0x16, 0x62, 0xD2),
  MBEDTLS_BYTES_TO_T_UINT_8(0x0B, 0x76, 0xFD, 0x3C, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_14_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xCB, 0x14, 0xA1, 0xFA, 0xA0, 0x18, 0xBE, 0x07),
  MBEDTLS_BYTES_TO_T_UINT_8(0x03, 0x2A, 0xE1, 0xD7, 0xB0, 0x6C, 0xA0, 0xDE),
  MBEDTLS_BYTES_TO_T_UINT_8(0xD1, 0xC0, 0xB0, 0xC6, 0x63, 0x24, 0xCD, 0x4E),
  MBEDTLS_BYTES_TO_T_UINT_8(0x33, 0x38, 0x2C, 0xB1, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_14_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xEE, 0xCD, 0x7D, 0x20, 0x0C, 0xFE, 0xAC, 0xC3),
  MBEDTLS_BYTES_TO_T_UINT_8(0x09, 0x97, 0x9F, 0xA2, 0xB6, 0x45, 0xF7, 0x7B),
  MBEDTLS_BYTES_TO_T_UINT_8(0xCA, 0x99, 0xF3, 0xD2, 0x20, 0x02, 0xEB, 0x04),
  MBEDTLS_BYTES_TO_T_UINT_8(0x43, 0x18, 0x5B, 0x7B, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_15_X[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0x2B, 0xDD, 0x77, 0x91, 0x60, 0xEA, 0xFD, 0xD3),
  MBEDTLS_BYTES_TO_T_UINT_8(0x7D, 0xD3, 0xB5, 0xD6, 0x90, 0x17, 0x0E, 0x1A),
  MBEDTLS_BYTES_TO_T_UINT_8(0x00, 0xF4, 0x28, 0xC1, 0xF2, 0x53, 0xF6, 0x63),
  MBEDTLS_BYTES_TO_T_UINT_8(0x49, 0x58, 0xDC, 0x61, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_mpi_uint secp224r1_T_15_Y[] = {
  MBEDTLS_BYTES_TO_T_UINT_8(0xA8, 0x20, 0x01, 0xFB, 0xF1, 0xBD, 0x5F, 0x45),
  MBEDTLS_BYTES_TO_T_UINT_8(0xD0, 0x7F, 0x06, 0xDA, 0x11, 0xCB, 0xBA, 0xA6),
  MBEDTLS_BYTES_TO_T_UINT_8(0xA7, 0x41, 0x00, 0xA4, 0x1B, 0x30, 0x33, 0x79),
  MBEDTLS_BYTES_TO_T_UINT_8(0xF4, 0xFF, 0x27, 0xCA, 0x00, 0x00, 0x00, 0x00),
};
static const mbedtls_ecp_point secp224r1_T[16] = {
  SLI_ECP_POINT_INIT_XY_Z1(secp224r1_T_0_X, secp224r1_T_0_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_1_X, secp224r1_T_1_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_2_X, secp224r1_T_2_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_3_X, secp224r1_T_3_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_4_X, secp224r1_T_4_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_5_X, secp224r1_T_5_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_6_X, secp224r1_T_6_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_7_X, secp224r1_T_7_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_8_X, secp224r1_T_8_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_9_X, secp224r1_T_9_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_10_X, secp224r1_T_10_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_11_X, secp224r1_T_11_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_12_X, secp224r1_T_12_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_13_X, secp224r1_T_13_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_14_X, secp224r1_T_14_Y),
  SLI_ECP_POINT_INIT_XY_Z0(secp224r1_T_15_X, secp224r1_T_15_Y),
};
#else
#define secp224r1_T NULL
#endif /* MBEDTLS_ECP_FIXED_POINT_OPTIM == 1 */

#if defined(MBEDTLS_ECP_WITH_MPI_UINT)
#if defined(MBEDTLS_ECP_NIST_OPTIM)

#define LOAD32      cur = A(i);

#if defined(MBEDTLS_HAVE_INT32)

#define MAX32       X_limbs
#define A(j)        X[j]
#define STORE32     X[i] = (mbedtls_mpi_uint) cur;
#define STORE0      X[i] = 0;

#else

#define MAX32       X_limbs * 2
#define A(j)                      \
  (j) % 2                         \
  ? (uint32_t) (X[(j) / 2] >> 32) \
  : (uint32_t) (X[(j) / 2])
#define STORE32                         \
  if (i % 2) {                          \
    X[i / 2] &= 0x00000000FFFFFFFF;     \
    X[i / 2] |= (uint64_t) (cur) << 32; \
  } else {                              \
    X[i / 2] &= 0xFFFFFFFF00000000;     \
    X[i / 2] |= (uint32_t) cur;         \
  }

#define STORE0                      \
  if (i % 2) {                      \
    X[i / 2] &= 0x00000000FFFFFFFF; \
  } else {                          \
    X[i / 2] &= 0xFFFFFFFF00000000; \
  }

#endif

static inline int8_t sli_extract_carry(int64_t cur)
{
  return (int8_t) (cur >> 32);
}

#define ADD(j)    cur += A(j)
#define SUB(j)    cur -= A(j)

#define ADD_CARRY(cc) cur += (cc)
#define SUB_CARRY(cc) cur -= (cc)

#define ADD_LAST ADD_CARRY(last_c)
#define SUB_LAST SUB_CARRY(last_c)

/*
 * Helpers for the main 'loop'
 */
#define INIT(b)         \
  int8_t c = 0, last_c; \
  int64_t cur;          \
  size_t i = 0;         \
  LOAD32;

#define NEXT                  \
  c = sli_extract_carry(cur); \
  STORE32; i++; LOAD32;       \
  ADD_CARRY(c);

#define RESET                 \
  c = sli_extract_carry(cur); \
  last_c = c;                 \
  STORE32; i = 0; LOAD32;     \
  c = 0;                      \

#define LAST                                 \
  c = sli_extract_carry(cur);                \
  STORE32; i++;                              \
  if (c != 0) {                              \
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA; } \
  while (i < MAX32) { STORE0; i++; }

static int sli_ecp_mod_p224_raw(mbedtls_mpi_uint *X, size_t X_limbs);

/*
 * Fast quasi-reduction modulo p224 (FIPS 186-3 D.2.2)
 */
static int sli_ecp_mod_p224(mbedtls_mpi *N)
{
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
  size_t expected_width = BITS_TO_LIMBS(224) * 2;
  MBEDTLS_MPI_CHK(mbedtls_mpi_grow(N, expected_width));
  ret = sli_ecp_mod_p224_raw(N->p, expected_width);
  cleanup:
  return ret;
}

static int sli_ecp_mod_p224_raw(mbedtls_mpi_uint *X, size_t X_limbs)
{
  if (X_limbs != BITS_TO_LIMBS(224) * 2) {
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
  }

  INIT(224);

  SUB(7);  SUB(11);           NEXT;     // A0 += -A7  - A11
  SUB(8);  SUB(12);           NEXT;     // A1 += -A8  - A12
  SUB(9);  SUB(13);           NEXT;     // A2 += -A9  - A13
  SUB(10); ADD(7);  ADD(11);  NEXT;     // A3 += -A10 + A7 + A11
  SUB(11); ADD(8);  ADD(12);  NEXT;     // A4 += -A11 + A8 + A12
  SUB(12); ADD(9);  ADD(13);  NEXT;     // A5 += -A12 + A9 + A13
  SUB(13); ADD(10);                     // A6 += -A13 + A10

  RESET;

  /* Use 2^224 = P + 2^96 - 1 to modulo reduce the final carry */
  SUB_LAST; NEXT;                       // A0 -= last_c
  ;         NEXT;                       // A1
  ;         NEXT;                       // A2
  ADD_LAST; NEXT;                       // A3 += last_c
  ;         NEXT;                       // A4
  ;         NEXT;                       // A5
                                        // A6

  /* The carry reduction cannot generate a carry
   * (see commit 73e8553 for details)*/

  LAST;

  return 0;
}

#endif /* MBEDTLS_ECP_NIST_OPTIM */
#else /* !MBEDTLS_ECP_WITH_MPI_UINT */
#if defined(MBEDTLS_ECP_NIST_OPTIM)

#define LOAD32      cur = A(i);

#if defined(MBEDTLS_HAVE_INT32)

#define MAX32       N->n
#define A(j)        N->p[j]
#define STORE32     N->p[i] = cur;

#else

#define MAX32       N->n * 2
#define A(j) (j) % 2 ? (uint32_t) (N->p[(j) / 2] >> 32) \
  : (uint32_t) (N->p[(j) / 2])
#define STORE32                                    \
  if (i % 2) {                                     \
    N->p[i / 2] &= 0x00000000FFFFFFFF;             \
    N->p[i / 2] |= ((mbedtls_mpi_uint) cur) << 32; \
  } else {                                         \
    N->p[i / 2] &= 0xFFFFFFFF00000000;             \
    N->p[i / 2] |= (mbedtls_mpi_uint) cur;         \
  }

#endif /* sizeof(mbedtls_mpi_uint) */

static inline void sli_add32(uint32_t *dst, uint32_t src, signed char *carry)
{
  *dst += src;
  *carry += (*dst < src);
}

static inline void sli_sub32(uint32_t *dst, uint32_t src, signed char *carry)
{
  *carry -= (*dst < src);
  *dst -= src;
}

#define ADD(j)    sli_add32(&cur, A(j), &c);
#define SUB(j)    sli_sub32(&cur, A(j), &c);

#define INIT(b)                                                     \
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;                  \
  signed char c = 0, cc;                                            \
  uint32_t cur;                                                     \
  size_t i = 0, bits = (b);                                         \
  /* N is the size of the product of two b-bit numbers, plus one */ \
  /* limb for fix_negative */                                       \
  MBEDTLS_MPI_CHK(mbedtls_mpi_grow(N, (b) * 2 / biL + 1));          \
  LOAD32;

#define NEXT                    \
  STORE32; i++; LOAD32;         \
  cc = c; c = 0;                \
  if (cc < 0) {                 \
    sli_sub32(&cur, -cc, &c); } \
  else {                        \
    sli_add32(&cur, cc, &c); }  \

#define LAST                                \
  STORE32; i++;                             \
  cur = c > 0 ? c : 0; STORE32;             \
  cur = 0; while (++i < MAX32) { STORE32; } \
  if (c < 0) { sli_ecp_fix_negative(N, c, bits); }

/*
 * If the result is negative, we get it in the form
 * c * 2^bits + N, with c negative and N positive shorter than 'bits'
 */
static void sli_ecp_fix_negative(mbedtls_mpi *N, signed char c, size_t bits)
{
  size_t i;

  /* Set N := 2^bits - 1 - N. We know that 0 <= N < 2^bits, so
   * set the absolute value to 0xfff...fff - N. There is no carry
   * since we're subtracting from all-bits-one. */
  for (i = 0; i <= bits / 8 / sizeof(mbedtls_mpi_uint); i++) {
    N->p[i] = ~(mbedtls_mpi_uint) 0 - N->p[i];
  }
  /* Add 1, taking care of the carry. */
  i = 0;
  do {
    ++N->p[i];
  } while (N->p[i++] == 0 && i <= bits / 8 / sizeof(mbedtls_mpi_uint));
  /* Invert the sign.
   * Now N = N0 - 2^bits where N0 is the initial value of N. */
  N->s = -1;

  /* Add |c| * 2^bits to the absolute value. Since c and N are
   * negative, this adds c * 2^bits. */
  mbedtls_mpi_uint msw = (mbedtls_mpi_uint) - c;
#if defined(MBEDTLS_HAVE_INT64)
  if (bits == 224) {
    msw <<= 32;
  }
#endif
  N->p[bits / 8 / sizeof(mbedtls_mpi_uint)] += msw;
}

/*
 * Fast quasi-reduction modulo p224 (FIPS 186-3 D.2.2)
 */
static int sli_ecp_mod_p224(mbedtls_mpi *N)
{
  INIT(224);

  SUB(7); SUB(11);               NEXT;          // A0 += -A7 - A11
  SUB(8); SUB(12);               NEXT;          // A1 += -A8 - A12
  SUB(9); SUB(13);               NEXT;          // A2 += -A9 - A13
  SUB(10); ADD(7); ADD(11);    NEXT;            // A3 += -A10 + A7 + A11
  SUB(11); ADD(8); ADD(12);    NEXT;            // A4 += -A11 + A8 + A12
  SUB(12); ADD(9); ADD(13);    NEXT;            // A5 += -A12 + A9 + A13
  SUB(13); ADD(10);               LAST;         // A6 += -A13 + A10

  cleanup:
  return ret;
}
#endif /* MBEDTLS_ECP_NIST_OPTIM */
#endif /* MBEDTLS_ECP_WITH_MPI_UINT */

/*
 * Create an MPI from embedded constants
 * (assumes len is an exact multiple of sizeof(mbedtls_mpi_uint))
 * Based on tf-psa-crypto ecp_mpi_load().
 */
static inline void sli_ecp_mpi_load(mbedtls_mpi *X, const mbedtls_mpi_uint *p, size_t len)
{
  X->s = 1;
  X->n = (unsigned short) (len / sizeof(mbedtls_mpi_uint));
  X->p = (mbedtls_mpi_uint *) p;
}

int sli_mbedtls_ecp_secp224r1_group_load(mbedtls_ecp_group *grp)
{
  if (grp == NULL) {
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
  }

  mbedtls_ecp_group_free(grp);
  mbedtls_ecp_group_init(grp);

  /* Set group id to NONE to prevent mbedtls function that depend on the
   * group id from being called on a deprecated secp224r1 group. */
  grp->id = MBEDTLS_ECP_DP_NONE;
  sli_ecp_mpi_load(&grp->P, secp224r1_p, sizeof(secp224r1_p));
  sli_ecp_mpi_load(&grp->B, secp224r1_b, sizeof(secp224r1_b));
  sli_ecp_mpi_load(&grp->N, secp224r1_n, sizeof(secp224r1_n));
  sli_ecp_mpi_load(&grp->G.X, secp224r1_gx, sizeof(secp224r1_gx));
  sli_ecp_mpi_load(&grp->G.Y, secp224r1_gy, sizeof(secp224r1_gy));
  sli_ecp_mpi_load(&grp->G.Z, mpi_one, sizeof(mpi_one));
  grp->pbits = mbedtls_mpi_bitlen(&grp->P);
  grp->nbits = mbedtls_mpi_bitlen(&grp->N);
  grp->h = 1;
#if defined(MBEDTLS_ECP_NIST_OPTIM)
  grp->modp = sli_ecp_mod_p224;
#else
  grp->modp = NULL;
#endif
#if MBEDTLS_ECP_FIXED_POINT_OPTIM == 1
  grp->T = (mbedtls_ecp_point *) secp224r1_T;
#else
  grp->T = NULL;
#endif
  // A zero size marks the read-only fixed-point table as static.
  grp->T_size = 0;

  return 0;
}

int sli_mbedtls_ecp_secp224r1_group_copy(mbedtls_ecp_group *dst,
                                         const mbedtls_ecp_group *src)
{
  if ((dst == NULL) || (src == NULL)) {
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
  }

  return sli_mbedtls_ecp_secp224r1_group_load(dst);
}
