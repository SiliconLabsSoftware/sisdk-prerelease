/***************************************************************************//**
 * @file
 * @brief SL Diagnostic internal integration API
 *
 * @note This header ships with the sl_diag component for platform integrators
 *       (Crash Manager, wireless stacks, device services). It is not part of the
 *       application-facing @ref sl_diag group in product documentation; symbols
 *       here are documented for internal review and integration.
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

#ifndef SLI_DIAG_H
#define SLI_DIAG_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "sl_enum.h"
#include "sl_status.h"
#include "sl_diag.h"
#include "sl_diag_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @brief Initializes SL Diagnostic from the service_init event.
 *
 * @details Calls sl_diag_init() and ignores its status. If it fails, SL
 *          Diagnostic stays uninitialized: captures and ring entries are
 *          refused with SL_STATUS_NOT_INITIALIZED until a later
 *          sl_diag_init() call succeeds.
 *
 * @note Does nothing when SL_DIAG_PROFILE is Off, so the
 *       generated event handler builds with every profile.
 ******************************************************************************/
void sli_diag_service_init(void);

/***************************************************************************//**
 * @brief Latches the reset cause of this boot from the platform_init event.
 *
 * @details Copies the reset cause into RAM once, before any platform_init
 *          handler can clear the reset-cause hardware. The Reset/Boot TLV of
 *          every capture in this boot and the Summary Ring boot breadcrumbs
 *          use this copy.
 *
 * @note Runs at the start of platform_init, before the platform_init handlers
 *       that read the reset cause. It does not run before code called ahead
 *       of platform_init, such as app_init_pre_clock() and app_init_early()
 *       with sl_main. That code must not clear the reset-cause hardware,
 *       unless the platform reset-cause interface keeps the value it read for
 *       the latch.
 * @note Does nothing when SL_DIAG_PROFILE is Off, so the generated event
 *       handler builds with every profile.
 ******************************************************************************/
void sli_diag_platform_init(void);

#if SL_DIAG_ENABLE

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/// Default privacy class for the CPU Crash provider evidence.
#define SLI_DIAG_PROVIDER_CPU_CRASH_PRIVACY_CLASS   SL_DIAG_PRIVACY_PUBLIC
/// Default privacy class for the System provider evidence.
#define SLI_DIAG_PROVIDER_SYSTEM_PRIVACY_CLASS      SL_DIAG_PRIVACY_PUBLIC
/// Default privacy class for the Heap provider evidence.
#define SLI_DIAG_PROVIDER_HEAP_PRIVACY_CLASS        SL_DIAG_PRIVACY_OPERATIONAL
/// Default privacy class for the Host interface provider evidence.
#define SLI_DIAG_PROVIDER_HOST_IF_PRIVACY_CLASS     SL_DIAG_PRIVACY_OPERATIONAL
/// Default privacy class for the NWP health provider evidence.
#define SLI_DIAG_PROVIDER_NWP_HEALTH_PRIVACY_CLASS  SL_DIAG_PRIVACY_OPERATIONAL

/// Builds a one-hot trigger mask from a @ref sl_diag_trigger_t value for provider descriptors.
#define SLI_DIAG_TRIGGER_MASK(trigger_)  (1u << (uint32_t)(trigger_))

/// Bit in provider context_flags: callback is safe in thread context.
#define SLI_DIAG_CONTEXT_TASK       (1u << 0)
/// Bit in provider context_flags: callback is safe in interrupt context.
#define SLI_DIAG_CONTEXT_ISR        (1u << 1)
/// Bit in provider context_flags: callback is safe in fault handler context.
#define SLI_DIAG_CONTEXT_FAULT      (1u << 2)
/// Bit in provider context_flags: callback is safe just before watchdog reset.
#define SLI_DIAG_CONTEXT_PRE_RESET  (1u << 3)

/// Bit in sli_diag_capture_request_t::force_flags. Bypasses priority-based preserve policy only.
#define SLI_DIAG_CAPTURE_FORCE_OVERWRITE  (1u << 0)

