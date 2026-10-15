/***************************************************************************//**
 * @file sl_log_captive_ring.c
 * @brief Single-writer captive-core shared-memory Logger ring.
 *******************************************************************************
 * SPDX-License-Identifier: Zlib
 ******************************************************************************/

#include "sl_log_captive_ring.h"

#include <stdint.h>

#if defined(__riscv)
#define SLI_LOG_CAPTIVE_RELEASE_BARRIER() \
  __asm__ volatile ("fence w, w" ::: "memory")
#define SLI_LOG_CAPTIVE_ACQUIRE_BARRIER() \
  __asm__ volatile ("fence r, rw" ::: "memory")
#elif defined(__arm__) || defined(__ARM_ARCH) || defined(__ICCARM__)
#include "cmsis_compiler.h"
#define SLI_LOG_CAPTIVE_RELEASE_BARRIER() __DMB()
#define SLI_LOG_CAPTIVE_ACQUIRE_BARRIER() __DMB()
#elif defined(__GNUC__) || defined(__clang__)
#define SLI_LOG_CAPTIVE_RELEASE_BARRIER() __sync_synchronize()
#define SLI_LOG_CAPTIVE_ACQUIRE_BARRIER() __sync_synchronize()
#else
#error "Captive Logger needs target memory barriers"
#endif

static bool sli_log_captive_seq_is_valid(uint32_t seq, uint32_t modulus)
{
  return ((modulus != 0u) && (seq < modulus));
}

/* Largest multiple of capacity that fits in uint32_t. Keeping the sequence
 * space an exact multiple of the capacity is what makes slot = seq % capacity
 * continuous across sequence rollover. */
static uint32_t sli_log_captive_seq_modulus(uint32_t capacity)
{
  if ((capacity <= 1u) || ((UINT32_MAX / capacity) < 2u)) {
    return 0u;
  }

  return (UINT32_MAX / capacity) * capacity;
}

