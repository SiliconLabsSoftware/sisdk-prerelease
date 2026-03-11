/***************************************************************************//**
 * @file
 * @brief Provides abstraction layer, utility and wrapper functions sli crypto accelerators.
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
#include "em_device.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
// Must be before sli_crypto.h so SLI_MBEDTLS_DEVICE_VSE_V2 is defined for SLI_AES_COUNTERMEASURES_PRESENT
#if defined(_SILICON_LABS_32B_SERIES_2) && defined(CRYPTOACC_PRESENT) && !defined(SL_TRUSTZONE_NONSECURE)
#include "sli_mbedtls_omnipresent.h"
#endif
#include "sli_crypto.h"
#if SLI_AES_COUNTERMEASURES_PRESENT && (!defined(SL_TRUSTZONE_NONSECURE))
// required for SL_CRYPTO_USE_HOST_ENTROPY and SL_USE_CM_RESEED
#include "psa_crypto_config.h"
#endif
#include "sl_assert.h"

// Locks are only needed when using RTOS and at least one of:
// - Host entropy pool
// - CM counters
#if (defined(SL_CATALOG_MICRIUMOS_KERNEL_PRESENT) || defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)) \
    && (SLI_CRYPTO_USE_HOST_ENTROPY || SLI_CM_COUNTERS_ENABLED)
#include "sli_psec_osal.h"
// Lock mutex for synchronizing multiple threads calling into the sl_crypto API.
static sli_psec_osal_lock_t sli_crypto_lock = { 0 };

/***************************************************************************//**
 * @brief                Initialize the crypto subsystem lock.
 ******************************************************************************/
sl_status_t sli_crypto_init_lock(void)
{
  return sli_psec_osal_init_lock(&sli_crypto_lock);
}

/**
 * @brief
 *   Acquire the crypto lock for exclusive access if necessary (thread mode).
 */
static inline sl_status_t sli_crypto_lock_acquire(void)
{
  sl_status_t sl_status = SL_STATUS_OK;
  // Acquire the crypto lock (mutex) to protect crypto operations
  sl_status = sli_psec_osal_take_lock(&sli_crypto_lock);
  if (sl_status != SL_STATUS_OK) {
    return sl_status;
  }
  return sl_status;
}

/**
 * @brief
 *   Release the crypto lock if necessary (thread mode).
 */
static inline sl_status_t sli_crypto_lock_release(void)
{
  sl_status_t sl_status = SL_STATUS_OK;
  // Release the crypto lock (mutex)
  sl_status = sli_psec_osal_give_lock(&sli_crypto_lock);
  if (sl_status != SL_STATUS_OK) {
    return sl_status;
  }
  return sl_status;
}

#else 
#define sli_crypto_lock_acquire() (SL_STATUS_OK)
#define sli_crypto_lock_release() (SL_STATUS_OK)
#endif

#if (!defined(SL_TRUSTZONE_NONSECURE))
#if (SLI_AES_COUNTERMEASURES_PRESENT || SLI_CRYPTO_USE_HOST_ENTROPY)
extern sl_status_t sli_crypto_trng_get(uint8_t *dest, size_t nbytes);
#endif
#if SLI_AES_COUNTERMEASURES_PRESENT
/***************************************************************************//**
 * @brief                Hardware-specific access to AES countermeasure reseed register
 *                       don't call this directly - use sli_crypto_countermeasure_reseed
 *                       as it synchronizes the aes reseed counter.
 * @param engine         The crypto engine to reseed
 * @param seed           Pointer to the entropy to use for reseeding.
 ******************************************************************************/
extern sl_status_t sli_crypto_engine_cm_reseed(sli_crypto_engine_t engine, sli_crypto_seed_t *seed);
#endif // SLI_AES_COUNTERMEASURES_PRESENT