/// Bit in @ref sli_diag_cpu_crash_scratch_t::flags for a valid stacked CPU frame.
/// Must match @c SL_CRASH_FLAG_CPU_FRAME_VALID (Crash Manager) when Tier 0 fills the scratch.
#define SLI_DIAG_CPU_CRASH_FLAG_FRAME_VALID  (1u << 0)

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/** @brief Capture depth requested for a profile run. */
SL_ENUM(sli_diag_capture_depth_t) {
  /// Smallest evidence set allowed for the trigger.
  SLI_DIAG_CAPTURE_DEPTH_MINIMAL = 0,
  /// Full evidence set allowed for the trigger.
  SLI_DIAG_CAPTURE_DEPTH_NORMAL  = 1
};

/*******************************************************************************
 ********************************   TYPES   ************************************
 ******************************************************************************/

/** @brief Inputs for sli_diag_fault_seal() and sli_diag_capture(). */
typedef struct {
  sl_diag_trigger_t trigger;       ///< Capture trigger for this incident.
  uint32_t reason_id;              ///< Product-specific reason identifier.
  uint32_t detail;                 ///< Trigger-specific detail code or data.
  sli_diag_capture_depth_t depth;  ///< Requested capture depth, clamped to the profile maximum.
  uint32_t force_flags;            ///< Bitwise OR of SLI_DIAG_CAPTURE_FORCE_* flags.
} sli_diag_capture_request_t;

/** @brief Context passed to a provider capture callback for one incident. */
typedef struct {
  sl_diag_trigger_t trigger;       ///< Trigger being captured.
  sli_diag_capture_depth_t depth;  ///< Effective capture depth after clamping.
  uint32_t context_flags;          ///< SLI_DIAG_CONTEXT_* flags of the caller. Several may be set.
  uint32_t bytes_remaining;        ///< Bytes the provider writer accepts: one TLV header plus
                                   ///< max_output_bytes, not total record space remaining.
  uint32_t time_remaining_us;      ///< Remaining capture time budget in microseconds.
  const void *scratch;             ///< CPU crash scratch on crash triggers, otherwise NULL.
} sli_diag_capture_context_t;

/** @brief Opaque TLV writer for the active record slot. Do not access members directly. */
typedef struct sli_diag_writer sli_diag_writer_t;

/***************************************************************************//**
 * @brief Provider capture callback.
 *
 * @param[in] ctx Capture context.
 * @param[in,out] writer TLV writer. Valid only during the call.
 * @param[in] provider_state Provider state from sli_diag_provider_t::state.
 *
 * @return SL_DIAG_PROVIDER_STATUS_OK if the provider wrote its complete
 *         evidence TLV, otherwise the non-OK result to report in a Provider
 *         status TLV. An undefined value, or OK without exactly one TLV of
 *         the provider's ID and version, is reported as
 *         SL_DIAG_PROVIDER_STATUS_FAILED.
 *
 * @note SL Diagnostic calls the callback only for a registered provider
 *       selected by the active capture profile and eligible for the trigger,
 *       when its context_flags include every context flag of the caller and
 *       the dependency, privacy, byte and time gates pass. The writer accepts at most one TLV header
 *       plus max_output_bytes; a larger write returns
 *       SL_STATUS_WOULD_OVERFLOW, and a write after preemption returns
 *       SL_STATUS_ABORT.
 * @note The provider is responsible for collecting and encoding its
 *       payload as a TLV whose type is its provider ID, for reporting OK only
 *       when that TLV was written, and for not blocking, allocating, using a
 *       transport, changing the core clock, entering sleep, reconfiguring
 *       the cycle counter or exceeding max_latency_us. SL Diagnostic checks
 *       the TLV framing of an OK result, not the payload, and does not
 *       interrupt a slow callback. It
 *       charges the measured time to the capture budget only where it can
 *       measure it reliably, otherwise at least max_latency_us; see
 *       sli_diag_run_providers().
 * @note Bytes written before a non-OK result are kept, as the HLD allows
 *       partial evidence for TRUNCATED and FAILED.
 ******************************************************************************/
