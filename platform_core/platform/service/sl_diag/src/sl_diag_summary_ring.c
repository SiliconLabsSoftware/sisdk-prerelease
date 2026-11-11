/***************************************************************************//**
 * @file
 * @brief SL Diagnostic Summary Ring
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
#include "sl_common.h"
#include "sl_core.h"
#include "sl_diag.h"
#include "sl_diag_reason_ids.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_SUMMARY_RING_ENABLE

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// The exported Summary Ring starts with the retained ring header: magic, head,
// and count.
#define RING_HEADER_BYTES  ((uint32_t)offsetof(sli_diag_summary_ring_t, entries))

#define RING_ENTRY_BYTES   ((uint32_t)sizeof(sl_diag_ring_entry_t))

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Counts ring changes in this boot. A reader can detect any change during a
// copy, even when head and count return to the same values. Updated only in
// critical sections.
static uint32_t ring_change_count;

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * Gets the retained Summary Ring.
 ******************************************************************************/
static volatile sli_diag_summary_ring_t *get_ring(void)
{
  return &sli_diag_get_retained_region()->summary_ring;
}

/*******************************************************************************
 * Checks the ring header. Head and count must stay in range because they
 * index the entries.
 ******************************************************************************/
static bool ring_is_valid(uint32_t magic, uint16_t head, uint16_t count)
{
  return (magic == SLI_DIAG_SUMMARY_MAGIC)
         && (head < SLI_DIAG_SUMMARY_RING_CAPACITY)
         && (count <= SLI_DIAG_SUMMARY_RING_CAPACITY);
}

/*******************************************************************************
 * Gets the index of the oldest entry among the latest entries written before
 * head.
 ******************************************************************************/
static uint32_t get_first_index(uint16_t head, uint32_t entries)
{
  return ((uint32_t)head + SLI_DIAG_SUMMARY_RING_CAPACITY - entries) % SLI_DIAG_SUMMARY_RING_CAPACITY;
}

/*******************************************************************************
 * Empties the ring if the token is the current owner. The whole ring is
 * written in one critical section, so readers never see a partly cleared ring.
 * Magic is cleared first and set last, so a ring cut by reset is cleared again
 * at boot.
 ******************************************************************************/
