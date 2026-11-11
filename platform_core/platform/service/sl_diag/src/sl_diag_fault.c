/***************************************************************************//**
 * @file
 * @brief SL Diagnostic crash fault seal and non-crash capture
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

#include <stddef.h>
#include <stdint.h>

#include "em_device.h"
#include "sl_core.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// IPSR exception numbers. Numbers 2 to 7 are NMI and fault handlers. Numbers
// 4 to 7 are not present on ARMv6-M and ARMv8-M Baseline.
#define EXCEPTION_THREAD        0u
#define EXCEPTION_NMI           2u
#define EXCEPTION_SECURE_FAULT  7u

#if SL_DIAG_EMERGENCY_ENABLE
// Retry count when other captures keep changing the tail during stash.
#define TAIL_STASH_ATTEMPTS  3u
#endif

/*******************************************************************************
 ********************************   TYPES   ************************************
 ******************************************************************************/

// What to do with the emergency tail this capture stashed.
typedef enum {
  TAIL_KEEP,     // Keep this tail because the incident is not fully recorded.
  TAIL_RESTORE,  // Restore the tail this capture replaced.
  TAIL_DISCARD   // Discard this tail because the record is valid.
} tail_outcome_t;

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * Checks for a crash trigger.
 ******************************************************************************/
static bool is_crash_trigger(sl_diag_trigger_t trigger)
{
  return (trigger == SL_DIAG_TRIGGER_HARD_FAULT)
         || (trigger == SL_DIAG_TRIGGER_ASSERT)
         || (trigger == SL_DIAG_TRIGGER_WATCHDOG_WARNING);
}

/*******************************************************************************
 * Checks the depth and force flags of a request.
 ******************************************************************************/
static bool request_options_are_valid(const sli_diag_capture_request_t *request)
{
  return ((request->depth == SLI_DIAG_CAPTURE_DEPTH_MINIMAL)
          || (request->depth == SLI_DIAG_CAPTURE_DEPTH_NORMAL))
         && ((request->force_flags & ~SLI_DIAG_CAPTURE_FORCE_OVERWRITE) == 0u);
}

/*******************************************************************************
 * Checks the trigger, depth and force flags of a crash request.
 ******************************************************************************/
static bool request_is_valid(const sli_diag_capture_request_t *request)
{
  return is_crash_trigger(request->trigger) && request_options_are_valid(request);
}

#if SL_DIAG_NON_CRASH_TRIGGERS_ENABLE
/*******************************************************************************
 * Checks for a non-crash trigger. A watchdog reset is only known at boot, from
 * the reset cause, so it is not one.
 ******************************************************************************/
static bool is_non_crash_trigger(sl_diag_trigger_t trigger)
{
  return (trigger > SL_DIAG_TRIGGER_NONE)
         && (trigger < SL_DIAG_TRIGGER_COUNT)
         && !is_crash_trigger(trigger)
         && (trigger != SL_DIAG_TRIGGER_WATCHDOG_RESET);
}
#endif

/*******************************************************************************
 * Gets the scratch, or NULL if it is not 4-byte aligned. The scratch is read
 * word by word, which faults on a misaligned address on some cores.
 ******************************************************************************/
static const sli_diag_cpu_crash_scratch_t *get_aligned_scratch(const sli_diag_cpu_crash_scratch_t *scratch)
{
  return (((uintptr_t)scratch & 3u) == 0u) ? scratch : NULL;
}

/*******************************************************************************
 * Gets the SLI_DIAG_CONTEXT_* flags of the calling context.
 ******************************************************************************/
static uint32_t get_context_flags(sl_diag_trigger_t trigger)
{
  uint32_t exception = __get_IPSR();
  uint32_t flags;

  // Get the context flags based on the exception number.
  if (exception == EXCEPTION_THREAD) {
    flags = SLI_DIAG_CONTEXT_TASK;
  } else if ((exception >= EXCEPTION_NMI) && (exception <= EXCEPTION_SECURE_FAULT)) {
    flags = SLI_DIAG_CONTEXT_FAULT;
  } else {
    flags = SLI_DIAG_CONTEXT_ISR;
  }
  // An assert may leave locks or state inconsistent. Only fault-safe
  // providers may run.
  if (trigger == SL_DIAG_TRIGGER_ASSERT) {
    flags |= SLI_DIAG_CONTEXT_FAULT;
  }
  // A watchdog warning is followed by a watchdog reset.
  if (trigger == SL_DIAG_TRIGGER_WATCHDOG_WARNING) {
    flags |= SLI_DIAG_CONTEXT_PRE_RESET;
  }
  return flags;
}

