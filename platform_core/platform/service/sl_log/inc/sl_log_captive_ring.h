/***************************************************************************//**
 * @file sl_log_captive_ring.h
 * @brief Single-writer captive-core shared-memory Logger ring.
 *******************************************************************************
 * SPDX-License-Identifier: Zlib
 ******************************************************************************/

#ifndef SL_LOG_CAPTIVE_RING_H
#define SL_LOG_CAPTIVE_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sl_log.h"

#if (SL_LOG_CONFIG_ARG != 3)
#error "Captive Logger requires SL_LOG_CONFIG_ARG == 3"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SL_LOG_CAPTIVE_RING_MAGIC_V1       0x534C5201u
#define SL_LOG_CAPTIVE_RING_HEADER_BYTES   24u
#define SL_LOG_CAPTIVE_RING_RECORD_BYTES   28u
#define SL_LOG_CAPTIVE_RING_MIN_BYTES      80u
#define SL_LOG_CAPTIVE_INVALID_U32         UINT32_MAX

/***************************************************************************//**
 * @brief Shared ring header mapped by both the producer and the consumer.
 *
 * Exactly one core produces into a ring and exactly one core consumes from it.
 * The consumer may be the host core or another captive core; the primitive is
 * topology-neutral and never assumes which.
 *
 * Ownership:
 *   - Producer writes @p magic, @p ring_bytes, @p reset_generation,
 *     @p commit_seq and @p producer_drop_count.
 *   - Consumer writes @p consumer_read_seq only.
 *
 * Every word is written by one core and observed by another, so all six are
 * volatile: the compiler must not cache, elide or fuse these accesses.
 * Volatile alone orders nothing between cores. The implementation additionally
 * issues a release barrier before publishing and acquire barriers after
 * observing shared state.
 *
 * Frozen ABI, 24 bytes:
 *   +0x00 magic
 *   +0x04 ring_bytes
 *   +0x08 reset_generation
 *   +0x0C commit_seq
 *   +0x10 consumer_read_seq
 *   +0x14 producer_drop_count
 ******************************************************************************/
typedef struct {
  /** Ring signature. Cleared first and written last during initialization. */
  volatile uint32_t magic;
  /** Total ring size in bytes, header plus record area plus unused tail. */
  volatile uint32_t ring_bytes;
  /** Producer session identity. See @ref sl_log_captive_ring_init. */
  volatile uint32_t reset_generation;
  /** Next sequence the producer will publish. Records below it are committed. */
  volatile uint32_t commit_seq;
  /** Sequence the single consumer has acknowledged. Consumer-owned. */
  volatile uint32_t consumer_read_seq;
  /** Events the producer discarded before a sequence was assigned. */
  volatile uint32_t producer_drop_count;
} sl_log_captive_ring_header_t;

_Static_assert(SL_LOG_CONFIG_ARG == 3, "Captive Logger requires three arguments");
_Static_assert(sizeof(sl_log_event_t) == SL_LOG_CAPTIVE_RING_RECORD_BYTES,
               "Captive Logger requires a 28-byte sl_log_event_t");
_Static_assert(sizeof(sl_log_captive_ring_header_t) == SL_LOG_CAPTIVE_RING_HEADER_BYTES,
               "Captive Logger ring header must be 24 bytes");
_Static_assert(offsetof(sl_log_captive_ring_header_t, magic) == 0x00u,
               "Captive Logger magic offset");
_Static_assert(offsetof(sl_log_captive_ring_header_t, ring_bytes) == 0x04u,
               "Captive Logger ring_bytes offset");
_Static_assert(offsetof(sl_log_captive_ring_header_t, reset_generation) == 0x08u,
               "Captive Logger reset_generation offset");
_Static_assert(offsetof(sl_log_captive_ring_header_t, commit_seq) == 0x0Cu,
               "Captive Logger commit_seq offset");
_Static_assert(offsetof(sl_log_captive_ring_header_t, consumer_read_seq) == 0x10u,
               "Captive Logger consumer_read_seq offset");
_Static_assert(offsetof(sl_log_captive_ring_header_t, producer_drop_count) == 0x14u,
               "Captive Logger producer_drop_count offset");
_Static_assert(_Alignof(sl_log_captive_ring_header_t) == 4u,
               "Captive Logger ring header must be four-byte aligned");

/***************************************************************************//**
 * @brief Per-core ring state. Private to one core and never placed in the
 *        shared window.
 *
 * @p capacity and @p seq_modulus are derived once at init or bind time so the
 * publish path needs no runtime division. @p producer_slot is meaningful only
 * on a producer-owned view and tracks the physical slot matching
 * @c header->commit_seq.
 ******************************************************************************/