#if defined(_SILICON_LABS_32B_SERIES_3) && (SLI_CM_COUNTERS_ENABLED)
// AES opcounters
static volatile uint32_t _aes_op_count[SLI_CRYPTO_ENGINE_COUNT] = { 0 };
#define ENGINE_IDX(e)  (((e) == SLI_CRYPTO_HOSTSYMCRYPTO) ? SLI_CRYPTO_HOSTSYMCRYPTO_IDX \
                        : ((e) == SLI_CRYPTO_LPWAES) ? SLI_CRYPTO_LPWAES_IDX             \
                        : SLI_CRYPTO_INVALID_ENGINE_IDX)
#elif defined(CRYPTOACC_PRESENT) && (SLI_CM_COUNTERS_ENABLED)
static volatile uint32_t _aes_op_count[SLI_CRYPTO_ENGINE_COUNT] = { 0 };
#define ENGINE_IDX(e) (((e) == SLI_CRYPTO_ENGINE_CRYPTOACC) ? SLI_CRYPTO_CRYPTOACC_IDX \
                       : SLI_CRYPTO_INVALID_ENGINE_IDX)
#else // Series 2 block
#define ENGINE_IDX(e) (SLI_CRYPTO_INVALID_ENGINE_IDX)
#endif // unsupported

#if (SLI_CRYPTO_USE_HOST_ENTROPY)
#if (CRYPTO_CM_HOST_ENTROPY_POOL_BYTES > UINT16_MAX)
  #error "CRYPTO_CM_HOST_ENTROPY_POOL_BYTES is too large for uint16_t, update sl_crypto.c:sli_crypto_entropy_pool_t"
#endif

// A struct that defines a circular buffer of entropy from the TRNG
typedef struct {
  ///< Pointer to the buffer holding buffered entropy
  volatile uint8_t *const base;   ///< Pointer to the base of the buffer
  volatile uint16_t head;         ///< Index of the head of the buffer
  volatile uint16_t tail;         ///< Index of the tail of the buffer
  volatile uint16_t count;        ///< Number of bytes in the buffer
} sli_crypto_entropy_pool_t;

// internal buffer to hold entropy, may be updated by bg thread or ISR, set volatile
volatile uint8_t _entropy_buffer_base[CRYPTO_CM_HOST_ENTROPY_POOL_BYTES] = { 0 };

// The sli_crypto host entropy pool
static sli_crypto_entropy_pool_t entropy_pool = {
  .base = (volatile uint8_t *)&_entropy_buffer_base[0],
  .head = 0,
  .tail = 0,
  // At head=tail, count=0 pool is 'empty'
  .count = 0,
};

uint16_t sli_crypto_entropy_pool_bytes_remaining(void)
{
  // Direct read of volatile uint16_t is atomic on ARM Cortex-M.
  // No need to acquire lock.
  return entropy_pool.count;
}

#if defined(UNITY_TEST)
void host_entropy_pool_flush(void)
{
  // Reset the entropy pool
  memset((void *)entropy_pool.base, 0, CRYPTO_CM_HOST_ENTROPY_POOL_BYTES);
  entropy_pool.head = entropy_pool.tail = entropy_pool.count = 0;
}
#endif