/*******************************************************************************
 * Notes the result of a core TLV write. Returns SL_STATUS_ABORT if ownership
 * was lost, otherwise SL_STATUS_OK; any other failure sets failed.
 ******************************************************************************/
#if SL_DIAG_INCIDENT_METADATA_ENABLE || SL_DIAG_SUMMARY_RING_ENABLE
static sl_status_t note_core_tlv(sl_status_t status, bool *failed)
{
  // Return ABORT if ownership was lost.
  if (status == SL_STATUS_ABORT) {
    return SL_STATUS_ABORT;
  }

  // Set the failed flag if the write failed.
  if (status != SL_STATUS_OK) {
    *failed = true;
  }
  return SL_STATUS_OK;
}
#endif

/*******************************************************************************
 * Writes the core TLVs that precede the provider evidence.
 ******************************************************************************/
static sl_status_t write_core_tlvs(sli_diag_writer_t *writer,
                                   const sli_diag_capture_request_t *request,
                                   sli_diag_capture_depth_t depth,
                                   bool *failed)
{
  sl_status_t status = SL_STATUS_OK;

  (void)writer;
  (void)request;
  (void)depth;
  (void)failed;
  // Write core TLVs before providers so they still fit when providers fill the
  // record. Stop the sequence only on ABORT. A TLV that does not fit makes
  // the record incomplete.
#if SL_DIAG_INCIDENT_METADATA_ENABLE

  status = note_core_tlv(sli_diag_write_tlv_trigger(writer, request), failed);
  if (status == SL_STATUS_OK) {
    // Write the reset boot TLV.
    status = note_core_tlv(sli_diag_write_tlv_reset_boot(writer), failed);
  }
  if (status == SL_STATUS_OK) {
    // Write the build TLV.
    status = note_core_tlv(sli_diag_write_tlv_build(writer), failed);
  }
#endif
#if SL_DIAG_SUMMARY_RING_ENABLE
  // Minimal depth captures copy fewer ring breadcrumbs because they have less
  // time.
  if (status == SL_STATUS_OK) {
    status = note_core_tlv(sli_diag_write_tlv_recent_events(writer,
                                                            (depth == SLI_DIAG_CAPTURE_DEPTH_MINIMAL)
                                                            ? SLI_DIAG_SUMMARY_RING_TAIL_MINIMAL
                                                            : SLI_DIAG_SUMMARY_RING_TAIL_NORMAL),
                           failed);
  }
#endif
  return status;
}

/*******************************************************************************
 * Checks that the record holds the CPU Crash TLV. If the CPU Crash provider
 * left nothing, for example because it is not registered, adds a Provider
 * status TLV with UNAVAILABLE.
 ******************************************************************************/
static sl_status_t check_cpu_crash_evidence(sli_diag_writer_t *writer,
                                            const volatile sli_diag_record_slot_t *slot,
                                            bool *present)
{
  sl_diag_provider_status_tlv_payload_t status_payload;
  sli_diag_tlv_info_t tlv;
  uint32_t offset = 0u;
  bool reported = false;
  sl_status_t status;

  *present = false;

  // Check if the CPU Crash provider left evidence.
  while (sli_diag_tlv_next(slot->tlv_data, writer->bytes_written, &offset, &tlv) == SL_STATUS_OK) {
    if (tlv.type == SL_DIAG_PROVIDER_CPU_CRASH) {
      // Set the present flag.
      *present = true;
    } else if ((tlv.type == SL_DIAG_TLV_PROVIDER_STATUS)
               && (tlv.length == sizeof(status_payload))) {
      // Read the status payload. A payload that cannot be read is skipped.
      status = sli_diag_retained_read((uint8_t *)&status_payload,
                                      slot->tlv_data + tlv.payload_offset,
                                      sizeof(status_payload));
      if (status != SL_STATUS_OK) {
        continue;
      }
      if (status_payload.provider_id == SL_DIAG_PROVIDER_CPU_CRASH) {
        reported = true;
      }
    }
  }
  if (*present || reported) {
    return SL_STATUS_OK;
  }

  status = sli_diag_write_tlv_provider_status(writer,
                                              SL_DIAG_PROVIDER_CPU_CRASH,
                                              SL_DIAG_PROVIDER_STATUS_UNAVAILABLE);
  return (status == SL_STATUS_ABORT) ? SL_STATUS_ABORT : SL_STATUS_OK;
}

