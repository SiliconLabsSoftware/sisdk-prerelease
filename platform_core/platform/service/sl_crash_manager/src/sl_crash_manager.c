/***************************************************************************/ /**
 * @file sl_crash_manager.c
 * @brief Implementation of the Silicon Labs Crash Manager
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

#include "sl_common.h"
#include "sli_crash_manager.h"

#if defined(__ARM_ARCH) || defined(__CORTEX_M)
#include "em_device.h"
#endif

#if defined(RMU_COUNT) && (RMU_COUNT > 0)
#include "em_rmu.h"
#endif

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT) \
  || defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
#include "sl_log_helper.h"
#endif

#include <stddef.h>
#include <string.h>

/*******************************************************************************
 ***************************   GLOBAL VARIABLES   ********************************
 ******************************************************************************/

#if defined(SL_CATALOG_CRASH_MANAGER_BACKEND_RAM_PRESENT)
volatile sl_crash_manager_data_t g_crash_manager_data NAMED_SECTION(".ram_crash_data.noinit");
#else
volatile sl_crash_manager_data_t g_crash_manager_data;
#endif

/*******************************************************************************
 ***************************   LOCAL VARIABLES   ********************************
 ******************************************************************************/

static sl_crash_callback_t *p_application_callbacks = NULL;

static const sli_crash_manager_storage_ops_t *g_crash_storage_ops = NULL;
static void *g_crash_storage_ctx = NULL;

static uint32_t s_crash_seqno_session;

/** True when retained RAM holds a valid crash that should be written via @ref sl_crash_manager_persist_pending_record. */
static bool s_crash_pending_persist;

/*******************************************************************************
 ***************************   LOCAL DEFINES   **********************************
 ******************************************************************************/

/** CRC-32 reflected form (IEEE 802.3). */
#define SL_CRASH_RECORD_CRC32_POLY  (0xEDB88320u)

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/**
 * @brief Load the current crash record: valid RAM first, else storage get_log.
 *
 * Mirrors @ref sl_crash_manager_get_log selection rules and storage status mapping.
 */
static sl_status_t crash_load_current_record(sl_crash_manager_data_t *out)
{
  if (out == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  (void)memcpy(out, (const void *)&g_crash_manager_data, sizeof(*out));
  if (sli_crash_log_is_valid(out)) {
    return SL_STATUS_OK;
  }

  if (g_crash_storage_ops != NULL && g_crash_storage_ops->get_log != NULL) {
    sl_status_t storage_status = g_crash_storage_ops->get_log(g_crash_storage_ctx, out);

    if (storage_status == SL_STATUS_OK) {
      return SL_STATUS_OK;
    }
    if (storage_status == SL_STATUS_EMPTY || storage_status == SL_STATUS_NOT_FOUND
        || storage_status == SL_STATUS_INVALID_PARAMETER) {
      return SL_STATUS_EMPTY;
    }
    return storage_status;
  }
  return SL_STATUS_EMPTY;
}

/**
 * @brief Refresh @ref s_crash_pending_persist from @ref g_crash_manager_data and storage backend.
 */
static void crash_update_pending_persist_flag(void)
{
  sl_crash_manager_data_t tmp;

  (void)memcpy(&tmp, (const void *)&g_crash_manager_data, sizeof(tmp));
  s_crash_pending_persist = (g_crash_storage_ops != NULL
                             && g_crash_storage_ops->write != NULL
                             && sli_crash_log_is_valid(&tmp));
}

/**
 * @brief Reflected CRC-32 step for crash-record checksum (polynomial 0xEDB88320).
 */
static uint32_t crash_crc32_update(uint32_t crc, uint8_t data)
{
  crc ^= data;
  for (int i = 0; i < 8; i++) {
    uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
    crc = (crc >> 1) ^ (SL_CRASH_RECORD_CRC32_POLY & mask);
  }
  return crc;
}

/**
 * @brief CRC-32 over a buffer (reflected polynomial 0xEDB88320).
 */
static uint32_t crash_crc32_compute(const uint8_t *buf, size_t len)
{
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc = crash_crc32_update(crc, buf[i]);
  }
  return ~crc;
}

/**
 * @brief CRC-32 of a full record with crc32 forced to zero for the digest.
 */
static uint32_t crash_record_crc_digest(const sl_crash_manager_data_t *rec)
{
  sl_crash_manager_data_t tmp;
  (void)memcpy(&tmp, rec, sizeof(tmp));
  tmp.crc32 = 0U;
  return crash_crc32_compute((const uint8_t *)&tmp, sizeof(tmp));
}

