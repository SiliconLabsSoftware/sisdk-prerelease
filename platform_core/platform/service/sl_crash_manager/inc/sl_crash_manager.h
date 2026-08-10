/***************************************************************************/ /**
 * @file sl_crash_manager.h
 * @brief Silicon Labs Crash Manager API
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

#ifndef SL_CRASH_MANAGER_H
#define SL_CRASH_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 ***************************   INCLUDES   **************************************
 ******************************************************************************/

#include "sl_enum.h"
#include "sl_status.h"
#include "sl_crash_manager_config.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Thread name field in @ref sl_crash_thread_t. */
#define SL_CRASH_THREAD_NAME_MAX      16U

/** @brief Words in @ref sl_crash_manager_data_t::user_data. */
#define SL_CRASH_USER_DATA_WORDS      4U

/** @brief Bytes in @ref sl_crash_manager_data_t::build_id (raw firmware build id / digest). */
#define SL_CRASH_BUILD_ID_BYTES       16U

/** @brief Stacked CPU frame captured (@ref sl_crash_manager_data_t::flags). */
#define SL_CRASH_FLAG_CPU_FRAME_VALID       (1u << 0)
/** @brief SCB fault registers captured (@ref sl_crash_manager_data_t::flags). */
#define SL_CRASH_FLAG_FAULT_REGS_VALID      (1u << 1)
/** @brief Build id meaningful (@ref sl_crash_manager_data_t::flags, @ref sl_crash_manager_data_t::build_id). */
#define SL_CRASH_FLAG_BUILD_ID_VALID        (1u << 2)
/** @brief Stack snapshot present (@ref sl_crash_manager_data_t::flags when @ref SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED). */
#define SL_CRASH_FLAG_STACK_SNAPSHOT_VALID  (1u << 3)

/*******************************************************************************
 ***************************   TYPE DEFINITIONS   ******************************
 ******************************************************************************/

/** @addtogroup crash_manager Crash Manager
 * @brief Crash capture, optional logging, storage, and crash log retrieval
 *
 * Captures and stores crash events from Cortex-M fault handlers, watchdog
 * warnings, and asserts. Supports RAM and CTM (NVM3) backends and per-type
 * callbacks. Crash-time logging requires a log backend (e.g. proprietary or
 * SystemView).
 *
 * Key features:
 * - MemManage, HardFault, BusFault, UsageFault handlers
 * - Stack overflow and MPU fault refinement from CFSR
 * - Watchdog warning and EFM_ASSERT integration
 * - Optional non-volatile backend; explicit @ref sl_crash_manager_persist_pending_record after boot or capture
 * - Application callbacks per crash type
 * - Optional platform hooks: boot count, uptime, image base, build id; @ref SL_CRASH_FLAG_* bits
 * - Stack snapshot fields in the crash record only when @ref SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
 *
 * @{
 */

/**
 * @defgroup sl_crash_manager_types Types
 * @{
 */

/** @brief Crash category; callback index; STACKOVERFLOW/MPUFAULT refined from CFSR. */
SL_ENUM(sl_crash_type_t) {
  SL_CRASH_TYPE_NONE,            /**< Unused */
  SL_CRASH_TYPE_HARDFAULT,       /**< HardFault */
  SL_CRASH_TYPE_MEMFAULT,        /**< MemManage */
  SL_CRASH_TYPE_BUSFAULT,        /**< BusFault */
  SL_CRASH_TYPE_USAGEFAULT,      /**< UsageFault */
  SL_CRASH_TYPE_WATCHDOG,        /**< WDOG warning */
  SL_CRASH_TYPE_ASSERT,          /**< Assert */
  SL_CRASH_TYPE_STACKOVERFLOW,   /**< Refined Mem/Bus fault */
  SL_CRASH_TYPE_MPUFAULT,        /**< Refined MemManage */
  SL_CRASH_TYPE_CUSTOM,          /**< App-defined */
  SL_CRASH_TYPE_SOFTWARE_ABORT,  /**< Deliberate fatal path (abort(), app_fatal, etc.) */
  SL_CRASH_TYPE_MAX              /**< Exclusive upper bound; not a valid crash type */
};