static uint32_t sli_log_captive_seq_distance(uint32_t newer,
                                             uint32_t older,
                                             uint32_t modulus)
{
  if (!sli_log_captive_seq_is_valid(newer, modulus)
      || !sli_log_captive_seq_is_valid(older, modulus)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  return (newer >= older) ? (newer - older) : ((modulus - older) + newer);
}

static uint32_t sli_log_captive_seq_slot(uint32_t seq, uint32_t capacity)
{
  return (capacity == 0u) ? SL_LOG_CAPTIVE_INVALID_U32 : (seq % capacity);
}

static uint32_t sli_log_captive_seq_advance(uint32_t seq,
                                            uint32_t delta,
                                            uint32_t modulus)
{
  if (!sli_log_captive_seq_is_valid(seq, modulus) || (delta >= modulus)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  return (seq >= (modulus - delta))
         ? (seq - (modulus - delta))
         : (seq + delta);
}

static void sli_log_captive_clear_view(sl_log_captive_ring_view_t *view)
{
  view->header = NULL;
  view->records = NULL;
  view->ring_bytes = 0u;
  view->capacity = 0u;
  view->seq_modulus = 0u;
  view->producer_slot = 0u;
}

/* Hot-path check. Only covers state another core can change underneath us:
 * the view itself is private, so its derived geometry cannot be corrupted and
 * is not re-derived here. */
static bool sli_log_captive_view_is_bound(const sl_log_captive_ring_view_t *view)
{
  return ((view != NULL)
          && (view->header != NULL)
          && (view->records != NULL)
          && (view->header->magic == SL_LOG_CAPTIVE_RING_MAGIC_V1));
}

/* Full structural validation. Used only at init and bind, where the shared
 * header is still untrusted and the derived geometry must be proven to match
 * it. Everything expensive lives here so the publish path stays clear of it. */
static bool sli_log_captive_view_is_structurally_valid(
  const sl_log_captive_ring_view_t *view)
{
  if (!sli_log_captive_view_is_bound(view)) {
    return false;
  }
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();

  if ((view->ring_bytes != view->header->ring_bytes)
      || (view->capacity != sl_log_captive_ring_capacity(view->ring_bytes))
      || (view->seq_modulus != sli_log_captive_seq_modulus(view->capacity))
      || (view->header->magic != SL_LOG_CAPTIVE_RING_MAGIC_V1)) {
    return false;
  }

  return sli_log_captive_seq_is_valid(view->header->commit_seq,
                                      view->seq_modulus);
}

/* Distinguishes a cursor trailing the producer from one that cannot belong to
 * this session. Caller must already have established a bound view. */
static bool sli_log_captive_ack_is_plausible(const sl_log_captive_ring_view_t *view,
                                             uint32_t commit_seq,
                                             uint32_t consumer_read_seq)
{
  uint32_t distance;

  distance = sli_log_captive_seq_distance(commit_seq,
                                          consumer_read_seq,
                                          view->seq_modulus);
  return ((distance != SL_LOG_CAPTIVE_INVALID_U32)
          && (distance <= (view->seq_modulus / 2u)));
}

static bool sli_log_captive_configure_view(sl_log_captive_ring_view_t *view,
                                           void *shared_base,
                                           uint32_t ring_bytes)
{
  uint32_t capacity;
  uint32_t modulus;
  uintptr_t base;

  if (view == NULL) {
    return false;
  }

  sli_log_captive_clear_view(view);

  if (shared_base == NULL) {
    return false;
  }

  base = (uintptr_t)shared_base;
  if (((base % sizeof(uint32_t)) != 0u)
      || ((ring_bytes % sizeof(uint32_t)) != 0u)
      || (ring_bytes > (UINTPTR_MAX - base))) {
    return false;
  }

  capacity = sl_log_captive_ring_capacity(ring_bytes);
  modulus = sli_log_captive_seq_modulus(capacity);
  if ((capacity <= 1u) || (modulus == 0u)) {
    return false;
  }

  view->header = (sl_log_captive_ring_header_t *)shared_base;
  view->records =
    (sl_log_event_t *)((uint8_t *)shared_base + SL_LOG_CAPTIVE_RING_HEADER_BYTES);
  view->ring_bytes = ring_bytes;
  view->capacity = capacity;
  view->seq_modulus = modulus;
  view->producer_slot = 0u;
  return true;
}

uint32_t sl_log_captive_ring_capacity(uint32_t ring_bytes)
{
  if ((ring_bytes < SL_LOG_CAPTIVE_RING_MIN_BYTES)
      || ((ring_bytes % sizeof(uint32_t)) != 0u)) {
    return 0u;
  }

  return (ring_bytes - SL_LOG_CAPTIVE_RING_HEADER_BYTES)
         / SL_LOG_CAPTIVE_RING_RECORD_BYTES;
}

uint32_t sl_log_captive_ring_safe_depth(uint32_t capacity)
{
  return (capacity > 1u) ? (capacity - 1u) : 0u;
}

uint32_t sl_log_captive_seq_next(uint32_t seq, uint32_t modulus)
{
  if (!sli_log_captive_seq_is_valid(seq, modulus)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  return (seq == (modulus - 1u)) ? 0u : (seq + 1u);
}

bool sl_log_captive_ring_init(sl_log_captive_ring_view_t *view,
                              void *shared_base,
                              uint32_t ring_bytes,
                              uint32_t reset_generation)
{
  sl_log_captive_ring_header_t *header;

  if (!sli_log_captive_configure_view(view, shared_base, ring_bytes)) {
    return false;
  }

  header = view->header;
  header->magic = 0u;
  SLI_LOG_CAPTIVE_RELEASE_BARRIER();
  header->ring_bytes = ring_bytes;
  header->reset_generation = reset_generation;
  header->commit_seq = 0u;
  header->consumer_read_seq = 0u;
  header->producer_drop_count = 0u;
  SLI_LOG_CAPTIVE_RELEASE_BARRIER();
  header->magic = SL_LOG_CAPTIVE_RING_MAGIC_V1;

  /* commit_seq starts at 0, so the matching physical slot is 0. */
  view->producer_slot = 0u;
  return true;
}

bool sl_log_captive_ring_bind(sl_log_captive_ring_view_t *view,
                              void *shared_base,
                              uint32_t accessible_bytes)
{
  sl_log_captive_ring_header_t *header;
  uint32_t ring_bytes;
  uintptr_t base;

  if (view == NULL) {
    return false;
  }

  sli_log_captive_clear_view(view);
  if ((shared_base == NULL)
      || (accessible_bytes < SL_LOG_CAPTIVE_RING_HEADER_BYTES)) {
    return false;
  }

  base = (uintptr_t)shared_base;
  if (((base % sizeof(uint32_t)) != 0u)
      || (accessible_bytes > (UINTPTR_MAX - base))) {
    return false;
  }

  header = (sl_log_captive_ring_header_t *)shared_base;
  if (header->magic != SL_LOG_CAPTIVE_RING_MAGIC_V1) {
    return false;
  }
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  ring_bytes = header->ring_bytes;

  if ((ring_bytes > accessible_bytes)
      || !sli_log_captive_configure_view(view, shared_base, ring_bytes)
      || !sli_log_captive_view_is_structurally_valid(view)) {
    sli_log_captive_clear_view(view);
    return false;
  }

  /* Bind-time division is acceptable; it keeps the publish path free of it.
   * Only meaningful when a producer recovers its own ring, but harmless on a
   * consumer view, which never reads producer_slot. */
  view->producer_slot = sli_log_captive_seq_slot(view->header->commit_seq,
                                                 view->capacity);

  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  return true;
}

bool sl_log_captive_ring_publish_serialized(sl_log_captive_ring_view_t *view,
                                            const sl_log_event_t *record)
{
  uint32_t sequence;
  uint32_t slot;
  uint32_t next;

  if (!sli_log_captive_view_is_bound(view) || (record == NULL)) {
    return false;
  }

  sequence = view->header->commit_seq;
  slot = view->producer_slot;
  if (!sli_log_captive_seq_is_valid(sequence, view->seq_modulus)
      || (slot >= view->capacity)) {
    return false;
  }

  next = (sequence == (view->seq_modulus - 1u)) ? 0u : (sequence + 1u);

  view->records[slot] = *record;
  SLI_LOG_CAPTIVE_RELEASE_BARRIER();
  view->header->commit_seq = next;

  /* The canonical modulus is an exact multiple of the capacity, so the
   * sequence and the slot wrap together and stay in step. */
  view->producer_slot = ((slot + 1u) == view->capacity) ? 0u : (slot + 1u);
  return true;
}

uint32_t sl_log_captive_ring_read_commit_seq(const sl_log_captive_ring_view_t *view)
{
  uint32_t commit_seq;

  if (!sli_log_captive_view_is_bound(view)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  commit_seq = view->header->commit_seq;
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  if (!sli_log_captive_seq_is_valid(commit_seq, view->seq_modulus)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  return commit_seq;
}

uint32_t sl_log_captive_reader_available(const sl_log_captive_ring_view_t *view,
                                         uint32_t commit_seq,
                                         uint32_t read_cursor)
{
  if (!sli_log_captive_view_is_bound(view)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  return sli_log_captive_seq_distance(commit_seq, read_cursor, view->seq_modulus);
}

uint32_t sl_log_captive_reader_oldest_safe_seq(const sl_log_captive_ring_view_t *view,
                                               uint32_t commit_seq)
{
  if (!sli_log_captive_view_is_bound(view)
      || !sli_log_captive_seq_is_valid(commit_seq, view->seq_modulus)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  return sli_log_captive_seq_advance(commit_seq,
                                     view->seq_modulus
                                     - sl_log_captive_ring_safe_depth(view->capacity),
                                     view->seq_modulus);
}

uint32_t sl_log_captive_reader_lost_count(const sl_log_captive_ring_view_t *view,
                                          uint32_t commit_seq,
                                          uint32_t read_cursor)
{
  uint32_t available;
  uint32_t safe_depth;

  if (!sli_log_captive_view_is_bound(view)) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  available = sli_log_captive_seq_distance(commit_seq,
                                           read_cursor,
                                           view->seq_modulus);
  if (available == SL_LOG_CAPTIVE_INVALID_U32) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  safe_depth = sl_log_captive_ring_safe_depth(view->capacity);
  return (available > safe_depth) ? (available - safe_depth) : 0u;
}

bool sl_log_captive_reader_copy(const sl_log_captive_ring_view_t *view,
                                uint32_t expected_reset_generation,
                                uint32_t sequence,
                                sl_log_event_t *record)
{
  uint32_t slot;
  uint32_t commit_seq_after;
  uint32_t distance;

  if (!sli_log_captive_view_is_bound(view)
      || (record == NULL)
      || (view->header->reset_generation != expected_reset_generation)
      || !sli_log_captive_seq_is_valid(sequence, view->seq_modulus)) {
    return false;
  }

  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  slot = sli_log_captive_seq_slot(sequence, view->capacity);
  *record = view->records[slot];
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  commit_seq_after = view->header->commit_seq;
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  if ((view->header->magic != SL_LOG_CAPTIVE_RING_MAGIC_V1)
      || (view->header->reset_generation != expected_reset_generation)) {
    return false;
  }

  distance = sli_log_captive_seq_distance(commit_seq_after,
                                          sequence,
                                          view->seq_modulus);
  return ((distance >= 1u)
          && (distance <= sl_log_captive_ring_safe_depth(view->capacity)));
}

uint32_t sl_log_captive_reader_resume_seq(const sl_log_captive_ring_view_t *view)
{
  uint32_t commit_seq;
  uint32_t consumer_read_seq;

  commit_seq = sl_log_captive_ring_read_commit_seq(view);
  if (commit_seq == SL_LOG_CAPTIVE_INVALID_U32) {
    return SL_LOG_CAPTIVE_INVALID_U32;
  }

  consumer_read_seq = view->header->consumer_read_seq;
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  if (sli_log_captive_ack_is_plausible(view, commit_seq, consumer_read_seq)) {
    return consumer_read_seq;
  }

  return commit_seq;
}

bool sl_log_captive_reader_acknowledge(const sl_log_captive_ring_view_t *view,
                                       uint32_t expected_reset_generation,
                                       uint32_t consumer_read_seq)
{
  uint32_t commit_seq;
  uint32_t current_consumer_read_seq;
  uint32_t current_to_commit;
  uint32_t current_to_proposed;

  if (!sli_log_captive_view_is_bound(view)
      || (view->header->reset_generation != expected_reset_generation)
      || !sli_log_captive_seq_is_valid(consumer_read_seq, view->seq_modulus)) {
    return false;
  }

  commit_seq = view->header->commit_seq;
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  if (!sli_log_captive_seq_is_valid(commit_seq, view->seq_modulus)
      || !sli_log_captive_ack_is_plausible(view, commit_seq, consumer_read_seq)) {
    return false;
  }

  current_consumer_read_seq = view->header->consumer_read_seq;
  SLI_LOG_CAPTIVE_ACQUIRE_BARRIER();
  if (sli_log_captive_seq_is_valid(current_consumer_read_seq,
                                   view->seq_modulus)) {
    if (!sli_log_captive_ack_is_plausible(view,
                                          commit_seq,
                                          current_consumer_read_seq)) {
      if (consumer_read_seq != commit_seq) {
        return false;
      }
    } else {
      current_to_commit =
        sli_log_captive_seq_distance(commit_seq,
                                     current_consumer_read_seq,
                                     view->seq_modulus);
      current_to_proposed =
        sli_log_captive_seq_distance(consumer_read_seq,
                                     current_consumer_read_seq,
                                     view->seq_modulus);
      if (current_to_proposed > current_to_commit) {
        return false;
      }
    }
  }

  SLI_LOG_CAPTIVE_RELEASE_BARRIER();
  if ((view->header->magic != SL_LOG_CAPTIVE_RING_MAGIC_V1)
      || (view->header->reset_generation != expected_reset_generation)) {
    return false;
  }

  view->header->consumer_read_seq = consumer_read_seq;
  return true;
}