typedef sl_diag_provider_status_t (*sli_diag_capture_fn_t)(const sli_diag_capture_context_t *ctx,
                                                           sli_diag_writer_t *writer,
                                                           void *provider_state);

/** @brief Static descriptor for one diagnostic provider registered at runtime. */
typedef struct {
  uint16_t id;                            ///< Provider ID and evidence TLV type.
  uint8_t version;                        ///< Payload layout version for the evidence TLV.
  uint8_t dependency_count;               ///< Number of entries in dependencies.
  uint32_t trigger_mask;                  ///< Bit mask of supported triggers from SLI_DIAG_TRIGGER_MASK().
  uint32_t context_flags;                 ///< Contexts where capture is safe, SLI_DIAG_CONTEXT_* bits.
  sl_diag_privacy_class_t privacy_class;  ///< Privacy class of data emitted in the evidence TLV.
  uint16_t max_latency_us;                ///< Worst-case capture time in microseconds, at least 1.
  uint16_t max_output_bytes;              ///< Maximum encoded payload bytes, at least 1.
  const uint16_t *dependencies;           ///< Provider IDs that must run first, in order, or NULL.
  sli_diag_capture_fn_t capture;          ///< Capture callback invoked during an incident.
  void *state;                            ///< Opaque provider state passed to capture, or NULL.
} sli_diag_provider_t;

/** @brief Recipe of providers and budget for one trigger. */
typedef struct {
  sl_diag_trigger_t trigger;           ///< Trigger this profile applies to.
  sli_diag_capture_depth_t max_depth;  ///< Maximum capture depth allowed for this trigger.
  uint32_t time_budget_us;             ///< Total provider time budget in microseconds.
  uint16_t provider_count;             ///< Number of provider IDs in providers.
  const uint16_t *providers;           ///< Provider IDs to run, in execution order.
} sli_diag_capture_profile_t;

/** @brief CPU register and stack snapshot from Crash Manager Tier 0. Not stored in retained RAM as a whole. */
typedef struct {
  uint32_t r0;                ///< Exception frame R0.
  uint32_t r1;                ///< Exception frame R1.
  uint32_t r2;                ///< Exception frame R2.
  uint32_t r3;                ///< Exception frame R3.
  uint32_t r12;               ///< Exception frame R12.
  uint32_t lr;                ///< Link register.
  uint32_t pc;                ///< Program counter.
  uint32_t xpsr;              ///< xPSR from the stacked frame when @ref SLI_DIAG_CPU_CRASH_FLAG_FRAME_VALID.
  uint32_t sp;                ///< SP of the faulting context before exception entry (snapshot starts here).
  uint32_t cfsr;              ///< CFSR.
  uint32_t hfsr;              ///< HFSR.
  uint32_t bfar;              ///< BFAR.
  uint32_t mmfar;             ///< MMFAR.
  uint32_t file_hash;         ///< Assert file hash, or 0.
  uint32_t expression_hash;   ///< Assert expression hash, or 0.
  uint32_t line;              ///< Assert line, or 0.
  uint32_t active_thread_id;  ///< Active thread ID, or 0.
  uint32_t exc_return;        ///< EXC_RETURN at fault, or 0 if not from an exception context.
  uint32_t flags;             ///< @ref SLI_DIAG_CPU_CRASH_FLAG_FRAME_VALID and future Tier 0 flags.
#if (SL_DIAG_STACK_SNAPSHOT_BYTES > 0)
  uint16_t stack_snapshot_size;                         ///< Valid bytes in stack_snapshot.
  uint16_t reserved0;                                   ///< Reserved.
  uint8_t stack_snapshot[SL_DIAG_STACK_SNAPSHOT_BYTES]; ///< Stack snapshot.
#endif
} sli_diag_cpu_crash_scratch_t;