/** @brief Legacy names for @ref sl_crash_type_t values. */
#define SL_CRASH_CAUSE_HARDFAULT      SL_CRASH_TYPE_HARDFAULT
#define SL_CRASH_CAUSE_MEMFAULT       SL_CRASH_TYPE_MEMFAULT
#define SL_CRASH_CAUSE_BUSFAULT       SL_CRASH_TYPE_BUSFAULT
#define SL_CRASH_CAUSE_USAGEFAULT     SL_CRASH_TYPE_USAGEFAULT
#define SL_CRASH_CAUSE_WDOG           SL_CRASH_TYPE_WATCHDOG
#define SL_CRASH_CAUSE_ASSERT         SL_CRASH_TYPE_ASSERT
#define SL_CRASH_CAUSE_STACKOVERFLOW  SL_CRASH_TYPE_STACKOVERFLOW
#define SL_CRASH_CAUSE_MPUFAULT       SL_CRASH_TYPE_MPUFAULT
#define SL_CRASH_CAUSE_CUSTOM         SL_CRASH_TYPE_CUSTOM
#define SL_CRASH_CAUSE_SOFTWARE_ABORT SL_CRASH_TYPE_SOFTWARE_ABORT

/** @brief Valid log magic ("SLCM"). */
#define SL_CRASH_LOG_MAGIC            0x534C434DU

/** @brief @ref sl_crash_manager_data_t layout; increment if struct changes. */
#define SL_CRASH_LOG_VERSION          1U

/** @brief RTOS hint from build (catalog). */
typedef enum {
  SL_CRASH_OS_NONE = 0,
  SL_CRASH_OS_FREERTOS,
  SL_CRASH_OS_MICRIUM,
  SL_CRASH_OS_ZEPHYR,
} sl_crash_os_t;

/**
 * @brief ARM Cortex-M exception/stacked frame (fault path) + metadata.
 * Assert/watchdog paths may only populate pc, lr, sp; other fields zero.
 */
typedef struct {
  uint32_t r0;         /**< R0 */
  uint32_t r1;         /**< R1 */
  uint32_t r2;         /**< R2 */
  uint32_t r3;         /**< R3 */
  uint32_t r12;        /**< R12 */
  uint32_t lr;         /**< Link register from stacked frame */
  uint32_t pc;         /**< Program counter from stacked frame */
  uint32_t xpsr;       /**< xPSR from stacked frame */
  uint32_t sp;         /**< Stack pointer (exception frame / context) */
} sl_crash_cpu_frame_arm_t;

/** @brief ARMv7-M / Cortex-M fault status snapshot (SCB). */
typedef struct {
  uint32_t cfsr;   /**< CFSR */
  uint32_t hfsr;   /**< HFSR */
  uint32_t mmfar;  /**< MMFAR */
  uint32_t bfar;   /**< BFAR */
  uint32_t afsr;   /**< AFSR (0 if not present on device) */
  uint32_t shcsr;  /**< SHCSR */
} sl_crash_fault_regs_t;

/** @brief RTOS/thread context placeholder (zeroed until RTOS integration). */
typedef struct {
  uint32_t thread_ptr;                      /**< Thread handle / control block */
  uint32_t stack_start;                     /**< Stack base */
  uint32_t stack_end;                       /**< Stack limit */
  char     name[SL_CRASH_THREAD_NAME_MAX];  /**< Optional thread name */
} sl_crash_thread_t;

#if SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED
/** @brief Low 16 bits of @ref sl_crash_data_t::reserved used as assert line when @ref sl_crash_type_t is @ref SL_CRASH_TYPE_ASSERT. */
#define SL_CRASH_ASSERT_LINE_MASK  (0xFFFFu)
#endif

/** @brief Compact crash input for @ref sli_crash_fill_record_from_sl_data. @note reserved: wdog mask; assert line (low 16b, assert handler only); software-abort reason (copied to app_reason). */
typedef struct {
  sl_crash_type_t      type;     /**< Crash type */
  sl_crash_cpu_frame_arm_t cpu;      /**< Stacked frame (faults); else pc/lr/sp only */
  uint32_t             reserved; /**< Type-specific */
} sl_crash_data_t;

