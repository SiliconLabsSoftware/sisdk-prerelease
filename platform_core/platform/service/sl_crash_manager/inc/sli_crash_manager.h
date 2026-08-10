/***************************************************************************/ /**
* @file sli_crash_manager.h
* @brief Internal Silicon Labs Crash Manager API
* @version 1.0.0
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

#ifndef SLI_CRASH_MANAGER_H
#define SLI_CRASH_MANAGER_H

#include "sl_crash_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check whether a crash record looks structurally valid.
 *
 * Verifies magic, layout version, crash type range, and CRC-32 over the record.
 *
 * @param[in] rec  Crash record to inspect; must not be NULL.
 *
 * @return True if @p rec passes magic, version, type, and CRC checks; false otherwise.
 */
bool sli_crash_log_is_valid(const sl_crash_manager_data_t *rec);

/**
 * @brief Zero entire crash record buffer.
 *
 * Clears all bytes (e.g. fault path before capture or synthetic injection).
 *
 * @param[in,out] record  Crash record to clear; must not be NULL.
 */
void sli_crash_record_clear(volatile sl_crash_manager_data_t *record);

/**
 * @brief Fill standard header fields on a crash record.
 *
 * Sets magic, layout version, reset reason, OS hint, device id, and related
 * metadata used across backends.
 *
 * @param[in,out] record  Record whose header to fill; must not be NULL.
 */
void sli_crash_fill_common_header(volatile sl_crash_manager_data_t *record);

/**
 * @brief Next monotonic sequence number for this boot session.
 *
 * Used when allocating @ref sl_crash_manager_data_t::seqno for a new record.
 *
 * @return Session sequence value for the next crash.
 */
uint32_t sli_crash_take_next_seqno(void);

/**
 * @brief Whether this crash type should capture live SCB fault registers.
 *
 * Hardware fault classes return true; software-only paths return false.
 *
 * @param[in] crash_type  Crash category (@ref sl_crash_type_t).
 *
 * @return True if fault registers should be sampled into @ref sl_crash_fault_regs_t.
 */
bool sli_crash_type_is_hardware_fault(sl_crash_type_t crash_type);

/**
 * @brief Snapshot Cortex-M SCB fault status registers.
 *
 * Copies CFSR, HFSR, MMFAR, BFAR (and related fields where present).
 *
 * @param[out] fault  Destination fault snapshot; must not be NULL.
 */
void sli_crash_capture_fault_regs(volatile sl_crash_fault_regs_t *fault);

/**
 * @brief Compute and store CRC-32 over the crash record.
 *
 * Digest excludes the stored @ref sl_crash_manager_data_t::crc32 field (cleared for the run).
 *
 * @param[in,out] record  Record to finalize; must not be NULL.
 */
void sli_crash_finalize_record_crc(volatile sl_crash_manager_data_t *record);

/**
 * @brief Build full crash record from compact sl_crash_data_t (tests and app injection).
 *
 * Writes @ref g_crash_manager_data, then finalizes CRC. Call
 * @ref sl_crash_manager_handle_crash afterward for callbacks. If a callback
 * changes the record, it must call @ref sli_crash_finalize_record_crc on
 * @ref g_crash_manager_data.
 *
 * @param[in] info  Compact crash description; must not be NULL.
 */
void sli_crash_fill_record_from_sl_data(const sl_crash_data_t *info);

/**
 * @brief Non-volatile crash storage backend (runtime-selected vtable).
 *
 * All function pointers may be NULL except as noted. `ctx` is passed to each
 * call and is chosen at registration (often NULL). Backends should
 * supply init, write, erase, and get_log together for NVM3-style persistence.
 */
typedef struct sli_crash_manager_storage_ops {
  /** Open storage; required before write/get/erase. */
  sl_status_t (*init)(void *ctx);
  /** Persist full crash record (task context; @ref sl_crash_manager_persist_pending_record, not fault/ISR path). */
  sl_status_t (*write)(void *ctx, const volatile sl_crash_manager_data_t *data);
  /** Remove persisted record. */
  sl_status_t (*erase)(void *ctx);
  /** Load valid full record into data. */
  sl_status_t (*get_log)(void *ctx, sl_crash_manager_data_t *data);
} sli_crash_manager_storage_ops_t;

/**
 * @brief Install or replace the crash storage backend.
 *
 * Typically called once before @ref sl_crash_manager_init. Passing NULL for
 * @p ops clears registration (RAM-only operation).
 *
 * @param[in] ops  Storage vtable, or NULL to unregister.
 * @param[in] ctx  Opaque context passed to every backend hook (may be NULL).
 */
void sli_crash_manager_register_storage(const sli_crash_manager_storage_ops_t *ops, void *ctx);

#if defined(SL_CATALOG_CRASH_MANAGER_BACKEND_CTM_PRESENT)
/**
 * @brief Register the CTM dynamic-token storage backend.
 *
 * Invoked from service_init before @ref sl_crash_manager_init when the CTM
 * backend component is present.
 */
void sli_crash_manager_ctm_plugin_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SLI_CRASH_MANAGER_H */