/*******************************************************************************
 * Builds and commits the record of an incident, or counts it as a repeat.
 * Releases ownership. Only a crash record needs the CPU Crash TLV. tail is
 * TAIL_DISCARD when the record is VALID, TAIL_RESTORE when the incident was
 * counted as a repeat or the policy did not record it, and TAIL_KEEP
 * otherwise.
 ******************************************************************************/
static sl_status_t seal_record(const sli_diag_capture_request_t *request,
                               const sli_diag_capture_profile_t *profile,
                               sli_diag_capture_depth_t depth,
                               uint32_t context_flags,
                               const sli_diag_cpu_crash_scratch_t *scratch,
                               uint32_t owner_token,
                               tail_outcome_t *tail)
{
  volatile sli_diag_record_slot_t *slot;
  sli_diag_provider_run_summary_t summary = { 0 };
  sli_diag_writer_t writer;
  bool cpu_crash_required = is_crash_trigger(request->trigger);
  bool core_tlv_failed = false;
  bool cpu_crash_present = false;
  bool incomplete;
  sl_status_t run_status;
  sl_status_t status;

  *tail = TAIL_KEEP;

  // Get the record action and handle it.
  switch (sli_diag_get_record_action(request)) {
    case SLI_DIAG_PRESERVE:
      sli_diag_owner_release(owner_token);
      *tail = TAIL_RESTORE;
      return SL_STATUS_ALREADY_EXISTS;
#if SL_DIAG_REPEAT_COUNT_ENABLE
    case SLI_DIAG_REPEAT:
      // The repeat count is incremented and ownership is released.
      status = sli_diag_increment_repeat_count(owner_token);
      if (status == SL_STATUS_OK) {
        *tail = TAIL_RESTORE;
      }
      return status;
#endif
    default:
      break;
  }

  // Selects the target slot and prepares the record.
  slot = sli_diag_select_target_slot();

  // Prepare the record.
  status = sli_diag_prepare_record(owner_token, slot);
  if (status != SL_STATUS_OK) {
    // If the preparation fails, release ownership and return the error.
    sli_diag_owner_release(owner_token);
    return status;
  }
  sli_diag_writer_init(&writer, slot, owner_token);

  // Write core TLVs, then run providers for this trigger.
  run_status = write_core_tlvs(&writer, request, depth, &core_tlv_failed);
  if (run_status == SL_STATUS_OK) {
    run_status = sli_diag_run_providers(&writer, profile, depth, context_flags, scratch, &summary);
  }
  if ((run_status == SL_STATUS_OK) && cpu_crash_required) {
    run_status = check_cpu_crash_evidence(&writer, slot, &cpu_crash_present);
  }
  if (run_status == SL_STATUS_ABORT) {
    // A higher-priority capture owns the record now.
    sli_diag_owner_release(owner_token);
    return SL_STATUS_ABORT;
  }

  // Commit as incomplete when orchestration failed, even without a reliable
  // provider summary.
  incomplete = (run_status != SL_STATUS_OK)
               || core_tlv_failed
               || (cpu_crash_required && !cpu_crash_present)
               || (summary.not_ok_count != 0u)
               || summary.budget_limited
               || summary.status_tlv_dropped;

  // Releases ownership.
  status = sli_diag_finalize_record(&writer, slot, incomplete);
  if ((status == SL_STATUS_OK) && !incomplete) {
    *tail = TAIL_DISCARD;
  }
  return status;
}

#if SL_DIAG_EMERGENCY_ENABLE
/*******************************************************************************
 * Gets the retained Mini Emergency Tail.
 ******************************************************************************/
static volatile sli_diag_mini_emergency_tail_t *get_tail(void)
{
  // Get the retained region and return the mini emergency tail.
  return &sli_diag_get_retained_region()->mini_emergency_tail;
}

/*******************************************************************************
 * Checks a copy of the Mini Emergency Tail.
 ******************************************************************************/
static bool tail_is_valid(const sli_diag_mini_emergency_tail_t *tail)
{
  // Check if the tail is valid.
  return (tail->magic == SLI_DIAG_EMERGENCY_MAGIC)
         && (tail->core_id == SLI_DIAG_CORE_ID)
         && is_crash_trigger(tail->trigger)
         && (tail->emergency_crc
             == sli_diag_crc32(tail, (uint32_t)offsetof(sli_diag_mini_emergency_tail_t, emergency_crc)));
}

/*******************************************************************************
 * Checks whether a tail byte belongs to magic.
 ******************************************************************************/