/** @brief Persisted crash log (header, context, snapshot, CRC). */
typedef struct {
  uint32_t              magic;       /**< Magic */
  uint16_t              version;     /**< Layout version */
  uint16_t              total_size;  /**< sizeof record */
  uint32_t              seqno;       /**< Session index */
  uint32_t              boot_count;   /**< Monotonic boot counter (platform hook; else 0) */
  uint32_t              uptime_ms;   /**< Uptime at crash (platform hook; else 0) */
  uint32_t              reset_reason;/**< Reset cause */
  sl_crash_type_t       type;        /**< Crash type */
  uint8_t               os;          /**< OS id */
  uint8_t               _pad_header[3]; /**< Pad */
  uint32_t              device_id;   /**< PART / device */
  uint32_t              image_addr;  /**< Application image base (platform hook; else 0) */
  uint32_t              flags;       /**< @ref SL_CRASH_FLAG_* */
  uint8_t               build_id[SL_CRASH_BUILD_ID_BYTES]; /**< Build id / digest (see copy hook) */
  sl_crash_cpu_frame_arm_t  cpu;         /**< Registers */
  sl_crash_fault_regs_t fault;  /**< SCB fault */
  sl_crash_thread_t     thread;      /**< Thread (optional) */
#if SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED
  uint32_t              assert_line; /**< Assert line (only if assert handler enabled) */
#endif
  uint32_t              watchdog_info;    /**< WDOG flags */
  uint32_t              app_reason;  /**< App reason */
  uint32_t              user_data[SL_CRASH_USER_DATA_WORDS]; /**< Callback data */
#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
  uint16_t              stack_snapshot_len; /**< Snapshot bytes */
  uint16_t              reserved16;  /**< Reserved */
  uint8_t               stack_snapshot[SL_CRASH_MANAGER_STACK_SNAPSHOT_BYTE_MAX];
#endif
  uint32_t              reserved;    /**< @ref sl_crash_data_t::reserved */
  uint32_t              crc32;       /**< Record CRC-32 */
} sl_crash_manager_data_t;

/**
 * @brief Metadata for a validated crash record (no full struct copy).
 *
 * @ref sl_crash_manager_get_info fills this from the same source as @ref sl_crash_manager_get_log
 * (retained RAM, else non-volatile backend). @ref total_size copies @ref sl_crash_manager_data_t::total_size
 * (CRC-validated with the record; for current layout equals sizeof(sl_crash_manager_data_t)).
 */
typedef struct {
  /** @ref SL_CRASH_RECORD_INFO_INVALID or @ref SL_CRASH_RECORD_INFO_VALID. */
  uint8_t  valid;
  uint16_t version;     /**< Layout @ref SL_CRASH_LOG_VERSION */
  uint16_t crash_type;  /**< @ref sl_crash_type_t */
  uint32_t total_size;  /**< @ref sl_crash_manager_data_t::total_size (stored layout size in bytes) */
  uint32_t seqno;       /**< Session sequence */
  uint32_t crc32;       /**< Stored record CRC-32 */
  uint32_t reset_reason;/**< Platform reset cause at capture */
  uint32_t uptime_ms;   /**< Uptime at capture */
  uint8_t  build_id[SL_CRASH_BUILD_ID_BYTES]; /**< Build id / digest */
} sl_crash_record_info_t;

/** @brief @ref sl_crash_record_info_t::valid when @ref sl_crash_manager_get_info did not fill the struct. */
#define SL_CRASH_RECORD_INFO_INVALID  (0U)

/** @brief @ref sl_crash_record_info_t::valid after successful @ref sl_crash_manager_get_info (boolean; do not compare to other non-zero values). */
#define SL_CRASH_RECORD_INFO_VALID  (1U)

/** @brief Post-capture callback. */
typedef void (*sl_crash_callback_t)(sl_crash_manager_data_t *);

/** @} (end addtogroup sl_crash_manager_types) */

/*******************************************************************************
 ***************************   MACROS   ****************************************
 ******************************************************************************/

