/***************************************************************************//**
 * @file
 * @brief SL Diagnostic API
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

#ifndef SL_DIAG_H
#define SL_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif
#include "sl_enum.h"
#include "sl_status.h"
#include "sl_diag_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SL_DIAG_PROFILE) && !defined(SL_CATALOG_SL_DIAG_PRESENT)
#error "SL_DIAG_PROFILE set but the sl_diag component is not installed"
#endif

#if defined(SL_DIAG_COPY_ON_COMMIT)
#error "SL_DIAG_COPY_ON_COMMIT is derived from SL_DIAG_SLOT_COUNT and must not be defined"
#endif

/*******************************************************************************
 ****************************   FEATURE MACROS   *******************************
 ******************************************************************************/

/// True when SL_DIAG_PROFILE is not Off. Gates every other SL Diagnostic feature and API.
#define SL_DIAG_ENABLE  (SL_DIAG_PROFILE != SL_DIAG_PROFILE_OFF)

#if SL_DIAG_ENABLE

/// True when Trigger, Reset/Boot and Build TLVs are compiled into the record format.
#define SL_DIAG_INCIDENT_METADATA_ENABLE                      \
  ((SL_DIAG_PROFILE == SL_DIAG_PROFILE_CRASH_STANDARD)        \
   || (SL_DIAG_PROFILE == SL_DIAG_PROFILE_CRASH_WITH_SUMMARY) \
   || (SL_DIAG_PROFILE == SL_DIAG_PROFILE_FULL_TEMPLATE))

/// True when the retained Summary Ring and Recent-events TLV support are included.
#define SL_DIAG_SUMMARY_RING_ENABLE                        \
  ((SL_DIAG_PROFILE == SL_DIAG_PROFILE_CRASH_WITH_SUMMARY) \
   || (SL_DIAG_PROFILE == SL_DIAG_PROFILE_FULL_TEMPLATE))

/// True when runtime non-crash capture entry points are included in the build.
#define SL_DIAG_NON_CRASH_TRIGGERS_ENABLE \
  (SL_DIAG_PROFILE == SL_DIAG_PROFILE_FULL_TEMPLATE)

/// True when the Full Template profile links extended diagnostic providers.
#define SL_DIAG_EXTENDED_PROVIDERS_ENABLE \
  (SL_DIAG_PROFILE == SL_DIAG_PROFILE_FULL_TEMPLATE)

/// True when repeat counting is stored in the record header. Requires incident metadata.
#define SL_DIAG_REPEAT_COUNT_ENABLE                           \
  ((SL_DIAG_PROFILE == SL_DIAG_PROFILE_CRASH_STANDARD)        \
   || (SL_DIAG_PROFILE == SL_DIAG_PROFILE_CRASH_WITH_SUMMARY) \
   || (SL_DIAG_PROFILE == SL_DIAG_PROFILE_FULL_TEMPLATE))

/// True when two record slots are used so one stays readable while the other is written.
#define SL_DIAG_COPY_ON_COMMIT  (SL_DIAG_SLOT_COUNT == 2)

#else // SL_DIAG_ENABLE

// Every feature is disabled when SL_DIAG_PROFILE is Off.
#define SL_DIAG_INCIDENT_METADATA_ENABLE   0
#define SL_DIAG_SUMMARY_RING_ENABLE        0
#define SL_DIAG_NON_CRASH_TRIGGERS_ENABLE  0
#define SL_DIAG_EXTENDED_PROVIDERS_ENABLE  0
#define SL_DIAG_REPEAT_COUNT_ENABLE        0
#define SL_DIAG_COPY_ON_COMMIT             0

#endif // SL_DIAG_ENABLE

#if SL_DIAG_ENABLE