static bool is_magic_byte(uint32_t index)
{
  // Check if the index is within the magic field.
  return (index >= offsetof(sli_diag_mini_emergency_tail_t, magic))
         && (index < (offsetof(sli_diag_mini_emergency_tail_t, magic) + sizeof(uint32_t)));
}

/*******************************************************************************
 * Writes the tail. magic is cleared first and written last, so a tail cut by
 * a reset is never valid. Call with interrupts masked.
 *
 * A HardFault or an NMI is not masked and may write its own tail during this
 * write. Each store first checks that magic is still 0, so this write stops
 * once a nested capture has published, and the written bytes are checked
 * before magic is set. Returns false if the write was disturbed.
 *
 * A nested capture can still run between a check and its store. That one
 * store then breaks the nested tail's CRC, so the tail reads as invalid,
 * never as a valid mix of two captures.
 ******************************************************************************/
static bool write_tail(const sli_diag_mini_emergency_tail_t *tail)
{
  volatile sli_diag_mini_emergency_tail_t *dst = get_tail();
  volatile uint8_t *dst_bytes = (volatile uint8_t *)dst;
  const uint8_t *src_bytes = (const uint8_t *)tail;

  // Clear the magic field.
  dst->magic = 0u;
  __DMB();

  // Copy the tail bytes.
  for (uint32_t i = 0u; i < sizeof(*tail); i++) {
    // Check if the byte is within the magic field.
    if (!is_magic_byte(i)) {
      if (dst->magic != 0u) {
        return false;
      }
      dst_bytes[i] = src_bytes[i];
    }
  }
  __DMB();

  // Check if the tail bytes are different.
  for (uint32_t i = 0u; i < sizeof(*tail); i++) {
    if (!is_magic_byte(i) && (dst_bytes[i] != src_bytes[i])) {
      return false;
    }
  }

  // Check if the magic field is still 0.
  if (dst->magic != 0u) {
    return false;
  }

  // Set the magic field.
  dst->magic = tail->magic;
  __DMB();
  return true;
}

/*******************************************************************************
 * Zeroes the tail. magic is cleared first. Stops if a nested capture
 * publishes a tail. Call with interrupts masked.
 ******************************************************************************/
static void zero_tail(void)
{
  volatile sli_diag_mini_emergency_tail_t *dst = get_tail();
  volatile uint8_t *dst_bytes = (volatile uint8_t *)dst;

  // Clear the magic field.
  dst->magic = 0u;
  __DMB();

  // Copy the tail bytes.
  for (uint32_t i = 0u; i < sizeof(*dst); i++) {
    if (!is_magic_byte(i)) {
      // Check if the magic field is still 0.
      if (dst->magic != 0u) {
        return;
      }
      dst_bytes[i] = 0u;
    }
  }
  __DMB();
}

/*******************************************************************************
 * Stashes the tail unless a valid tail of this boot with a higher or
 * equal-priority trigger is kept. SLI_DIAG_CAPTURE_FORCE_OVERWRITE applies to
 * the record only, never to the tail. Returns true, the stashed CRC and a copy
 * of the replaced tail if it was written.
 ******************************************************************************/
static bool stash_tail(const sli_diag_capture_request_t *request,
                       const sli_diag_cpu_crash_scratch_t *scratch,
                       uint32_t *stashed_crc,
                       sli_diag_mini_emergency_tail_t *replaced)
{
  volatile sli_diag_mini_emergency_tail_t *emergency_tail = get_tail();
  sli_diag_mini_emergency_tail_t tail = { 0 };
  sli_diag_mini_emergency_tail_t current;
  sli_diag_control_t ctrl;
  bool stashed = false;
  sl_status_t status;
  CORE_DECLARE_IRQ_STATE;

  // Build the tail image outside critical sections.
  sli_diag_read_control(&ctrl);
  tail.core_id = SLI_DIAG_CORE_ID;
  tail.magic = SLI_DIAG_EMERGENCY_MAGIC;
  tail.trigger = request->trigger;
  tail.reason_id = request->reason_id;
  tail.detail = request->detail;
  if (scratch != NULL) {
    tail.pc = scratch->pc;
    tail.lr = scratch->lr;
    tail.sp = scratch->sp;
  }
  // Get the uptime and write the TLV.
  tail.uptime_ms = sl_diag_get_uptime_ms();
  // Get the boot ID and write the TLV.
  tail.boot_id = ctrl.boot_id;
  tail.emergency_crc = sli_diag_crc32(&tail, (uint32_t)offsetof(sli_diag_mini_emergency_tail_t, emergency_crc));

  for (uint32_t attempt = 0u; (attempt < TAIL_STASH_ATTEMPTS) && !stashed; attempt++) {
    CORE_ENTER_CRITICAL();
    status = sli_diag_retained_read((uint8_t *)&current,
                                    (const volatile uint8_t *)emergency_tail,
                                    sizeof(current));
    CORE_EXIT_CRITICAL();
    if (status != SL_STATUS_OK) {
      return false;
    }

    // Only a tail from this boot can block a new crash tail.
    if (tail_is_valid(&current)
        && (current.boot_id == ctrl.boot_id)
        && (sli_diag_get_trigger_priority(request->trigger)
            >= sli_diag_get_trigger_priority(current.trigger))) {
      return false;
    }

    // Retry when magic or emergency_crc changed before the write finished.
    CORE_ENTER_CRITICAL();
    if ((emergency_tail->magic == current.magic)
        && (emergency_tail->emergency_crc == current.emergency_crc)) {
      stashed = write_tail(&tail);
    }
    CORE_EXIT_CRITICAL();
  }

  // Set the stashed CRC and replaced tail if the tail was stashed.
  if (stashed) {
    *stashed_crc = tail.emergency_crc;
    *replaced = current;
  }
  return stashed;
}