/** @brief Place symbol in a named section (linker). */
#if defined(__GNUC__) || defined(__clang__)
    #define NAMED_SECTION(section_name) \
        __attribute__((section(section_name), used, aligned(4)))
#elif defined(__ICCARM__)
    #define NAMED_SECTION(section_name) @ section_name
#else
    #error "Unsupported compiler for named sections."
#endif

/*******************************************************************************
 ***************************   VARIABLES   *************************************
 ******************************************************************************/

/** Global crash data (RAM or .ram_crash_data.noinit in linker region .ram_crash_data when RAM backend is used). */
extern volatile sl_crash_manager_data_t g_crash_manager_data;

/**
 * @brief Build id bytes from the firmware image (linker / post-build step).
 *
 * Override with a strong definition in the application; a weak default may be
 * provided by the crash manager when the toolchain supports it.
 */
extern const uint8_t g_firmware_build_id[SL_CRASH_BUILD_ID_BYTES];

/**
 * @brief Optional platform hooks (weak defaults in sl_crash_manager.c; override in app).
 *
 * Used when filling @ref sli_crash_fill_common_header and @ref sli_crash_take_next_seqno.
 */
uint32_t platform_get_boot_count(void);
uint32_t platform_get_uptime_ms(void);
uint32_t platform_get_reset_reason(void);
uint32_t platform_get_device_id(void);
uint32_t platform_get_image_addr(void);
uint32_t platform_get_next_crash_seqno(void);

/**
 * @defgroup sl_crash_manager_api API Functions
 * @brief Initialization, callbacks, crash handling, and log retrieval
 * @{
 */

/**
 * @brief Initialize the Crash Manager
 *
 * Enables MemManage, BusFault, and UsageFault in SCB. Inspects retained RAM,
 * validates magic/version/CRC, and primes an empty record when RAM does not hold
 * a valid crash. Does not write non-volatile storage; call
 * @ref sl_crash_manager_persist_pending_record when a retained-RAM crash must be
 * saved. If a valid crash exists (RAM and/or backend), the read APIs
 * (@ref sl_crash_manager_get_log, @ref sl_crash_manager_has_record, etc.) return
 * it without the caller distinguishing RAM vs storage. Registers whether a
 * retained-RAM crash still needs persistence when a write backend is present.
 *
 * If a log backend is present and a record is readable after init, emits a short
 * diagnostic (type, PC, SP).
 *
 * @note If storage init fails: returns error; retained RAM crash is left intact
 *       when valid, otherwise RAM is primed empty.
 * @note Does not erase NVM; call @ref sl_crash_manager_clear_log() after consuming a record.
 *
 * @return SL_STATUS_OK on success, or storage backend init status on failure
 */
sl_status_t sl_crash_manager_init(void);

/**
 * @brief Persist retained-RAM crash to the registered non-volatile backend
 *
 * When @ref sl_crash_manager_init or @ref sl_crash_manager_handle_crash left a
 * valid crash in RAM that still needs to be written to the backend, copies it via
 * the storage write operation. After success, clears the pending flag so the
 * same crash remains readable from RAM or from storage (public APIs unchanged).
 *
 * @return SL_STATUS_OK on success; SL_STATUS_EMPTY if nothing is pending or RAM is not valid;
 *         SL_STATUS_NOT_AVAILABLE if no write backend; or backend write status on failure
 */
sl_status_t sl_crash_manager_persist_pending_record(void);

/**
 * @brief Register per-type crash callbacks
 *
 * After sl_crash_manager_handle_crash(), the callback for the current
 * crash type runs with the live record. You may write user_data[] (and other
 * fields); any change requires @ref sli_crash_finalize_record_crc() (see
 * sli_crash_manager.h) so the stored CRC matches the payload.
 *
 * @code
 * static void my_crash_cb(sl_crash_manager_data_t *record)
 * {
 *   if (record == NULL) {
 *     return;
 *   }
 *   record->user_data[0] = 0xABCD0001u;
 *   record->user_data[1] = record->cpu.pc;
 *   record->user_data[2] = record->fault.cfsr;
 *   record->user_data[3] = record->reset_reason;
 *   sli_crash_finalize_record_crc((volatile sl_crash_manager_data_t *)record);
 * }
 *
 * static sl_crash_callback_t s_cbs[SL_CRASH_TYPE_MAX];
 * // s_cbs[SL_CRASH_TYPE_HARDFAULT] = my_crash_cb; ...
 * sl_crash_manager_register_callback(s_cbs);
 * @endcode
 *
 * @param[in] application_callbacks  Array indexed by @ref sl_crash_type_t (size @ref SL_CRASH_TYPE_MAX), or NULL to clear
 */