/***************************************************************************//**
 * @addtogroup sl_diag SL Diagnostic
 * @brief Retained incident records for field diagnostics.
 * @{
 *
 * @details SL Diagnostic captures evidence of an incident, such as a CPU
 *          fault, an assert, a watchdog warning or a Silicon Labs subsystem
 *          timeout, into RAM that is retained across the reset that usually
 *          follows. After the reset, the application checks for a record with
 *          sl_diag_has_record(), reads it with sl_diag_get_info() and
 *          sl_diag_read(), delivers it to a host for decoding, and then
 *          removes it with sl_diag_clear(). SL_DIAG_PROFILE selects which
 *          evidence is captured.
 *
 * @note The API is core-local. Each core owns its own retained region.
 ******************************************************************************/

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/// Clear flag for sl_diag_clear(). Invalidates every record slot on this core.
#define SL_DIAG_CLEAR_RECORD        (1u << 0)

#if SL_DIAG_SUMMARY_RING_ENABLE
/// Clear flag for sl_diag_clear(). Clears the Summary Ring when implemented.
#define SL_DIAG_CLEAR_SUMMARY_RING  (1u << 1)
#endif

/// Length in bytes of build_id in sl_diag_info_t and sl_diag_get_build_info().
#define SL_DIAG_BUILD_ID_LEN  20u

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/// Provider privacy class. Values are defined in sl_diag_config.h as
/// SL_DIAG_PRIVACY_PUBLIC through SL_DIAG_PRIVACY_SECRET. Providers above
/// SL_DIAG_MAX_PRIVACY_CLASS are not linked in this build.
typedef uint8_t sl_diag_privacy_class_t;

/// Lifecycle state of a retained diagnostic record slot.
SL_ENUM(sl_diag_record_state_t) {
  /// No committed record. The slot may be reused for a new capture.
  SL_DIAG_STATE_EMPTY      = 0,
  /// Capture in progress or interrupted before commit. Not exported to callers.
  SL_DIAG_STATE_WRITING    = 1,
  /// Committed record with complete evidence and valid checksums.
  SL_DIAG_STATE_VALID      = 2,
  /// Committed record with missing or partial evidence. Still readable.
  SL_DIAG_STATE_INCOMPLETE = 3,
  /// Header or payload failed validation. Ignored until replaced or cleared.
  SL_DIAG_STATE_CORRUPT    = 4
};

/// Capture trigger: the broad class of an incident. reason_id and detail of
/// the capture request identify the exact failure point.
/// @note Crash triggers (HARD_FAULT, ASSERT, WATCHDOG_WARNING) are captured
///       from the fault, assert and watchdog warning handlers. Non-crash
///       triggers are captured by the Silicon Labs subsystem that detects the
///       failure, and only with SL_DIAG_NON_CRASH_TRIGGERS_ENABLE.
///       HOST_API_TIMEOUT, WAKE_TIMEOUT, TA_ACTIVE_TIMEOUT, CLOCK_TIMEOUT,
///       ALLOC_FAILURE and INFORMATIONAL have no capture profile yet and are
///       not captured.
/// @note SL_DIAG_TRIGGER_WATCHDOG_RESET is assigned at boot from the reset
///       cause and has no runtime entry point. SL Diagnostic never clears the
///       reset-cause hardware, and its bits accumulate until software clears
///       them. The application must clear them through the platform
///       reset-cause interface once the cause of this boot has been read, for
///       example from app_init(). Otherwise a watchdog cause from an earlier
///       boot is reported again.
SL_ENUM(sl_diag_trigger_t) {
  /// No trigger or unused value.
  SL_DIAG_TRIGGER_NONE               = 0,
  /// Cortex-M HardFault or equivalent CPU fault path.
  SL_DIAG_TRIGGER_HARD_FAULT         = 1,
  /// EFM_ASSERT or platform assert handler.
  SL_DIAG_TRIGGER_ASSERT             = 2,
  /// Watchdog warning interrupt before reset.
  SL_DIAG_TRIGGER_WATCHDOG_WARNING   = 3,
  /// Watchdog reset detected on boot from reset cause.
  SL_DIAG_TRIGGER_WATCHDOG_RESET     = 4,
  /// Network processor heartbeat loss.
  SL_DIAG_TRIGGER_NWP_HEARTBEAT_LOSS = 5,
  /// Inter-processor communication timeout.
  SL_DIAG_TRIGGER_IPC_TIMEOUT        = 6,
  /// Host API call timeout.
  SL_DIAG_TRIGGER_HOST_API_TIMEOUT   = 7,
  /// TLS operation timeout.
  SL_DIAG_TRIGGER_TLS_TIMEOUT        = 8,
  /// Wake or resume timeout.
  SL_DIAG_TRIGGER_WAKE_TIMEOUT       = 9,
  /// Trusted application active timeout.
  SL_DIAG_TRIGGER_TA_ACTIVE_TIMEOUT  = 10,
  /// Clock or oscillator readiness timeout.
  SL_DIAG_TRIGGER_CLOCK_TIMEOUT      = 11,
  /// Memory allocation failure.
  SL_DIAG_TRIGGER_ALLOC_FAILURE      = 12,
  /// Application-requested manual capture.
  SL_DIAG_TRIGGER_MANUAL             = 13,
  /// Informational capture without a failure classification.
  SL_DIAG_TRIGGER_INFORMATIONAL      = 14,
  /// Number of defined triggers. Not a valid trigger value.
  SL_DIAG_TRIGGER_COUNT
};