/*******************************************************************************
 * Removes the tail this capture stashed, if it is still there. With restore,
 * a valid tail it replaced is written back, so the evidence of an incident
 * that is not in the kept record survives; otherwise the tail is zeroed.
 * A VALID record does not restore: the tail it replaced belongs to an
 * incident whose record was just replaced.
 ******************************************************************************/
static void discard_stashed_tail(uint32_t stashed_crc,
                                 const sli_diag_mini_emergency_tail_t *replaced,
                                 bool restore)
{
  volatile sli_diag_mini_emergency_tail_t *dst = get_tail();
  CORE_DECLARE_IRQ_STATE;

  // Check if the replaced tail is valid.
  restore = restore && tail_is_valid(replaced);

  // Check if the tail is valid.
  CORE_ENTER_CRITICAL();
  if ((dst->magic == SLI_DIAG_EMERGENCY_MAGIC) && (dst->emergency_crc == stashed_crc)) {
    if (restore) {
      // Write the replaced tail if restoring.
      (void)write_tail(replaced);
    } else {
      // Zero the tail if not restoring.
      zero_tail();
    }
  }
  CORE_EXIT_CRITICAL();
}
#endif // SL_DIAG_EMERGENCY_ENABLE

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Seals crash evidence into the retained region.
 ******************************************************************************/