/**
 * @brief Write CRC-32 into a crash record (digest uses crc32 cleared).
 */
static void crash_finalize_crc(sl_crash_manager_data_t *rec)
{
  sl_crash_manager_data_t crc_tmp;
  (void)memcpy(&crc_tmp, rec, sizeof(crc_tmp));
  crc_tmp.crc32 = 0U;
  rec->crc32 = crash_crc32_compute((const uint8_t *)&crc_tmp, sizeof(crc_tmp));
}

SL_WEAK const uint8_t g_firmware_build_id[SL_CRASH_BUILD_ID_BYTES] = { 0 };

SL_WEAK uint32_t platform_get_boot_count(void)
{
  return 0U;
}

SL_WEAK uint32_t platform_get_uptime_ms(void)
{
  return 0U;
}

SL_WEAK uint32_t platform_get_reset_reason(void)
{
#if defined(RMU_COUNT) && (RMU_COUNT > 0)
  return RMU_ResetCauseGet();
#else
  return 0U;
#endif
}

SL_WEAK uint32_t platform_get_device_id(void)
{
#if defined(DEVINFO)
  return DEVINFO->PART;
#else
  return 0U;
#endif
}

SL_WEAK uint32_t platform_get_image_addr(void)
{
  return 0U;
}

SL_WEAK uint32_t platform_get_next_crash_seqno(void)
{
  return ++s_crash_seqno_session;
}

/**
 * @brief Clear crash payload from cpu through crc32; set type to NONE.
 *
 * Preserves header fields before @ref sl_crash_manager_data_t::cpu (magic through build_id).
 */
static void sli_crash_record_clear_payload(void)
{
  uint8_t *base = (uint8_t *)(void *)&g_crash_manager_data;
  g_crash_manager_data.type = SL_CRASH_TYPE_NONE;
  size_t off = offsetof(sl_crash_manager_data_t, cpu);
  (void)memset(base + off, 0, sizeof(sl_crash_manager_data_t) - off);
}

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT) \
  || defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
/**
 * @brief Emit a short diagnostic for a valid log found at init (log backend present).
 *
 * @param[in] crash_type  Stored crash type
 * @param[in] pc          Program counter
 * @param[in] sp          Stack pointer
 */
static void report_stored_crash(sl_crash_type_t crash_type, uint32_t pc, uint32_t sp)
{
#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT)
  SL_PRINT_EVENT_CRASH(SL_LOG_EVENT_ID_CRASH, (uint32_t)crash_type, pc, sp);
  sl_log_flush();
#endif
#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SL_PRINT_STRING_CRASH("Stored crash at init: type=%u PC=0x%08x SP=0x%08x",
                        (unsigned int)crash_type, pc, sp);
#endif
}
#endif

bool sli_crash_log_is_valid(const sl_crash_manager_data_t *rec)
{
  if (rec->magic != SL_CRASH_LOG_MAGIC) {
    return false;
  }
  if (rec->version != SL_CRASH_LOG_VERSION) {
    return false;
  }
  if (rec->type == SL_CRASH_TYPE_NONE || rec->type >= SL_CRASH_TYPE_MAX) {
    return false;
  }
  return crash_record_crc_digest(rec) == rec->crc32;
}

void sli_crash_record_clear(volatile sl_crash_manager_data_t *record)
{
  (void)memset((void *)record, 0, sizeof(*record));
}

bool sli_crash_type_is_hardware_fault(sl_crash_type_t crash_type)
{
  switch (crash_type) {
    case SL_CRASH_TYPE_HARDFAULT:
    case SL_CRASH_TYPE_MEMFAULT:
    case SL_CRASH_TYPE_BUSFAULT:
    case SL_CRASH_TYPE_USAGEFAULT:
    case SL_CRASH_TYPE_STACKOVERFLOW:
    case SL_CRASH_TYPE_MPUFAULT:
    case SL_CRASH_TYPE_CUSTOM:
      return true;
    default:
      return false;
  }
}

void sli_crash_capture_fault_regs(volatile sl_crash_fault_regs_t *fault)
{
#if defined(SCB)
  fault->cfsr = SCB->CFSR;
  fault->hfsr = SCB->HFSR;
  fault->mmfar = SCB->MMFAR;
  fault->bfar = SCB->BFAR;
#else
  fault->cfsr = 0U;
  fault->hfsr = 0U;
  fault->mmfar = 0U;
  fault->bfar = 0U;
#endif
  fault->afsr = 0U;
#if defined(SCB)
  fault->shcsr = SCB->SHCSR;
#else
  fault->shcsr = 0U;
#endif
}