/// Result reported for one provider during capture.
SL_ENUM(sl_diag_provider_status_t) {
  /// Provider wrote its evidence TLV successfully.
  SL_DIAG_PROVIDER_STATUS_OK                 = 0,
  /// Provider output was shortened to fit the record budget.
  SL_DIAG_PROVIDER_STATUS_TRUNCATED          = 1,
  /// Provider data was too old to be trustworthy.
  SL_DIAG_PROVIDER_STATUS_STALE              = 2,
  /// Provider could not supply data for this capture.
  SL_DIAG_PROVIDER_STATUS_UNAVAILABLE        = 3,
  /// Provider withheld data per privacy policy.
  SL_DIAG_PROVIDER_STATUS_REDACTED           = 4,
  /// Provider was skipped by gating rules.
  SL_DIAG_PROVIDER_STATUS_SKIPPED            = 5,
  /// Provider callback failed or returned malformed output.
  SL_DIAG_PROVIDER_STATUS_FAILED             = 6,
  /// Provider observation hook failed before capture.
  SL_DIAG_PROVIDER_STATUS_OBSERVATION_FAILED = 7
};

/*******************************************************************************
 ********************************   TYPES   ************************************
 ******************************************************************************/

/** @brief Retained record summary returned by sl_diag_get_info(). */
typedef struct {
  uint8_t core_id;                         ///< Core that owns this retained region.
  sl_diag_record_state_t state;            ///< Validated state of the active record.
  uint16_t format_version;                 ///< On-wire record format version.
  uint16_t block_count;                    ///< Number of TLV blocks in the record.
  uint32_t generation;                     ///< Monotonic record generation counter.
  uint32_t total_size;                     ///< Encoded TLV payload size in bytes.
#if SL_DIAG_INCIDENT_METADATA_ENABLE
  sl_diag_trigger_t trigger;               ///< Trigger stored in the Trigger TLV.
  uint32_t reason_id;                      ///< Product-specific reason identifier.
  uint32_t detail;                         ///< Trigger-specific detail code or data.
  uint32_t uptime_ms;                      ///< Device uptime at capture in milliseconds.
  uint32_t boot_id;                        ///< Boot counter when the record was captured.
  uint32_t reset_reason;                   ///< Primary reset reason latched at boot.
  uint32_t reset_reason_ext;               ///< Extended reset reason latched at boot.
  uint8_t build_id[SL_DIAG_BUILD_ID_LEN];  ///< Firmware build identifier bytes.
  uint32_t dictionary_id;                  ///< Diagnostic dictionary identifier.
#endif
#if SL_DIAG_REPEAT_COUNT_ENABLE
  uint16_t repeat_count;                   ///< Count of repeated identical incidents.
#endif
} sl_diag_info_t;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Initializes SL Diagnostic for this core.
 *
 * @details Validates the retained RAM left by the previous boot and makes the
 *          other SL Diagnostic APIs usable in this boot:
 *          - Latches the reset cause of this boot, if it is not latched yet.
 *          - Checks the control block. A retained region written by a build
 *            with a different profile or layout, or never written, is zeroed
 *            and rebuilt with boot ID 1.
 *          - Validates every record slot and selects the newest readable
 *            record. A record cut by a reset during a capture is marked
 *            CORRUPT. A damaged control block is rebuilt from the readable
 *            slots.
 *          - Increments the boot ID, or restarts it at 1 when the control
 *            block is rebuilt.
 *          - Validates the Mini Emergency Tail, with SL_DIAG_EMERGENCY_ENABLE.
 *          - Validates the Summary Ring and adds the reset-cause breadcrumbs
 *            of this boot, with SL_DIAG_SUMMARY_RING_ENABLE.
 *          - Enables the counter that measures provider time against the
 *            capture time budget.
 *          A record of an earlier boot is kept until sl_diag_clear().
 *
 * @return SL_STATUS_OK on success, or if SL Diagnostic is already
 *         initialized,
 *         SL_STATUS_BUSY if another sl_diag_init() call is in progress,
 *         SL_STATUS_ABORT or SL_STATUS_INVALID_STATE if a retained-region
 *         write was interrupted or its state could not be validated. Not
 *         expected in normal operation.
 *         On any error SL Diagnostic stays uninitialized and the call can be
 *         retried. Until then no incident is captured, the read APIs report
 *         SL_STATUS_NOT_FOUND and sl_diag_clear() reports
 *         SL_STATUS_NOT_INITIALIZED.
 *
 * @note Called automatically from the service_init event. The application
 *       does not need to call it.
 * @note Call from task context, not from an interrupt or fault handler.
 ******************************************************************************/