/// Fixed byte size of the CPU Crash scratch and TLV payload excluding stack snapshot bytes.
#if (SL_DIAG_STACK_SNAPSHOT_BYTES > 0u)
#define SLI_DIAG_CPU_CRASH_BASE_BYTES \
  ((uint32_t)offsetof(sli_diag_cpu_crash_scratch_t, stack_snapshot_size))
#else
#define SLI_DIAG_CPU_CRASH_BASE_BYTES  sizeof(sli_diag_cpu_crash_scratch_t)
#endif

/// Maximum encoded CPU Crash provider payload in bytes. Covers the whole scratch,
/// including stack_snapshot_size, reserved0, the stack snapshot and any trailing
/// padding.
#define SLI_DIAG_CPU_CRASH_MAX_ENCODED_BYTES \
  ((uint32_t)sizeof(sli_diag_cpu_crash_scratch_t))

/** @brief Reset causes latched before reset-reason hardware is cleared at boot. */
typedef struct {
  uint32_t reset_reason;      ///< Primary reset reason from the platform latch.
  uint32_t reset_reason_ext;  ///< Extended reset reason from the platform latch.
} sli_diag_reset_boot_t;

static_assert(offsetof(sli_diag_cpu_crash_scratch_t, active_thread_id) == 64u, "sli_diag_cpu_crash_scratch_t layout");
static_assert(offsetof(sli_diag_cpu_crash_scratch_t, exc_return) == 68u, "sli_diag_cpu_crash_scratch_t layout");
static_assert(offsetof(sli_diag_cpu_crash_scratch_t, flags) == 72u, "sli_diag_cpu_crash_scratch_t layout");
#if (SL_DIAG_STACK_SNAPSHOT_BYTES > 0u)
static_assert(offsetof(sli_diag_cpu_crash_scratch_t, stack_snapshot_size) == 76u,
              "sli_diag_cpu_crash_scratch_t layout");
#endif
static_assert(SLI_DIAG_CPU_CRASH_BASE_BYTES == 76u, "sli_diag_cpu_crash_scratch_t fixed size");
static_assert(SLI_DIAG_CPU_CRASH_MAX_ENCODED_BYTES <= 0xFFFFu,
              "CPU Crash payload must fit the 16-bit TLV length");
static_assert(sizeof(sli_diag_reset_boot_t) == 8u, "sli_diag_reset_boot_t must be 8 bytes");

/*******************************************************************************
 ***************   INTERNAL STACK / PROVIDER INTEGRATION APIs   ****************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Registers a diagnostic provider.
 *
 * @param[in] provider Provider descriptor. Must stay valid and unchanged
 *            while registered.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_NULL_POINTER if provider is NULL,
 *         SL_STATUS_INVALID_PARAMETER if the descriptor is invalid or declares
 *         more output than its retained budget,
 *         SL_STATUS_NOT_SUPPORTED if the provider is not linked in this build,
 *         SL_STATUS_ALREADY_EXISTS if the provider ID is already registered,
 *         SL_STATUS_FULL if the registry is full.
 *
 * @note Validates the ID, callback, trigger mask, context flags, privacy
 *       class, a nonzero max_latency_us, a nonzero maximum output that fits
 *       a slot, and the dependency list.
 *       Each dependency must be a different linked provider, listed once.
 *       Dependencies do not have to be registered yet.
 * @note Only provider IDs listed in @c SLI_DIAG_LINKED_PROVIDER_LIST for this
 *       build (from SLC autogen or the default header) can register. An
 *       unlinked provider never takes a linked provider's retained budget.
 *       Registration stores the descriptor and does not run the callback. Can
 *       be called from task or interrupt context.
 * @see sl_diag_format.h (stack @c .slcc @c template_contribution guide)
 ******************************************************************************/
sl_status_t sli_diag_register_provider(const sli_diag_provider_t *provider);

