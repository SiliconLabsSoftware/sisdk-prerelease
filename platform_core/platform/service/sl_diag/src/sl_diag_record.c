/***************************************************************************//**
 * @file
 * @brief SL Diagnostic record lifecycle
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

#include "em_device.h"
#include "sl_core.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Runtime state below is not retained. A WRITING slot found at boot never
// implies an active writer.

// sl_diag_init() completed in this boot.
static bool initialized = false;
// A writer owns the retained region. At most one writer is active at a time;
// a higher-priority capture replaces it instead of running beside it.
static volatile bool writer_active = false;
// Trigger priority of the active writer, set on each acquire. Lower values
// have higher priority. SLI_DIAG_WRITER_PRIORITY_IDLE when no writer is active.
static volatile uint8_t writer_priority = SLI_DIAG_WRITER_PRIORITY_IDLE;
// Incremented on every acquire. Its value is the current owner token.
static volatile uint32_t current_owner_token = 0u;

static_assert(SL_DIAG_TRIGGER_COUNT == 15u, "Update trigger_priority for the new trigger");

// Trigger priority. Lower values have higher priority.
static const uint8_t trigger_priority[SL_DIAG_TRIGGER_COUNT] = {
  [SL_DIAG_TRIGGER_NONE]               = UINT8_MAX,
  [SL_DIAG_TRIGGER_HARD_FAULT]         = 0u,
  [SL_DIAG_TRIGGER_ASSERT]             = 1u,
  [SL_DIAG_TRIGGER_WATCHDOG_WARNING]   = 2u,
  [SL_DIAG_TRIGGER_WATCHDOG_RESET]     = 3u,
  [SL_DIAG_TRIGGER_NWP_HEARTBEAT_LOSS] = 4u,
  [SL_DIAG_TRIGGER_IPC_TIMEOUT]        = 5u,
  [SL_DIAG_TRIGGER_HOST_API_TIMEOUT]   = 5u,
  [SL_DIAG_TRIGGER_TLS_TIMEOUT]        = 5u,
  [SL_DIAG_TRIGGER_WAKE_TIMEOUT]       = 6u,
  [SL_DIAG_TRIGGER_TA_ACTIVE_TIMEOUT]  = 6u,
  [SL_DIAG_TRIGGER_CLOCK_TIMEOUT]      = 6u,
  [SL_DIAG_TRIGGER_ALLOC_FAILURE]      = 7u,
  [SL_DIAG_TRIGGER_MANUAL]             = 8u,
  [SL_DIAG_TRIGGER_INFORMATIONAL]      = 9u
};

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * Checks that the token is the current owner. Call with interrupts masked, so
 * ownership cannot change before the caller's store.
 ******************************************************************************/
static bool owner_is_current(uint32_t owner_token)
{
  // Check if the writer is active and the token matches the current generation.
  return writer_active && (owner_token == current_owner_token);
}

/*******************************************************************************
 * Stores a 32-bit value to retained memory if the token is the current owner.
 * Each store runs with interrupts masked. HardFault is not blocked by BASEPRI,
 * so fault-path capture uses sli_diag_owner_acquire() fault preemption instead.
 ******************************************************************************/