sl_status_t sl_diag_init(void);

/***************************************************************************//**
 * @brief Checks whether a retained record can be read on this core.
 *
 * @details Use it at boot to decide whether a record from an earlier boot
 *          needs to be read and delivered. It reports the same record that
 *          sl_diag_get_info() and sl_diag_read() return. An INCOMPLETE record
 *          is reported too: it holds the evidence that could be captured.
 *
 * @return true if a VALID or INCOMPLETE record exists,
 *         false before sl_diag_init() has completed, after
 *         sl_diag_clear() with SL_DIAG_CLEAR_RECORD, while a capture is
 *         writing the record, or when the record is corrupt.
 ******************************************************************************/
bool sl_diag_has_record(void);

/***************************************************************************//**
 * @brief Gets a summary of the active retained record.
 *
 * Copies header fields and, when incident metadata is enabled, decodes the
 * Trigger, Reset/Boot and Build TLVs into @p info. Does not copy the full
 * TLV payload; use sl_diag_read() for that.
 *
 * @details Fills info from the record slot header and, with
 *          SL_DIAG_INCIDENT_METADATA_ENABLE, from the Trigger, Reset/Boot and
 *          Build TLVs of the record. Use it before sl_diag_read() to get the
 *          record size, and to compare generation with a record already
 *          delivered.
 *
 * @param[out] info Record summary. Zeroed first, and zeroed again on any
 *                  error. Metadata fields of a TLV that the record does not
 *                  hold, or holds in an unknown version, are 0.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_NULL_POINTER if info is NULL,
 *         SL_STATUS_NOT_FOUND if sl_diag_init() has not completed or there is
 *         no record,
 *         SL_STATUS_INVALID_STATE if a capture is writing the record, the
 *         record is corrupt or has a malformed TLV stream, or a capture
 *         replaced the record during the call. Retry later for a record
 *         being written or replaced.
 *
 * @note uptime_ms, boot_id and the reset reasons describe the boot in which
 *       the incident was captured, not the current boot.
 * @note Does not change the record.
 ******************************************************************************/
sl_status_t sl_diag_get_info(sl_diag_info_t *info);