void sli_crash_fill_common_header(volatile sl_crash_manager_data_t *record)
{
  record->magic = SL_CRASH_LOG_MAGIC;
  record->version = (uint16_t)SL_CRASH_LOG_VERSION;
  record->total_size = (uint16_t)sizeof(sl_crash_manager_data_t);
  record->seqno = 0U;
  record->boot_count = platform_get_boot_count();
  record->uptime_ms = platform_get_uptime_ms();
  record->reset_reason = platform_get_reset_reason();
#if defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)
  record->os = (uint8_t)SL_CRASH_OS_FREERTOS;
#elif defined(SL_CATALOG_MICRIUMOS_KERNEL_PRESENT)
  record->os = (uint8_t)SL_CRASH_OS_MICRIUM;
#elif defined(SL_CATALOG_ZEPHYR_KERNEL_PRESENT) || defined(__ZEPHYR__)
  record->os = (uint8_t)SL_CRASH_OS_ZEPHYR;
#else
  record->os = (uint8_t)SL_CRASH_OS_NONE;
#endif
  record->_pad_header[0] = 0U;
  record->_pad_header[1] = 0U;
  record->_pad_header[2] = 0U;
  record->device_id = platform_get_device_id();
  record->image_addr = platform_get_image_addr();
  record->flags = 0U;
  (void)memcpy((void *)record->build_id,
               (const void *)g_firmware_build_id,
               sizeof(record->build_id));
  for (size_t i = 0; i < sizeof(record->build_id); i++) {
    if (record->build_id[i] != 0U) {
      record->flags |= SL_CRASH_FLAG_BUILD_ID_VALID;
      break;
    }
  }
#if SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED
  record->assert_line = 0U;
#endif
  record->watchdog_info = 0U;
  record->app_reason = 0U;
}

uint32_t sli_crash_take_next_seqno(void)
{
  return platform_get_next_crash_seqno();
}

void sli_crash_finalize_record_crc(volatile sl_crash_manager_data_t *record)
{
  crash_finalize_crc((sl_crash_manager_data_t *)(void *)record);
}

void sli_crash_fill_record_from_sl_data(const sl_crash_data_t *info)
{
  if (info == NULL) {
    return;
  }

  sli_crash_fill_common_header(&g_crash_manager_data);
  g_crash_manager_data.seqno = sli_crash_take_next_seqno();
  g_crash_manager_data.type = info->type;
  g_crash_manager_data.cpu = info->cpu;
  g_crash_manager_data.reserved = info->reserved;
  if (info->type == SL_CRASH_TYPE_WATCHDOG) {
    g_crash_manager_data.watchdog_info = info->reserved;
  }
#if SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED
  if (info->type == SL_CRASH_TYPE_ASSERT) {
    g_crash_manager_data.assert_line = info->reserved & SL_CRASH_ASSERT_LINE_MASK;
  }
#endif
  if (info->type == SL_CRASH_TYPE_SOFTWARE_ABORT) {
    g_crash_manager_data.app_reason = info->reserved;
  }
  (void)memset((void *)&g_crash_manager_data.thread, 0, sizeof(g_crash_manager_data.thread));
  if (sli_crash_type_is_hardware_fault(info->type)) {
    sli_crash_capture_fault_regs(&g_crash_manager_data.fault);
    g_crash_manager_data.flags |= (uint32_t)SL_CRASH_FLAG_CPU_FRAME_VALID
                                  | (uint32_t)SL_CRASH_FLAG_FAULT_REGS_VALID;
  } else {
    (void)memset((void *)&g_crash_manager_data.fault, 0, sizeof(g_crash_manager_data.fault));
  }
  (void)memset((void *)g_crash_manager_data.user_data, 0, sizeof(g_crash_manager_data.user_data));
#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
  /* Injection path does not populate stack_snapshot; clear stale RAM before CRC. */
  g_crash_manager_data.stack_snapshot_len = 0U;
  g_crash_manager_data.reserved16 = 0U;
  (void)memset((void *)g_crash_manager_data.stack_snapshot, 0,
               sizeof(g_crash_manager_data.stack_snapshot));
#endif
  crash_finalize_crc((sl_crash_manager_data_t *)(void *)&g_crash_manager_data);
}

/*******************************************************************************
 ***************************   GLOBAL FUNCTIONS   ******************************
 ******************************************************************************/

/**
 * @brief Install non-volatile storage backend (CTM / plugin calls before init).
 *
 * @param[in] ops  Vtable; NULL clears backend (RAM-only)
 * @param[in] ctx  Opaque context passed to ops
 */