sl_status_t sli_crypto_entropy_pool_accumulate(void)
{
  sl_status_t rc = sli_crypto_lock_acquire();
  if (rc != SL_STATUS_OK) {
    return rc;
  }

  uint16_t head = entropy_pool.head;
  uint16_t tail = entropy_pool.tail;
  uint16_t count = entropy_pool.count;
  // Detect overflow caused by glitched or invalid pool count...
  EFM_ASSERT(head < CRYPTO_CM_HOST_ENTROPY_POOL_BYTES);
  EFM_ASSERT(tail < CRYPTO_CM_HOST_ENTROPY_POOL_BYTES);
  EFM_ASSERT(count <= CRYPTO_CM_HOST_ENTROPY_POOL_BYTES);
  uint16_t bytes_to_fill = CRYPTO_CM_HOST_ENTROPY_POOL_BYTES - count;
  if (bytes_to_fill) {
    // If head is ahead of tail... | _ _ t - - - h _ |
    // Reduce se manager calls by shifting tail back then fill from head to end
    if (count == 0) {
      // Empty pool, reset head and tail
      head = 0;
      tail = 0;
    } else if (tail != 0 && (tail < head)) {
      memmove((void*)entropy_pool.base, (void*)&entropy_pool.base[tail], count);
      tail = 0;
      head = (tail + count) % CRYPTO_CM_HOST_ENTROPY_POOL_BYTES;
    }

    // | t - - - h _ _ _ | or | - h _ _ _ t - - | or count == 0
    rc = sli_crypto_trng_get((uint8_t*)&entropy_pool.base[head], bytes_to_fill);
    if (rc == SL_STATUS_OK) {
      entropy_pool.count = count + bytes_to_fill;
      entropy_pool.head = (head + bytes_to_fill) % CRYPTO_CM_HOST_ENTROPY_POOL_BYTES;
      entropy_pool.tail = tail;
    }
  } else {
    rc = SL_STATUS_OK;
  }

  sl_status_t release_rc = sli_crypto_lock_release();
  if (release_rc != SL_STATUS_OK) {
    return release_rc;
  }

  return rc;
}

sl_status_t sli_crypto_entropy_pool_consume(uint8_t *out, size_t *nbytes)
{
  if ((out == NULL) || (nbytes == NULL)) {
    if (nbytes) {
      *nbytes = 0;
    }
    return SL_STATUS_NULL_POINTER;
  }

  if ((*nbytes == 0) || (*nbytes > CRYPTO_CM_HOST_ENTROPY_POOL_BYTES)) {
    *nbytes = 0;
    return SL_STATUS_INVALID_PARAMETER;
  }

  sl_status_t rc = sli_crypto_lock_acquire();
  if (rc != SL_STATUS_OK) {
    return rc;
  }

  uint16_t head = entropy_pool.head;
  uint16_t tail = entropy_pool.tail;
  uint16_t count = entropy_pool.count;

  if (count == 0) {
    *nbytes = 0;
    sl_status_t release_rc = sli_crypto_lock_release();

    if (release_rc == SL_STATUS_OK) {
      return SL_STATUS_NO_MORE_RESOURCE;
    } else {
      return release_rc;
    }
  }

  // Prevent overflow caused by glitched or invalid pool count...
  EFM_ASSERT(head < CRYPTO_CM_HOST_ENTROPY_POOL_BYTES);
  EFM_ASSERT(tail < CRYPTO_CM_HOST_ENTROPY_POOL_BYTES);
  EFM_ASSERT(count <= CRYPTO_CM_HOST_ENTROPY_POOL_BYTES);

  uint16_t bytes_to_read = (*nbytes < count)
                           ? *nbytes : count;

  uint16_t bytes_to_end = CRYPTO_CM_HOST_ENTROPY_POOL_BYTES - ((head < tail)
                                                               ? tail : head);

  uint16_t bytes_read = 0;
  if (head <= tail) {
    // Tail is ahead of head or equal to head and count is nonzero... | - - h _ _ _ t - - |
    if (bytes_to_read > bytes_to_end) {     // wrap...
      memcpy(out, (void*)&entropy_pool.base[tail], bytes_to_end);
      tail = 0;
      bytes_read += bytes_to_end;
    }
    // If there was remainder after wrap or if there's no wrap read from tail
    if (bytes_read != bytes_to_read) {
      memcpy(out + bytes_read, (void*)&entropy_pool.base[tail], bytes_to_read - bytes_read);
      tail = (tail + bytes_to_read - bytes_read) % CRYPTO_CM_HOST_ENTROPY_POOL_BYTES;
      bytes_read = bytes_to_read;
    }
  } else { // if (tail < head)...
    // | _ _ t - - h _ _ _ | simple case: read from tail to head
    memcpy(out, (void*)&entropy_pool.base[tail], bytes_to_read);
    tail = (tail + bytes_to_read) % CRYPTO_CM_HOST_ENTROPY_POOL_BYTES;
    bytes_read = bytes_to_read;
  }

  entropy_pool.count = count - bytes_read;
  entropy_pool.tail = tail;
  entropy_pool.head = head;

  rc = sli_crypto_lock_release();

  if (rc != SL_STATUS_OK) {
    *nbytes = bytes_read;
    return rc;
  }

  if (*nbytes == bytes_read) {
    return SL_STATUS_OK;
  } else {
    *nbytes = bytes_read;   // report actual bytes consumed to user
    return SL_STATUS_NO_MORE_RESOURCE;
  }
}
#endif // (SLI_CRYPTO_USE_HOST_ENTROPY)