/***************************************************************************//**
 * @brief Appends one TLV block to the active record.
 *
 * @details Writes a 6-byte TLV header (type, version, reserved byte,
 *          payload length, little-endian) followed by the payload at the
 *          writer cursor, then advances the cursor. The block is written only
 *          if all of it fits, so a rejected block leaves the record unchanged.
 *
 * @param[in,out] writer TLV writer.
 * @param[in] type TLV type or provider ID.
 * @param[in] version Payload version.
 * @param[in] payload Payload bytes. May be NULL when payload_length is 0.
 * @param[in] payload_length Payload length in bytes. 0 writes a header-only
 *            TLV.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_WOULD_OVERFLOW if the TLV exceeds the writer limit,
 *         SL_STATUS_ABORT if the capture lost ownership of the record,
 *         SL_STATUS_NULL_POINTER if writer is NULL, or payload is NULL when
 *         payload_length is non-zero,
 *         SL_STATUS_INVALID_PARAMETER if type is 0 or payload_length is larger
 *         than a record slot.
 ******************************************************************************/
sl_status_t sli_diag_writer_append_tlv(sli_diag_writer_t *writer,
                                       uint16_t type,
                                       uint8_t version,
                                       const void *payload,
                                       uint16_t payload_length);

/***************************************************************************//**
 * @brief Gets the number of bytes left in the writer.
 *
 * @details A block of 6 + payload_length bytes fits if it is not larger than
 *          this value. For a provider writer, this is the provider's own
 *          limit, not the space left in the record.
 *
 * @param[in] writer TLV writer.
 *
 * @brief Gets writable bytes remaining for this provider writer window.
 *
 * @param[in] writer TLV writer from the capture callback. Must not be NULL.
 *
 * @return Writable bytes remaining, including the TLV header bytes. 0 if
 *         writer is NULL.
 ******************************************************************************/
uint32_t sli_diag_writer_bytes_remaining(const sli_diag_writer_t *writer);

#if SL_DIAG_NON_CRASH_TRIGGERS_ENABLE
/***************************************************************************//**
 * @brief Captures a non-crash incident into the retained region.
 *
 * @details Records a failure that the device survives, such as an IPC or TLS
 *          timeout, so it can be read after recovery or after a later reset.
 *          In order:
 *          - Validates the request, the trigger's capture profile and the
 *            calling context. Nothing is written if one check fails.
 *          - Takes ownership of the record at the trigger's priority.
 *          - Applies the overwrite policy. An EMPTY, CORRUPT or INCOMPLETE
 *            record is replaced. A VALID record is replaced by a trigger of
 *            higher priority, or with SLI_DIAG_CAPTURE_FORCE_OVERWRITE. The
 *            same trigger and reason_id again increments the repeat count
 *            instead.
 *          - Writes the core TLVs (Trigger, Reset/Boot, Build and
 *            Recent-events), then runs the providers of the trigger's
 *            capture profile, and commits the record.
 *          depth is limited to the profile's maximum depth.
 *
 * @param[in] request Capture request. trigger must be a non-crash trigger:
 *            not SL_DIAG_TRIGGER_NONE, a crash trigger or
 *            SL_DIAG_TRIGGER_WATCHDOG_RESET. Only read during the call.
 *
 * @return SL_STATUS_OK if a VALID or INCOMPLETE record was committed, or the
 *         repeat count of the committed record was incremented,
 *         SL_STATUS_ALREADY_EXISTS if the overwrite policy keeps the
 *         committed record, for example a crash record. The incident is not
 *         recorded,
 *         SL_STATUS_BUSY if a capture of equal or higher priority is in
 *         progress. The incident is not recorded,
 *         SL_STATUS_ABORT if a higher-priority capture took over during this
 *         capture. That capture's record replaces this one,
 *         SL_STATUS_NOT_INITIALIZED if sl_diag_init() has not completed,
 *         SL_STATUS_NULL_POINTER if request is NULL,
 *         SL_STATUS_INVALID_PARAMETER if trigger is not a non-crash trigger,
 *         depth is not a sli_diag_capture_depth_t value, or force_flags has
 *         a bit other than SLI_DIAG_CAPTURE_FORCE_OVERWRITE,
 *         SL_STATUS_NOT_SUPPORTED if the trigger has no capture profile in
 *         this build,
 *         SL_STATUS_INVALID_STATE if called from a fault or NMI handler.
 *         Another status from the record commit means it could not be
 *         completed; not expected in normal operation.
 *
 * @note Called by the Silicon Labs subsystem that detects the failure, at the
 *       first reliable failure boundary and before recovery starts. The device
 *       keeps running: nothing is reset. Never blocks or allocates.
 * @note Can be called from task and interrupt context, but not from a fault
 *       or NMI handler, which use sli_diag_fault_seal(). Context flags come
 *       from IPSR: Thread mode is SLI_DIAG_CONTEXT_TASK, others are
 *       SLI_DIAG_CONTEXT_ISR.
 * @note Uses the same priority, overwrite policy, record format and commit as
 *       sli_diag_fault_seal(). The providers get NULL as ctx->scratch, no CPU
 *       Crash TLV is needed and the Mini Emergency Tail is not used. The
 *       record is INCOMPLETE if a core TLV does not fit, a selected provider
 *       is not OK or the time budget limited the evidence.
 ******************************************************************************/
