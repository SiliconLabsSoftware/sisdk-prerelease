/***************************************************************************//**
 * @file
 * @brief SL Diagnostic retained region
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

#include <string.h>

#include "sl_common.h"
#include "sl_diag.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

#if defined(SL_CATALOG_SLEEPTIMER_PRESENT)
#include "sl_sleeptimer.h"
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Retained region. Startup code does not initialize it, so it survives reset.
// SL_ATTRIBUTE_SECTION must come before the declaration. On IAR it is a
// location pragma for the next declaration.
SL_ALIGN(4)
SL_ATTRIBUTE_SECTION(".sl_diag_retained.noinit")
static volatile sli_diag_retained_region_t retained_region SL_ATTRIBUTE_ALIGN(4);

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * Fills a new control block: boot_id 1, generation 0, active slot 0.
 ******************************************************************************/
static void init_control_block(sli_diag_control_t *ctrl)
{
  (void)memset(ctrl, 0, sizeof(*ctrl));
  ctrl->core_id = SLI_DIAG_CORE_ID;
  ctrl->magic = SLI_DIAG_CTRL_MAGIC;
  ctrl->profile_flags = SLI_DIAG_PROFILE_FLAGS;
  ctrl->boot_id = 1u;
}

/*******************************************************************************
 * Validates the retained region at boot and applies the boot_id lifecycle.
 ******************************************************************************/
static sl_status_t validate_retained_region(uint32_t owner_token)
{
  sli_diag_control_t ctrl;
  uint32_t selected_slot;
  uint32_t selected_generation;
  bool ctrl_valid;
  sl_status_t status;

  // Read the control block.
  sli_diag_read_control(&ctrl);

  // A different layout signature means member offsets cannot be trusted.
  if ((ctrl.magic != SLI_DIAG_CTRL_MAGIC)
      || (ctrl.core_id != SLI_DIAG_CORE_ID)
      || (ctrl.profile_flags != SLI_DIAG_PROFILE_FLAGS)) {
    // Initialize the retained region with zeros.
    status = sli_diag_owned_zero(owner_token, (volatile uint8_t *)&retained_region, sizeof(retained_region));
    if (status != SL_STATUS_OK) {
      return status;
    }

    // Initialize the control block with the default values.
    init_control_block(&ctrl);

    // Publish the control block to the retained region.
    return sli_diag_write_control(owner_token, &ctrl);
  }

  // Validate the control block.
  ctrl_valid = (ctrl.ctrl_crc == sli_diag_compute_control_crc(&ctrl))
               && (ctrl.active_slot < SL_DIAG_SLOT_COUNT);

  // A bad control block does not invalidate slots that pass their own checks,
  // such as a slot committed just before an interrupted control update.
  status = sli_diag_validate_and_select_active_slot(owner_token,
                                                    ctrl_valid ? ctrl.active_slot : SLI_DIAG_SLOT_NONE,
                                                    &selected_slot,
                                                    &selected_generation);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (ctrl_valid) {
    // Trusted control block on a warm boot. The boot_id wraps at 2^32 and is
    // compared for equality only.
    ctrl.boot_id++;
    if (selected_slot != SLI_DIAG_SLOT_NONE) {
      ctrl.active_slot = selected_slot;
      // A slot committed before an interrupted publication is newer than the
      // control block. The generation never moves backwards, so a generation
      // number is never reused.
      if ((int32_t)(selected_generation - ctrl.generation) > 0) {
        ctrl.generation = selected_generation;
      }
    }
  } else {
    // Damaged control block. Rebuild it from the readable slots.
    init_control_block(&ctrl);
    if (selected_slot != SLI_DIAG_SLOT_NONE) {
      ctrl.active_slot = selected_slot;
      ctrl.generation = selected_generation;
    } else {
      // If no slot is readable, invalidate all slots.
      status = sli_diag_invalidate_records(owner_token);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }
  }
  // Publish the control block to the retained region.
  return sli_diag_write_control(owner_token, &ctrl);
}

#if SL_DIAG_INCIDENT_METADATA_ENABLE
/*******************************************************************************
 * Reads a core metadata TLV. A TLV with an unexpected version or size is
 * reported as absent.
 ******************************************************************************/