#if SLI_AES_COUNTERMEASURES_PRESENT && (!defined(SL_TRUSTZONE_NONSECURE))
sl_status_t sli_crypto_countermeasure_reseed(sli_crypto_engine_t engine, sli_crypto_seed_t *opt_seed)
{
  sli_engine_id_t e = ENGINE_IDX(engine);
  sli_crypto_seed_t seed = (opt_seed == NULL) ? (sli_crypto_seed_t){ 0 } : *opt_seed;
  if (e == SLI_CRYPTO_INVALID_ENGINE_IDX) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  sl_status_t rc = SL_STATUS_FAIL;

  if (opt_seed == NULL) {
    // track the number of bytes consumed from host entropy pool
    size_t bytes_consumed = 0;

    // If the host entropy pool is enabled, try to use it first
    #if (SLI_CRYPTO_USE_HOST_ENTROPY)
    // If the host entropy pool is enabled and has entropy ready, use it
    uint16_t old_count = entropy_pool.count;
    if (old_count > 0) {
      if (old_count < sizeof(seed.u8)) {
        // If we don't have enough entropy, but we have some, try to use it
        bytes_consumed = old_count;
      } else {
        bytes_consumed = sizeof(seed.u8);
      }

      rc = sli_crypto_entropy_pool_consume(seed.u8, &bytes_consumed);
      if ((rc != SL_STATUS_OK) && (rc != SL_STATUS_NO_MORE_RESOURCE)) {
        // If the entropy pool read failed,
        // we can still obtain the entire seed from TRNG
        bytes_consumed = 0;
      }
    }
    #endif

    // In the event there was no host entropy pool, or an incomplete read,
    // get the remaining bytes between bytes_consumed from pool and size of the seed
    if (bytes_consumed < sizeof(seed.u8)) {
      // If we didn't get enough bytes from the pool, get the rest from TRNG
      rc = sli_crypto_trng_get(&seed.u8[bytes_consumed], sizeof(seed.u8) - bytes_consumed);
    }

    if (rc != SL_STATUS_OK) {
      return rc;
    }
  } 
  
  rc = sli_crypto_engine_cm_reseed(engine, &seed);

  // Zeroize seed material immediately after use - seed contains TRNG/entropy
  // data that must not remain on the stack
  memset(&seed, 0, sizeof(seed));

  #if (SLI_CM_COUNTERS_ENABLED)
  if (rc == SL_STATUS_OK) {
      // assuming another thread interrupts right after reseed
      // there may be a slight loss of opcounts, however this should be 
      // negligible: the count shouldn't be near the 2^30 threshold unless
      // the interrupting thread is somehow performing a streaming AES operation
      // on a memory space larger than our entire flash size.
      // Added complexity of lock does not seem worth it here. 
      _aes_op_count[e] = 0;
  }
  #endif

  return rc;
}
#if (SLI_CM_COUNTERS_ENABLED)
uint32_t sli_crypto_get_engine_aes_op_count(sli_crypto_engine_t engine)
{
  sli_engine_id_t e = ENGINE_IDX(engine);
  if (e == SLI_CRYPTO_INVALID_ENGINE_IDX) {
    return (uint32_t)(-1);
  }

  return _aes_op_count[e];
}