sl_status_t sli_diag_capture(const sli_diag_capture_request_t *request);
#endif // SL_DIAG_NON_CRASH_TRIGGERS_ENABLE

#if SL_DIAG_SUMMARY_RING_ENABLE
/***************************************************************************//**
 * @brief Appends one breadcrumb entry to the Summary Ring.
 *
 * @details Records a breadcrumb: a short event, such as a retry or a state
 *          change, that helps explain a later incident. The ring is retained
 *          across resets. A capture copies its latest entries into the
 *          Recent-events TLV of the record, and sl_diag_read_summary_ring()
 *          reads the live ring.
 *
 * @param[in] reason_id Reason ID, see sl_diag_reason_ids.h. Not checked
 *            against the registry; any value is recorded as given.
 * @param[in] detail Reason-specific detail.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_NOT_INITIALIZED if sl_diag_init() has not completed. The
 *         entry is dropped,
 *         SL_STATUS_INVALID_STATE if the ring header is damaged, for example
 *         by a stray RAM write. The entry is dropped, and so are later ones
 *         until the ring is reset by sl_diag_clear() or at the next boot.
 *
 * @note Can be called from task and interrupt context, but not from a fault
 *       or NMI handler. Never blocks or allocates. Interrupts are masked for
 *       a few stores only.
 * @note Only adds a breadcrumb to the live ring, never creates a record. A
 *       full ring overwrites its oldest entry.
 ******************************************************************************/
sl_status_t sli_diag_ring_record(uint32_t reason_id,
                                 uint32_t detail);
#endif // SL_DIAG_SUMMARY_RING_ENABLE