static sl_status_t read_metadata_tlv(const volatile sli_diag_record_slot_t *slot,
                                     const sli_diag_record_header_t *header,
                                     uint16_t type,
                                     void *payload,
                                     uint16_t payload_size)
{
  sli_diag_tlv_info_t tlv;
  sl_status_t status;

  status = sli_diag_tlv_find(slot->tlv_data, header->total_size, type, &tlv);
  if (status != SL_STATUS_OK) {
    return status;
  }
  if ((tlv.version != SLI_DIAG_CORE_TLV_VERSION) || (tlv.length != payload_size)) {
    return SL_STATUS_NOT_FOUND;
  }
  if (sli_diag_retained_read((uint8_t *)payload, slot->tlv_data + tlv.payload_offset, payload_size)
      != SL_STATUS_OK) {
    return SL_STATUS_INVALID_STATE;
  }
  return SL_STATUS_OK;
}

/*******************************************************************************
 * Fills the metadata fields of info from the Trigger, Reset/Boot and Build
 * TLVs. Absent TLVs leave their fields 0.
 ******************************************************************************/
static sl_status_t read_record_metadata(const volatile sli_diag_record_slot_t *slot,
                                        const sli_diag_record_header_t *header,
                                        sl_diag_info_t *info)
{
  sl_diag_trigger_tlv_payload_t trigger;
  sl_diag_reset_boot_tlv_payload_t reset_boot;
  sl_diag_build_tlv_payload_t build;
  sl_status_t status;

  status = read_metadata_tlv(slot, header, SL_DIAG_TLV_TRIGGER, &trigger, sizeof(trigger));
  if (status == SL_STATUS_OK) {
    info->trigger = trigger.trigger;
    info->reason_id = trigger.reason_id;
    info->detail = trigger.detail;
    info->uptime_ms = trigger.uptime_ms;
  } else if (status != SL_STATUS_NOT_FOUND) {
    return status;
  }

  status = read_metadata_tlv(slot, header, SL_DIAG_TLV_RESET_BOOT, &reset_boot, sizeof(reset_boot));
  if (status == SL_STATUS_OK) {
    info->boot_id = reset_boot.boot_id;
    info->reset_reason = reset_boot.reset_reason;
    info->reset_reason_ext = reset_boot.reset_reason_ext;
  } else if (status != SL_STATUS_NOT_FOUND) {
    return status;
  }

  status = read_metadata_tlv(slot, header, SL_DIAG_TLV_BUILD, &build, sizeof(build));
  if (status == SL_STATUS_OK) {
    (void)memcpy(info->build_id, build.build_id, sizeof(info->build_id));
    info->dictionary_id = build.dictionary_id;
  } else if (status != SL_STATUS_NOT_FOUND) {
    return status;
  }
  return SL_STATUS_OK;
}
#endif // SL_DIAG_INCIDENT_METADATA_ENABLE

/*******************************************************************************
 * Checks that the record did not change since header was read.
 ******************************************************************************/