void sli_crash_manager_register_storage(const sli_crash_manager_storage_ops_t *ops, void *ctx)
{
  g_crash_storage_ops = ops;
  g_crash_storage_ctx = ctx;
}

/**
 * @brief Initialize the Crash Manager.
 *
 * Enables MemManage, BusFault, and UsageFault in SCB. Validates retained RAM
 * (magic/version/CRC) and primes an empty RAM record when invalid. Opens the
 * storage backend when registered. Does not write the backend; use
 * @ref sl_crash_manager_persist_pending_record to persist a retained-RAM crash.
 * Records whether a valid retained-RAM crash still needs persistence when a write backend exists.
 *
 * @note If storage init fails: returns error; retained-RAM crash data is left
 *       untouched when it was valid; otherwise RAM is primed empty.
 * @note Does not erase non-volatile storage; call sl_crash_manager_clear_log()
 *       after consuming the record.
 *
 * @return SL_STATUS_OK on success, or storage backend init status on failure
 */
sl_status_t sl_crash_manager_init(void)
{
#if defined(SCB_SHCSR_MEMFAULTENA_Msk)
  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif
#if defined(SCB_SHCSR_BUSFAULTENA_Msk)
  SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
#endif

  sl_crash_manager_data_t ram_snapshot;
  (void)memcpy(&ram_snapshot, (const void *)&g_crash_manager_data, sizeof(ram_snapshot));
  bool ram_valid = sli_crash_log_is_valid(&ram_snapshot);

  if (g_crash_storage_ops != NULL) {
    if (g_crash_storage_ops->init != NULL) {
      sl_status_t storage_init_status = g_crash_storage_ops->init(g_crash_storage_ctx);
      if (storage_init_status != SL_STATUS_OK) {
        if (!ram_valid) {
          sli_crash_fill_common_header(&g_crash_manager_data);
          sli_crash_record_clear_payload();
        }
        s_crash_pending_persist = false;
        return storage_init_status;
      }
    }
  }

  {
    sl_crash_manager_data_t check;
    (void)memcpy(&check, (const void *)&g_crash_manager_data, sizeof(check));
    if (!sli_crash_log_is_valid(&check)) {
      sli_crash_fill_common_header(&g_crash_manager_data);
      sli_crash_record_clear_payload();
    }
  }

  crash_update_pending_persist_flag();

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT) \
  || defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  {
    sl_crash_manager_data_t crash_log_copy;

    if (crash_load_current_record(&crash_log_copy) == SL_STATUS_OK) {
      report_stored_crash(crash_log_copy.type, crash_log_copy.cpu.pc, crash_log_copy.cpu.sp);
    }
  }
#endif
  return SL_STATUS_OK;
}

sl_status_t sl_crash_manager_persist_pending_record(void)
{
  sl_crash_manager_data_t tmp;

  if (!s_crash_pending_persist) {
    return SL_STATUS_EMPTY;
  }
  if (g_crash_storage_ops == NULL || g_crash_storage_ops->write == NULL) {
    s_crash_pending_persist = false;
    return SL_STATUS_NOT_AVAILABLE;
  }

  (void)memcpy(&tmp, (const void *)&g_crash_manager_data, sizeof(tmp));
  if (!sli_crash_log_is_valid(&tmp)) {
    s_crash_pending_persist = false;
    return SL_STATUS_EMPTY;
  }

  {
    sl_status_t st = g_crash_storage_ops->write(
      g_crash_storage_ctx,
      (const volatile sl_crash_manager_data_t *)&g_crash_manager_data);
    if (st == SL_STATUS_OK) {
      s_crash_pending_persist = false;
    }
    return st;
  }
}

/**
 * @brief Register application callbacks for crash events.
 *
 * @param[in] application_callbacks  Array indexed by @ref sl_crash_type_t, or NULL
 */
void sl_crash_manager_register_callback(sl_crash_callback_t *application_callbacks)
{
  p_application_callbacks = application_callbacks;
}

/**
 * @brief Deregister all crash callbacks.
 */
void sl_crash_manager_deregister_callback(void)
{
  p_application_callbacks = NULL;
}

/**
 * @brief Dispatch after the crash record is filled and CRC-finalized by the caller.
 *
 * Copies record into g_crash_manager_data when record is not the global, invokes
 * callbacks, and emits optional log events. Does not re-finalize CRC: if a
 * callback changes the record, it must call sli_crash_finalize_record_crc().
 * Does not write NVM.
 *
 * @param[in] record  Filled crash log; NULL returns immediately
 */