sl_status_t sli_diag_fault_seal(const sli_diag_capture_request_t *request,
                                const sli_diag_cpu_crash_scratch_t *scratch)
{
  const sli_diag_capture_profile_t *profile;
  sli_diag_capture_depth_t depth;
  uint32_t context_flags;
  uint32_t owner_token;
  tail_outcome_t tail = TAIL_KEEP;
  sl_status_t status;
#if SL_DIAG_EMERGENCY_ENABLE
  sli_diag_mini_emergency_tail_t replaced = { 0 };
  uint32_t stashed_crc = 0u;
  bool stashed;
#endif

  // Ignore a misaligned scratch. Still record the crash without CPU crash
  // evidence.
  scratch = get_aligned_scratch(scratch);

  // Check if the request is NULL.
  if (request == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Check if the request is valid.
  if (!request_is_valid(request)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Look up the capture profile.
  profile = sli_diag_lookup_profile(request->trigger);
  if (profile == NULL) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Check if the diagnostic is initialized.
  if (!sli_diag_is_initialized()) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Limit depth to what the profile allows.
  depth = (request->depth < profile->max_depth) ? request->depth : profile->max_depth;
  context_flags = get_context_flags(request->trigger);

#if SL_DIAG_EMERGENCY_ENABLE
  // Stash the tail before taking ownership so it survives reset or nested
  // fault on the record path.
  stashed = stash_tail(request, scratch, &stashed_crc, &replaced);
#endif

  status = sli_diag_owner_acquire(sli_diag_get_trigger_priority(request->trigger),
                                  context_flags,
                                  &owner_token);
  if (status == SL_STATUS_OK) {
    // Seal the record.
    status = seal_record(request, profile, depth, context_flags, scratch, owner_token, &tail);
  } else if (status == SL_STATUS_BUSY) {
    // A capture of equal or higher priority keeps the record.
    tail = TAIL_RESTORE;
  }

#if SL_DIAG_EMERGENCY_ENABLE
  // Discard the stashed tail if it was stashed and the record was not kept.
  if (stashed && (tail != TAIL_KEEP)) {
    discard_stashed_tail(stashed_crc, &replaced, tail == TAIL_RESTORE);
  }
#else
  (void)tail;
#endif
  return status;
}

#if SL_DIAG_NON_CRASH_TRIGGERS_ENABLE
/***************************************************************************//**
 * Captures a non-crash incident.
 ******************************************************************************/
sl_status_t sli_diag_capture(const sli_diag_capture_request_t *request)
{
  const sli_diag_capture_profile_t *profile;
  sli_diag_capture_depth_t depth;
  uint32_t context_flags;
  uint32_t owner_token;
  tail_outcome_t tail;
  sl_status_t status;

  // Check the request before init state so errors stay the same either way.
  // Write nothing before ownership is taken.
  if (request == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  // Use sli_diag_fault_seal() for crash triggers. Watchdog reset is known
  // only at boot.
  if (!is_non_crash_trigger(request->trigger) || !request_options_are_valid(request)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  // This trigger has no capture profile in this build.
  profile = sli_diag_lookup_profile(request->trigger);
  if (profile == NULL) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Check if the diagnostic is initialized.
  if (!sli_diag_is_initialized()) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Fault and NMI handlers must call sli_diag_fault_seal() instead.
  context_flags = get_context_flags(request->trigger);
  if ((context_flags & SLI_DIAG_CONTEXT_FAULT) != 0u) {
    return SL_STATUS_INVALID_STATE;
  }

  // Limit depth to what the profile allows.
  depth = (request->depth < profile->max_depth) ? request->depth : profile->max_depth;

  // Return BUSY when a capture of equal or higher priority is active. This
  // call must never block.
  status = sli_diag_owner_acquire(sli_diag_get_trigger_priority(request->trigger),
                                  context_flags,
                                  &owner_token);
  if (status != SL_STATUS_OK) {
    return status;
  }
  // No CPU crash scratch and no emergency tail update on this path.
  return seal_record(request, profile, depth, context_flags, NULL, owner_token, &tail);
}
#endif // SL_DIAG_NON_CRASH_TRIGGERS_ENABLE

#if SL_DIAG_EMERGENCY_ENABLE
/***************************************************************************//**
 * Stashes the Mini Emergency Tail.
 ******************************************************************************/
void sli_diag_stash_mini_emergency_tail(const sli_diag_capture_request_t *req,
                                        const sli_diag_cpu_crash_scratch_t *scratch)
{
  sli_diag_mini_emergency_tail_t replaced;
  uint32_t stashed_crc;

  // Check if the request is NULL and if the trigger is a crash trigger.
  if ((req != NULL) && is_crash_trigger(req->trigger)) {
    // Stash the tail.
    (void)stash_tail(req, get_aligned_scratch(scratch), &stashed_crc, &replaced);
  }
}

/***************************************************************************//**
 * Validates the Mini Emergency Tail at boot.
 ******************************************************************************/
sl_status_t sli_diag_validate_mini_emergency_tail(uint32_t owner_token)
{
  volatile sli_diag_mini_emergency_tail_t *emergency_tail = get_tail();
  sli_diag_mini_emergency_tail_t tail;
  sl_status_t status;

  // Copy the retained tail to a local copy.
  status = sli_diag_retained_read((uint8_t *)&tail,
                                  (const volatile uint8_t *)emergency_tail,
                                  sizeof(tail));
  if (status != SL_STATUS_OK) {
    return SL_STATUS_INVALID_STATE;
  }

  // Check if the tail is valid.
  if ((tail.magic == 0u) || tail_is_valid(&tail)) {
    return SL_STATUS_OK;
  }
  // Clear a tail that was interrupted or damaged.
  return sli_diag_owned_zero(owner_token,
                             (volatile uint8_t *)emergency_tail,
                             sizeof(*emergency_tail));
}

/***************************************************************************//**
 * Clears the Mini Emergency Tail.
 ******************************************************************************/
sl_status_t sli_diag_clear_mini_emergency_tail(uint32_t owner_token)
{
  // Clear the tail.
  return sli_diag_owned_zero(owner_token, (volatile uint8_t *)get_tail(), sizeof(sli_diag_mini_emergency_tail_t));
}
#endif // SL_DIAG_EMERGENCY_ENABLE

#endif // SL_DIAG_ENABLE