static bool record_is_unchanged(const volatile sli_diag_record_slot_t *slot,
                                const sli_diag_record_header_t *header)
{
  sli_diag_record_header_t current;

  sli_diag_read_record_header(slot, &current);
  return memcmp(header, &current, sizeof(current)) == 0;
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Gets the retained region.
 ******************************************************************************/
volatile sli_diag_retained_region_t *sli_diag_get_retained_region(void)
{
  return &retained_region;
}

/***************************************************************************//**
 * Initializes SL Diagnostic.
 ******************************************************************************/
sl_status_t sl_diag_init(void)
{
  uint32_t owner_token;
  sl_status_t status;

  // Initialization is idempotent for the lifetime of this boot.
  if (sli_diag_is_initialized()) {
    return SL_STATUS_OK;
  }

  // Latch reset cause here if platform_init has not run yet. Later calls do
  // nothing.
  sli_diag_latch_reset_boot();

  // Take admin ownership so boot validation can update the retained region.
  status = sli_diag_owner_acquire(SLI_DIAG_PRIORITY_ADMIN, 0u, &owner_token);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Validate the retained region.
  status = validate_retained_region(owner_token);

#if SL_DIAG_EMERGENCY_ENABLE
  if (status == SL_STATUS_OK) {
    status = sli_diag_validate_mini_emergency_tail(owner_token);
  }
#endif

#if SL_DIAG_SUMMARY_RING_ENABLE
  // Last step, so a failed init that is retried never adds the boot
  // breadcrumbs twice.
  if (status == SL_STATUS_OK) {
    status = sli_diag_init_summary_ring(owner_token);
  }
#endif

  if (status == SL_STATUS_OK) {
    sli_diag_set_initialized();
  }
  sli_diag_owner_release(owner_token);
  return status;
}

/***************************************************************************//**
 * Checks whether a retained record exists.
 ******************************************************************************/
bool sl_diag_has_record(void)
{
  volatile sli_diag_record_slot_t *slot;
  sli_diag_record_header_t header;

  return sli_diag_is_initialized()
         && (sli_diag_get_readable_record(&slot, &header) == SL_STATUS_OK);
}

/***************************************************************************//**
 * Gets a summary of the retained record.
 ******************************************************************************/
sl_status_t sl_diag_get_info(sl_diag_info_t *info)
{
  volatile sli_diag_record_slot_t *slot;
  sli_diag_record_header_t header;
  sl_status_t status;

  // Reject a NULL info pointer.
  if (info == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Zero info first so errors never return stale fields. Missing TLV fields
  // stay zero.
  (void)memset(info, 0, sizeof(*info));

  // Before init the retained layout is not trusted, so report no record.
  if (!sli_diag_is_initialized()) {
    return SL_STATUS_NOT_FOUND;
  }

  // No record gives NOT_FOUND. A writing or corrupt record gives
  // INVALID_STATE. All fields below come from this header copy.
  status = sli_diag_get_readable_record(&slot, &header);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Copy the record header into info.
  info->core_id = SLI_DIAG_CORE_ID;
  info->state = header.state;
  info->format_version = header.format_version;
  info->block_count = header.block_count;
  info->generation = header.generation;
  info->total_size = header.total_size;
#if SL_DIAG_REPEAT_COUNT_ENABLE
  info->repeat_count = header.repeat_count;
#endif
#if SL_DIAG_INCIDENT_METADATA_ENABLE
  status = read_record_metadata(slot, &header, info);
#endif

  // Clear info if a new capture replaced the record during this call.
  if ((status != SL_STATUS_OK) || !record_is_unchanged(slot, &header)) {
    (void)memset(info, 0, sizeof(*info));
    return SL_STATUS_INVALID_STATE;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads part of the retained record.
 ******************************************************************************/
sl_status_t sl_diag_read(uint32_t offset,
                         uint8_t *buffer,
                         uint32_t requested_len,
                         uint32_t *returned_len)
{
  volatile sli_diag_record_slot_t *slot;
  sli_diag_record_header_t header;
  uint32_t exported_size;
  uint32_t copy_len;
  sl_status_t status;

  // Reject NULL buffer or returned_len.
  if ((buffer == NULL) || (returned_len == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  // Set returned_len to zero first so every error returns zero bytes read.
  *returned_len = 0u;

  // Before init the retained layout is not trusted, so report no record.
  if (!sli_diag_is_initialized()) {
    return SL_STATUS_NOT_FOUND;
  }

  // No record gives NOT_FOUND. A writing or corrupt record gives
  // INVALID_STATE.
  status = sli_diag_get_readable_record(&slot, &header);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Export is the slot header plus the used TLV bytes.
  exported_size = SLI_DIAG_RECORD_HEADER_BYTES + header.total_size;

  // Offset equal to size means end of record. That read is OK with zero
  // bytes.
  if (offset > exported_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  copy_len = exported_size - offset;
  if (requested_len < copy_len) {
    copy_len = requested_len;
  }

  // A record replaced during the copy is reported instead of returning
  // mixed bytes.
  if (sli_diag_retained_read(buffer, (const volatile uint8_t *)slot + offset, copy_len) != SL_STATUS_OK) {
    return SL_STATUS_INVALID_STATE;
  }

  // Fail if a new capture committed while this copy ran.
  if (!record_is_unchanged(slot, &header)) {
    return SL_STATUS_INVALID_STATE;
  }
  // Set the returned length.
  *returned_len = copy_len;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Clears retained diagnostic state.
 ******************************************************************************/
sl_status_t sl_diag_clear(uint32_t clear_flags)
{
  uint32_t supported_flags = SL_DIAG_CLEAR_RECORD;
  sli_diag_control_t ctrl;
  uint32_t owner_token;
  sl_status_t status;

#if SL_DIAG_SUMMARY_RING_ENABLE
  supported_flags |= SL_DIAG_CLEAR_SUMMARY_RING;
#endif
  // Reject bad flags before clearing anything.
  if ((clear_flags == 0u) || ((clear_flags & ~supported_flags) != 0u)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (!sli_diag_is_initialized()) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Clear is the lowest-priority writer: it never preempts a capture, and a
  // capture may preempt it.
  status = sli_diag_owner_acquire(SLI_DIAG_PRIORITY_ADMIN, 0u, &owner_token);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if ((clear_flags & SL_DIAG_CLEAR_RECORD) != 0u) {
    // Invalidate every slot so an old slot cannot reappear.
    status = sli_diag_invalidate_records(owner_token);
    if (status == SL_STATUS_OK) {
      // Keep generation. Only active_slot goes back to zero.
      sli_diag_read_control(&ctrl);

      // Set the active slot to 0.
      ctrl.active_slot = 0u;

      // Publish the control block to the retained region.
      status = sli_diag_write_control(owner_token, &ctrl);
    }
#if SL_DIAG_EMERGENCY_ENABLE
    // Clear the emergency tail with the record because it is record fallback
    // data.
    if (status == SL_STATUS_OK) {
      status = sli_diag_clear_mini_emergency_tail(owner_token);
    }
#endif
  }
#if SL_DIAG_SUMMARY_RING_ENABLE
  // The ring is separate. Clearing only the ring leaves the record and tail.
  if ((status == SL_STATUS_OK) && ((clear_flags & SL_DIAG_CLEAR_SUMMARY_RING) != 0u)) {
    status = sli_diag_clear_summary_ring(owner_token);
  }
#endif

  // Release is a no-op when a capture already took ownership.
  sli_diag_owner_release(owner_token);
  return status;
}

/***************************************************************************//**
 * Gets the boot ID of this boot.
 ******************************************************************************/
uint32_t sli_diag_get_boot_id(void)
{
  sli_diag_control_t ctrl;

  // Trust the control block only after sl_diag_init() validated it.
  if (!sli_diag_is_initialized()) {
    return 0u;
  }

  // Read the control block.
  sli_diag_read_control(&ctrl);

  // Return the boot ID.
  return ctrl.boot_id;
}

/***************************************************************************//**
 * Gets the device uptime. 0 means the uptime is not available.
 ******************************************************************************/
SL_WEAK uint32_t sl_diag_get_uptime_ms(void)
{
#if defined(SL_CATALOG_SLEEPTIMER_PRESENT)
  uint64_t uptime_ms;

  // This must not block because fault handlers call it.
  if (!sl_sleeptimer_is_initialized()
      || (sl_sleeptimer_tick64_to_ms(sl_sleeptimer_get_tick_count64(), &uptime_ms) != SL_STATUS_OK)) {
    return 0u;
  }
  // Saturate at UINT32_MAX so a late capture never wraps to a small uptime.
  return (uptime_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)uptime_ms;
#else
  return 0u;
#endif
}

#if SL_DIAG_INCIDENT_METADATA_ENABLE
/***************************************************************************//**
 * Gets the firmware build ID and the diagnostic dictionary ID.
 *
 * @note This weak default sets both outputs to 0, because the platform cannot
 *       know the image identity. The application overrides it to return its
 *       build and dictionary IDs.
 ******************************************************************************/
SL_WEAK void sl_diag_get_build_info(uint8_t build_id[SL_DIAG_BUILD_ID_LEN],
                                    uint32_t *dictionary_id)
{
  // Must not block; fault handlers call this during metadata capture.
  if (build_id != NULL) {
    (void)memset(build_id, 0, SL_DIAG_BUILD_ID_LEN);
  }
  if (dictionary_id != NULL) {
    *dictionary_id = 0u;
  }
}
#endif // SL_DIAG_INCIDENT_METADATA_ENABLE

#endif // SL_DIAG_ENABLE

/***************************************************************************//**
 * Initializes SL Diagnostic from the service_init event.
 ******************************************************************************/
void sli_diag_service_init(void)
{
#if SL_DIAG_ENABLE
  (void)sl_diag_init();
#endif
}