void sl_crash_manager_handle_crash(sl_crash_manager_data_t *record)
{
  volatile sl_crash_manager_data_t *live = &g_crash_manager_data;

  if (record == NULL) {
    return;
  }

  if ((void *)record != (void *)live) {
    (void)memcpy((void *)live, (const void *)record, sizeof(*record));
  }

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SL_PRINT_STRING_CRASH("Crash manager invoked.");
#endif

  if (p_application_callbacks != NULL) {
    sl_crash_type_t crash_type_live = live->type;
    if (crash_type_live != SL_CRASH_TYPE_NONE
        && crash_type_live < SL_CRASH_TYPE_MAX) {
      sl_crash_callback_t cb = p_application_callbacks[crash_type_live];
      if (cb != NULL) {
        cb((sl_crash_manager_data_t *)live);
      }
    }
  }

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT)
  {
    uint32_t crash_type = (uint32_t)live->type;
    uint32_t pc = live->cpu.pc;
    uint32_t sp = live->cpu.sp;
    SL_PRINT_EVENT_CRASH(SL_LOG_EVENT_ID_CRASH, crash_type, pc, sp);
  }
  sl_log_flush();
#endif

  crash_update_pending_persist_flag();
}

/**
 * @brief Copy the most recent valid crash log into log_out.
 *
 * Checks RAM first, then the registered get_log backend when RAM has no valid log.
 *
 * @param[out] log_out  Destination buffer
 * @return SL_STATUS_OK if copied; SL_STATUS_EMPTY if none; SL_STATUS_INVALID_PARAMETER if NULL
 */
sl_status_t sl_crash_manager_get_log(sl_crash_manager_data_t *log_out)
{
  return crash_load_current_record(log_out);
}

bool sl_crash_manager_has_record(void)
{
  sl_crash_manager_data_t tmp;

  return crash_load_current_record(&tmp) == SL_STATUS_OK;
}

sl_status_t sl_crash_manager_get_info(sl_crash_record_info_t *info_out)
{
  sl_crash_manager_data_t rec;

  if (info_out == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  (void)memset(info_out, 0, sizeof(*info_out));

  {
    sl_status_t st = crash_load_current_record(&rec);
    if (st != SL_STATUS_OK) {
      return st;
    }
  }

  info_out->valid = SL_CRASH_RECORD_INFO_VALID;
  info_out->version = rec.version;
  info_out->crash_type = (uint16_t)rec.type;
  /* CRC-validated field; matches serialized layout size (see sl_crash_manager_data_t::total_size). */
  info_out->total_size = (uint32_t)rec.total_size;
  info_out->seqno = rec.seqno;
  info_out->crc32 = rec.crc32;
  info_out->reset_reason = rec.reset_reason;
  info_out->uptime_ms = rec.uptime_ms;
  (void)memcpy(info_out->build_id, rec.build_id, SL_CRASH_BUILD_ID_BYTES);
  return SL_STATUS_OK;
}

sl_status_t sl_crash_manager_read(uint32_t offset,
                                  uint8_t *buffer,
                                  uint32_t requested_len,
                                  uint32_t *returned_len)
{
  const uint32_t rec_size = (uint32_t)sizeof(sl_crash_manager_data_t);
  sl_crash_manager_data_t rec;
  sl_status_t load_status;

  if (returned_len == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  *returned_len = 0U;

  if (requested_len > 0U && buffer == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  load_status = crash_load_current_record(&rec);
  if (load_status != SL_STATUS_OK) {
    return load_status;
  }

  if (offset >= rec_size) {
    return SL_STATUS_INVALID_RANGE;
  }

  {
    uint32_t available = rec_size - offset;
    uint32_t n = (requested_len < available) ? requested_len : available;
    if (n > 0U) {
      (void)memcpy(buffer, (const uint8_t *)(void *)&rec + offset, n);
    }
    *returned_len = n;
  }
  return SL_STATUS_OK;
}

/**
 * @brief Erase persisted crash data and clear the RAM mirror to an empty log.
 *
 * @return SL_STATUS_OK on success, or storage erase status on failure
 */
sl_status_t sl_crash_manager_clear_log(void)
{
  sl_status_t erase_status = SL_STATUS_OK;

  if (g_crash_storage_ops != NULL && g_crash_storage_ops->erase != NULL) {
    erase_status = g_crash_storage_ops->erase(g_crash_storage_ctx);
  }
  sli_crash_fill_common_header(&g_crash_manager_data);
  sli_crash_record_clear_payload();
  s_crash_pending_persist = false;
  return erase_status;
}