/*******************************************************************************
 ***************   CRASH MANAGER -> SL_DIAG INTERNAL INTEGRATION   *************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Seals crash evidence into the retained region.
 *
 * @param[in] request Capture request. trigger must be
 *            SL_DIAG_TRIGGER_HARD_FAULT, SL_DIAG_TRIGGER_ASSERT or
 *            SL_DIAG_TRIGGER_WATCHDOG_WARNING.
 * @param[in] scratch CPU crash context from Crash Manager Tier 0, or NULL.
 *            Must be 4-byte aligned, otherwise it is ignored. Only read
 *            during the call. @p scratch->sp is the pre-exception SP; the stack
 *            snapshot begins at that address when present. @p scratch->exc_return
 *            and @p scratch->flags (@ref SLI_DIAG_CPU_CRASH_FLAG_FRAME_VALID) are
 *            filled by Tier 0; SL Diagnostic does not interpret them during seal.
 *
 * @return SL_STATUS_OK if a VALID or INCOMPLETE record was committed, or the
 *         repeat count was incremented,
 *         SL_STATUS_ALREADY_EXISTS if the overwrite policy keeps the
 *         committed record,
 *         SL_STATUS_BUSY if a capture of equal or higher priority is in
 *         progress,
 *         SL_STATUS_ABORT if a higher-priority capture took over,
 *         SL_STATUS_NOT_INITIALIZED if sl_diag_init() has not completed,
 *         SL_STATUS_NULL_POINTER if request is NULL,
 *         SL_STATUS_INVALID_PARAMETER if the trigger, depth or force_flags is
 *         invalid,
 *         SL_STATUS_NOT_SUPPORTED if the trigger has no capture profile.
 *
 * @note Called by Crash Manager after Tier 0, from the fault handler, the
 *       assert handler or the watchdog warning interrupt. Never blocks or
 *       allocates. Crash Manager resets the device after it returns.
 * @note Every provider gets the scratch as ctx->scratch. Crash Manager owns
 *       the CPU Crash provider, which encodes it as the
 *       SL_DIAG_PROVIDER_CPU_CRASH TLV. Pass NULL on a watchdog warning
 *       without an exception frame; the record is then INCOMPLETE.
 * @note Context flags come from IPSR: Thread mode is SLI_DIAG_CONTEXT_TASK,
 *       exceptions 2 to 7 are SLI_DIAG_CONTEXT_FAULT, others are
 *       SLI_DIAG_CONTEXT_ISR. An assert adds SLI_DIAG_CONTEXT_FAULT and a
 *       watchdog warning adds SLI_DIAG_CONTEXT_PRE_RESET.
 * @note A watchdog warning is captured at minimal depth within
 *       SL_DIAG_CAPTURE_TIME_BUDGET_WDT_US.
 * @note The record is INCOMPLETE if any evidence is missing, including the
 *       CPU Crash TLV. A CPU Crash provider that wrote nothing, for example
 *       because it is not registered, is reported as UNAVAILABLE.
 * @note With SL_DIAG_EMERGENCY_ENABLE, the Mini Emergency Tail is written
 *       first, unless a valid tail of this boot with a higher or
 *       equal-priority trigger is kept; SLI_DIAG_CAPTURE_FORCE_OVERWRITE does
 *       not change this. A tail of an earlier boot is always replaced. This
 *       capture's tail is zeroed when the record is VALID. It is removed and
 *       a valid tail it replaced is written back when the incident is
 *       counted as a repeat or not recorded (ALREADY_EXISTS, BUSY). It is
 *       kept when the record is INCOMPLETE or the capture is aborted.
 ******************************************************************************/
sl_status_t sli_diag_fault_seal(const sli_diag_capture_request_t *request,
                                const sli_diag_cpu_crash_scratch_t *scratch);

/*******************************************************************************
 *********************   PLATFORM INTERNAL INTEGRATION   ***********************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Latches the reset cause.
 *
 * @param[out] out Reset causes to fill. Must not be NULL.
 *
 * @note Called once per boot, from sli_diag_platform_init() or, if that has
 *       not run, from sl_diag_init(). Must not clear the reset-cause hardware:
 *       clearing it is owned by the application.
 * @note The weak default reads the reset-cause registers of the device. On
 *       devices with hard and soft reset-cause registers, the hard cause is
 *       the primary reason and the soft cause the extended reason. On devices
 *       with a single reset-cause register, it is the primary reason and the
 *       extended reason is 0. Without a supported register, both are 0.
 * @note Watchdog-reset classification assumes the default register layout. A
 *       platform override must keep it.
 ******************************************************************************/
void sli_platform_latch_reset_boot(sli_diag_reset_boot_t *out);

#endif // SL_DIAG_ENABLE

#ifdef __cplusplus
}
#endif

#endif // SLI_DIAG_H