typedef struct {
  sl_log_captive_ring_header_t *header;
  sl_log_event_t *records;
  uint32_t ring_bytes;
  uint32_t capacity;
  uint32_t seq_modulus;
  uint32_t producer_slot;
} sl_log_captive_ring_view_t;

/***************************************************************************//**
 * @brief Records that fit in a ring of the given total size.
 *
 * @param[in] ring_bytes Total ring size, including the 24-byte header.
 * @return Record capacity, or 0 if @p ring_bytes is below
 *         @ref SL_LOG_CAPTIVE_RING_MIN_BYTES or not a multiple of four.
 ******************************************************************************/
uint32_t sl_log_captive_ring_capacity(uint32_t ring_bytes);

/***************************************************************************//**
 * @brief Deepest backlog a consumer can still read without risking an
 *        overwrite by the producer.
 *
 * @param[in] capacity Record capacity of the ring.
 * @return @p capacity - 1, or 0 when @p capacity is 0 or 1.
 ******************************************************************************/
uint32_t sl_log_captive_ring_safe_depth(uint32_t capacity);

/***************************************************************************//**
 * @brief Advance one sequence using the canonical rollover modulus.
 *
 * @param[in] seq     Current sequence, must be below @p modulus.
 * @param[in] modulus Canonical modulus, available as @c view->seq_modulus.
 * @return Next sequence, or @ref SL_LOG_CAPTIVE_INVALID_U32 on invalid input.
 ******************************************************************************/
uint32_t sl_log_captive_seq_next(uint32_t seq, uint32_t modulus);

/***************************************************************************//**
 * @brief Initialize a ring in shared memory and bind a producer-owned view.
 *
 * Called by the producer, which owns the ring lifetime. Clears @c magic first
 * and republishes it last so a concurrently polling consumer never observes a
 * partially written header.
 *
 * @p reset_generation identifies one producer session:
 *   - Physical ring wrap does not change it.
 *   - Logical sequence rollover does not change it.
 *   - Reinitializing a ring MUST use a value different from the previous
 *     producer session, otherwise a stale consumer cannot detect the reset.
 *   - This component does not allocate generation values; the subsystem
 *     adapter owns that policy.
 *
 * @param[out] view             View to populate. Producer-owned.
 * @param[in]  shared_base      Four-byte-aligned base of the shared window.
 * @param[in]  ring_bytes       Total ring size in bytes.
 * @param[in]  reset_generation Producer session identity, see above.
 * @return true on success, false on invalid base, size or derived geometry.
 ******************************************************************************/
bool sl_log_captive_ring_init(sl_log_captive_ring_view_t *view,
                              void *shared_base,
                              uint32_t ring_bytes,
                              uint32_t reset_generation);

/***************************************************************************//**
 * @brief Attach to a ring another core has already initialized.
 *
 * Validates the untrusted shared header before deriving geometry, and bounds
 * the declared @c ring_bytes by @p accessible_bytes. An implausible or stale
 * @c consumer_read_seq does not fail the bind; use
 * @ref sl_log_captive_reader_resume_seq to pick a safe restart point.
 *
 * @param[out] view             View to populate.
 * @param[in]  shared_base      Four-byte-aligned base of the shared window.
 * @param[in]  accessible_bytes Bytes the caller may legally access at
 *                              @p shared_base.
 * @return true on success, false if the header is absent or inconsistent.
 ******************************************************************************/
bool sl_log_captive_ring_bind(sl_log_captive_ring_view_t *view,
                              void *shared_base,
                              uint32_t accessible_bytes);

/***************************************************************************//**
 * @brief Publish one already-serialized event into the ring.
 *
 * Producer hot path. Copies the record, issues a release barrier, then
 * advances @c commit_seq, so a consumer observing the new commit value always
 * observes a complete record. Performs no division, allocation, locking or
 * notification.
 *
 * Requires a producer-owned view created by @ref sl_log_captive_ring_init, or
 * by @ref sl_log_captive_ring_bind under single-producer ownership when a
 * producer recovers an existing ring. Mutates @c view->producer_slot, so the
 * view is not const.
 *
 * @param[in,out] view   Producer-owned view.
 * @param[in]     record Event to publish. Copied by value.
 * @return true when published, false if the view is unbound, the ring magic is
 *         gone, or @p record is NULL.
 ******************************************************************************/
bool sl_log_captive_ring_publish_serialized(sl_log_captive_ring_view_t *view,
                                            const sl_log_event_t *record);