static sl_status_t owned_store_u32(uint32_t owner_token,
                                   volatile uint32_t *dst,
                                   uint32_t value)
{
  sl_status_t status = SL_STATUS_ABORT;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  if (owner_is_current(owner_token)) {
    *dst = value;
    status = SL_STATUS_OK;
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/*******************************************************************************
 * Stores a 16-bit value to retained memory if the token is the current owner.
 ******************************************************************************/
static sl_status_t owned_store_u16(uint32_t owner_token,
                                   volatile uint16_t *dst,
                                   uint16_t value)
{
  sl_status_t status = SL_STATUS_ABORT;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  if (owner_is_current(owner_token)) {
    *dst = value;
    status = SL_STATUS_OK;
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/*******************************************************************************
 * Stores an 8-bit value to retained memory if the token is the current owner.
 ******************************************************************************/
static sl_status_t owned_store_u8(uint32_t owner_token,
                                  volatile uint8_t *dst,
                                  uint8_t value)
{
  sl_status_t status = SL_STATUS_ABORT;
  CORE_DECLARE_IRQ_STATE;

  // Enter the critical section.
  CORE_ENTER_CRITICAL();

  // Store the value if the token is the current owner.
  if (owner_is_current(owner_token)) {
    *dst = value;
    status = SL_STATUS_OK;
  }

  // Exit the critical section.
  CORE_EXIT_CRITICAL();
  return status;
}

/*******************************************************************************
 * Copies bytes to retained memory if the token is the current owner. The whole
 * range is written in one critical section, so a reader never sees a mix of
 * old and new bytes. For small fixed blocks only, such as the control block
 * and the slot header: interrupts stay masked for the whole copy. Large ranges
 * use sli_diag_owned_copy().
 ******************************************************************************/
static sl_status_t owned_write_atomic(uint32_t owner_token,
                                      volatile uint8_t *dst,
                                      const uint8_t *src,
                                      uint32_t len)
{
  sl_status_t status = SL_STATUS_ABORT;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  if (owner_is_current(owner_token)) {
    for (uint32_t i = 0u; i < len; i++) {
      dst[i] = src[i];
    }
    status = SL_STATUS_OK;
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/*******************************************************************************
 * Writes a slot header. commit_token is cleared first, so a partially written
 * header is never a committed record.
 ******************************************************************************/
static sl_status_t store_slot_header(uint32_t owner_token,
                                     volatile sli_diag_record_slot_t *slot,
                                     const sli_diag_record_header_t *header)
{
  sli_diag_record_header_t header_copy = *header;
  sl_status_t status;

  // Clear the commit token.
  status = owned_store_u32(owner_token, &slot->commit_token, 0u);
  if (status != SL_STATUS_OK) {
    return status;
  }
  // Ensure that the commit token is cleared before writing the other fields.
  __DMB();

  // Clear the reserved field.
  header_copy.reserved = 0u;

  // Write the other fields as one group.
  return owned_write_atomic(owner_token,
                            (volatile uint8_t *)slot,
                            (const uint8_t *)&header_copy,
                            (uint32_t)offsetof(sli_diag_record_header_t, commit_token));
}

/*******************************************************************************
 * Checks whether generation a is newer than generation b. Wrap-safe, so 0 is
 * newer than 0xFFFFFFFF.
 ******************************************************************************/
static bool generation_is_newer(uint32_t a, uint32_t b)
{
  return (int32_t)(a - b) > 0;
}

/*******************************************************************************
 * Checks that the TLV stream ends exactly at total_size and holds block_count
 * blocks.
 ******************************************************************************/
static bool tlv_stream_is_valid(const volatile uint8_t *data,
                                uint32_t total_size,
                                uint16_t block_count)
{
  sli_diag_tlv_info_t tlv;
  uint32_t offset = 0u;
  uint32_t blocks_found = 0u;
  sl_status_t status;

  do {
    // Decode the next TLV of the TLV stream.
    status = sli_diag_tlv_next(data, total_size, &offset, &tlv);

    // Count the TLV if it is valid.
    if (status == SL_STATUS_OK) {
      blocks_found++;
    }
  } while (status == SL_STATUS_OK);

  // Return true if the TLV stream is valid.
  return (status == SL_STATUS_NOT_FOUND) && (blocks_found == block_count);
}

/*******************************************************************************
 * Checks a record against its header copy. Returns the record state, or
 * WRITING or CORRUPT.
 ******************************************************************************/
static sl_diag_record_state_t check_record_integrity(const volatile sli_diag_record_slot_t *slot,
                                                     const sli_diag_record_header_t *header)
{
  uint32_t payload_crc;

  // Validate the header.
  if ((header->magic != SLI_DIAG_RECORD_MAGIC)
      || (header->format_version != SLI_DIAG_FORMAT_VERSION)) {
    return SL_DIAG_STATE_CORRUPT;
  }

  // Validate the state.
  if (header->state == SL_DIAG_STATE_WRITING) {
    return SL_DIAG_STATE_WRITING;
  }

  // Validate the commit token ,total size, header CRC
  if (((header->state != SL_DIAG_STATE_VALID) && (header->state != SL_DIAG_STATE_INCOMPLETE))
      || (header->commit_token != SLI_DIAG_COMMIT_TOKEN)
      || (header->total_size > SLI_DIAG_SLOT_MAX_BYTES)
      || (sli_diag_compute_header_crc(header) != header->header_crc)) {
    return SL_DIAG_STATE_CORRUPT;
  }

  // Compute the payload CRC.
  payload_crc = sli_diag_crc32_update_retained(SLI_DIAG_CRC32_INIT, slot->tlv_data, header->total_size)
                ^ SLI_DIAG_CRC32_XOROUT;

  // Validate the payload CRC and the TLV stream.
  if ((payload_crc != header->payload_crc)
      || !tlv_stream_is_valid(slot->tlv_data, header->total_size, header->block_count)) {
    return SL_DIAG_STATE_CORRUPT;
  }

  // Return the record state.
  return header->state;
}

#if (SL_DIAG_PROFILE != SL_DIAG_PROFILE_CRASH_BASIC)
/*******************************************************************************
 * Reads trigger and reason_id from the Trigger TLV of a readable record.
 * Returns false if the identity is unknown.
 ******************************************************************************/
static bool get_incident_identity(const volatile sli_diag_record_slot_t *slot,
                                  const sli_diag_record_header_t *header,
                                  sl_diag_trigger_t *trigger,
                                  uint32_t *reason_id)
{
  sl_diag_trigger_tlv_payload_t payload;
  sli_diag_tlv_info_t tlv;

  if ((sli_diag_tlv_find(slot->tlv_data, header->total_size, SL_DIAG_TLV_TRIGGER, &tlv) != SL_STATUS_OK)
      || (tlv.version != SLI_DIAG_CORE_TLV_VERSION)
      || (tlv.length != sizeof(payload))) {
    return false;
  }
  if (sli_diag_retained_read((uint8_t *)&payload, slot->tlv_data + tlv.payload_offset, sizeof(payload))
      != SL_STATUS_OK) {
    return false;
  }
  if (payload.trigger >= SL_DIAG_TRIGGER_COUNT) {
    return false;
  }
  *trigger = payload.trigger;
  *reason_id = payload.reason_id;
  return true;
}
#endif

/*******************************************************************************
 * Commits a WRITING slot. Every store checks ownership, and commit_token is
 * written last.
 ******************************************************************************/
static sl_status_t commit_slot(sli_diag_writer_t *writer,
                               volatile sli_diag_record_slot_t *slot,
                               bool incomplete)
{
  sli_diag_record_header_t header;
  uint32_t owner_token = writer->owner_token;
  uint32_t payload_crc;
  sl_status_t status;

  // Store the total size, block count, payload CRC, and state.
  status = owned_store_u32(owner_token, &slot->total_size, writer->bytes_written);
  if (status == SL_STATUS_OK) {
    status = owned_store_u16(owner_token, &slot->block_count, writer->blocks_written);
  }
  if (status == SL_STATUS_OK) {
    payload_crc = sli_diag_crc32_update_retained(SLI_DIAG_CRC32_INIT, slot->tlv_data, writer->bytes_written)
                  ^ SLI_DIAG_CRC32_XOROUT;
    status = owned_store_u32(owner_token, &slot->payload_crc, payload_crc);
  }
  if (status == SL_STATUS_OK) {
    status = owned_store_u8(owner_token,
                            &slot->state,
                            incomplete ? SL_DIAG_STATE_INCOMPLETE : SL_DIAG_STATE_VALID);
  }
  if (status == SL_STATUS_OK) {
    // The header CRC covers the fields stored above.
    sli_diag_read_record_header(slot, &header);
    status = owned_store_u32(owner_token, &slot->header_crc, sli_diag_compute_header_crc(&header));
  }
  if (status == SL_STATUS_OK) {
    // Every other byte of the record must be in RAM before the commit token.
    __DMB();
    __DSB();
    status = owned_store_u32(owner_token, &slot->commit_token, SLI_DIAG_COMMIT_TOKEN);
  }
  if (status == SL_STATUS_OK) {
    __DMB();
    __DSB();
  }
  return status;
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Checks whether sl_diag_init() completed in this boot.
 ******************************************************************************/
bool sli_diag_is_initialized(void)
{
  return initialized;
}

/***************************************************************************//**
 * Marks SL Diagnostic as initialized for this boot.
 ******************************************************************************/
void sli_diag_set_initialized(void)
{
  initialized = true;
}

/***************************************************************************//**
 * Acquires retained-region mutation ownership.
 ******************************************************************************/
sl_status_t sli_diag_owner_acquire(uint8_t priority,
                                   uint32_t caller_context_flags,
                                   uint32_t *owner_token)
{
  sl_status_t status = SL_STATUS_OK;
  CORE_DECLARE_IRQ_STATE;
  bool fault_context;

  if (owner_token == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  fault_context = ((caller_context_flags & SLI_DIAG_CONTEXT_FAULT) != 0u);

  CORE_ENTER_CRITICAL();
  if ((priority != SLI_DIAG_PRIORITY_ADMIN) && !initialized) {
    status = SL_STATUS_NOT_INITIALIZED;
  } else if (writer_active) {
    // Lower numeric priority wins. Equal priority is BUSY unless the caller is
    // in fault context, so a nested fault can finish sealing instead of
    // leaving a torn WRITING slot. HardFault is not blocked by BASEPRI.
    if ((priority > writer_priority)
        || ((priority == writer_priority) && !fault_context)) {
      status = SL_STATUS_BUSY;
    } else {
      current_owner_token++;
      writer_active = true;
      writer_priority = priority;
      *owner_token = current_owner_token;
    }
  } else {
    current_owner_token++;
    writer_active = true;
    writer_priority = priority;
    *owner_token = current_owner_token;
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/***************************************************************************//**
 * Releases retained-region mutation ownership.
 ******************************************************************************/
void sli_diag_owner_release(uint32_t owner_token)
{
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  if (owner_is_current(owner_token)) {
    writer_active = false;
    writer_priority = SLI_DIAG_WRITER_PRIORITY_IDLE;
  }
  CORE_EXIT_CRITICAL();
}

/***************************************************************************//**
 * Checks whether a token is still the current owner.
 ******************************************************************************/
bool sli_diag_owner_is_current(uint32_t owner_token)
{
  bool current;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  current = owner_is_current(owner_token);
  CORE_EXIT_CRITICAL();
  return current;
}

/***************************************************************************//**
 * Copies bytes into retained memory while still owning the writer.
 ******************************************************************************/
sl_status_t sli_diag_owned_copy(uint32_t owner_token,
                                volatile uint8_t *dst,
                                const uint8_t *src,
                                uint32_t len)
{
  // One byte per critical section keeps interrupt latency bounded.
  for (uint32_t i = 0u; i < len; i++) {
    if (owned_store_u8(owner_token, &dst[i], src[i]) != SL_STATUS_OK) {
      return SL_STATUS_ABORT;
    }
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Zeroes retained memory while still owning the writer.
 ******************************************************************************/
sl_status_t sli_diag_owned_zero(uint32_t owner_token,
                                volatile uint8_t *dst,
                                uint32_t len)
{
  // One byte per critical section keeps interrupt latency bounded.
  for (uint32_t i = 0u; i < len; i++) {
    if (owned_store_u8(owner_token, &dst[i], 0u) != SL_STATUS_OK) {
      return SL_STATUS_ABORT;
    }
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Copies retained bytes to normal memory.
 ******************************************************************************/
sl_status_t sli_diag_retained_read(uint8_t *dst,
                                   const volatile uint8_t *src,
                                   uint32_t len)
{
  uintptr_t region_start = (uintptr_t)sli_diag_get_retained_region();
  uintptr_t src_start = (uintptr_t)src;

  // Check for NULL pointer.
  if (dst == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (len == 0u) {
    return SL_STATUS_OK;
  }
  // The range must lie inside the retained region, whose size is set by the
  // profile. Bounds are checked by subtraction so len cannot overflow.
  if ((src_start < region_start)
      || ((src_start - region_start) >= SLI_DIAG_REGION_BYTES)
      || (len > (SLI_DIAG_REGION_BYTES - (src_start - region_start)))) {
    return SL_STATUS_INVALID_RANGE;
  }
  for (uint32_t i = 0u; i < len; i++) {
    dst[i] = src[i];
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads the control block.
 ******************************************************************************/
void sli_diag_read_control(sli_diag_control_t *ctrl)
{
  volatile sli_diag_retained_region_t *region = sli_diag_get_retained_region();
  sl_status_t status;
  CORE_DECLARE_IRQ_STATE;

  // Mask interrupts for a consistent control-block snapshot while retention
  // may be updated (capture on the fault path, init and clear in task
  // context). Do not use an OS mutex: fault paths must not block. HardFault
  // and NMI are not masked here; fault writers serialize via owner_acquire().
  CORE_ENTER_CRITICAL();
  status = sli_diag_retained_read((uint8_t *)ctrl, (const volatile uint8_t *)&region->ctrl, sizeof(*ctrl));
  CORE_EXIT_CRITICAL();
  // A zeroed control block fails the magic check and is treated as invalid.
  if (status != SL_STATUS_OK) {
    (void)memset(ctrl, 0, sizeof(*ctrl));
  }
}

/***************************************************************************//**
 * Computes the control block CRC.
 ******************************************************************************/
uint32_t sli_diag_compute_control_crc(const sli_diag_control_t *ctrl)
{
  return sli_diag_crc32(ctrl, (uint32_t)offsetof(sli_diag_control_t, ctrl_crc));
}

/***************************************************************************//**
 * Writes the control block with a recomputed ctrl_crc.
 ******************************************************************************/
sl_status_t sli_diag_write_control(uint32_t owner_token,
                                   const sli_diag_control_t *ctrl)
{
  volatile sli_diag_retained_region_t *region = sli_diag_get_retained_region();
  sli_diag_control_t ctrl_copy = *ctrl;

  // Compute the CRC of the control block.
  ctrl_copy.ctrl_crc = sli_diag_compute_control_crc(&ctrl_copy);

  // All fields and ctrl_crc are written together.
  return owned_write_atomic(owner_token, (volatile uint8_t *)&region->ctrl, (const uint8_t *)&ctrl_copy, sizeof(ctrl_copy));
}

/***************************************************************************//**
 * Reads a record slot header.
 ******************************************************************************/
void sli_diag_read_record_header(const volatile sli_diag_record_slot_t *slot,
                                 sli_diag_record_header_t *header)
{
  sl_status_t status;
  CORE_DECLARE_IRQ_STATE;

  // One critical section gives a consistent copy. The header copy has the
  // same layout as the slot header.
  CORE_ENTER_CRITICAL();
  status = sli_diag_retained_read((uint8_t *)header, (const volatile uint8_t *)slot, sizeof(*header));
  CORE_EXIT_CRITICAL();
  // A zeroed header has magic 0 and is treated as an empty slot.
  if (status != SL_STATUS_OK) {
    (void)memset(header, 0, sizeof(*header));
  }
}

/***************************************************************************//**
 * Computes the record header CRC.
 ******************************************************************************/
uint32_t sli_diag_compute_header_crc(const sli_diag_record_header_t *header)
{
  uint32_t crc;

  // Bytes [0..20), then payload_crc. header_crc itself and commit_token are
  // excluded.
  crc = sli_diag_crc32_update(SLI_DIAG_CRC32_INIT,
                              header,
                              (uint32_t)offsetof(sli_diag_record_header_t, header_crc));
  crc = sli_diag_crc32_update(crc, &header->payload_crc, sizeof(header->payload_crc));
  return crc ^ SLI_DIAG_CRC32_XOROUT;
}

/***************************************************************************//**
 * Validates a record slot.
 ******************************************************************************/
sl_diag_record_state_t sli_diag_validate_slot(const volatile sli_diag_record_slot_t *slot,
                                              sli_diag_record_header_t *header)
{
  sli_diag_record_header_t header_after;
  sl_diag_record_state_t state;

  // Read the record header.
  sli_diag_read_record_header(slot, header);
  if (header->magic == 0u) {
    return SL_DIAG_STATE_EMPTY;
  }

  // Validate the record.
  state = check_record_integrity(slot, header);

  // The payload is validated outside a critical section. A changed header
  // means a writer modified the slot meanwhile.
  sli_diag_read_record_header(slot, &header_after);
  if (memcmp(header, &header_after, sizeof(header_after)) != 0) {
    return SL_DIAG_STATE_WRITING;
  }
  return state;
}

/***************************************************************************//**
 * Gets the published record if it is readable.
 ******************************************************************************/
sl_status_t sli_diag_get_readable_record(volatile sli_diag_record_slot_t **slot,
                                         sli_diag_record_header_t *header)
{
  volatile sli_diag_retained_region_t *region = sli_diag_get_retained_region();
  uint32_t active_slot = region->ctrl.active_slot;

  if (active_slot >= SL_DIAG_SLOT_COUNT) {
    return SL_STATUS_INVALID_STATE;
  }
  *slot = &region->slot[active_slot];

  switch (sli_diag_validate_slot(*slot, header)) {
    case SL_DIAG_STATE_EMPTY:
      return SL_STATUS_NOT_FOUND;
    case SL_DIAG_STATE_VALID:
    case SL_DIAG_STATE_INCOMPLETE:
      return SL_STATUS_OK;
    default:
      return SL_STATUS_INVALID_STATE;
  }
}

#if SL_DIAG_REPEAT_COUNT_ENABLE
/*******************************************************************************
 * Recovers a record torn by a reset inside sli_diag_increment_repeat_count(), after
 * repeat_count was stored but before header_crc was. Restores the previous
 * count only if the whole record is valid with it. Returns the slot state.
 ******************************************************************************/
static sl_diag_record_state_t recover_repeat_count(uint32_t owner_token,
                                                   volatile sli_diag_record_slot_t *slot,
                                                   sli_diag_record_header_t *header)
{
  sli_diag_record_header_t previous = *header;

  // Only a VALID record with an incremented count can be torn this way.
  if ((header->state != SL_DIAG_STATE_VALID) || (header->repeat_count == 0u)) {
    return SL_DIAG_STATE_CORRUPT;
  }
  previous.repeat_count = (uint16_t)(header->repeat_count - 1u);

  // Checks magic, version, state, commit token, bounds, header CRC, payload
  // CRC and TLV framing with the previous count.
  if (check_record_integrity(slot, &previous) != SL_DIAG_STATE_VALID) {
    return SL_DIAG_STATE_CORRUPT;
  }

  // Store the previous repeat count.
  if (owned_store_u16(owner_token, &slot->repeat_count, previous.repeat_count) != SL_STATUS_OK) {
    return SL_DIAG_STATE_CORRUPT;
  }
  return sli_diag_validate_slot(slot, header);
}
#endif // SL_DIAG_REPEAT_COUNT_ENABLE

/***************************************************************************//**
 * Validates the record slots and selects the active one.
 ******************************************************************************/
sl_status_t sli_diag_validate_and_select_active_slot(uint32_t owner_token,
                                                     uint32_t preferred_slot,
                                                     uint32_t *selected_slot,
                                                     uint32_t *selected_generation)
{
  volatile sli_diag_retained_region_t *region = sli_diag_get_retained_region();
  sli_diag_record_header_t header;
  sl_diag_record_state_t state;
  sl_status_t status;

  *selected_slot = SLI_DIAG_SLOT_NONE;
  *selected_generation = 0u;

  // Keeps the newest readable slot. Unreadable non-empty slots are marked
  // CORRUPT.
  for (uint32_t i = 0u; i < SL_DIAG_SLOT_COUNT; i++) {
    // Validate the slot.
    state = sli_diag_validate_slot(&region->slot[i], &header);

    // If repeat counting is enabled, recover the repeat count if the slot is CORRUPT due to
    // header_crc being torn between the last commit and the reset.
#if SL_DIAG_REPEAT_COUNT_ENABLE
    if (state == SL_DIAG_STATE_CORRUPT) {
      state = recover_repeat_count(owner_token, &region->slot[i], &header);
    }
#endif

    // If the slot is valid or incomplete, keep it.
    if ((state == SL_DIAG_STATE_VALID) || (state == SL_DIAG_STATE_INCOMPLETE)) {
      // Equal generations: keep the preferred slot, otherwise the lowest index.
      if ((*selected_slot == SLI_DIAG_SLOT_NONE)
          || generation_is_newer(header.generation, *selected_generation)
          || ((header.generation == *selected_generation) && (i == preferred_slot))) {
        *selected_slot = i;
        *selected_generation = header.generation;
      }
    } else if ((state != SL_DIAG_STATE_EMPTY) && (header.state != SL_DIAG_STATE_CORRUPT)) {
      // Mark the slot unreadable. Payload bytes are not erased here; the next
      // capture overwrites the slot header and TLV stream via prepare_record().
      status = owned_store_u8(owner_token, &region->slot[i].state, SL_DIAG_STATE_CORRUPT);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the priority of a trigger.
 ******************************************************************************/
uint8_t sli_diag_get_trigger_priority(sl_diag_trigger_t trigger)
{
  if (trigger >= SL_DIAG_TRIGGER_COUNT) {
    return UINT8_MAX;
  }
  return trigger_priority[trigger];
}

/***************************************************************************//**
 * Gets the action for a new incident against the committed record.
 ******************************************************************************/
sli_diag_record_action_t sli_diag_get_record_action(const sli_diag_capture_request_t *req)
{
  volatile sli_diag_record_slot_t *slot;
  sli_diag_record_header_t header;

  if (req == NULL) {
    return SLI_DIAG_PRESERVE;
  }

  // EMPTY, WRITING and CORRUPT records are replaced.
  if (sli_diag_get_readable_record(&slot, &header) != SL_STATUS_OK) {
    return SLI_DIAG_WRITE_NEW;
  }
  if (header.state == SL_DIAG_STATE_INCOMPLETE) {
    return SLI_DIAG_WRITE_NEW;
  }

#if (SL_DIAG_PROFILE == SL_DIAG_PROFILE_CRASH_BASIC)
  // The first VALID record is kept until it is cleared.
  return SLI_DIAG_PRESERVE;
#else
  sl_diag_trigger_t old_trigger;
  uint32_t old_reason_id;
  uint8_t new_priority;
  uint8_t old_priority;
  bool force = (req->force_flags & SLI_DIAG_CAPTURE_FORCE_OVERWRITE) != 0u;

  // FORCE_OVERWRITE bypasses only the priority decision.
  if (force) {
    return SLI_DIAG_WRITE_NEW;
  }
  // Without a readable Trigger TLV the committed incident may be a crash, so
  // it is kept.
  if (!get_incident_identity(slot, &header, &old_trigger, &old_reason_id)) {
    return SLI_DIAG_PRESERVE;
  }

  new_priority = sli_diag_get_trigger_priority(req->trigger);
  old_priority = sli_diag_get_trigger_priority(old_trigger);
  if (new_priority < old_priority) {
    return SLI_DIAG_WRITE_NEW;
  }
#if SL_DIAG_REPEAT_COUNT_ENABLE
  // Same incident again: count it instead of replacing the record.
  if ((new_priority == old_priority)
      && (req->trigger == old_trigger)
      && (req->reason_id == old_reason_id)) {
    return SLI_DIAG_REPEAT;
  }
#endif
  return SLI_DIAG_PRESERVE;
#endif
}

/***************************************************************************//**
 * Checks whether a new incident may replace the current record.
 ******************************************************************************/
bool sli_diag_may_overwrite(const sli_diag_capture_request_t *req)
{
  return sli_diag_get_record_action(req) == SLI_DIAG_WRITE_NEW;
}

/***************************************************************************//**
 * Selects the slot for a new record.
 ******************************************************************************/
volatile sli_diag_record_slot_t *sli_diag_select_target_slot(void)
{
  volatile sli_diag_retained_region_t *region = sli_diag_get_retained_region();

#if SL_DIAG_COPY_ON_COMMIT
  // The published slot stays readable while the other one is written.
  return (region->ctrl.active_slot == 0u) ? &region->slot[1] : &region->slot[0];
#else
  return &region->slot[0];
#endif
}

/***************************************************************************//**
 * Prepares a slot as a WRITING record with the next generation.
 ******************************************************************************/
sl_status_t sli_diag_prepare_record(uint32_t owner_token,
                                    volatile sli_diag_record_slot_t *slot)
{
  sli_diag_record_header_t header = { 0 };
  sli_diag_control_t ctrl;

  if (slot == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  sli_diag_read_control(&ctrl);
  header.magic = SLI_DIAG_RECORD_MAGIC;
  header.format_version = SLI_DIAG_FORMAT_VERSION;
  header.state = SL_DIAG_STATE_WRITING;
  header.generation = ctrl.generation + 1u;
  // Store the header in the slot.
  return store_slot_header(owner_token, slot, &header);
}

/***************************************************************************//**
 * Finalizes and commits a record, then publishes it.
 ******************************************************************************/
sl_status_t sli_diag_finalize_record(sli_diag_writer_t *writer,
                                     volatile sli_diag_record_slot_t *slot,
                                     bool incomplete)
{
  sli_diag_control_t ctrl;
  sl_status_t status;

  if ((writer == NULL) || (slot == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  // The writer must belong to this slot.
  if ((writer->end != (slot->tlv_data + SLI_DIAG_SLOT_MAX_BYTES))
      || (writer->bytes_written > SLI_DIAG_SLOT_MAX_BYTES)) {
    status = SL_STATUS_INVALID_PARAMETER;
  } else {
    // Commit the slot.
    status = commit_slot(writer, slot, incomplete);
  }

  // Publish the control block to the retained region.
  if (status == SL_STATUS_OK) {
    // Read the control block.
    sli_diag_read_control(&ctrl);

    // If copy on commit is enabled, set the active slot to the slot index.
#if SL_DIAG_COPY_ON_COMMIT
    ctrl.active_slot = (uint32_t)(slot - sli_diag_get_retained_region()->slot);
#endif
    // Set the generation to the slot generation.
    ctrl.generation = slot->generation;
    status = sli_diag_write_control(writer->owner_token, &ctrl);
  }

  // Release the owner token.
  sli_diag_owner_release(writer->owner_token);
  return status;
}

#if SL_DIAG_REPEAT_COUNT_ENABLE
/***************************************************************************//**
 * Increments repeat_count of the published record.
 ******************************************************************************/
sl_status_t sli_diag_increment_repeat_count(uint32_t owner_token)
{
  volatile sli_diag_record_slot_t *slot;
  sli_diag_record_header_t header;
  sl_status_t status = SL_STATUS_INVALID_STATE;
  CORE_DECLARE_IRQ_STATE;

  if ((sli_diag_get_readable_record(&slot, &header) == SL_STATUS_OK)
      && (header.state == SL_DIAG_STATE_VALID)) {
    if (header.repeat_count < UINT16_MAX) {
      header.repeat_count++;
    }
    header.header_crc = sli_diag_compute_header_crc(&header);

    // repeat_count and header_crc change together, so a reader never sees
    // one without the other.
    status = SL_STATUS_ABORT;
    CORE_ENTER_CRITICAL();
    if (owner_is_current(owner_token)) {
      slot->repeat_count = header.repeat_count;
      slot->header_crc = header.header_crc;
      status = SL_STATUS_OK;
    }
    CORE_EXIT_CRITICAL();
  }

  sli_diag_owner_release(owner_token);
  return status;
}
#endif // SL_DIAG_REPEAT_COUNT_ENABLE

/***************************************************************************//**
 * Invalidates every record slot.
 ******************************************************************************/
sl_status_t sli_diag_invalidate_records(uint32_t owner_token)
{
  volatile sli_diag_retained_region_t *region = sli_diag_get_retained_region();
  const sli_diag_record_header_t empty = { 0 };
  sl_status_t status = SL_STATUS_OK;

  // An all-zero header has magic 0, so the slot reads as EMPTY.
  for (uint32_t i = 0u; (i < SL_DIAG_SLOT_COUNT) && (status == SL_STATUS_OK); i++) {
    // Store the empty header in the slot.
    status = store_slot_header(owner_token, &region->slot[i], &empty);
  }
  return status;
}

#endif // SL_DIAG_ENABLE