/***************************************************************************//**
 * @brief Reads bytes from the exported retained record.
 *
 * The exported view is the fixed record header followed by tlv_data bytes
 * (total_size from the header). Reading does not modify or clear retention.
 *
 * @details Copies up to requested_len bytes of the exported record, starting
 *          at offset. The exported record is the 32-byte record slot header
 *          followed by total_size bytes of TLVs, total_size as reported by
 *          sl_diag_get_info(). To read the whole record, start at offset 0 and
 *          advance offset by returned_len until returned_len is 0. The record
 *          is decoded on the host.
 *
 * @param[in] offset Byte offset in the exported record. An offset equal to
 *                   the exported size reads 0 bytes.
 * @param[out] buffer Destination buffer of at least requested_len bytes.
 * @param[in] requested_len Maximum number of bytes to read. 0 is accepted.
 * @param[out] returned_len Number of bytes read. 0 at the end of the record
 *                          and on any error.
 *
 * @return SL_STATUS_OK on success, including a read at the end of the record,
 *         SL_STATUS_NULL_POINTER if buffer or returned_len is NULL,
 *         SL_STATUS_NOT_FOUND if sl_diag_init() has not completed or there is
 *         no record,
 *         SL_STATUS_INVALID_PARAMETER if offset is beyond the exported size,
 *         SL_STATUS_INVALID_STATE if a capture is writing the record, the
 *         record is corrupt, or a capture replaced the record during the
 *         copy. buffer may then hold partial data and must be discarded.
 *
 * @note Each call returns bytes of one record. Separate calls are not atomic
 *       together: compare generation from sl_diag_get_info() before and after
 *       a multi-chunk read to detect a record replaced between calls.
 * @note Reading does not clear the record.
 ******************************************************************************/
sl_status_t sl_diag_read(uint32_t offset,
                         uint8_t *buffer,
                         uint32_t requested_len,
                         uint32_t *returned_len);

/***************************************************************************//**
 * @brief Clears retained diagnostic state selected by flags.
 *
 * @details Call it once a record has been read and delivered, or to discard
 *          all diagnostic state, for example at a factory reset.
 *          - SL_DIAG_CLEAR_RECORD invalidates every record slot and, with
 *            SL_DIAG_EMERGENCY_ENABLE, clears the Mini Emergency Tail.
 *            sl_diag_has_record() then returns false.
 *          - SL_DIAG_CLEAR_SUMMARY_RING empties the Summary Ring. The
 *            reset-cause breadcrumbs of this boot are not added again.
 *          Each flag leaves the state of the other unchanged. The generation
 *          and the boot ID are kept, so a later record never reuses a
 *          generation number.
 *
 * @param[in] clear_flags SL_DIAG_CLEAR_* flags OR-ed together, at least one.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_INVALID_PARAMETER if clear_flags is 0 or has a flag that
 *         is not supported by this build, such as the
 *         SL_DIAG_CLEAR_SUMMARY_RING bit without the Summary Ring. Nothing
 *         is cleared,
 *         SL_STATUS_NOT_INITIALIZED if sl_diag_init() has not completed.
 *         Nothing is cleared,
 *         SL_STATUS_BUSY if a capture or another sl_diag_clear() call is in
 *         progress. Nothing is cleared; retry later,
 *         SL_STATUS_ABORT if a capture started during the clear and took
 *         over. Part of the state may be cleared, and the record of that
 *         capture is kept.
 *
 * @note With both flags, the record is cleared first. If that fails, the
 *       Summary Ring is not cleared.
 * @note Call from task context, not from a fault handler.
 ******************************************************************************/
sl_status_t sl_diag_clear(uint32_t clear_flags);

/***************************************************************************//**
 * @brief Gets device uptime for diagnostic metadata.
 *
 * @details SL Diagnostic calls it to timestamp an incident: the uptime_ms
 *          field of the Trigger TLV and of the Mini Emergency Tail.
 *
 * @return Uptime in milliseconds, 0 if not available.
 *
 * @note SL Diagnostic provides a weak default. The application can override
 *       it, for example to use another time source. An override is called from
 *       fault handlers, so it must not block, wait for an interrupt or call
 *       an RTOS API.
 * @note The weak default returns the sleeptimer time since its initialization,
 *       saturated at UINT32_MAX. It returns 0 if the sleeptimer component is
 *       not in the project or not yet initialized.
 ******************************************************************************/