void sl_crash_manager_register_callback(sl_crash_callback_t *application_callbacks);

/**
 * @brief Clear all registered crash callbacks
 */
void sl_crash_manager_deregister_callback(void);

/**
 * @brief Dispatch after hooks or application code filled g_crash_manager_data
 *
 * Invokes the per-type callback if set and emits optional log events. Does not
 * update CRC: the capture path must finalize before this call; if a callback
 * modifies the record, it must call @ref sli_crash_finalize_record_crc on
 * g_crash_manager_data (see sli_crash_manager.h). If record points elsewhere,
 * the record is copied into g_crash_manager_data first. Non-volatile persistence is explicit
 * via @ref sl_crash_manager_persist_pending_record (not from this call).
 *
 * For injecting a crash from type + CPU + reserved without duplicating hook logic,
 * use @ref sli_crash_fill_record_from_sl_data then this function.
 *
 * @param[in] record  Filled crash log; NULL is ignored
 */
void sl_crash_manager_handle_crash(sl_crash_manager_data_t *record);

/**
 * @brief Copy the most recent valid crash log
 *
 * Reads RAM first, then the registered get_log backend if RAM has no valid log.
 *
 * @param[out] log_out  Output buffer (must not be NULL)
 *
 * @return SL_STATUS_OK, SL_STATUS_EMPTY, SL_STATUS_INVALID_PARAMETER, or backend status
 */
sl_status_t sl_crash_manager_get_log(sl_crash_manager_data_t *log_out);

/**
 * @brief True if a valid crash record is present (RAM or storage), same criterion as @ref sl_crash_manager_get_log.
 */
bool sl_crash_manager_has_record(void);

/**
 * @brief Crash metadata without copying the full record
 *
 * @param[out] info_out  Filled on success; cleared when returning SL_STATUS_EMPTY or error (except NULL)
 *
 * @return SL_STATUS_OK, SL_STATUS_EMPTY, SL_STATUS_INVALID_PARAMETER, or backend status from storage
 */
sl_status_t sl_crash_manager_get_info(sl_crash_record_info_t *info_out);

/**
 * @brief Read a byte range of the current crash record (transport-friendly)
 *
 * Reads the same validated record as @ref sl_crash_manager_get_log without clearing it.
 * At most sizeof(sl_crash_manager_data_t) bytes exist; @a offset plus returned length
 * never exceeds the record size.
 *
 * @param[in]  offset         Byte offset into the record (0 = start of struct layout)
 * @param[out] buffer         Destination (required when @a requested_len is non-zero)
 * @param[in]  requested_len  Max bytes to copy
 * @param[out] returned_len   Bytes copied on SL_STATUS_OK; left 0 on error or non-success status
 *
 * @return SL_STATUS_OK, SL_STATUS_EMPTY, SL_STATUS_INVALID_PARAMETER, SL_STATUS_INVALID_RANGE, or backend status
 */
sl_status_t sl_crash_manager_read(uint32_t offset,
                                  uint8_t *buffer,
                                  uint32_t requested_len,
                                  uint32_t *returned_len);

/**
 * @brief Erase persisted crash data and clear the RAM log
 *
 * @return SL_STATUS_OK on success, or storage erase status on failure
 */
sl_status_t sl_crash_manager_clear_log(void);

/** @} (end addtogroup sl_crash_manager_api) */

/** @} (end addtogroup crash_manager) */

#ifdef __cplusplus
}
#endif

#endif /* SL_CRASH_MANAGER_H */