uint32_t sli_crypto_inc_engine_aes_op_count(sli_crypto_engine_t engine, uint32_t n_ops)
{
  sli_engine_id_t e = ENGINE_IDX(engine);
  if (e == SLI_CRYPTO_INVALID_ENGINE_IDX) {
    return (uint32_t)(-1);
  }

  sl_status_t rc = sli_crypto_lock_acquire();
  if (rc != SL_STATUS_OK) {
    // In the current sli_psec_osal_take_lock, the lack of _timeout
    // implies we expect this API to spin forever waiting on lock.
    // In the event of an error here, the OS reports the lock could 
    // not be acquired, or is the mutex has become corrupted. 
    // In these instances, defensively assume we may have missed reseed 
    // interval due to tampering or another critical RTOS error.
    // Trust nothing and assume a reseed is immediately required.
    return (uint32_t)(-1);
  }

  uint32_t old_count = _aes_op_count[e];
  // defensively assume overflow would occur / reseed rquired in case of glitch
  uint32_t new_count = ((uint32_t)(-1)) - 1; 
  if (old_count + n_ops < old_count) {
    _aes_op_count[e] = new_count;
  } else {
    _aes_op_count[e] = new_count = old_count + n_ops;
  }

  rc = sli_crypto_lock_release();
  if (rc != SL_STATUS_OK) {
    return (uint32_t)(-1);
  }

  return new_count;
}

uint32_t sli_crypto_cm_get_opcount(sli_cm_aes_mode_t mode, size_t nbytes)
{
  // Calculate number of 16-byte AES blocks
  uint32_t blocks = (nbytes + 15) / 16;

  switch (mode) {
    case SLI_CM_AES_MODE_BLOCK:
      // ECB, CBC, CTR, CFB, OFB: 1 block per 16 bytes, minimum 1
      return blocks ? blocks : 1;

    case SLI_CM_AES_MODE_GCM:
      // GCM: data blocks + 1 for tag computation
      // (no minimum on data blocks - 0 bytes of plaintext = 1 op for tag only)
      return blocks + 1;

    case SLI_CM_AES_MODE_CCM:
      // CCM: data blocks + 1 for tag
      // Note: AAD blocks must be added separately by caller
      // (no minimum on data blocks)
      return blocks + 1;

    case SLI_CM_AES_MODE_CMAC:
      // CMAC: data blocks + 1 for subkey generation
      // (minimum 1 data block)
      return (blocks ? blocks : 1) + 1;

    default:
      // Default: minimum 1 block
      return blocks ? blocks : 1;
  }
}

sl_status_t sli_crypto_cm_check_threshold(sli_crypto_engine_t engine,
                                          uint32_t n_ops,
                                          bool reseed)
{
  sli_engine_id_t e = ENGINE_IDX(engine);
  if (e == SLI_CRYPTO_INVALID_ENGINE_IDX) {
    // Unknown engine - can't track, return unsupported
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint32_t current = _aes_op_count[e];
  uint32_t projected = current + n_ops;

  // Check for overflow
  if (projected < current) {
    projected = SLI_CRYPTO_CM_RESEED_THRESH_MAX;
  }

  // Check security threshold (2^30 - 1)
  if (projected >= SLI_CRYPTO_CM_RESEED_THRESH_MAX) {
    return SL_STATUS_SECURITY_AES_CM_FAIL;
  }

  // Check reseed threshold
  if (projected >= SLI_CRYPTO_CM_RESEED_THRESH) {
    if (reseed) {
      // Attempt reseed
      sl_status_t reseed_status = sli_crypto_countermeasure_reseed(engine, NULL);
      if (reseed_status == SL_STATUS_OK) {
        // Reseed succeeded, counter reset to 0, threshold no longer crossed
        return SL_STATUS_OK;
      }
    }
    // Reseed not attempted or failed
    return SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED;
  }

  return SL_STATUS_OK;
}
#endif // (SLI_CM_COUNTERS_ENABLED)
#endif // SLI_AES_COUNTERMEASURES_PRESENT && (!defined(SL_TRUSTZONE_NONSECURE))
#endif // (!defined(SL_TRUSTZONE_NONSECURE))