uint32_t sl_diag_get_uptime_ms(void);

#if SL_DIAG_INCIDENT_METADATA_ENABLE
/***************************************************************************//**
 * @brief Gets firmware build and dictionary identifiers.
 *
 * @details SL Diagnostic calls it for the Build TLV of every capture. The
 *          host uses build_id to match the record with the firmware image
 *          and its symbols, and dictionary_id to select the dictionary used to
 *          decode the record.
 *
 * @param[out] build_id Firmware build ID, SL_DIAG_BUILD_ID_LEN bytes, for
 *                      example the GNU build ID of the image.
 * @param[out] dictionary_id Diagnostic dictionary ID.
 *
 * @note SL Diagnostic provides a weak default that returns zeros. The
 *       application overrides it to identify its build. An override is called
 *       from fault handlers, so it must not block, wait for an interrupt or
 *       call an RTOS API. Bytes it leaves unset are
 *       recorded as 0.
 ******************************************************************************/
void sl_diag_get_build_info(uint8_t build_id[SL_DIAG_BUILD_ID_LEN],
                            uint32_t *dictionary_id);
#endif // SL_DIAG_INCIDENT_METADATA_ENABLE

#if SL_DIAG_SUMMARY_RING_ENABLE
/***************************************************************************//**
 * @brief Reads bytes from the exported Summary Ring.
 *
 * @details The Summary Ring holds the latest breadcrumbs (reason ID and
 *          detail) recorded by Silicon Labs subsystems, across resets. Use it
 *          to see what happened before an incident, or before a reset that
 *          left no record. Copies up to requested_len bytes of the exported
 *          ring, starting at offset. To read the whole ring, start at offset 0
 *          and advance offset by returned_len until returned_len is 0.
 *
 * @param[in] offset Byte offset in the exported Summary Ring. An offset equal
 *                   to the exported size reads 0 bytes.
 * @param[out] buffer Destination buffer of at least requested_len bytes.
 * @param[in] requested_len Maximum number of bytes to read. 0 is accepted.
 * @param[out] returned_len Number of bytes read. 0 at the end of the ring and
 *                          on any error.
 *
 * @return SL_STATUS_OK on success, including a read at the end of the ring,
 *         SL_STATUS_NULL_POINTER if buffer or returned_len is NULL,
 *         SL_STATUS_NOT_FOUND if sl_diag_init() has not completed,
 *         SL_STATUS_INVALID_PARAMETER if offset is beyond the exported size,
 *         SL_STATUS_INVALID_STATE if the ring header is damaged, or an entry
 *         was added or the ring was cleared during the copy. buffer may then
 *         hold partial data and must be discarded; retry the read.
 *
 * @note The exported ring is the 8-byte retained ring header (magic, head,
 *       count) followed by count sl_diag_ring_entry_t entries, oldest first,
 *       so its size is 8 + 8 * count bytes. head is the retained write index
 *       and does not index the exported entries.
 * @note This is the live ring, not the frozen Recent-events TLV of a record.
 *       Each call returns bytes of one ring state: a change during the copy,
 *       even one that leaves head and count unchanged, gives
 *       SL_STATUS_INVALID_STATE. Separate calls are not atomic together, and
 *       comparing head and count between calls does not detect a change: on
 *       a full ring, 32 new entries restore the same head and count. To get
 *       a consistent ring, read it in one call with a buffer of at least
 *       264 bytes (8 + 8 * 32 entries).
 * @note Reading does not clear the ring. Can be called from task or interrupt
 *       context, not from a fault handler.
 ******************************************************************************/
sl_status_t sl_diag_read_summary_ring(uint32_t offset,
                                      uint8_t *buffer,
                                      uint32_t requested_len,
                                      uint32_t *returned_len);
#endif // SL_DIAG_SUMMARY_RING_ENABLE

/** @} (end addtogroup sl_diag) */

#endif // SL_DIAG_ENABLE

#ifdef __cplusplus
}
#endif

#endif // SL_DIAG_H
