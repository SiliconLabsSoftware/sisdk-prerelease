/*
 * Copyright (c) 2019-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
/**
 * \file psa/crypto_compat.h
 *
 * \brief PSA cryptography module: Backward compatibility aliases
 *
 * This header declares alternative names for macro and functions.
 * New application code should not use these names.
 * These names may be removed in a future version of Mbed Crypto.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 */

#if defined(SL_TRUSTZONE_SECURE)

/* The Secure library must use the crypto_compat.h from the mbedtls repo. */
#include <include/psa/crypto_compat.h>

#else /* SL_TRUSTZONE_SECURE */

#ifndef PSA_CRYPTO_COMPAT_H
#define PSA_CRYPTO_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* PSA_CRYPTO_COMPAT_H */

#endif /* SL_TRUSTZONE_SECURE */