/***************************************************************************//**
 * @brief Snapshot the producer commit point.
 *
 * @param[in] view Bound view.
 * @return Current @c commit_seq, or @ref SL_LOG_CAPTIVE_INVALID_U32 if the
 *         ring is unbound or the stored sequence is out of range.
 ******************************************************************************/
uint32_t sl_log_captive_ring_read_commit_seq(const sl_log_captive_ring_view_t *view);

/***************************************************************************//**
 * @brief Records committed but not yet consumed.
 *
 * The result may exceed @ref sl_log_captive_ring_safe_depth when the consumer
 * has fallen behind; use @ref sl_log_captive_reader_lost_count to size the gap.
 *
 * @param[in] view        Bound view.
 * @param[in] commit_seq  Commit snapshot from
 *                        @ref sl_log_captive_ring_read_commit_seq.
 * @param[in] read_cursor Consumer cursor.
 * @return Backlog in records, or @ref SL_LOG_CAPTIVE_INVALID_U32 on invalid
 *         input.
 ******************************************************************************/
uint32_t sl_log_captive_reader_available(const sl_log_captive_ring_view_t *view,
                                         uint32_t commit_seq,
                                         uint32_t read_cursor);

/***************************************************************************//**
 * @brief Oldest sequence still guaranteed readable at this commit point.
 *
 * Resynchronization target for a consumer that has fallen behind.
 *
 * @param[in] view       Bound view.
 * @param[in] commit_seq Commit snapshot.
 * @return Oldest safe sequence, or @ref SL_LOG_CAPTIVE_INVALID_U32 on invalid
 *         input.
 ******************************************************************************/
uint32_t sl_log_captive_reader_oldest_safe_seq(const sl_log_captive_ring_view_t *view,
                                               uint32_t commit_seq);

/***************************************************************************//**
 * @brief Records the producer overwrote before the consumer read them.
 *
 * Distinct from @c producer_drop_count, which counts events discarded by the
 * producer before a sequence was ever assigned.
 *
 * @param[in] view        Bound view.
 * @param[in] commit_seq  Commit snapshot.
 * @param[in] read_cursor Consumer cursor.
 * @return Lost record count, 0 when nothing was lost, or
 *         @ref SL_LOG_CAPTIVE_INVALID_U32 on invalid input.
 ******************************************************************************/
uint32_t sl_log_captive_reader_lost_count(const sl_log_captive_ring_view_t *view,
                                          uint32_t commit_seq,
                                          uint32_t read_cursor);

/***************************************************************************//**
 * @brief Copy one record, rejecting anything the producer may have disturbed.
 *
 * Rechecks @c reset_generation before and after the copy and re-reads
 * @c commit_seq afterwards, so the copy is rejected if the record was never
 * committed, was overwritten during the copy, or the producer reinitialized
 * the ring mid-copy.
 *
 * @param[in]  view                     Bound view.
 * @param[in]  expected_reset_generation Generation the consumer is tracking.
 * @param[in]  sequence                 Sequence to read.
 * @param[out] record                   Destination event.
 * @return true when @p record holds a stable committed event.
 ******************************************************************************/
bool sl_log_captive_reader_copy(const sl_log_captive_ring_view_t *view,
                                uint32_t expected_reset_generation,
                                uint32_t sequence,
                                sl_log_event_t *record);

/***************************************************************************//**
 * @brief Sequence a consumer should resume from after attaching or restarting.
 *
 * Returns the shared @c consumer_read_seq when it is plausible for the current
 * commit point, otherwise the current @c commit_seq, which starts a fresh
 * session rather than replaying records of unknown provenance.
 *
 * @param[in] view Bound view.
 * @return Resume sequence, or @ref SL_LOG_CAPTIVE_INVALID_U32 if unbound.
 ******************************************************************************/
uint32_t sl_log_captive_reader_resume_seq(const sl_log_captive_ring_view_t *view);

/***************************************************************************//**
 * @brief Publish the consumer cursor into the shared header.
 *
 * Monotonic within a generation: the cursor may stay put or advance up to
 * @c commit_seq, never move backwards and never run ahead of the producer. A
 * mismatched generation is rejected so a stale consumer cannot resurrect a
 * cursor from a previous producer session.
 *
 * @param[in] view                      Bound view.
 * @param[in] expected_reset_generation Generation the consumer is tracking.
 * @param[in] consumer_read_seq         Cursor to publish.
 * @return true when the cursor was accepted and stored.
 ******************************************************************************/
bool sl_log_captive_reader_acknowledge(const sl_log_captive_ring_view_t *view,
                                       uint32_t expected_reset_generation,
                                       uint32_t consumer_read_seq);

#ifdef __cplusplus
}
#endif

#endif // SL_LOG_CAPTIVE_RING_H