static sl_status_t reset_ring(uint32_t owner_token)
{
  volatile sli_diag_summary_ring_t *ring = get_ring();
  sl_status_t status = SL_STATUS_ABORT;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  // Clear the ring only when this token still owns the retained region.
  if (sli_diag_owner_is_current(owner_token)) {
    // Clear the ring header.
    ring->magic = 0u;
    __DMB();
    ring->head = 0u;
    ring->count = 0u;

    // Clear every ring entry.
    for (uint32_t i = 0u; i < SLI_DIAG_SUMMARY_RING_CAPACITY; i++) {
      ring->entries[i].reason_id = 0u;
      ring->entries[i].detail = 0u;
    }
    __DMB();

    // Publish a valid empty ring and bump the change count.
    ring->magic = SLI_DIAG_SUMMARY_MAGIC;
    ring_change_count++;
    status = SL_STATUS_OK;
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/*******************************************************************************
 * Appends an entry, overwriting the oldest one when the ring is full.
 *
 * The entry is stored before head, and head before count. Interrupts are
 * masked, so other contexts see one complete append. A reset or a HardFault
 * at any store leaves entries[head] as the only partly written entry. It is
 * outside the window that ends at head, unless the ring is full and head has
 * not moved yet: then it is the oldest entry, never in a recent tail, and the
 * boot breadcrumb that the next init writes at head replaces it before the
 * ring can be read. A stop between the head and count stores drops the
 * oldest entry and keeps the new one.
 ******************************************************************************/
static sl_status_t append_entry(uint32_t reason_id, uint32_t detail)
{
  volatile sli_diag_summary_ring_t *ring = get_ring();
  sl_status_t status = SL_STATUS_INVALID_STATE;
  uint16_t head;
  uint16_t count;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  // Read head and count once so every check and store below uses the same
  // ring state.
  head = ring->head;
  count = ring->count;

  // Head indexes the entries, so never store using a damaged header.
  if (ring_is_valid(ring->magic, head, count)) {
    // Write the full entry before head moves past it.
    ring->entries[head].reason_id = reason_id;
    ring->entries[head].detail = detail;
    __DMB();

    // Advance head and wrap to drop the oldest entry when the ring is full.
    // Count stops at capacity.
    ring->head = (uint16_t)(((uint32_t)head + 1u) % SLI_DIAG_SUMMARY_RING_CAPACITY);
    if (count < SLI_DIAG_SUMMARY_RING_CAPACITY) {
      __DMB();
      ring->count = (uint16_t)(count + 1u);
    }

    // Bump the change count so readers can detect this append.
    ring_change_count++;
    status = SL_STATUS_OK;
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/*******************************************************************************
 * Copies the latest entries, at most max_entries, oldest first. Returns
 * SL_STATUS_INVALID_STATE if the ring is damaged.
 ******************************************************************************/
static sl_status_t copy_latest_entries(sl_diag_ring_entry_t *dst,
                                       uint32_t max_entries,
                                       uint32_t *copied)
{
  volatile sli_diag_summary_ring_t *ring = get_ring();
  sl_status_t status = SL_STATUS_INVALID_STATE;
  uint16_t head;
  uint16_t count;
  uint32_t entries;
  uint32_t index;
  CORE_DECLARE_IRQ_STATE;

  *copied = 0u;
  // Keep interrupts masked for the whole copy so an append cannot move entries.
  // The entry limit is small, so this does not hold interrupts long.
  CORE_ENTER_CRITICAL();
  head = ring->head;
  count = ring->count;
  if (ring_is_valid(ring->magic, head, count)) {
    // The latest entries sit just before head. Walk backward from head and
    // wrap at the start of the array.
    entries = SL_MIN((uint32_t)count, max_entries);
    index = get_first_index(head, entries);
    for (uint32_t i = 0u; i < entries; i++) {
      dst[i].reason_id = ring->entries[index].reason_id;
      dst[i].detail = ring->entries[index].detail;
      index = (index + 1u) % SLI_DIAG_SUMMARY_RING_CAPACITY;
    }
    *copied = entries;
    status = SL_STATUS_OK;
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Validates the Summary Ring at boot and adds the boot breadcrumbs.
 ******************************************************************************/
sl_status_t sli_diag_init_summary_ring(uint32_t owner_token)
{
  volatile sli_diag_summary_ring_t *ring = get_ring();
  sli_diag_reset_boot_t reset;
  sl_status_t status = SL_STATUS_OK;
  CORE_DECLARE_IRQ_STATE;

  // Boot breadcrumbs explain why this boot happened, using the latched reset
  // cause.
  sli_diag_get_reset_boot(&reset);

  // Validate the ring, reset it if needed, then add breadcrumbs in one step.
  // A retry after a failed init does not add the same breadcrumb twice.
  // A valid ring keeps its entries across the reset.
  CORE_ENTER_CRITICAL();
  // Reset a ring that was never written, is damaged, or was cut mid-clear.
  if (!ring_is_valid(ring->magic, ring->head, ring->count)) {
    status = reset_ring(owner_token);
  }
  if (status == SL_STATUS_OK) {
    status = append_entry(SL_DIAG_REASON_BOOT_RESET_REASON, reset.reset_reason);
  }
  // Skip the extended breadcrumb when the latched extended cause is zero.
  if ((status == SL_STATUS_OK) && (reset.reset_reason_ext != 0u)) {
    status = append_entry(SL_DIAG_REASON_BOOT_RESET_REASON_EXT, reset.reset_reason_ext);
  }
  CORE_EXIT_CRITICAL();
  return status;
}

/***************************************************************************//**
 * Empties the Summary Ring.
 ******************************************************************************/
sl_status_t sli_diag_clear_summary_ring(uint32_t owner_token)
{
  return reset_ring(owner_token);
}

/***************************************************************************//**
 * Adds an entry to the Summary Ring.
 ******************************************************************************/
sl_status_t sli_diag_ring_record(uint32_t reason_id,
                                 uint32_t detail)
{
  // Do not use the ring until sl_diag_init() has validated it.
  if (!sli_diag_is_initialized()) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Append the entry.
  return append_entry(reason_id, detail);
}

/***************************************************************************//**
 * Writes the latest Summary Ring entries as a Recent-events TLV.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_recent_events(sli_diag_writer_t *writer,
                                             uint16_t tail_entries)
{
  sl_diag_ring_entry_t tail[SLI_DIAG_SUMMARY_RING_TAIL_NORMAL];
  uint32_t requested = SL_MIN((uint32_t)tail_entries, SLI_DIAG_SUMMARY_RING_TAIL_NORMAL);
  uint32_t remaining;
  uint32_t entries;
  uint32_t fit;
  sl_status_t status;

  if (writer == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Copy entries in one critical section so the tail is one ring snapshot.
  // Later ring updates stay in the live ring only.
  status = copy_latest_entries(tail, requested, &entries);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // The slot fits a full normal tail. This clamp applies only when the record
  // stream is already over budget. Keep the newest entries.
  remaining = sli_diag_writer_bytes_remaining(writer);
  if (remaining < SLI_DIAG_TLV_HDR_BYTES) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  // Calculate the number of entries that fit in the TLV.
  fit = SL_MIN(entries, (remaining - SLI_DIAG_TLV_HDR_BYTES) / RING_ENTRY_BYTES);

  // An empty payload means the ring had no entries, so it is not written for
  // entries that do not fit.
  if ((fit == 0u) && (entries != 0u)) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  // Write the newest entries that fit. A shorter tail marks the record
  // incomplete.
  status = sli_diag_writer_append_tlv(writer,
                                      SL_DIAG_TLV_RECENT_EVENTS,
                                      SLI_DIAG_CORE_TLV_VERSION,
                                      &tail[entries - fit],
                                      (uint16_t)(fit * RING_ENTRY_BYTES));
  if ((status == SL_STATUS_OK) && (fit < entries)) {
    status = SL_STATUS_WOULD_OVERFLOW;
  }
  return status;
}

/***************************************************************************//**
 * Reads part of the Summary Ring.
 ******************************************************************************/
sl_status_t sl_diag_read_summary_ring(uint32_t offset,
                                      uint8_t *buffer,
                                      uint32_t requested_len,
                                      uint32_t *returned_len)
{
  volatile sli_diag_summary_ring_t *ring = get_ring();
  uint8_t header[RING_HEADER_BYTES];
  uint32_t change_count;
  uint32_t exported_size;
  uint32_t first_index;
  uint32_t copy_len;
  uint32_t position;
  uint32_t entry;
  uint16_t head;
  uint16_t count;
  bool valid;
  sl_status_t status;
  CORE_DECLARE_IRQ_STATE;

  // Validate the buffer and returned length.
  if ((buffer == NULL) || (returned_len == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  // Set returned_len to zero first so every error returns zero bytes read.
  *returned_len = 0u;

  // Do not read the ring until sl_diag_init() has validated it.
  if (!sli_diag_is_initialized()) {
    return SL_STATUS_NOT_FOUND;
  }

  // Snapshot the header and change count together. Export header bytes from
  // this copy so they match the entry bytes below.
  CORE_ENTER_CRITICAL();
  change_count = ring_change_count;
  head = ring->head;
  count = ring->count;
  valid = ring_is_valid(ring->magic, head, count);
  if (!valid) {
    CORE_EXIT_CRITICAL();
    return SL_STATUS_INVALID_STATE;
  }
  status = sli_diag_retained_read(header, (const volatile uint8_t *)ring, RING_HEADER_BYTES);
  CORE_EXIT_CRITICAL();
  if (status != SL_STATUS_OK) {
    return SL_STATUS_INVALID_STATE;
  }

  // The export is the header plus count entries, oldest first.
  exported_size = RING_HEADER_BYTES + ((uint32_t)count * RING_ENTRY_BYTES);
  if (offset > exported_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  copy_len = SL_MIN(exported_size - offset, requested_len);
  first_index = get_first_index(head, count);

  // Copy outside the critical section so interrupt latency does not grow with
  // the read size. Report an error instead of mixed bytes if the ring
  // changed during the copy.
  for (uint32_t i = 0u; i < copy_len; i++) {
    position = offset + i;
    if (position < RING_HEADER_BYTES) {
      buffer[i] = header[position];
    } else {
      // Map export position to the retained entry, starting at the oldest
      // index and wrapping at capacity.
      position -= RING_HEADER_BYTES;
      entry = (first_index + (position / RING_ENTRY_BYTES)) % SLI_DIAG_SUMMARY_RING_CAPACITY;
      buffer[i] = ((const volatile uint8_t *)&ring->entries[entry])[position % RING_ENTRY_BYTES];
    }
  }

  // Fail if any append or clear ran after the snapshot, even when head and
  // count match again.
  CORE_ENTER_CRITICAL();
  valid = (change_count == ring_change_count);
  CORE_EXIT_CRITICAL();
  if (!valid) {
    return SL_STATUS_INVALID_STATE;
  }
  *returned_len = copy_len;
  return SL_STATUS_OK;
}

#endif // SL_DIAG_SUMMARY_RING_ENABLE
