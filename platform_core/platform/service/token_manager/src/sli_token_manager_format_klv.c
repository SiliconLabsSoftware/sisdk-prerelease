/***************************************************************************//**
 * @file sli_token_manager_format_klv.c
 * @brief klv read/write API handling implementation.
 * @version 1.0.0
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_device_clock.h"
#include "sl_hal_gpcrc.h"
#include "sl_token_manager_defines.h"
#include "sli_token_manager_format_klv.h"
#include "sli_token_manager_internal.h"
#include "sli_token_manager_backend_nvm.h"
#include "sl_memory_manager.h"
#include <stdio.h>
#if defined(SL_TOKEN_MANAGER_BACKEND_EXT_FLASH)
#include "sl_se_manager_util.h"
#endif
#if defined(SL_TOKEN_MANAGER_SECURITY)
#include "sli_token_manager_hal_crypto.h"
#include "sl_token_manager_config.h"
#endif

// CRC Polynomial and initialization value configuration
#define CRC16_POLY          0x1021U
#define INIT_VALUE_CRC16    0xFFFFU

// CRC Instance and Clock enable configuration
#if defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH)
#define GPCRC_INSTANCE GPCRC
#define GPCRC_CLKEN    CMU_CLKEN0_GPCRC
#else
#define GPCRC_INSTANCE GPCRC0
#define GPCRC_CLKEN    CMU_CLKEN0_GPCRC0
#endif

/***************************************************************************//**
 * Normalized KLV layout after magic-based decode (@ref sl_klv_header_t or
 * @ref sli_klv_obj_header_t).
 ******************************************************************************/
typedef struct {
  uint16_t key;                ///< 16-bit KLV key
  uint16_t payload_crc;        ///< 16-bit CRC of the payload
  uint32_t payload_length;     ///< 32-bit length of the payload
  uint8_t header_size;         ///< header size in bytes
  bool is_readable;            ///< flag indicating if the object is readable
  bool is_write_in_progress;   ///< ACTIVE object with WIP still set (stream/one-shot provisional)
  bool is_torn;                ///< Non-erased but non-followable header (including
                               ///< partial magic/length/key/CRC). Not proof that
                               ///< there are no later committed transactions.
                               ///< Discovery stops here. Flash must be quarantined.
  uint32_t aad_length;         ///< length of the AAD in bytes used in static secure tokens
} sli_klv_hdr_parse_t;

//--- Series 2 ---
// KLV support for static tokens read/write is not supported.
// However, KLV support for upgradable tokens is supported.
// --- Series 3 ---
// KLV support for static tokens and upgradable tokens read/write is supported.
#if (!defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH) \
     || (defined(SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS)))
#define SLI_TOKEN_MANAGER_KLV_COMMON_SUPPORT 1
#endif

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS)


///< UT object information.
typedef struct {
  uint32_t obj_addr;                     ///< address of the current KLV object
  uint32_t next_addr;                    ///< address of the next KLV object in the chain.
  sli_klv_hdr_parse_t header;            ///< header information of the KLV object.
} sli_klv_ut_obj_info_t;

///< UT chain scan result
typedef struct {
  uint32_t tail;                         ///< address of the tail of the KLV chain.
  sli_klv_ut_obj_info_t match;           ///< information of the matching KLV object.
  uint32_t largest_live_obj_size;        ///< size of the largest live KLV object in the chain.
  uint32_t reclaimable_bytes;            ///< Deleted/unreadable bytes in the committed
                                         ///< live span [anchor, tail], plus unreadable
                                         ///< stale objects in the logical prefix (when
                                         ///< scanned). May exceed occupied_bytes.
  uint32_t live_span_reclaimable_bytes;  ///< Deleted/unreadable bytes in the committed
                                         ///< live span [anchor, tail] only. Always
                                         ///< <= occupied_bytes. Includes ACTIVE+WIP.
  uint32_t live_span_wip_bytes;          ///< WIP bytes inside the live span.
                                         ///< Still counted in live_span_reclaimable_bytes
                                         ///< for erase/free accounting, but treated as
                                         ///< committed capacity by the registered-capacity
                                         ///< gate (concurrent direct streams).
  uint32_t largest_wip_obj_size;         ///< Largest WIP object in the live span.
  uint32_t occupied_bytes;               ///< All accounted bytes in the committed
                                         ///< live span [anchor, tail], live + reclaimable.
  bool chain_empty;                      ///< flag indicating if the KLV chain is empty.
  bool match_found;                      ///< flag indicating if a matching KLV object is found.
  bool chain_full;                       ///< flag indicating if the KLV chain is full.
  bool torn_tail;                        ///< Unresolved programmed HEADER_PENDING
                                         ///< (or a torn header found by an unbounded
                                         ///< legacy scan). STABLE flash beyond the
                                         ///< authoritative chain is NOT torn.
  bool wrap_start_allowed;               ///< True when the logical prefix before the
                                         ///< anchor is clear enough to wrap a write to
                                         ///< region start (stale prefix objects clear this).
} sli_klv_ut_scan_result_t;

///< UT reclaim state shared across the entire UT reclaim operation.
typedef struct {
  uint32_t initial_anchor;               ///< Current head for in-walk window geometry.
                                         ///< Updated by refresh_reclaim_window() after a
                                         ///< mid-walk anchor commit. The walk-start head
                                         ///< lives in reservation.initial_anchor.
  uint32_t initial_tail;                 ///< address of the initial tail.
  uint32_t erase_stop_addr;              ///< page containing data that must remain recoverable.
  uint32_t reclaim_gap_start;            ///< address of the start of the reclaim gap.
  uint32_t reclaim_gap_end;              ///< Logical end of the reclaim gap (live head).
                                         ///< Admission reserve ends at this exact
                                         ///< address; page erase still stops at
                                         ///< SLI_KLV_ANCHOR_PAGE_START of it.
  uint32_t repack_start;                 ///< Start of the reserve window. The window
                                         ///< ends at the current chain anchor and
                                         ///< moves with it. This is not the erase cursor.
  uint32_t repack_reserve_size;          ///< size of the repack reserve.
  uint32_t erase_cursor;                 ///< Next page-aligned address reclaim may erase.
                                         ///< Moves only forward, and never onto a live page.
  bool initial_chain_wrapped;            ///< flag indicating if the initial chain is wrapped.
  bool repack_uses_gap_window;           ///< True when the reserve is the span that
                                         ///< ends at the live anchor.
} sli_klv_ut_reclaim_window_t;

///< UT write plan shared across the entire UT write operation.
typedef struct {
  uint32_t chain_anchor;                 ///< address of the chain anchor.
  uint32_t tail;                         ///< address of the tail of the chain.
  uint32_t object_size;                  ///< size of the object.
  uint32_t repack_reserve_size;          ///< size of the repack reserve.
  uint32_t repack_guard_bytes;           ///< size of the repack guard bytes.
  bool chain_empty;                      ///< flag indicating if the chain is empty.
  bool chain_full;                       ///< flag indicating if the chain is full.
  bool wrap_start_allowed;               ///< flag indicating if the wrap of the token data is allowed.
} sli_klv_ut_write_plan_t;

/***************************************************************************//**
 * Holds the reservation outcome for a UT stream write.
 ******************************************************************************/
typedef struct {
  uint32_t write_addr;                    ///< Address of the NVM memory where the data is written.
  uint32_t erase_cursor;                  ///< Address of the erase cursor.
  uint32_t first_relocated_addr;          ///< First relocated object, or on the
                                          ///< head-overwrite append shortcut the
                                          ///< first surviving in-place live object
                                          ///< after the skipped head (see
                                          ///< reclaim_final_anchor).
  uint32_t target_delete_addr;            ///< Address of the target delete object.
  uint32_t initial_anchor;                ///< Anchor when a reclaim walk started.
  bool chain_empty;                       ///< Flag indicating if the chain is empty.
  bool used_reclaim;                      ///< Flag indicating if the reclaim is used.
  bool target_delete_pending;             ///< Flag indicating if the target delete is pending.
  bool target_delete_was_anchor;          ///< Flag indicating if the target delete object is the anchor object.
} sli_klv_ut_stream_reservation_t;

/***************************************************************************//**
 * Outcome of the shared UT reclaim relocation walk.
 *
 * @p reservation carries the same fields persisted for streaming finish and for
 * direct reclaim-and-write completion. @p reclaim_window retains in-flight
 * reclaim geometry (repack bounds, gap layout) that streaming does not need
 * after the walk completes.
 ******************************************************************************/
typedef struct {
  sli_klv_ut_reclaim_window_t reclaim_window;
  sli_klv_ut_stream_reservation_t reservation;
} sli_klv_ut_reclaim_result_t;

/***************************************************************************//**
 * In/out bundle for @ref try_relocate_pinned_overwrite_target.
 ******************************************************************************/
typedef struct {
  uint32_t *write_addr;                   ///< Reclaim write frontier (advanced past the relocated head).
  uint32_t *chain_anchor;                 ///< Persisted logical head (advanced to the next live object).
  uint32_t *target_delete_addr;           ///< Overwrite target; retargeted at the relocated old value.
  uint32_t *reclaim_erase_stop_addr;      ///< Local erase-stop mirror (kept in sync with the window).
  uint32_t *first_relocated_addr;         ///< Post-write head candidate (next live survivor when present).
  uint32_t tail;                          ///< Walk-start tail used as the soft-deleted-prefix bound.
  uint32_t repack_guard_bytes;            ///< Guard size passed to @ref refresh_reclaim_window.
  sli_klv_ut_reclaim_window_t *reclaim_window; ///< In-flight reclaim geometry.
} sli_klv_ut_pinned_head_relocate_t;

/***************************************************************************//**
 * Internal streaming-write context for upgradable tokens.
 *
 * One node is dynamically allocated per active streaming session and keyed by
 * token ID. Direct-append sessions may run concurrently, while a session that
 * reserved space through reclaim serializes other writers until finish.
 ******************************************************************************/
 typedef struct sli_stream_context_node_t {
  struct sli_stream_context_node_t *next;       ///< Pointer to the next stream context node.
  sli_klv_ut_stream_reservation_t reservation;  ///< Reservation information for the stream.
  uint32_t *chain_anchor;                       ///< Pointer to the chain anchor.
  uint32_t token;                               ///< Token identifier.
  uint32_t total_length;                        ///< Total length of the token data.
  uint32_t bytes_written;                       ///< Number of bytes written.
  uint32_t payload_addr;                        ///< Address of the payload in the NVM memory.
  uint32_t next_write_addr;                     ///< Address of the free slot in the NVM memory for the next write.
  uint32_t remaining_length;                    ///< Remaining length of the token data.
  uint32_t klv_start_address;                   ///< Start address of the KLV region.
  uint32_t klv_end_address;                     ///< End address of the KLV region.
  uint32_t total_region_size;                   ///< Logical layout size stamped at start (base + active extensions).
  uint32_t extension_info_count;                ///< Active extension count stamped at start.
  const sli_klv_extension_header_t *extension_info; ///< Active extension table stamped at start.
  uint8_t pending_buf[sizeof(uint32_t)];        ///< Buffer used to store unaligned bytes from the previous update until they can be written.
  uint8_t pending_len;                          ///< Length of the pending_buf content to be written.
  bool is_active;                               ///< Flag indicating if the stream is active.
} sli_stream_context_node_t;

/***************************************************************************//**
 * Half-open logical range inside the flattened UT address space.
 *
 * The KLV backend models the base UT region followed by each active extension
 * as one logical circular buffer. Overlap checks operate on offsets in that
 * logical buffer, so each segment here is represented as [start_offset, end_offset).
 ******************************************************************************/
 typedef struct {
  uint32_t start_offset;
  uint32_t end_offset;
} sli_klv_logical_range_t;

/***************************************************************************//**
 * Head of the linked list of active UT stream-write sessions, keyed by token ID.
 ******************************************************************************/
static sli_stream_context_node_t *sli_stream_ctx_head = NULL;

static sl_status_t erase_reclaimed_pages(uint32_t *erase_cursor,
                                         uint32_t reclaim_progress_addr,
                                         uint32_t write_base,
                                         uint32_t relocation_floor,
                                         uint32_t klv_start_address,
                                         uint32_t klv_end_address);
static uint32_t reclaim_final_anchor(const sli_klv_ut_stream_reservation_t *reservation,
                                     uint32_t chain_anchor);
static uint32_t reclaim_final_erase_cursor(uint32_t erase_cursor,
                                           uint32_t initial_anchor,
                                           uint32_t final_anchor,
                                           uint32_t live_bytes,
                                           uint32_t klv_start_address,
                                           uint32_t klv_end_address);
static bool reclaim_page_overlaps_committed_live_chain(uint32_t addr,
                                                       uint32_t final_anchor,
                                                       uint32_t live_bytes,
                                                       uint32_t klv_start_address,
                                                       uint32_t klv_end_address);
static bool reclaim_destination_overlaps_live_head(uint32_t write_addr,
                                                   uint32_t required_size,
                                                   uint32_t gap_end,
                                                   uint32_t klv_start_address,
                                                   uint32_t klv_end_address);
static sl_status_t reclaim_final_erase_sweep(uint32_t erase_cursor,
                                             uint32_t initial_anchor,
                                             uint32_t final_anchor,
                                             uint32_t write_addr,
                                             uint32_t written_object_size,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address);
static uint32_t predict_reclaim_erase_cursor(uint32_t erase_cursor,
                                             uint32_t reclaim_progress_addr,
                                             uint32_t write_base,
                                             uint32_t relocation_floor,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address);
static sl_status_t scan_tail_from_anchor(uint32_t chain_anchor,
                                         uint32_t klv_start_address,
                                         uint32_t klv_end_address,
                                         uint32_t *tail);
static sl_status_t scan_largest_live_object_size(uint32_t chain_anchor,
                                                 uint32_t tail,
                                                 uint32_t klv_start_address,
                                                 uint32_t klv_end_address);
static sl_status_t ut_refresh_largest_live_object_cache(uint32_t chain_anchor,
                                                        uint32_t klv_start_address,
                                                        uint32_t klv_end_address);
static sl_status_t ut_largest_live_object_cache_ensure_valid(uint32_t chain_anchor,
                                                             uint32_t klv_start_address,
                                                             uint32_t klv_end_address);
static void ut_largest_live_object_cache_bump(uint32_t object_size);
static sl_status_t ut_largest_live_object_cache_on_delete(uint32_t chain_anchor,
                                                          uint32_t klv_start_address,
                                                          uint32_t klv_end_address,
                                                          uint32_t deleted_object_size);
static sl_status_t ut_largest_live_object_cache_on_write_commit(uint32_t chain_anchor,
                                                                uint32_t klv_start_address,
                                                                uint32_t klv_end_address,
                                                                uint32_t new_object_size,
                                                                uint32_t replaced_object_size,
                                                                bool replaced_was_live);
static sl_status_t activate_pending_extensions_for_reclaim(const sli_klv_ut_context_t *ut_context,
                                                           sli_klv_ut_context_t *registered_context,
                                                           bool *activated_out);
static sl_status_t try_direct_append_after_extension_activation(const sli_klv_ut_context_t *ut_context,
                                                                const sli_klv_ut_scan_result_t *scan_result,
                                                                uint32_t object_size,
                                                                uint32_t *write_addr_out);
static sl_status_t try_direct_write_after_extension_activation(const sl_klv_handle_t *klv_handle,
                                                               uint32_t *chain_anchor,
                                                               const sli_klv_ut_context_t *reclaim_context,
                                                               const sli_klv_ut_scan_result_t *scan_result,
                                                               uint32_t new_object_size,
                                                               uint32_t replaced_object_size,
                                                               bool replaced_was_live,
                                                               void const *data,
                                                               bool *completed_out);
static bool check_object_fits_registered_capacity(const sli_klv_ut_scan_result_t *scan_result,
                                                  uint32_t new_object_size,
                                                  uint32_t registered_total_size);
static sl_status_t decode_object_from_anchor(uint32_t obj_addr,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address,
                                             sli_klv_ut_obj_info_t *obj_info);

///< Token value that requests a full-chain @ref scan_from_anchor walk.
#define SLI_KLV_UT_FULL_CHAIN_SCAN_TOKEN 0xFFFFU

///< Cache of the largest live object size in the UT chain.
static uint32_t largest_live_object_size = 0U;

///< Flag indicating if the largest live object cache is valid.
static bool largest_live_object_cache_valid = false;

///< Snapshot of the currently active UT logical layout.
static sli_klv_ut_context_t active_ut_context = { 0 };
static bool active_ut_context_valid = false;

/***************************************************************************//**
 * Sets the active UT context.
 *
 * @param[in] ut_context            The UT context.
 ******************************************************************************/
static void set_active_ut_context(const sli_klv_ut_context_t *ut_context)
{
  if (ut_context == NULL) {
    active_ut_context_valid = false;
    memset(&active_ut_context, 0, sizeof(active_ut_context));
    return;
  }

  // Use this snapshot to track the active UT context.
  active_ut_context = *ut_context;
  if ((active_ut_context.extension_info == NULL)
      || (active_ut_context.extension_info_count > SLI_TOKEN_MANAGER_EXT_INFO_MAX)
      || (active_ut_context.total_region_size < (active_ut_context.klv_end_address
                                                 - active_ut_context.klv_start_address))) {
    // The active UT context is invalid, reset the extension info.
    active_ut_context.extension_info = NULL;
    active_ut_context.extension_info_count = 0U;
    active_ut_context.total_region_size =
      active_ut_context.klv_end_address - active_ut_context.klv_start_address;
  }
  active_ut_context_valid = true;
}

/***************************************************************************//**
 * Checks if the active UT context matches the given KLV region.
 *
 * @param[in] klv_start_address     The start address of the KLV region.
 * @param[in] klv_end_address       The end address of the KLV region.
 *
 * @return true if the active UT context matches the given KLV region, false otherwise.
 ******************************************************************************/
static bool ut_context_matches(uint32_t klv_start_address,
                               uint32_t klv_end_address)
{
  return (active_ut_context_valid
          && (active_ut_context.klv_start_address == klv_start_address)
          && (active_ut_context.klv_end_address == klv_end_address));
}

/***************************************************************************//**
 * Calculates the total size of the region. Including the size of the active extensions.
 *
 * @param[in] klv_start_address     The start address of the KLV region.
 * @param[in] klv_end_address       The end address of the KLV region.
 *
 * @return The total size of the region.
 ******************************************************************************/
static uint32_t get_total_region_size(uint32_t klv_start_address,
                                      uint32_t klv_end_address)
{
  if (ut_context_matches(klv_start_address, klv_end_address)) {
    return active_ut_context.total_region_size;
  }

  return klv_end_address - klv_start_address;
}

/***************************************************************************//**
 * Checks if an address is within a segment.
 *
 * @param[in] address               The address.
 * @param[in] segment_start         The start address of the segment.
 * @param[in] segment_end           The end address of the segment.
 *
 * @return true if the address is within the segment, false otherwise.
 ******************************************************************************/
static bool check_address_in_segment(uint32_t address,
                                     uint32_t segment_start,
                                     uint32_t segment_end)
{
  return ((address >= segment_start) && (address < segment_end));
}

/***************************************************************************//**
 * Returns whether @p ut_context describes a usable active UT layout.
 *
 * This mirrors the acceptance checks in @ref set_active_ut_context() so callers
 * can reject a layout before committing irreversible side effects (for example
 * extension activation metadata).
 ******************************************************************************/
static bool ut_context_layout_is_valid(const sli_klv_ut_context_t *ut_context)
{
  uint32_t base_size = 0U;

  if (ut_context == NULL) {
    return false;
  }

  if (ut_context->klv_start_address >= ut_context->klv_end_address) {
    return false;
  }

  base_size = ut_context->klv_end_address - ut_context->klv_start_address;
  if (ut_context->total_region_size < base_size) {
    return false;
  }

  if (ut_context->extension_info_count == 0U) {
    return (ut_context->total_region_size == base_size);
  }

  return ((ut_context->extension_info != NULL)
          && (ut_context->extension_info_count <= SLI_TOKEN_MANAGER_EXT_INFO_MAX));
}

/***************************************************************************//**
 * Restores @ref active_ut_context from the layout stamped into a stream session.
 *
 * Streaming update/finish must keep using the layout that was active when the
 * reservation was made, even if an intervening public API rebuilt the caller's
 * context snapshot or reset the process-wide active layout.
 ******************************************************************************/
static void stream_apply_stamped_layout(const sli_stream_context_node_t *context)
{
  sli_klv_ut_context_t layout = { 0 };

  if (context == NULL) {
    return;
  }

  layout.chain_anchor = context->chain_anchor;
  layout.klv_start_address = context->klv_start_address;
  layout.klv_end_address = context->klv_end_address;
  layout.total_region_size = context->total_region_size;
  layout.extension_info = context->extension_info;
  layout.extension_info_count = context->extension_info_count;
  set_active_ut_context(&layout);
}

/***************************************************************************//**
 * Stamps the currently active logical layout into a stream session.
 ******************************************************************************/
static void stream_stamp_active_layout(sli_stream_context_node_t *context)
{
  if (context == NULL) {
    return;
  }

  context->total_region_size = get_total_region_size(context->klv_start_address,
                                                     context->klv_end_address);
  if (active_ut_context_valid
      && (active_ut_context.klv_start_address == context->klv_start_address)
      && (active_ut_context.klv_end_address == context->klv_end_address)) {
    context->extension_info = active_ut_context.extension_info;
    context->extension_info_count = active_ut_context.extension_info_count;
  } else {
    context->extension_info = NULL;
    context->extension_info_count = 0U;
  }
}

/***************************************************************************//**
 * Gets the segment bounds by address.
 *
 * @param[in] address               The address.
 * @param[in] klv_start_address     The start address of the KLV region.
 * @param[in] klv_end_address       The end address of the KLV region.
 * @param[out] segment_start_out    The start address of the segment.
 * @param[out] segment_end_out       The end address of the segment.
 *
 * @return true if the segment bounds are found, false otherwise.
 ******************************************************************************/
static bool get_segment_bounds_by_address(uint32_t address,
                                          uint32_t klv_start_address,
                                          uint32_t klv_end_address,
                                          uint32_t *segment_start_out,
                                          uint32_t *segment_end_out)
{
  if ((segment_start_out == NULL) || (segment_end_out == NULL)) {
    return false;
  }

  // Check if the address is in the base KLV region.
  if (check_address_in_segment(address, klv_start_address, klv_end_address)) {
    *segment_start_out = klv_start_address;
    *segment_end_out = klv_end_address;
    return true;
  }

  if (!ut_context_matches(klv_start_address, klv_end_address)) {
    return false;
  }

  // Check if the address is in any of the active extensions.
  for (uint32_t i = 0U; i < active_ut_context.extension_info_count; i++) {
    uint32_t ext_start = 0U;
    uint32_t ext_end = 0U;

    ext_start = active_ut_context.extension_info[i].extension_addr;
    ext_end = ext_start + active_ut_context.extension_info[i].extension_size;
    if (check_address_in_segment(address, ext_start, ext_end)) {
      *segment_start_out = ext_start;
      *segment_end_out = ext_end;
      return true;
    }
  }

  return false;
}

/***************************************************************************//**
 * Converts a physical address to a logical offset.
 *
 * @param[in] address               The physical address.
 * @param[in] klv_start_address     The start address of the KLV region.
 * @param[in] klv_end_address       The end address of the KLV region.
 * @param[out] offset_out           The logical offset.
 *
 * @return true if the conversion is successful, false otherwise.
 ******************************************************************************/
static bool convert_address_to_logical_offset(uint32_t address,
                                              uint32_t klv_start_address,
                                              uint32_t klv_end_address,
                                              uint32_t *offset_out)
{
  uint32_t offset = 0U;

  if (offset_out == NULL) {
    return false;
  }

  // Check if the address is in the base KLV region.
  if (check_address_in_segment(address, klv_start_address, klv_end_address)) {
    *offset_out = address - klv_start_address;
    return true;
  }

  offset = klv_end_address - klv_start_address;
  if (!ut_context_matches(klv_start_address, klv_end_address)) {
    return false;
  }

  // Check if the address is in any of the active extensions.
  for (uint32_t i = 0U; i < active_ut_context.extension_info_count; i++) {
    uint32_t ext_start = 0U;
    uint32_t ext_size = 0U;

    ext_start = active_ut_context.extension_info[i].extension_addr;
    ext_size = active_ut_context.extension_info[i].extension_size;
    if (check_address_in_segment(address, ext_start, ext_start + ext_size)) {
      *offset_out = offset + (address - ext_start);
      return true;
    }

    offset += ext_size;
  }

  return false;
}

/***************************************************************************//**
 * Converts a logical offset to a physical address.
 *
 * @param[in] offset                The logical offset.
 * @param[in] klv_start_address     The start address of the KLV region.
 * @param[in] klv_end_address       The end address of the KLV region.
 * @param[out] address_out          The physical address.
 *
 * @return true if the conversion is successful, false otherwise.
 ******************************************************************************/
static bool convert_logical_offset_to_address(uint32_t offset,
                                              uint32_t klv_start_address,
                                              uint32_t klv_end_address,
                                              uint32_t *address_out)
{
  uint32_t base_size = klv_end_address - klv_start_address;

  if (address_out == NULL) {
    return false;
  }

  // Check if the offset is in the base KLV region.
  if (offset < base_size) {
    *address_out = klv_start_address + offset;
    return true;
  }

  offset -= base_size;
  if (!ut_context_matches(klv_start_address, klv_end_address)) {
    return false;
  }

  // Check if the offset falls within any of the active extensions.
  for (uint32_t i = 0U; i < active_ut_context.extension_info_count; i++) {
    uint32_t ext_start = 0U;
    uint32_t ext_size = 0U;

    ext_start = active_ut_context.extension_info[i].extension_addr;
    ext_size = active_ut_context.extension_info[i].extension_size;
    if (offset < ext_size) {
      *address_out = ext_start + offset;
      return true;
    }

    offset -= ext_size;
  }

  return false;
}

/***************************************************************************//**
 * Calculates the number of bytes until the end of the segment.
 *
 * @param[in] address               The address.
 * @param[in] klv_start_address     The start address of the KLV region.
 * @param[in] klv_end_address       The end address of the KLV region.
 *
 * @return The number of bytes until the end of the segment.
 ******************************************************************************/
static uint32_t cal_bytes_until_segment_end(uint32_t address,
                                            uint32_t klv_start_address,
                                            uint32_t klv_end_address)
{
  uint32_t segment_start = 0U;
  uint32_t segment_end = 0U;

  if (!get_segment_bounds_by_address(address,
                                     klv_start_address,
                                     klv_end_address,
                                     &segment_start,
                                     &segment_end)) {
    return 0U;
  }

  (void)segment_start;
  return segment_end - address;
}


/***************************************************************************//**
 * Compares two logical addresses.
 *
 * @note Both operands must already be known to live inside the UT ring, that
 * is @ref is_address_in_range() must hold for each. An int32_t has no value left
 * to report "not comparable", so an operand that cannot be mapped to a logical
 * offset is reported as equal to the other one. Callers testing `>= 0` or
 * `<= 0` would then read that as an ordering result they can act on.
 *
 * Every caller in this file satisfies the precondition, because the operands
 * are only ever one of:
 * - @p klv_start_address, which is logical offset 0 by construction;
 * - a value returned by @ref advance_ut_adress(), which maps back through the
 *   logical ring and therefore lands on a real segment byte;
 * - a page-aligned address derived with SLI_KLV_ANCHOR_PAGE_START() from an
 *   in-range address. That macro masks against an absolute page boundary, so it
 *   stays inside its segment only because the base region and every registered
 *   extension are page aligned and page sized. Both are enforced:
 *   sl_token_manager_extend_storage() rejects a misaligned or non page multiple
 *   extension, and upgradable token init rejects a region size that is not a
 *   page multiple. The two helpers that page-align a reserve candidate also
 *   re-check it with @ref is_address_in_range() before comparing it.
 *
 * Keep that list true when adding callers, or validate the operand first.
 *
 * @param[in] lhs                 The left-hand side address.
 * @param[in] rhs                 The right-hand side address.
 * @param[in] klv_start_address   The start address of the KLV region.
 * @param[in] klv_end_address     The end address of the KLV region.
 *
 * @return -1 if lhs is less than rhs, 1 if lhs is greater than rhs, 0 if they are equal.
 ******************************************************************************/
static int32_t compare_logical_address(uint32_t lhs,
                                       uint32_t rhs,
                                       uint32_t klv_start_address,
                                       uint32_t klv_end_address)
{
  uint32_t lhs_offset = 0U;
  uint32_t rhs_offset = 0U;

  // Get the logical offsets of the provided physical addresses.
  if (!convert_address_to_logical_offset(lhs,
                                         klv_start_address,
                                         klv_end_address,
                                         &lhs_offset)
      || !convert_address_to_logical_offset(rhs,
                                            klv_start_address,
                                            klv_end_address,
                                            &rhs_offset)) {
    // Unreachable while the precondition above holds. Report it under debug
    // rather than silently answering "equal", since a caller that trips this is
    // about to make a wrap or free-space decision from a meaningless result.
    TOKENDBG(printf("compare_logical_address: unmappable operand lhs=0x%08lx rhs=0x%08lx\r\n",
                    (unsigned long)lhs,
                    (unsigned long)rhs); )
    return 0;
  }

  if (lhs_offset < rhs_offset) {
    return -1;
  }

  if (lhs_offset > rhs_offset) {
    return 1;
  }

  return 0;
}

/***************************************************************************//**
 * Calculates the distance forward from one address to another.
 *
 * @param[in] from_addr           The starting address.
 * @param[in] to_addr             The ending address.
 * @param[in] klv_start_address   The start address of the KLV region.
 * @param[in] klv_end_address     The end address of the KLV region.
 *
 * @return The distance forward from from_addr to to_addr.
 ******************************************************************************/
static uint32_t cal_distance_forward(uint32_t from_addr,
                                     uint32_t to_addr,
                                     uint32_t klv_start_address,
                                     uint32_t klv_end_address)
{
  uint32_t from_offset = 0U;
  uint32_t to_offset = 0U;
  uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);

  if ((total_region_size == 0U)
      || !convert_address_to_logical_offset(from_addr,
                                            klv_start_address,
                                            klv_end_address,
                                            &from_offset)
      || !convert_address_to_logical_offset(to_addr,
                                            klv_start_address,
                                            klv_end_address,
                                            &to_offset)) {
    return 0U;
  }

  if (to_offset >= from_offset) {
    return to_offset - from_offset;
  }

  return total_region_size - (from_offset - to_offset);
}

/***************************************************************************//**
 * Splits a logical UT range into one or two non-wrapping half-open ranges.
 *
 * The caller passes a logical start offset and a byte count within the combined
 * UT address space. If the range does not wrap past the logical end, a single
 * range is produced. If it wraps, the range is split into:
 * 1. the suffix from start_offset to total_region_size, and
 * 2. the prefix from 0 to wrapped_end.
 *
 * This keeps the later overlap test iterative and avoids recursive re-entry.
 *
 * @param[in]  start_offset        Start offset in the logical UT space.
 * @param[in]  size                Size of the range in bytes.
 * @param[in]  total_region_size   Total logical UT size in bytes.
 * @param[out] ranges_out          Output array of size 2.
 *
 * @return Number of valid ranges written into @p ranges_out.
 ******************************************************************************/
static uint32_t split_logical_range(uint32_t start_offset,
                                    uint32_t size,
                                    uint32_t total_region_size,
                                    sli_klv_logical_range_t ranges_out[2])
{
  uint64_t end_offset = 0U;

  if ((ranges_out == NULL) || (size == 0U) || (total_region_size == 0U)) {
    return 0U;
  }

  end_offset = (uint64_t)start_offset + (uint64_t)size;
  if (end_offset <= total_region_size) {
    ranges_out[0].start_offset = start_offset;
    ranges_out[0].end_offset = (uint32_t)end_offset;
    ranges_out[1].start_offset = 0U;
    ranges_out[1].end_offset = 0U;
    return 1U;
  }

  // Wrapped logical ranges are represented as a suffix plus a prefix. This
  // keeps the later overlap check equivalent to the original non-extension
  // implementation while also supporting multi-region logical layouts.
  ranges_out[0].start_offset = start_offset;
  ranges_out[0].end_offset = total_region_size;
  ranges_out[1].start_offset = 0U;
  ranges_out[1].end_offset = (uint32_t)(end_offset - total_region_size);
  return 2U;
}

/***************************************************************************//**
 * Returns whether two non-wrapping logical ranges overlap.
 *
 * Both inputs must already be normalized to half-open [start, end) intervals.
 * Using half-open ranges preserves the original overlap behavior from the
 * contiguous-buffer implementation: touching endpoints do not count as overlap.
 ******************************************************************************/
static bool check_logical_ranges_overlap(const sli_klv_logical_range_t *range_a,
                                         const sli_klv_logical_range_t *range_b)
{
  if ((range_a == NULL) || (range_b == NULL)) {
    return false;
  }

  return (range_a->start_offset < range_b->end_offset)
         && (range_b->start_offset < range_a->end_offset);
}

/***************************************************************************//**
 * Checks if a range overlaps with another range.
 *
 * @param[in] addr                The starting address of the range.
 * @param[in] size                The size of the range.
 * @param[in] range_start         The starting address of the other range.
 * @param[in] range_size          The size of the other range.
 * @param[in] klv_start_address   The start address of the KLV region.
 * @param[in] klv_end_address     The end address of the KLV region.
 *
 * @return true if the ranges overlap, false otherwise.
 ******************************************************************************/
static bool check_range_overlaps(uint32_t addr,
                                 uint32_t size,
                                 uint32_t range_start,
                                 uint32_t range_size,
                                 uint32_t klv_start_address,
                                 uint32_t klv_end_address)
{
  uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  uint32_t addr_offset = 0U;
  uint32_t range_offset = 0U;
  sli_klv_logical_range_t addr_ranges[2] = { 0 };
  sli_klv_logical_range_t repack_ranges[2] = { 0 };
  uint32_t addr_range_count = 0U;
  uint32_t repack_range_count = 0U;

  if ((size == 0U) || (range_size == 0U) || (total_region_size == 0U)) {
    return false;
  }

  // Reject addresses that are outside the base UT region and all active
  // extensions. The caller may only test overlap for bytes that belong to the
  // combined logical UT storage space.
  if (!convert_address_to_logical_offset(addr,
                                         klv_start_address,
                                         klv_end_address,
                                         &addr_offset)
      || !convert_address_to_logical_offset(range_start,
                                            klv_start_address,
                                            klv_end_address,
                                            &range_offset)) {
    return false;
  }

  if ((size >= total_region_size) || (range_size >= total_region_size)) {
    return true;
  }

  // Convert both physical address ranges into logical-offset ranges inside the
  // flattened UT space. Wrapped logical ranges are split into at most two
  // non-wrapping pieces so the overlap test stays iterative.
  addr_range_count = split_logical_range(addr_offset,
                                         size,
                                         total_region_size,
                                         addr_ranges);
  repack_range_count = split_logical_range(range_offset,
                                           range_size,
                                           total_region_size,
                                           repack_ranges);

  if ((addr_range_count == 0U) || (repack_range_count == 0U)) {
    return false;
  }

  for (uint32_t addr_idx = 0U; addr_idx < addr_range_count; addr_idx++) {
    for (uint32_t repack_idx = 0U; repack_idx < repack_range_count; repack_idx++) {
      if (check_logical_ranges_overlap(&addr_ranges[addr_idx],
                                       &repack_ranges[repack_idx])) {
        return true;
      }
    }
  }

  return false;
}

#endif // SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS

#if defined(SLI_TOKEN_MANAGER_KLV_COMMON_SUPPORT)
/***************************************************************************//**
 * Returns the CRC16 value of the data.
 *
 * @note The CRC16-CCITT is used.
 *
 * @param[in]  data  Pointer to data for which CRC needs to be calculated.
 * @param[in]  len   Length of the data.
 * @param[in]  seed  The seed value for the CRC calculation.
 *
 * @return a valid CRC16 value. zero otherwise.
 ******************************************************************************/
static uint16_t calculate_checksum_with_seed(const uint8_t *data,
                                             uint32_t len,
                                             uint16_t seed)
{
 #if defined(_CMU_CLKEN0_MASK)
  CMU->CLKEN0_SET = GPCRC_CLKEN;
 #endif
  GPCRC_INSTANCE->EN = GPCRC_EN_EN;
  GPCRC_INSTANCE->CTRL = CRC16_POLY;
  GPCRC_INSTANCE->INIT = seed;
  GPCRC_INSTANCE->CMD = GPCRC_CMD_INIT;

  while (len--) {
    GPCRC_INSTANCE->INPUTDATABYTE = *data++;
  }

  return (uint16_t)GPCRC_INSTANCE->DATA;
}

/***************************************************************************//**
 * Returns the CRC16 value of the data.
 *
 * @note The CRC16-CCITT is used.
 *
 * @param[in]  data  Pointer to data for which CRC needs to be calculated.
 * @param[in]  len   Length of the data.
 *
 * @return a valid CRC16 value. zero otherwise.
 ******************************************************************************/
static uint16_t calculate_checksum(const uint8_t *data, uint32_t len)
{
  return calculate_checksum_with_seed(data, len, INIT_VALUE_CRC16);
}

/***************************************************************************//**
 * Returns the CRC16 value of the data.
 *
 * @note The CRC16-CCITT is used.
 *
 * @param[in]  data  Pointer to data for which CRC needs to be calculated.
 * @param[in]  len   Length of the data.
 *
 * @return a valid CRC16 value. zero otherwise.
 ******************************************************************************/
uint16_t sli_klv_compute_crc16(const uint8_t *data, uint32_t len)
{
  if (data == NULL) {
    return 0U;
  }

  return calculate_checksum(data, len);
}

/***************************************************************************//**
 * Decodes the KLV header gets CRC, length and key information
 *
 * @note
 *
 * @param[in]  obj_adr           Address to the klv object.
 * @param[out] parse_out         A pointer to the sli_klv_hdr_parse_t structure
 *                               that will contain the CRC, length and key
 *                               information after header is decoded.
 *
 * @return SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
static sl_status_t decode_klv_header(const uint8_t *obj_adr,
                                     sli_klv_hdr_parse_t *parse_out)
{
  if ((obj_adr == NULL) || (parse_out == NULL)) {
    TOKENDBG(printf("Null pointer error in decode_klv_header\n"));
    return SL_STATUS_NULL_POINTER;
  }

  memset(parse_out, 0, sizeof(*parse_out));

  // Check if the object is a flash erase word
  if (*(const uint32_t *)obj_adr == SLI_TOKEN_KLV_ERASE_STATE) {
    TOKENDBG(printf("KLV object not found\n"));
    return SL_STATUS_NOT_FOUND;
  }

  uint32_t magic_number = *((const uint32_t *)obj_adr);

  // Validate the KLV magic number @ref sl_klv_obj_header_t
  if ((magic_number == SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE)
      || (magic_number == SLI_TOKEN_KLV_MAGIC_KLV_DELETED)) {
    const sli_klv_obj_header_t *oh = (const sli_klv_obj_header_t *)obj_adr;

    // An interrupted header program can leave complete magic without a
    // decodable length/key/CRC. Treat it as a non-followable tail, NOT as an
    // object or a skippable span. The next page may contain stale data from
    // lazy erase, or live relocation/stream data. Without a committed length
    // or a separate transaction record, choosing either would be speculation.
    // Never soft-delete or erase this header on boot.
    if ((oh->length == SLI_TOKEN_KLV_ERASE_STATE)
        || (oh->length == 0U)
        || (oh->key == 0xFFFFU)
        || (calculate_checksum((const uint8_t *)&oh->length,
                               SLI_TOKEN_KLV_EXTENDED_HEADER_CRC_PAYLOAD_LEN) != oh->crc)) {
      TOKENDBG(printf("Torn KLV header: length=0x%lx, key=0x%x, crc=0x%x\n",
                      (unsigned long)oh->length, oh->key, oh->crc));
      parse_out->is_torn = true;
      return SL_STATUS_NOT_FOUND;
    }

    parse_out->key = oh->key;
    parse_out->payload_length = oh->length;
    parse_out->payload_crc = oh->crc_data;
    parse_out->header_size = (uint8_t)SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE;
    parse_out->is_readable = (!(magic_number == SLI_TOKEN_KLV_MAGIC_KLV_DELETED)) && IS_KLV_HEADER_READABLE(oh->flags);
    parse_out->is_write_in_progress = (magic_number == SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE)
                                      && !IS_KLV_HEADER_READABLE(oh->flags);
    parse_out->aad_length = SLI_TOKEN_KLV_EXTENDED_HEADER_AAD_SIZE;
    return SL_STATUS_OK;
  }

  const sl_klv_header_t *klv_header_data = (const sl_klv_header_t *)obj_adr;

  // A programmed word with invalid magic is not an erased append slot.
  // In particular, interrupted programming of the first magic word need not
  // leave the *complete* active magic. A UT chain cannot skip over it safely.
  // Static KLV callers still receive the historical NOT_FOUND status.
  if (klv_header_data->magic_number != SLI_TOKEN_KLV_MAGIC_NUMBER) {
    parse_out->is_torn = true;
    TOKENDBG(printf("Programmed non-followable KLV header\n"));
    return SL_STATUS_NOT_FOUND;
  }

  // The legacy header form can also be interrupted. In a UT scan, this is
  // a non-followable programmed slot and must not become an ordinary tail.
  if (klv_header_data->length == 0xFFFF || klv_header_data->key == 0xFFFF) {
    parse_out->is_torn = true;
    TOKENDBG(printf("Invalid length or key: length=0x%x, key=0x%x\n", klv_header_data->length, klv_header_data->key));
    return SL_STATUS_FAIL;
  }

  parse_out->key = klv_header_data->key;
  parse_out->payload_length = klv_header_data->length;
  parse_out->payload_crc = klv_header_data->crc;
  parse_out->header_size = (uint8_t)SLI_TOKEN_KLV_HEADER_SIZE;
  parse_out->is_readable = true;
  parse_out->is_write_in_progress = false;
  parse_out->aad_length = (uint32_t)SLI_TOKEN_KLV_HEADER_SIZE;
  return SL_STATUS_OK;
}

#if !defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH)
/***************************************************************************//**
 * Walks a static KLV chain to find either an existing token entry or the first
 * free slot where a new entry could be appended.
 *
 * For the static-token path this helper has a dual role:
 * - if a readable entry with @p key already exists, it returns that object's
 *   address so the caller can consider overwrite semantics;
 * - otherwise it returns the next decodable free position at the end of the
 *   programmed chain.
 *
 * Unreadable entries are skipped because they represent deleted or incomplete
 * objects rather than valid overwrite targets.
 *
 * @param[in] key                      16-bit KLV key being searched.
 * @param[in] klv_chain_start_address  Address of the chain-start marker.
 * @param[in] klv_end_address          End address of the static KLV region.
 *
 * @return Address of the matching object or append slot. If an input pointer is
 *         NULL, the helper returns @ref SL_STATUS_NULL_POINTER cast to `uint32_t`
 *         for historical compatibility with the existing callers.
 ******************************************************************************/
static uint32_t check_token_in_klv_list(uint16_t key,
                                        uint32_t *klv_chain_start_address,
                                        uint32_t *klv_end_address)
{
  if ((klv_chain_start_address == NULL) || (klv_end_address == NULL)) {
    TOKENDBG(printf("Null pointer error in check_token_in_klv_list\n"));
    return SL_STATUS_NULL_POINTER;
  }

  uint32_t security_offset = SLI_TOKEN_MANAGER_GET_SECURITY_OFFSET(klv_chain_start_address);
  // Skip the first 4 bytes, that represents KLV chain start
  uint8_t *obj_adr = (uint8_t *)(klv_chain_start_address + 1);
  sli_klv_hdr_parse_t klv_header_info;

  while ((uint32_t *)obj_adr < klv_end_address
         && (SLI_KLV_ADDRESS_HAS_PROGRAMMED_HEADER(obj_adr))) {
    if (decode_klv_header(obj_adr, &klv_header_info) != SL_STATUS_OK) {
      // Stop at the first undecodable slot from the caller's perspective, that
      // is the earliest address where append/overwrite logic should take over.
      return (uint32_t)obj_adr;
    }

    // Check if the key matches
    if (key == klv_header_info.key) {
      // Check if the object is valid or not.
      if (!klv_header_info.is_readable) {
        // The object is not readable, skip it.
        obj_adr += SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES(klv_header_info.header_size,
                                                          security_offset,
                                                          klv_header_info.payload_length);
        continue;
      }
      // The object is valid, return object address
      return (uint32_t)obj_adr;
    }

    obj_adr += SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES(klv_header_info.header_size,
                                                      security_offset,
                                                      klv_header_info.payload_length);
  }

  return (uint32_t)obj_adr;
}
#endif //!SL_TOKEN_MANAGER_BACKEND_INT_FLASH

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS)
// Base-region UT address helpers are intentionally kept local to this block so
// future extension-region support can replace the region-walk logic here
// without changing the higher-level read/write/delete/reclaim flow.

/***************************************************************************//**
 * Returns whether @p key is an invalid KLV key.
 ******************************************************************************/
static bool is_invalid_klv_key(uint32_t key)
{
  return ((key & 0xFFFFU) == 0xFFFFU);
}

/***************************************************************************//**
 * Returns the UT repack reserve size for a given maximum object size.
 *
 * The reserve must hold one page-aligned maximum object plus one extra page so
 * reclaim can continue safely when a relocation or append crosses a page
 * boundary during the repack flow.
 ******************************************************************************/
 static uint32_t compute_ut_repack_reserve_size(uint32_t max_object_size)
 {
   if (max_object_size == 0U) {
     return 0U;
   }
 
   return SL_CEILING(max_object_size, FLASH_PAGE_SIZE) + FLASH_PAGE_SIZE;
 }

/***************************************************************************//**
 * Returns the largest object size from the chain.
 *
 * @param[in] scan_result     The scan result from the chain scan.
 * @param[in] new_object_size The size of the new object to be written.
 *
 * @return The largest object size.
 ******************************************************************************/
static uint32_t get_largest_object_size(const sli_klv_ut_scan_result_t *scan_result,
                                        uint32_t new_object_size)
{
  uint32_t largest = new_object_size;

  if (scan_result != NULL) {
    if (scan_result->largest_live_obj_size > largest) {
      largest = scan_result->largest_live_obj_size;
    }
    if (scan_result->largest_wip_obj_size > largest) {
      largest = scan_result->largest_wip_obj_size;
    }
  }

  if (largest_live_object_cache_valid
      && (largest_live_object_size > largest)) {
    largest = largest_live_object_size;
  }

  return largest;
}

/***************************************************************************//**
 * Returns whether an object and its required repack reserve fit within
 * @p registered_total which includes the base UT region and all registered
 * extensions, including pending extensions.
 *
 * The admission check uses the total registered capacity, allowing reclaim to
 * activate pending extensions when the active ring lacks sufficient space.
 * A failure indicates that the post-write live set and required reserve 
 * would not fit even after full compaction.
 *
 * @p largest_after is max(new_object_size, current largest live, largest
 * live-span WIP). When an overwrite shrinks the unique largest object,
 * retaining the pre-write maximum may overestimate the required reserve. This
 * conservative sizing is also used by the direct-write and stream-start paths
 * until the cache is refreshed after the commit. Consequently, a borderline
 * shrink operation may be rejected even though it would fit with the smaller
 * reserve.
 *
 * @p live_used is readable live-span bytes plus WIP reservations:
 *   occupied_bytes - live_span_reclaimable_bytes + live_span_wip_bytes.
 * Soft-deleted / corrupt reclaimable bytes stay excluded. Do not subtract the
 * full reclaimable_bytes, which may include prefix objects outside the live
 * span and may exceed occupied_bytes.
 *
 * WIP must count as committed capacity: concurrent direct-append streams leave
 * provisional objects that become live at finish. Treating them as reclaimable
 * alone would let a later set_start / one-shot pass this gate and over-commit
 * registered capacity once those streams complete.
 ******************************************************************************/
static bool check_object_fits_registered_capacity(const sli_klv_ut_scan_result_t *scan_result,
                                                  uint32_t new_object_size,
                                                  uint32_t registered_total_size)
{
  uint32_t live_used = 0U;
  uint32_t live_after = 0U;
  uint32_t matched_object_size = 0U;
  uint32_t largest_after = 0U;
  uint32_t reserve = 0U;

  if ((scan_result == NULL) || (registered_total_size == 0U) || (new_object_size == 0U)) {
    return false;
  }

  // Readable live + reserved WIP (WIP is also in live_span_reclaimable).
  live_used = (scan_result->occupied_bytes - scan_result->live_span_reclaimable_bytes)
              + scan_result->live_span_wip_bytes;
  live_after = live_used + new_object_size;

  if (scan_result->match_found) {
    matched_object_size =
      SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(scan_result->match.header.header_size,
                                          scan_result->match.header.payload_length);
    if (matched_object_size <= live_after) {
      live_after -= matched_object_size;
    } else {
      live_after = 0U;
    }
  }

  largest_after = get_largest_object_size(scan_result, new_object_size);
  reserve = compute_ut_repack_reserve_size(largest_after);
  return ((live_after + reserve) <= registered_total_size);
}

/***************************************************************************//**
 * Returns whether @p address is a valid word-aligned location inside the UT
 * circular-storage region.
 *
 * Every UT helper that consumes an address uses this guard before interpreting
 * the bytes as a KLV object. That prevents reclaim or recovery from following a
 * corrupt anchor into unrelated memory.
 ******************************************************************************/
static bool is_address_in_range(uint32_t address,
                                uint32_t klv_start_address,
                                uint32_t klv_end_address)
{
  return ((address % sizeof(uint32_t)) == 0U)
         && get_segment_bounds_by_address(address,
                                          klv_start_address,
                                          klv_end_address,
                                          &klv_start_address,
                                          &klv_end_address);
}

/***************************************************************************//**
 * Returns whether @p address is a valid byte location inside the UT region.
 ******************************************************************************/
static bool is_address_in_region(uint32_t address,
                                 uint32_t klv_start_address,
                                 uint32_t klv_end_address)
{
  return get_segment_bounds_by_address(address,
                                       klv_start_address,
                                       klv_end_address,
                                       &klv_start_address,
                                       &klv_end_address);
}

/***************************************************************************//**
 * Advances a UT address by @p size_bytes with wrap-around at region end.
 *
 * This is the canonical "next object" calculation for the circular chain. It is
 * used both when scanning existing objects and when reserving destination space
 * during reclaim.
 ******************************************************************************/
static uint32_t advance_ut_adress(uint32_t address,
                                  uint32_t size_bytes,
                                  uint32_t klv_start_address,
                                  uint32_t klv_end_address)
{
  uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  uint32_t offset = 0U;
  uint32_t advanced_offset = 0U;
  uint32_t advanced_addr = address;

  if ((total_region_size == 0U)
      || !convert_address_to_logical_offset(address,
                                            klv_start_address,
                                            klv_end_address,
                                            &offset)) {
    return address;
  }

  advanced_offset = (offset + size_bytes) % total_region_size;
  if (!convert_logical_offset_to_address(advanced_offset,
                                         klv_start_address,
                                         klv_end_address,
                                         &advanced_addr)) {
    return address;
  }

  return advanced_addr;
}

/***************************************************************************//**
 * Reads a logical UT byte range, wrapping at the region end when necessary.
 ******************************************************************************/
static sl_status_t logical_read(uint32_t addr,
                                void *buffer,
                                uint32_t length,
                                uint32_t klv_start_address,
                                uint32_t klv_end_address)
{
  uint8_t *dst = (uint8_t *)buffer;
  uint32_t current = addr;
  uint32_t remaining = length;
  sl_status_t status = SL_STATUS_OK;

  if ((buffer == NULL) && (length > 0U)) {
    return SL_STATUS_NULL_POINTER;
  }

  if ((length > 0U)
      && !is_address_in_region(addr, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  while (remaining > 0U) {
    uint32_t chunk_size = cal_bytes_until_segment_end(current,
                                                      klv_start_address,
                                                      klv_end_address);

    if (chunk_size > remaining) {
      chunk_size = remaining;
    }

    status = sli_token_manager_nvm_read(current,
                                        dst,
                                        chunk_size);
    if (status != SL_STATUS_OK) {
      return status;
    }

    remaining -= chunk_size;
    dst += chunk_size;
    current = advance_ut_adress(current,
                                chunk_size,
                                klv_start_address,
                                klv_end_address);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Writes a logical UT byte range, wrapping at the region end when necessary.
 ******************************************************************************/
static sl_status_t logical_write(uint32_t addr,
                                 const void *buffer,
                                 uint32_t length,
                                 uint32_t klv_start_address,
                                 uint32_t klv_end_address)
{
  const uint8_t *src = (const uint8_t *)buffer;
  uint32_t current = addr;
  uint32_t remaining = length;
  sl_status_t status = SL_STATUS_OK;

  if ((buffer == NULL) && (length > 0U)) {
    return SL_STATUS_NULL_POINTER;
  }

  if ((length > 0U)
      && !is_address_in_region(addr, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  while (remaining > 0U) {
    uint32_t chunk_size = cal_bytes_until_segment_end(current,
                                                      klv_start_address,
                                                      klv_end_address);

    if (chunk_size > remaining) {
      chunk_size = remaining;
    }

    status = sli_token_manager_nvm_write(current,
                                         src,
                                         chunk_size);
    if (status != SL_STATUS_OK) {
      return status;
    }

    remaining -= chunk_size;
    src += chunk_size;
    current = advance_ut_adress(current,
                                chunk_size,
                                klv_start_address,
                                klv_end_address);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns the CRC16 of a logical UT byte range.
 *
 * This mirrors @ref calculate_checksum but feeds GPCRC with wrapped
 * chunks read from the logical UT address space.
 ******************************************************************************/
static sl_status_t calculate_logical_checksum(uint32_t addr,
                                              uint32_t length,
                                              uint32_t klv_start_address,
                                              uint32_t klv_end_address,
                                              uint16_t *checksum_out)
{
  uint8_t scratch[32] = { 0 };
  uint32_t current = addr;
  uint32_t remaining = length;
  sl_status_t status = SL_STATUS_OK;

  if (checksum_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if ((length > 0U)
      && !is_address_in_region(addr, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

#if defined(_CMU_CLKEN0_MASK)
  CMU->CLKEN0_SET = GPCRC_CLKEN;
#endif
  GPCRC_INSTANCE->EN = GPCRC_EN_EN;
  GPCRC_INSTANCE->CTRL = CRC16_POLY;
  GPCRC_INSTANCE->INIT = INIT_VALUE_CRC16;
  GPCRC_INSTANCE->CMD = GPCRC_CMD_INIT;

  while (remaining > 0U) {
    uint32_t chunk_size = sizeof(scratch);
    uint32_t idx = 0U;

    if (chunk_size > remaining) {
      chunk_size = remaining;
    }

    status = logical_read(current,
                          scratch,
                          chunk_size,
                          klv_start_address,
                          klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    for (idx = 0U; idx < chunk_size; idx++) {
      GPCRC_INSTANCE->INPUTDATABYTE = scratch[idx];
    }

    remaining -= chunk_size;
    current = advance_ut_adress(current,
                                chunk_size,
                                klv_start_address,
                                klv_end_address);
  }

  *checksum_out = (uint16_t)GPCRC_INSTANCE->DATA;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Compares a logical UT byte range against a RAM buffer.
 ******************************************************************************/
static bool logical_equals_buffer(uint32_t addr,
                                  const void *buffer,
                                  uint32_t length,
                                  uint32_t klv_start_address,
                                  uint32_t klv_end_address)
{
  uint8_t scratch[32] = { 0 };
  const uint8_t *expected = (const uint8_t *)buffer;
  uint32_t current = addr;
  uint32_t remaining = length;
  sl_status_t status = SL_STATUS_OK;

  if ((buffer == NULL) && (length > 0U)) {
    return false;
  }

  while (remaining > 0U) {
    uint32_t chunk_size = sizeof(scratch);

    if (chunk_size > remaining) {
      chunk_size = remaining;
    }

    status = logical_read(current,
                          scratch,
                          chunk_size,
                          klv_start_address,
                          klv_end_address);
    if (status != SL_STATUS_OK) {
      return false;
    }

    if (memcmp(scratch, expected, chunk_size) != 0) {
      return false;
    }

    remaining -= chunk_size;
    expected += chunk_size;
    current = advance_ut_adress(current,
                                chunk_size,
                                klv_start_address,
                                klv_end_address);
  }

  return true;
}

/***************************************************************************//**
 * Compares two logical UT byte ranges for equality.
 ******************************************************************************/
static bool logical_ranges_equal(uint32_t addr_a,
                                 uint32_t addr_b,
                                 uint32_t length,
                                 uint32_t klv_start_address,
                                 uint32_t klv_end_address)
{
  uint8_t scratch_a[32] = { 0 };
  uint8_t scratch_b[32] = { 0 };
  uint32_t current_a = addr_a;
  uint32_t current_b = addr_b;
  uint32_t remaining = length;
  sl_status_t status = SL_STATUS_OK;

  while (remaining > 0U) {
    uint32_t chunk_size = sizeof(scratch_a);

    if (chunk_size > remaining) {
      chunk_size = remaining;
    }

    status = logical_read(current_a,
                          scratch_a,
                          chunk_size,
                          klv_start_address,
                          klv_end_address);
    if (status != SL_STATUS_OK) {
      return false;
    }

    status = logical_read(current_b,
                          scratch_b,
                          chunk_size,
                          klv_start_address,
                          klv_end_address);
    if (status != SL_STATUS_OK) {
      return false;
    }

    if (memcmp(scratch_a, scratch_b, chunk_size) != 0) {
      return false;
    }

    remaining -= chunk_size;
    current_a = advance_ut_adress(current_a,
                                  chunk_size,
                                  klv_start_address,
                                  klv_end_address);
    current_b = advance_ut_adress(current_b,
                                  chunk_size,
                                  klv_start_address,
                                  klv_end_address);
  }

  return true;
}

/***************************************************************************//**
 * Writes a UT payload with word padding preserved across a logical wrap.
 ******************************************************************************/
static sl_status_t write_ut_payload(uint32_t payload_addr,
                                    const void *data,
                                    uint32_t payload_length,
                                    uint32_t klv_start_address,
                                    uint32_t klv_end_address)
{
  uint8_t scratch[32] = { 0 };
  const uint8_t *src = (const uint8_t *)data;
  uint32_t padded_length = SL_CEILING(payload_length, sizeof(uint32_t));
  uint32_t current = payload_addr;
  uint32_t copied = 0U;
  sl_status_t status = SL_STATUS_OK;

  if (data == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  while (copied < padded_length) {
    uint32_t chunk_size = sizeof(scratch);
    uint32_t remaining_payload = 0U;

    if (chunk_size > (padded_length - copied)) {
      chunk_size = padded_length - copied;
    }

    memset(scratch, 0xFF, chunk_size);
    if (copied < payload_length) {
      remaining_payload = payload_length - copied;
      if (remaining_payload > chunk_size) {
        remaining_payload = chunk_size;
      }
      memcpy(scratch, src + copied, remaining_payload);
    }

    status = logical_write(current,
                           scratch,
                           chunk_size,
                           klv_start_address,
                           klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    copied += chunk_size;
    current = advance_ut_adress(current,
                                chunk_size,
                                klv_start_address,
                                klv_end_address);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Allocates and links a new streaming context for @p token.
 ******************************************************************************/
static sli_stream_context_node_t *stream_ctx_alloc(uint32_t token)
{
  sli_stream_context_node_t *node =
    (sli_stream_context_node_t *)sl_malloc(sizeof(sli_stream_context_node_t));

  if (node == NULL) {
    return NULL;
  }

  memset(node, 0, sizeof(*node));
  node->token = token;
  node->next = sli_stream_ctx_head;
  sli_stream_ctx_head = node;
  return node;
}

/***************************************************************************//**
 * Finds the active streaming context for @p token.
 ******************************************************************************/
static sli_stream_context_node_t *stream_ctx_find(uint32_t token)
{
  sli_stream_context_node_t *node = NULL;

  for (node = sli_stream_ctx_head; node != NULL; node = node->next) {
    if (node->token == token) {
      return node;
    }
  }

  return NULL;
}

/***************************************************************************//**
 * Removes and frees the streaming context for @p token.
 ******************************************************************************/
static void stream_ctx_free(uint32_t token)
{
  sli_stream_context_node_t **node = &sli_stream_ctx_head;

  while (*node != NULL) {
    if ((*node)->token == token) {
      sli_stream_context_node_t *to_free = *node;

      *node = to_free->next;
      sl_free(to_free);
      return;
    }

    node = &(*node)->next;
  }
}

/***************************************************************************//**
 * Drops the in-RAM stream session for a UT object retired by recovery.
 *
 * Recovery can invalidate a write-in-progress object directly in flash before
 * the owning API call returns. When that happens in the same process lifetime,
 * the matching stream context must be discarded too so later set_ut_start() calls
 * do not keep pointing at a deleted on-flash object.
 ******************************************************************************/
static void stream_ctx_invalidate_recovered_ut_key(uint16_t key)
{
  stream_ctx_free(SL_TOKEN_GET_UPGRADABLE_TOKEN(key));
}

/***************************************************************************//**
 * Discards every in-RAM UT stream session.
 *
 * Recovery runs after power loss, so any partially completed stream state in
 * RAM must be dropped before the chain is reconciled.
 ******************************************************************************/
static void stream_ctx_free_all(void)
{
  while (sli_stream_ctx_head != NULL) {
    sli_stream_context_node_t *to_free = sli_stream_ctx_head;

    sli_stream_ctx_head = to_free->next;
    sl_free(to_free);
  }
}

/***************************************************************************//**
 * Returns true if any active stream reserved its destination through reclaim.
 *
 * That session still has deferred erase, anchor, and layout side effects to
 * apply at finish. Another writer must not mutate the chain until then.
 ******************************************************************************/
static bool stream_ctx_any_used_reclaim(void)
{
  const sli_stream_context_node_t *node = NULL;

  for (node = sli_stream_ctx_head; node != NULL; node = node->next) {
    if (node->reservation.used_reclaim) {
      return true;
    }
  }

  return false;
}

/***************************************************************************//**
 * Returns true if a stream session other than @p token is already active.
 ******************************************************************************/
static bool stream_ctx_has_other_session(uint32_t token)
{
  const sli_stream_context_node_t *node = NULL;

  for (node = sli_stream_ctx_head; node != NULL; node = node->next) {
    if (node->token != token) {
      return true;
    }
  }

  return false;
}

/***************************************************************************//**
 * Blocks reclaim/repack while another stream session is using the same chain.
 *
 * Relocating objects, publishing a new head, or activating extensions would
 * invalidate the peer stream's reserved write address and stamped layout.
 ******************************************************************************/
static sl_status_t stream_reject_reclaim_with_peer_sessions(uint32_t self_token)
{
  if (stream_ctx_has_other_session(self_token)) {
    return SL_STATUS_BUSY;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Programs a buffered payload word and advances the logical write pointer.
 ******************************************************************************/
static sl_status_t stream_write_pending_word(sli_stream_context_node_t *context,
                                             const uint8_t word_data[sizeof(uint32_t)])
{
  uint32_t aligned_word = 0xFFFFFFFFU;
  sl_status_t status = SL_STATUS_OK;

  memcpy(&aligned_word, word_data, sizeof(aligned_word));
  status = logical_write(context->next_write_addr,
                         &aligned_word,
                         sizeof(uint32_t),
                         context->klv_start_address,
                         context->klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  context->next_write_addr = advance_ut_adress(context->next_write_addr,
                                               sizeof(uint32_t),
                                               context->klv_start_address,
                                               context->klv_end_address);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Writes a word-aligned payload block through an aligned temporary buffer.
 *
 * The UT stream path accepts arbitrary byte pointers from the caller, but the
 * internal flash backend programs data as words. Stage writes through an
 * aligned scratch buffer so the flash layer never has to consume an unaligned
 * source pointer.
 ******************************************************************************/
static sl_status_t stream_write_aligned_block(sli_stream_context_node_t *context,
                                              const uint8_t *data,
                                              uint32_t length)
{
  uint32_t scratch_words[8] = { 0 };
  uint8_t *scratch = (uint8_t *)scratch_words;
  uint32_t remaining = length;
  const uint8_t *src = data;
  sl_status_t status = SL_STATUS_OK;

  if ((context == NULL) || ((data == NULL) && (length > 0U))) {
    return SL_STATUS_NULL_POINTER;
  }

  while (remaining > 0U) {
    uint32_t chunk_size = remaining;

    if (chunk_size > sizeof(scratch_words)) {
      chunk_size = sizeof(scratch_words);
    }

    memcpy(scratch, src, chunk_size);
    status = logical_write(context->next_write_addr,
                           scratch,
                           chunk_size,
                           context->klv_start_address,
                           context->klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    remaining -= chunk_size;
    src += chunk_size;
    context->next_write_addr = advance_ut_adress(context->next_write_addr,
                                                 chunk_size,
                                                 context->klv_start_address,
                                                 context->klv_end_address);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Flushes the currently buffered partial word, padding with erase-state bytes.
 ******************************************************************************/
static sl_status_t stream_flush_pending_word(sli_stream_context_node_t *context)
{
  uint8_t padded_word[sizeof(uint32_t)] = { 0xFF, 0xFF, 0xFF, 0xFF };

  if (context == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (context->pending_len == 0U) {
    return SL_STATUS_OK;
  }

  memcpy(padded_word, context->pending_buf, context->pending_len);
  context->pending_len = 0U;
  memset(context->pending_buf, 0xFF, sizeof(context->pending_buf));
  return stream_write_pending_word(context, padded_word);
}

/***************************************************************************//**
 * Appends a sequential payload chunk to the reserved UT object.
 ******************************************************************************/
static sl_status_t stream_append_payload(sli_stream_context_node_t *context,
                                         const uint8_t *data,
                                         uint32_t length)
{
  const uint8_t *src = data;
  sl_status_t status = SL_STATUS_OK;

  if ((context == NULL) || ((data == NULL) && (length > 0U))) {
    return SL_STATUS_NULL_POINTER;
  }

  if (length > context->remaining_length) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  while (length > 0U) {
    if (context->pending_len != 0U) {
      uint32_t copy_len = sizeof(context->pending_buf) - context->pending_len;

      if (copy_len > length) {
        copy_len = length;
      }

      memcpy(&context->pending_buf[context->pending_len], src, copy_len);
      context->pending_len = (uint8_t)(context->pending_len + copy_len);
      src += copy_len;
      length -= copy_len;
      context->bytes_written += copy_len;
      context->remaining_length -= copy_len;

      if (context->pending_len == sizeof(context->pending_buf)) {
        status = stream_write_pending_word(context, context->pending_buf);
        if (status != SL_STATUS_OK) {
          return status;
        }

        context->pending_len = 0U;
        memset(context->pending_buf, 0xFF, sizeof(context->pending_buf));
      }

      continue;
    }

    if (length >= sizeof(uint32_t)) {
      uint32_t write_len = length - (length % sizeof(uint32_t));

      status = stream_write_aligned_block(context,
                                          src,
                                          write_len);
      if (status != SL_STATUS_OK) {
        return status;
      }

      src += write_len;
      length -= write_len;
      context->bytes_written += write_len;
      context->remaining_length -= write_len;
      continue;
    }

    memcpy(context->pending_buf, src, length);
    context->pending_len = (uint8_t)length;
    context->bytes_written += length;
    context->remaining_length -= length;
    length = 0U;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Durable UT append frontier. A valid WIP header occupies its complete length.
 * No physical address comparison is used (extensions can be anywhere).
 *
 * An erased/torn pending header is never promoted to STABLE. A torn header is
 * handed to the existing page-repair transaction on the next mutation.
 ******************************************************************************/
static sl_status_t ut_tail_read(sli_klv_ut_tail_record_t *rec)
{
  if (!active_ut_context_valid || active_ut_context.tail_load == NULL) {
    return SL_STATUS_NOT_FOUND; // Host-only KLV users without NVM3 callbacks.
  }
  return active_ut_context.tail_load(rec, active_ut_context.tail_context);
}

 /***************************************************************************//**
  * Stores the UT tail record to NVM3.
  ******************************************************************************/
static sl_status_t ut_tail_store(const sli_klv_ut_tail_record_t *rec)
{
  if (!active_ut_context_valid || active_ut_context.tail_store == NULL) {
    return SL_STATUS_OK;
  }
  return active_ut_context.tail_store(rec, active_ut_context.tail_context);
}

/***************************************************************************//**
 * Checks if a pending header is programmed.
 *
 * @note A reserved (HEADER_PENDING) header can have erased magic but programmed
 * later words. A stable frontier does not use this probe: bytes beyond it are
 * permitted to be historical stale flash, not evidence of a torn write.
 *
 * @param[in]  addr  The address of the header.
 * @param[in]  start  The start address of the range.
 * @param[in]  end  The end address of the range.
 * @param[out] programmed  Whether the header is programmed.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
static sl_status_t ut_pending_header_is_programmed(uint32_t addr,
                                                   uint32_t start,
                                                   uint32_t end,
                                                   bool *programmed)
{
  uint8_t bytes[SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE];
  sl_status_t status;

  if (programmed == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *programmed = false;

  status = logical_read(addr, bytes, sizeof(bytes), start, end);
  if (status != SL_STATUS_OK) {
    return status;
  }

  for (uint32_t i = 0U; i < sizeof(bytes); i++) {
    if (bytes[i] != 0xFFU) {
      *programmed = true;
      break;
    }
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns whether the UT tail record is valid.
 ******************************************************************************/
static bool ut_tail_record_valid(const sli_klv_ut_tail_record_t *rec,
                                 uint32_t start, uint32_t end)
{
  return rec != NULL
         && is_address_in_range(rec->stable_tail, start, end)
         && (rec->state == SLI_KLV_UT_TAIL_STABLE
             || (rec->state == SLI_KLV_UT_TAIL_HEADER_PENDING
                 && is_address_in_range(rec->pending_header_addr, start, end))
             || (rec->state == SLI_KLV_UT_TAIL_LAYOUT_PENDING
                 && rec->pending_header_addr != 0U
                 && (rec->pending_header_addr % sizeof(uint32_t)) == 0U));
}

/***************************************************************************//**
 * Program a single NVM3 item BEFORE the first flash word of a new header.
 *
 * A shifted destination is supported only when the previous chain was empty;
 * otherwise an unencoded logical gap would make the new header unreachable.
 ******************************************************************************/
static sl_status_t ut_tail_prepare_header(uint32_t header_addr,
                                           uint32_t start, uint32_t end)
{
  sli_klv_ut_tail_record_t rec = { 0 };
  sl_status_t status = ut_tail_read(&rec);
  if (status == SL_STATUS_NOT_FOUND) {
    return SL_STATUS_OK; // KLV-only host/older test contexts.
  }
  if (status != SL_STATUS_OK) {
    return status;
  }
  if (!ut_tail_record_valid(&rec, start, end)
      || rec.state != SLI_KLV_UT_TAIL_STABLE) {
    return SL_STATUS_INVALID_STATE;
  }
  if (rec.stable_tail != header_addr
      && (active_ut_context.chain_anchor == NULL
          || rec.stable_tail != *active_ut_context.chain_anchor)) {
    return SL_STATUS_INVALID_STATE;
  }
  rec.pending_header_addr = header_addr;
  rec.state = SLI_KLV_UT_TAIL_HEADER_PENDING;
  return ut_tail_store(&rec);
}

/***************************************************************************//**
 * Commit the reservation once the 16-byte header is structurally valid, even
 * if WIP remains set. Later streaming payload updates do not advance tail.
 ******************************************************************************/
static sl_status_t ut_tail_publish_header(uint32_t header_addr,
                                           uint32_t start, uint32_t end)
{
  sli_klv_ut_tail_record_t rec = { 0 };
  sli_klv_ut_obj_info_t info = { 0 };
  sl_status_t status = ut_tail_read(&rec);
  if (status == SL_STATUS_NOT_FOUND) {
    return SL_STATUS_OK;
  }
  if (status != SL_STATUS_OK) {
    return status;
  }
  if (!ut_tail_record_valid(&rec, start, end)
      || rec.state != SLI_KLV_UT_TAIL_HEADER_PENDING
      || rec.pending_header_addr != header_addr) {
    return SL_STATUS_INVALID_STATE;
  }
  status = decode_object_from_anchor(header_addr, start, end, &info);
  if (status != SL_STATUS_OK) {
    return (status == SL_STATUS_NOT_FOUND) ? SL_STATUS_INVALID_STATE : status;
  }
  // When the first object starts at a shifted empty-head destination, its
  // old persisted anchor is not the header address. Keep HEADER_PENDING until
  // the new anchor is durable, otherwise a reset would orphan this token.
  if (header_addr != rec.stable_tail) {
    if (active_ut_context.chain_anchor == NULL
        || active_ut_context.anchor_commit == NULL) {
      return SL_STATUS_INVALID_STATE;
    }
    if (*active_ut_context.chain_anchor == rec.stable_tail) {
      status = active_ut_context.anchor_commit(header_addr,
                                               active_ut_context.anchor_context);
      if (status != SL_STATUS_OK) {
        return status;
      }
      *active_ut_context.chain_anchor = header_addr;
    } else if (*active_ut_context.chain_anchor != header_addr) {
      return SL_STATUS_INVALID_STATE;
    }
  }
  rec.stable_tail = info.next_addr;
  rec.pending_header_addr = 0U;
  rec.state = SLI_KLV_UT_TAIL_STABLE;
  return ut_tail_store(&rec);
}

/***************************************************************************//**
 * Writes the provisional UT header while the object remains in progress.
 *
 * Programs magic, length, key, and the WIP flag so recovery and cleanup can
 * identify the in-flight object. The HEADER CRC is complete at start so
 * other streams can traverse this header; only the payload CRC is left
 * at the erased placeholder (0xFFFF) until finish() commits the final
 * crc_data/flags/reserved tail word.
 ******************************************************************************/
static sl_status_t stream_write_provisional_header(const sli_stream_context_node_t *context)
{
  sl_klv_handle_t klv_handle = {
    .key = context->token,
    .offset = 0U,
    .length = context->total_length,
  };
  sli_klv_obj_header_t obj_hdr = { 0 };

  // Start writes a fully decodable header immediately. Only crc_data stays at
  // the erased placeholder value until finish() has seen the complete payload.
  SLI_KLV_INIT_HDR_FOR_WRITE(obj_hdr, &klv_handle, 0xFFFFU);

  // Program a single NVM3 item BEFORE the first flash word of a new header.
  sl_status_t status = ut_tail_prepare_header(context->reservation.write_addr,
                                              context->klv_start_address,
                                              context->klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = logical_write(context->reservation.write_addr,
                         &obj_hdr,
                         sizeof(obj_hdr),
                         context->klv_start_address,
                         context->klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Commit the reservation once the 16-byte header is structurally valid, even
  // if WIP remains set. Later streaming payload updates do not advance tail.
  return ut_tail_publish_header(context->reservation.write_addr,
                                context->klv_start_address,
                                context->klv_end_address);
}

/***************************************************************************//**
 * Commits the final UT header tail after streaming completes.
 *
 * Programs the final crc_data/flags/reserved tail word once streaming
 * completes. The provisional header already programmed magic, length, key, and
 * the header CRC, while leaving crc_data at the erased placeholder value
 * (0xFFFF) and WIP set.
 ******************************************************************************/
static sl_status_t stream_commit_header(const sli_stream_context_node_t *context,
                                        uint16_t data_crc)
{
  sl_klv_handle_t klv_handle = {
    .key = context->token,
    .offset = 0U,
    .length = context->total_length,
  };
  sli_klv_obj_header_t obj_hdr;
  uint32_t final_header_word = 0U;
  uint32_t final_header_word_addr = 0U;
  sl_status_t status = SL_STATUS_OK;

  SLI_KLV_INIT_HDR_FOR_WRITE(obj_hdr, &klv_handle, data_crc);
  obj_hdr.flags = (uint8_t)(obj_hdr.flags & ~SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK);

  memcpy(&final_header_word,
         ((const uint8_t *)&obj_hdr) + offsetof(sli_klv_obj_header_t, crc_data),
         sizeof(final_header_word));
  final_header_word &= ~(uint32_t)(SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK
                                   << 16U);

  final_header_word_addr = advance_ut_adress(context->reservation.write_addr,
                                             offsetof(sli_klv_obj_header_t, crc_data),
                                             context->klv_start_address,
                                             context->klv_end_address);
  status = logical_write(final_header_word_addr,
                         &final_header_word,
                         sizeof(final_header_word),
                         context->klv_start_address,
                         context->klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns the first page boundary at which an erase may start.
 *
 * @p addr is returned unchanged when it is already page-aligned. A mid-page
 * address keeps that page, because bytes below @p addr can still belong to a
 * live object, and the erase begins at the next @c FLASH_PAGE_SIZE boundary.
 * Page size is device-specific.
 ******************************************************************************/
static uint32_t erase_page_start_at_or_after(uint32_t addr,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address)
{
  uint32_t page = SLI_KLV_ANCHOR_PAGE_START(addr);

  if (page == addr) {
    return addr;
  }

  return advance_ut_adress(page,
                           FLASH_PAGE_SIZE,
                           klv_start_address,
                           klv_end_address);
}

/***************************************************************************//**
 * Places the repack reserve as the @p repack_reserve_size bytes immediately
 * before @p gap_end.
 *
 * The tail slack is `[gap_start, repack_start)`. A direct append may fill that
 * slack exactly. The reserve is not pushed forward by a second copy of the
 * largest object, reclaim erases destination pages at write time, starting on
 * a page boundary, and never erases a page that still holds the live chain.
 *
 * @return true when @p repack_start lies in the half-open gap and the forward
 *         distance from it to @p gap_end is exactly @p repack_reserve_size.
 ******************************************************************************/
static bool try_place_repack_before_end(uint32_t gap_start,
                                        uint32_t gap_end,
                                        uint32_t repack_reserve_size,
                                        uint32_t klv_start_address,
                                        uint32_t klv_end_address,
                                        uint32_t *repack_start)
{
  uint32_t total_region_size = 0U;
  uint32_t candidate = 0U;

  if (repack_start == NULL) {
    return false;
  }

  total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  if ((repack_reserve_size == 0U)
      || (repack_reserve_size >= total_region_size)
      || (compare_logical_address(gap_start,
                                  gap_end,
                                  klv_start_address,
                                  klv_end_address) >= 0)
      || (cal_distance_forward(gap_start,
                               gap_end,
                               klv_start_address,
                               klv_end_address) < repack_reserve_size)) {
    return false;
  }

  candidate = advance_ut_adress(gap_end,
                                total_region_size - repack_reserve_size,
                                klv_start_address,
                                klv_end_address);
  if (!is_address_in_range(candidate, klv_start_address, klv_end_address)
      || (compare_logical_address(candidate,
                                  gap_start,
                                  klv_start_address,
                                  klv_end_address) < 0)
      || (compare_logical_address(candidate,
                                  gap_end,
                                  klv_start_address,
                                  klv_end_address) >= 0)
      || (cal_distance_forward(candidate,
                               gap_end,
                               klv_start_address,
                               klv_end_address) != repack_reserve_size)) {
    return false;
  }

  *repack_start = candidate;
  return true;
}

/***************************************************************************//**
 * Computes the preferred start address of the reserved repack window.
 *
 * The repack window is the reserve-sized span kept free so a future reclaim
 * can relocate the largest live object before any source page is erased. In
 * the circular-buffer model, that reserve is attached to the logical free gap
 * that ends at the current anchor, not to the fixed physical end of the
 * region. As the active write/tail region advances and reclaim moves the
 * anchor, the reserve therefore rotates through the ring as a moving window.
 *
 * On a wrapped chain the reserve is the @p repack_reserve_size bytes
 * immediately before the anchor. Bytes from the tail up to that point are
 * the direct-append slack, and an object that ends exactly on the reserve
 * start is admitted.
 *
 * The reserve end follows the exact @p chain_anchor (including mid-page heads),
 * not the page base of that head. Flash erase remains page-aligned elsewhere;
 * snapping the logical reserve to a page boundary would understate append slack
 * when the head sits mid-page.
 ******************************************************************************/
static uint32_t compute_repack_start(uint32_t chain_anchor,
                                     uint32_t tail,
                                     uint32_t repack_reserve_size,
                                     uint32_t klv_start_address,
                                     uint32_t klv_end_address)
{
  uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  uint32_t wrapped_repack = 0U;

  if ((repack_reserve_size == 0U) || (repack_reserve_size >= total_region_size)) {
    return chain_anchor;
  }

  // Wrapped: free gap is the half-open span [tail, head). Place R bytes
  // immediately before the exact head (same helper as gap/post-write paths).
  if (compare_logical_address(tail,
                              chain_anchor,
                              klv_start_address,
                              klv_end_address) < 0) {
    if (try_place_repack_before_end(tail,
                                    chain_anchor,
                                    repack_reserve_size,
                                    klv_start_address,
                                    klv_end_address,
                                    &wrapped_repack)) {
      return wrapped_repack;
    }

    return chain_anchor;
  }

  // Non-wrapped / empty: free gap wraps from tail back to the exact head.
  // try_place_repack_before_end() only accepts non-wrapping gaps, so place R
  // bytes before chain_anchor directly (erase stays page-aligned elsewhere).
  return advance_ut_adress(chain_anchor,
                           total_region_size - repack_reserve_size,
                           klv_start_address,
                           klv_end_address);
}

/***************************************************************************//**
 * Tries to place a full repack reserve inside the logical free gap between two
 * addresses in the UT ring.
 *
 * Used when the chain is already wrapped. The reserve is the
 * @p repack_reserve_size bytes immediately before @p gap_end (the anchor).
 * `[gap_start, repack_start)` is direct-append slack and may be filled
 * exactly. @p guard_bytes is unused: a second copy of the largest object is
 * not reserved in front of the window. Reclaim protects that window by
 * erasing only page-aligned destination pages that do not hold the live chain.
 ******************************************************************************/
static bool try_compute_gap_repack_start(uint32_t gap_start,
                                         uint32_t gap_end,
                                         uint32_t repack_reserve_size,
                                         uint32_t klv_start_address,
                                         uint32_t klv_end_address,
                                         uint32_t *repack_start)
{
  return try_place_repack_before_end(gap_start,
                                     gap_end,
                                     repack_reserve_size,
                                     klv_start_address,
                                     klv_end_address,
                                     repack_start);
}

/***************************************************************************//**
 * Computes the page-aligned erase cursor for an in-flight wrapped reclaim.
 *
 * The cursor is the first @c FLASH_PAGE_SIZE boundary at or after @p gap_start
 * that is still strictly before the anchor page. A mid-page tail keeps its
 * page, because that page still holds the last live object. Erase never starts
 * mid-page and never starts on the anchor page.
 *
 * @p guard_bytes is unused. The first relocated object is written at the tail
 * and may continue into the reserve. Destination pages are erased when that
 * write begins, not by reserving a second copy of the largest object.
 *
 * @return false when the gap has no page that can be erased without touching
 *         the live head.
 ******************************************************************************/
static bool try_compute_reclaim_gap_repack_start(uint32_t gap_start,
                                                 uint32_t gap_end,
                                                 uint32_t klv_start_address,
                                                 uint32_t klv_end_address,
                                                 uint32_t *repack_start)
{
  uint32_t erase_page = 0U;
  uint32_t anchor_page = 0U;

  if (repack_start == NULL) {
    return false;
  }

  if ((compare_logical_address(gap_start,
                               gap_end,
                               klv_start_address,
                               klv_end_address) >= 0)
      || !is_address_in_range(gap_start, klv_start_address, klv_end_address)
      || !is_address_in_range(gap_end, klv_start_address, klv_end_address)) {
    return false;
  }

  erase_page = erase_page_start_at_or_after(gap_start,
                                            klv_start_address,
                                            klv_end_address);
  anchor_page = SLI_KLV_ANCHOR_PAGE_START(gap_end);
  if (!is_address_in_range(erase_page, klv_start_address, klv_end_address)
      || (compare_logical_address(erase_page,
                                  anchor_page,
                                  klv_start_address,
                                  klv_end_address) >= 0)) {
    return false;
  }

  *repack_start = erase_page;
  return true;
}

/***************************************************************************//**
 * Recomputes the repack reserve as the moving window that ends at the anchor.
 *
 * The reserve is not a fixed physical region. While the relocation frontier is
 * still logically before @p chain_anchor, the window is the
 * @p repack_reserve_size bytes immediately before that anchor, the same
 * placement a later direct append uses. When the anchor advances, the window
 * rotates with it. If this reclaim has already written into that span, the
 * start stays at @p relocation_frontier so the window does not cover relocated
 * objects, and the end is still the new anchor.
 *
 * Once the frontier is no longer before the anchor, reclaim is filling the
 * suffix. The window then ends at the exact anchor, matching
 * @ref compute_repack_start() for a non-wrapped chain.
 *
 * This does not choose the erase cursor. Erase stays page-aligned and only
 * moves forward.
 ******************************************************************************/
static bool try_compute_frontier_repack_start(uint32_t relocation_frontier,
                                              uint32_t chain_anchor,
                                              uint32_t repack_reserve_size,
                                              uint32_t klv_start_address,
                                              uint32_t klv_end_address,
                                              uint32_t *repack_start)
{
  uint32_t total_region_size = 0U;
  uint32_t placed = 0U;

  if (repack_start == NULL) {
    return false;
  }

  if (!is_address_in_range(relocation_frontier, klv_start_address, klv_end_address)
      || !is_address_in_range(chain_anchor, klv_start_address, klv_end_address)) {
    return false;
  }

  total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  if ((repack_reserve_size == 0U) || (repack_reserve_size >= total_region_size)) {
    *repack_start = chain_anchor;
    return true;
  }

  if (compare_logical_address(relocation_frontier,
                              chain_anchor,
                              klv_start_address,
                              klv_end_address) < 0) {
    if (try_place_repack_before_end(relocation_frontier,
                                    chain_anchor,
                                    repack_reserve_size,
                                    klv_start_address,
                                    klv_end_address,
                                    &placed)) {
      *repack_start = placed;
      return true;
    }

    // Remaining free room is smaller than the reserve. Keep the end on the
    // new anchor; do not slide the start back into bytes already written.
    *repack_start = relocation_frontier;
    return true;
  }

  // Same exact-head placement as compute_repack_start() for non-wrapped gaps.
  *repack_start = advance_ut_adress(chain_anchor,
                                   total_region_size - repack_reserve_size,
                                   klv_start_address,
                                   klv_end_address);
  return is_address_in_range(*repack_start, klv_start_address, klv_end_address);
}

/***************************************************************************//**
 * Verifies that, after writing an object at @p write_addr, a valid repack gap
 * would still remain between the new tail and the logical anchor.
 *
 * This guards direct-wrap writes from consuming the only space where reclaim
 * could later park relocated objects.
 ******************************************************************************/
static bool try_compute_post_write_gap_repack_start(uint32_t write_addr,
                                                    uint32_t object_size,
                                                    uint32_t chain_anchor,
                                                    uint32_t repack_reserve_size,
                                                    uint32_t klv_start_address,
                                                    uint32_t klv_end_address,
                                                    uint32_t *repack_start)
{
  uint32_t future_tail = 0U;

  if (repack_start == NULL) {
    return false;
  }

  future_tail = advance_ut_adress(write_addr,
                                  object_size,
                                  klv_start_address,
                                  klv_end_address);
  if (compare_logical_address(future_tail,
                              chain_anchor,
                              klv_start_address,
                              klv_end_address) >= 0) {
    // No wrapped free gap remains after this write, so there is nowhere to
    // reserve the repack window inside the logical hole.
    return false;
  }

  return try_compute_gap_repack_start(future_tail,
                                      chain_anchor,
                                      repack_reserve_size,
                                      klv_start_address,
                                      klv_end_address,
                                      repack_start);
}

/***************************************************************************//**
 * Returns whether the range `[addr, addr + size)` intersects the reserved repack
 * window, accounting for wrap-around at the end of the UT region.
 *
 * This is used to stop ordinary appends from consuming bytes that must remain
 * free for future reclaim relocation.
 ******************************************************************************/
static bool check_range_overlaps_repack(uint32_t addr,
                                        uint32_t size,
                                        uint32_t repack_start,
                                        uint32_t repack_reserve_size,
                                        uint32_t klv_start_address,
                                        uint32_t klv_end_address)
{
  return check_range_overlaps(addr,
                              size,
                              repack_start,
                              repack_reserve_size,
                              klv_start_address,
                              klv_end_address);
}

/***************************************************************************//**
 * Advances a candidate write address past the reserved repack window when the
 * current address would land inside that protected range.
 *
 * The caller uses this helper while probing possible append addresses. The
 * returned address is either unchanged or points immediately after the reserved
 * repack region.
 ******************************************************************************/
static uint32_t skip_repack_window(uint32_t write_addr,
                                   uint32_t repack_start,
                                   uint32_t repack_reserve_size,
                                   uint32_t klv_start_address,
                                   uint32_t klv_end_address)
{
  if (check_range_overlaps_repack(write_addr,
                                  sizeof(uint32_t),
                                  repack_start,
                                  repack_reserve_size,
                                  klv_start_address,
                                  klv_end_address)) {
    return advance_ut_adress(repack_start,
                             repack_reserve_size,
                             klv_start_address,
                             klv_end_address);
  }

  return write_addr;
}

/***************************************************************************//**
 * Returns whether a non-wrapped direct append still leaves a valid repack
 * reserve window outside the live span after the write completes.
 *
 * For non-wrapped chains, the live objects occupy `[chain_anchor, future_tail)`.
 * The repack reserve must therefore live entirely in either:
 * - the suffix after `future_tail`, or
 * - the reclaimed/free prefix before `chain_anchor`.
 ******************************************************************************/
static bool has_valid_nonwrapped_repack_window(uint32_t chain_anchor,
                                               uint32_t future_tail,
                                               uint32_t repack_start,
                                               uint32_t repack_reserve_size,
                                               uint32_t klv_start_address,
                                               uint32_t klv_end_address)
{
  uint32_t repack_offset = 0U;
  uint32_t anchor_offset = 0U;
  uint32_t future_tail_offset = 0U;
  uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);

  if (repack_reserve_size == 0U) {
    return true;
  }

  if ((total_region_size == 0U)
      || !convert_address_to_logical_offset(repack_start,
                                            klv_start_address,
                                            klv_end_address,
                                            &repack_offset)
      || !convert_address_to_logical_offset(chain_anchor,
                                            klv_start_address,
                                            klv_end_address,
                                            &anchor_offset)
      || !convert_address_to_logical_offset(future_tail,
                                            klv_start_address,
                                            klv_end_address,
                                            &future_tail_offset)) {
    return false;
  }

  if (future_tail_offset <= repack_offset) {
    return true;
  }

  return ((repack_offset + repack_reserve_size) <= anchor_offset)
         && ((repack_offset + repack_reserve_size) <= total_region_size);
}

/***************************************************************************//**
 * Decodes a UT object located at @p obj_addr and returns its logical metadata.
 *
 * Beyond normal header parsing, this helper also computes the wrapped
 * `next_addr` so anchor-based scans can walk the ring without duplicating the
 * stored-size math at each call site.
 ******************************************************************************/
static sl_status_t decode_object_from_anchor(uint32_t obj_addr,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address,
                                             sli_klv_ut_obj_info_t *obj_info)
{
  sl_status_t status = SL_STATUS_OK;
  uint8_t header_bytes[SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE] = { 0 };

  if ((obj_info == NULL)
      || !is_address_in_range(obj_addr, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = logical_read(obj_addr,
                        header_bytes,
                        sizeof(header_bytes),
                        klv_start_address,
                        klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // This is strictly the UT ring: the 8-byte legacy static-token header
  // is not a supported UT object. A partially programmed 32-bit magic must
  // be treated as torn even when its low 16 bits resemble legacy magic.
  {
    uint32_t magic_word = 0U;
    memcpy(&magic_word, header_bytes, sizeof(magic_word));
    if ((magic_word != SLI_TOKEN_KLV_ERASE_STATE)
        && (magic_word != SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE)
        && (magic_word != SLI_TOKEN_KLV_MAGIC_KLV_DELETED)) {
      memset(&obj_info->header, 0, sizeof(obj_info->header));
      obj_info->header.is_torn = true;
      obj_info->obj_addr = obj_addr;
      obj_info->next_addr = obj_addr;
      return SL_STATUS_NOT_FOUND;
    }
  }
  status = decode_klv_header(header_bytes, &obj_info->header);
  if (status != SL_STATUS_OK) {
    // Never skip a header whose length cannot be trusted. In particular, the
    // previous mid-page pseudo-object advanced to the next page, where lazy
    // erase can leave stale, CRC-valid objects from an older ring generation.
    // Reporting NOT_FOUND with is_torn=true preserves reads of the committed
    // prefix and records a non-writable tail for all mutation entry points.
    // The same rule applies at page boundaries and in extension segments.
    if (obj_info->header.is_torn) {
      obj_info->obj_addr = obj_addr;
      obj_info->next_addr = obj_addr;
      return SL_STATUS_NOT_FOUND;
    }
    return status;
  }

  {
    uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
    uint32_t object_size = 0U;

    // Reject impossible lengths BEFORE rounding to word alignment: the
    // SL_CEILING macro can wrap for a torn 0xFFFFFFFE/0xFFFFFFFF length and
    // otherwise make that header look like a tiny, followable object.
    if ((total_region_size <= obj_info->header.header_size)
        || (obj_info->header.payload_length == 0U)
        || (obj_info->header.payload_length > UINT32_MAX - 3U)
        || (obj_info->header.payload_length
            > total_region_size - obj_info->header.header_size)) {
      obj_info->header.is_torn = true;
      obj_info->obj_addr = obj_addr;
      obj_info->next_addr = obj_addr;
      return SL_STATUS_NOT_FOUND;
    }
    object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(obj_info->header.header_size,
                                                       obj_info->header.payload_length);

    // A decodable header with an impossible size is also non-followable.
    // Treat it as an unsafe tail, never as a length that can jump around the
    // ring and accidentally make stale objects live again.
    if ((total_region_size == 0U)
        || (object_size < obj_info->header.header_size)
        || (object_size > total_region_size)) {
      obj_info->header.is_torn = true;
      obj_info->obj_addr = obj_addr;
      obj_info->next_addr = obj_addr;
      return SL_STATUS_NOT_FOUND;
    }
  }

  // Precompute the next address once so scan/reclaim callers can reason about
  // object-to-object transitions without redoing the size and wrap logic.
  obj_info->obj_addr = obj_addr;
  obj_info->next_addr = advance_ut_adress(obj_addr,
                                          SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(obj_info->header.header_size,
                                                                              obj_info->header.payload_length),
                                          klv_start_address,
                                          klv_end_address);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns whether the UT tail record is the frontier.
 ******************************************************************************/
static bool ut_tail_is_frontier(uint32_t addr, uint32_t start, uint32_t end)
{
  sli_klv_ut_tail_record_t rec = { 0 };
  sl_status_t status = ut_tail_read(&rec);
  if (status == SL_STATUS_NOT_FOUND) {
    return false;
  }
  // Unknown metadata must never authorize additional raw-flash traversal.
  return (status != SL_STATUS_OK)
         || !ut_tail_record_valid(&rec, start, end)
         || rec.stable_tail == addr;
}

/***************************************************************************//**
 * Called after repair of the pending torn header; previous stable tail remains
 * authoritative. Does not erase anything by itself.
 ******************************************************************************/
static sl_status_t ut_tail_cancel_torn_header(uint32_t start, uint32_t end)
{
  sli_klv_ut_tail_record_t rec = { 0 };
  sl_status_t status = ut_tail_read(&rec);
  if (status == SL_STATUS_NOT_FOUND) {
    return SL_STATUS_OK;
  }
  if (status != SL_STATUS_OK || !ut_tail_record_valid(&rec, start, end)) {
    return (status == SL_STATUS_OK) ? SL_STATUS_INVALID_STATE : status;
  }
  if (rec.state == SLI_KLV_UT_TAIL_HEADER_PENDING) {
    rec.pending_header_addr = 0U;
    rec.state = SLI_KLV_UT_TAIL_STABLE;
    return ut_tail_store(&rec);
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Used after the straighten-only reclaim and BEFORE promoting extensions.
 * Only the last-extension-to-base wrap frontier is remapped; all other
 * physical tail addresses retain their identity under the expanded ring.
 ******************************************************************************/
sl_status_t sli_klv_ut_tail_prepare_extension(const sli_klv_ut_context_t *ctx,
                                               uint32_t first_new_extension)
{
  sli_klv_ut_tail_record_t rec = { 0 };
  sl_status_t status;
  if (ctx == NULL || ctx->chain_anchor == NULL || ctx->tail_load == NULL
      || ctx->tail_store == NULL || first_new_extension == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  set_active_ut_context(ctx);
  status = ut_tail_read(&rec);
  if (status != SL_STATUS_OK
      || !ut_tail_record_valid(&rec, ctx->klv_start_address,
                                ctx->klv_end_address)
      || rec.state != SLI_KLV_UT_TAIL_STABLE) {
    return (status == SL_STATUS_OK) ? SL_STATUS_INVALID_STATE : status;
  }
  if (rec.stable_tail == ctx->klv_start_address
      && *ctx->chain_anchor != rec.stable_tail) {
    rec.state = SLI_KLV_UT_TAIL_LAYOUT_PENDING;
    rec.pending_header_addr = first_new_extension;
    return ut_tail_store(&rec);
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Inspects the logical prefix before @p chain_anchor for wrap-to-start safety.
 *
 * In a non-wrapped layout the live span starts at the anchor, but pages below it
 * may still contain stale objects from older wrapped layouts. This walk only
 * decides whether wrapping a write directly to region start is safe; it does
 * not contribute to live-chain occupied_bytes.
 ******************************************************************************/
sl_status_t sli_klv_ut_tail_recover(const sli_klv_ut_context_t *ctx)
{
  sli_klv_ut_tail_record_t rec = { 0 };
  sli_klv_ut_obj_info_t obj = { 0 };
  sl_status_t status;

  if ((ctx == NULL) || (ctx->chain_anchor == NULL)
      || (ctx->tail_load == NULL) || (ctx->tail_store == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  set_active_ut_context(ctx);
  status = ut_tail_read(&rec);
  if (status == SL_STATUS_NOT_FOUND) {
    rec.stable_tail = *ctx->chain_anchor;
    rec.state = SLI_KLV_UT_TAIL_STABLE;
    return ut_tail_store(&rec);
  }
  if (status != SL_STATUS_OK) {
    return status;
  }
  if (!ut_tail_record_valid(&rec, ctx->klv_start_address,
                            ctx->klv_end_address)) {
    return SL_STATUS_INVALID_STATE;
  }
  if (rec.state == SLI_KLV_UT_TAIL_STABLE) {
    return SL_STATUS_OK;
  }
  if (rec.state == SLI_KLV_UT_TAIL_LAYOUT_PENDING) {
    // NVM3 extension directory was either promoted atomically or is still
    // inactive. Determine which from the loaded active ring, then replace
    // this one tail record atomically. No physical address ordering assumed.
    if (is_address_in_range(rec.pending_header_addr,
                            ctx->klv_start_address, ctx->klv_end_address)) {
      rec.stable_tail = rec.pending_header_addr;
    }
    rec.pending_header_addr = 0U;
    rec.state = SLI_KLV_UT_TAIL_STABLE;
    return ut_tail_store(&rec);
  }

  // Recovery inspects precisely the address that was journaled BEFORE the
  // write. Unlike scanning after an erased page it cannot adopt stale data.
  status = decode_object_from_anchor(rec.pending_header_addr,
                                      ctx->klv_start_address,
                                      ctx->klv_end_address, &obj);
  if (status == SL_STATUS_NOT_FOUND) {
    bool programmed = false;
    // A pending header may have erased magic and partially programmed
    // length/key/CRC. Keep the reservation until journaled repair finishes.
    status = ut_pending_header_is_programmed(rec.pending_header_addr,
                                              ctx->klv_start_address,
                                              ctx->klv_end_address,
                                              &programmed);
    if (status != SL_STATUS_OK) {
      return status;
    }
    if (programmed) {
      return SL_STATUS_OK;
    }
    return ut_tail_cancel_torn_header(ctx->klv_start_address,
                                      ctx->klv_end_address);
  }
  if (status != SL_STATUS_OK) {
    return status;
  }
  // A fully committed copied/reclaimed object MUST have valid data as well.
  // A still-WIP object is allowed: normal WIP recovery will validate/skip it.
  if (obj.header.is_readable) {
    uint16_t crc = 0U;
    uint32_t payload = advance_ut_adress(rec.pending_header_addr,
                                         obj.header.header_size,
                                         ctx->klv_start_address,
                                         ctx->klv_end_address);
    status = calculate_logical_checksum(payload, obj.header.payload_length,
                                         ctx->klv_start_address,
                                         ctx->klv_end_address, &crc);
    if (status != SL_STATUS_OK || crc != obj.header.payload_crc) {
      return SL_STATUS_INVALID_SIGNATURE;
    }
  }

  // Empty-chain writes can choose a new first slot after a reserved gap.
  // Publish the new anchor while the reservation is still present; repeated
  // power failures replay this transition idempotently.
  if (rec.pending_header_addr != rec.stable_tail) {
    if (*ctx->chain_anchor == rec.stable_tail) {
      if (ctx->anchor_commit == NULL) {
        return SL_STATUS_INVALID_STATE;
      }
      status = ctx->anchor_commit(rec.pending_header_addr,
                                  ctx->anchor_context);
      if (status != SL_STATUS_OK) {
        return status;
      }
      *ctx->chain_anchor = rec.pending_header_addr;
    } else if (*ctx->chain_anchor != rec.pending_header_addr) {
      return SL_STATUS_INVALID_STATE;
    }
  }
  return ut_tail_publish_header(rec.pending_header_addr,
                                 ctx->klv_start_address,
                                 ctx->klv_end_address);
}

/***************************************************************************//**
 * Inspects the logical prefix before @p chain_anchor for wrap-to-start safety.
 *
 * In a non-wrapped layout the live span starts at the anchor, but pages below it
 * may still contain stale objects from older wrapped layouts. This walk only
 * decides whether wrapping a write directly to region start is safe; it does
 * not contribute to live-chain occupied_bytes.
 *
 * Wrap-to-start requires the entire prefix to be free of programmed objects.
 * Progress is tracked with logical offsets so erased holes cannot restart the
 * walk at an earlier physical page and hang recovery.
 ******************************************************************************/
static sl_status_t scan_prefix_before_anchor(uint32_t chain_anchor,
                                             uint32_t klv_start_address,
                                             uint32_t *reclaimable_bytes,
                                             bool *wrap_start_allowed)
{
  uint32_t klv_end_address = active_ut_context.klv_end_address;
  uint32_t anchor_offset = 0U;
  uint32_t current_offset = 0U;
  uint32_t max_steps = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((reclaimable_bytes == NULL) || (wrap_start_allowed == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  *wrap_start_allowed = true;
  if (!convert_address_to_logical_offset(chain_anchor,
                                         klv_start_address,
                                         klv_end_address,
                                         &anchor_offset)
      || (anchor_offset == 0U)) {
    return SL_STATUS_OK;
  }

  // One step per word is enough to bound any walk through the prefix.
  max_steps = (anchor_offset / sizeof(uint32_t)) + 1U;

  for (uint32_t step = 0U; (step < max_steps) && (current_offset < anchor_offset); step++) {
    sli_klv_ut_obj_info_t current_info = { 0 };
    uint32_t current_addr = 0U;
    uint32_t object_size = 0U;
    uint32_t next_offset = 0U;

    if (!convert_logical_offset_to_address(current_offset,
                                           klv_start_address,
                                           klv_end_address,
                                           &current_addr)) {
      return SL_STATUS_INVALID_STATE;
    }

    status = decode_object_from_anchor(current_addr,
                                       klv_start_address,
                                       klv_end_address,
                                       &current_info);
    if ((status == SL_STATUS_NOT_FOUND)
        || (status == SL_STATUS_INVALID_SIGNATURE)
        || (status == SL_STATUS_FAIL)) {
      // Erased flash, or programmed junk that is not a followable object.
      // Either way move forward by one logical page. Corrupt or torn headers
      // must not abort recovery, but they do forbid wrap-to-start.
      if ((status != SL_STATUS_NOT_FOUND) || current_info.header.is_torn) {
        *wrap_start_allowed = false;
      }
      next_offset = (current_offset - (current_offset % FLASH_PAGE_SIZE)) + FLASH_PAGE_SIZE;
      if (next_offset <= current_offset) {
        break;
      }
      current_offset = next_offset;
      continue;
    } else if (status != SL_STATUS_OK) {
      return status;
    }

    *wrap_start_allowed = false;
    object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(current_info.header.header_size,
                                                      current_info.header.payload_length);
    if (!current_info.header.is_readable) {
      *reclaimable_bytes += object_size;
    }

    if (!convert_address_to_logical_offset(current_info.next_addr,
                                           klv_start_address,
                                           klv_end_address,
                                           &next_offset)
        || (next_offset <= current_offset)
        || (next_offset > anchor_offset)) {
      break;
    }

    current_offset = next_offset;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Scans the UT ring from the logical anchor and summarizes the current chain.
 *
 * This is the main discovery helper used by anchor-based reads, writes, deletes,
 * and recovery. It identifies the current tail, whether the chain is empty or
 * full, the newest live match for a token, occupied/reclaimable byte counts, and
 * the largest live object size.
 *
 * occupied_bytes cover only the committed live span from chain_anchor
 * through the object before tail. Stale programmed data outside
 * that span is not counted here, for non-wrapped layouts it is inspected
 * separately only to set wrap_start_allowed and may add to reclaimable_bytes
 * (prefix) without changing occupied_bytes or live_span_reclaimable_bytes.
 *
 * When @p token is @ref SLI_KLV_UT_FULL_CHAIN_SCAN_TOKEN, the walk covers the
 * entire live span through the tail and @p scan_result->largest_live_obj_size
 * is authoritative for the chain. Token-specific scans may stop before the
 * tail, their per-scan maximum must not be used to update the global cache.
 *
 * When the live span is not wrapped, the helper also inspects the physical
 * prefix before the anchor to determine whether wrapping back to the region
 * start would collide with stale objects that have not yet been reclaimed.
 ******************************************************************************/
static sl_status_t scan_from_anchor(uint32_t token,
                                    uint32_t chain_anchor,
                                    uint32_t klv_start_address,
                                    uint32_t klv_end_address,
                                    sli_klv_ut_scan_result_t *scan_result)
{
  sl_status_t status = SL_STATUS_OK;
  uint32_t current = chain_anchor;
  bool saw_object = false;
  sli_klv_ut_tail_record_t tail_rec = { 0 };
  bool bounded = false;

  if ((scan_result == NULL)
      || !is_address_in_range(chain_anchor, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memset(scan_result, 0, sizeof(*scan_result));
  // An atomic NVM3 tail is the authoritative frontier; never walk into a
  // CRC-valid stale object beyond it. Tests without callbacks use legacy scan.
  status = ut_tail_read(&tail_rec);
  if (status == SL_STATUS_OK) {
    if (!ut_tail_record_valid(&tail_rec, klv_start_address, klv_end_address)) {
      return SL_STATUS_INVALID_STATE;
    }
    bounded = true;
  } else if (status != SL_STATUS_NOT_FOUND) {
    return status;
  }
  scan_result->wrap_start_allowed =
    (compare_logical_address(chain_anchor,
                             klv_start_address,
                             klv_start_address,
                             klv_end_address) == 0);

  {
    uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
    uint32_t max_steps = (total_region_size / sizeof(uint32_t)) + 1U;
    uint32_t step = 0U;
    bool terminated = false;

    while (step < max_steps) {
      sli_klv_ut_obj_info_t current_info = { 0 };
      uint32_t object_size = 0U;

      if (bounded && current == tail_rec.stable_tail) {
        // The stable NVM3 tail bounds the current chain. Its physical bytes
        // need NOT be blank; an old lap can leave valid, CRC-correct headers
        // outside this boundary. A stable tail is not a torn-header signal.
        // Check/erase any obsolete destination pages only AFTER the complete
        // append plan is accepted (not during reads or scanning).
        if (tail_rec.state == SLI_KLV_UT_TAIL_HEADER_PENDING
            && tail_rec.pending_header_addr == current) {
          bool programmed = false;
          status = ut_pending_header_is_programmed(current,
                                                   klv_start_address,
                                                   klv_end_address,
                                                   &programmed);
          if (status != SL_STATUS_OK) {
            return status;
          }
          scan_result->torn_tail = programmed;
          status = decode_object_from_anchor(current, klv_start_address,
                                             klv_end_address, &current_info);
          if (status == SL_STATUS_OK) {
            return SL_STATUS_INVALID_STATE; // Recover valid pending first.
          }
          if (status != SL_STATUS_NOT_FOUND) {
            return status;
          }
        }
        scan_result->tail = current;
        scan_result->chain_empty = !saw_object;
        terminated = true;
        break;
      }

      status = decode_object_from_anchor(current,
                                         klv_start_address,
                                         klv_end_address,
                                         &current_info);
      if (status == SL_STATUS_NOT_FOUND) {
        // Both erased flash and a non-followable programmed header end the
        // *discoverable* chain. Only erased flash is a writable tail.
        // Persist no speculative skip to a later page: it can contain stale
        // objects from a previous lazy-erase cycle.
        scan_result->torn_tail = current_info.header.is_torn;
        if (!saw_object) {
          scan_result->chain_empty = true;
        }
        scan_result->tail = current;
        terminated = true;
        break;
      } else if (status != SL_STATUS_OK) {
        return status;
      }

      saw_object = true;
      object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(current_info.header.header_size,
                                                        current_info.header.payload_length);
      if (bounded && tail_rec.state == SLI_KLV_UT_TAIL_STABLE) {
        uint32_t dist = cal_distance_forward(current,
                                              tail_rec.stable_tail,
                                              klv_start_address,
                                              klv_end_address);
        if (dist == 0U || object_size > dist) {
          return SL_STATUS_INVALID_STATE;
        }
      }
      scan_result->occupied_bytes += object_size;

      if (current_info.header.is_readable) {
        if (object_size > scan_result->largest_live_obj_size) {
          scan_result->largest_live_obj_size = object_size;
        }
      } else {
        scan_result->reclaimable_bytes += object_size;
        // WIP remains erase-reclaimable until finish/recovery, but its
        // declared size is reserved capacity for concurrent writers.
        if (current_info.header.is_write_in_progress) {
          scan_result->live_span_wip_bytes += object_size;
          if (object_size > scan_result->largest_wip_obj_size) {
            scan_result->largest_wip_obj_size = object_size;
          }
        }
      }

      if (current_info.header.is_readable
          && (current_info.header.key == (token & 0xFFFFU))) {
        scan_result->match = current_info;
        scan_result->match_found = true;
      }

      current = current_info.next_addr;
      step++;
      if (current == chain_anchor) {
        // Returning to the anchor without finding an erased slot means the ring is
        // fully populated end-to-end.
        scan_result->chain_full = true;
        scan_result->tail = current;
        terminated = true;
        break;
      }
    }

    if (!terminated) {
      return SL_STATUS_INVALID_STATE;
    }
  }

  // Snapshot live-span reclaimable before prefix inflation. Capacity and used
  // accounting must subtract only this value from occupied_bytes.
  scan_result->live_span_reclaimable_bytes = scan_result->reclaimable_bytes;

  // Prefix scan runs for non-full rings whose live span does not wrap, including
  // chain_empty. An erased head with soft-deleted/stale objects still sitting in
  // the logical prefix must report those bytes as reclaimable.
  if (!scan_result->chain_full
      && (compare_logical_address(scan_result->tail,
                                  chain_anchor,
                                  klv_start_address,
                                  klv_end_address) >= 0)
      && (compare_logical_address(chain_anchor,
                                  klv_start_address,
                                  klv_start_address,
                                  klv_end_address) > 0)) {
    // Prefix is outside the live span so it never changes occupied_bytes.
    // unreadable prefix objects still add to reclaimable_bytes for storage-info
    // accuracy and reclaim eligibility.
    status = scan_prefix_before_anchor(chain_anchor,
                                       klv_start_address,
                                       &scan_result->reclaimable_bytes,
                                       &scan_result->wrap_start_allowed);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Advances @p chain_anchor past consecutive soft-deleted heads and persists it.
 *
 * Stops at the first readable object, an in-progress (WIP) object, an erased
 * slot, or after a full wrap. WIP heads are intentionally kept: concurrent
 * streaming writes may leave a valid provisional object at the head, and
 * jumping over it would orphan that reservation behind the live span.
 *
 * When the walk would leave only soft-deleted objects behind an erased
 * "empty" head (sole remaining content is deleted, or a deleted-only
 * prefix before free space), the anchor stays on the last soft-deleted
 * object. Advancing into erased flash would make the next scan report
 * @c chain_empty with @c reclaimable_bytes == 0, so writes would reject
 * reclaim even though those deleted bytes still need a reclaim pass.
 *
 * When the head moves, @p ut_context->anchor_commit publishes it before return
 * so power loss cannot leave NVM3 pointing at a soft-deleted slot while the
 * live head only exists in RAM.
 ******************************************************************************/
static sl_status_t skip_unreadable_chain_head(uint32_t *chain_anchor,
                                              const sli_klv_ut_context_t *ut_context)
{
  sl_status_t status = SL_STATUS_OK;
  uint32_t start = 0U;
  uint32_t current = 0U;
  uint32_t previous_anchor = 0U;
  uint32_t last_deleted_addr = 0U;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;

  if ((chain_anchor == NULL)
      || (ut_context == NULL)
      || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;

  if (!is_address_in_range(*chain_anchor, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  previous_anchor = *chain_anchor;
  start = *chain_anchor;
  current = start;

  {
    uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
    uint32_t max_steps = (total_region_size / sizeof(uint32_t)) + 1U;
    uint32_t step = 0U;
    bool terminated = false;

    while (step < max_steps) {
      sli_klv_ut_obj_info_t current_info = { 0 };
      uint32_t magic_word = 0U;

      if (ut_tail_is_frontier(current, klv_start_address, klv_end_address)) {
        if (last_deleted_addr != 0U) {
          current = last_deleted_addr;
        }
        terminated = true;
        break;
      }
      status = decode_object_from_anchor(current,
                                         klv_start_address,
                                         klv_end_address,
                                         &current_info);
      if (status == SL_STATUS_NOT_FOUND) {
        // Erased slot (or a torn header at a page base, which ends the chain
        // the same way). If we already walked past soft-deleted object(s),
        // keep the last deleted address as the head so reclaim accounting
        // still sees those bytes in the live span.
        if (last_deleted_addr != 0U) {
          current = last_deleted_addr;
        }
        terminated = true;
        break;
      } else if (status != SL_STATUS_OK) {
        return status;
      }

      status = logical_read(current,
                            &magic_word,
                            sizeof(magic_word),
                            klv_start_address,
                            klv_end_address);
      if (status != SL_STATUS_OK) {
        return status;
      }

      // Soft-deleted heads are skipped when a later readable/WIP head exists.
      // Readable and ACTIVE+WIP objects both remain valid logical heads (WIP
      // must stay in the live span).
      if (magic_word != SLI_TOKEN_KLV_MAGIC_KLV_DELETED) {
        terminated = true;
        break;
      }

      last_deleted_addr = current;
      current = current_info.next_addr;
      step++;
      if (current == start) {
        // Full wrap of soft-deleted objects: keep the original head rather than
        // spinning. Those deleted bytes remain the live span until reclaim.
        current = start;
        terminated = true;
        break;
      }
    }

    if (!terminated) {
      return SL_STATUS_INVALID_STATE;
    }
  }

  *chain_anchor = current;
  if (*chain_anchor == previous_anchor) {
    return SL_STATUS_OK;
  }

  status = ut_context->anchor_commit(*chain_anchor, ut_context->anchor_context);
  if (status != SL_STATUS_OK) {
    *chain_anchor = previous_anchor;
  }

  return status;
}

/***************************************************************************//**
 * Returns whether a logical UT byte range still reads as erased.
 *
 * The range may wrap the logical ring end, so it is read through
 * @ref logical_read() rather than straight off the backend.
 ******************************************************************************/
static bool is_logical_range_blank(uint32_t addr,
                                   uint32_t length,
                                   uint32_t klv_start_address,
                                   uint32_t klv_end_address)
{
  uint32_t chunk[8] = { 0 };
  uint32_t remaining = length;
  uint32_t current = addr;

  while (remaining > 0U) {
    uint32_t read_size = (remaining < sizeof(chunk)) ? remaining : sizeof(chunk);
    uint32_t word_count = (read_size + sizeof(chunk[0]) - 1U) / sizeof(chunk[0]);

    memset(chunk, 0xFF, sizeof(chunk));
    if (logical_read(current,
                     chunk,
                     read_size,
                     klv_start_address,
                     klv_end_address) != SL_STATUS_OK) {
      return false;
    }

    for (uint32_t i = 0U; i < word_count; i++) {
      if (chunk[i] != 0xFFFFFFFFU) {
        return false;
      }
    }

    current = advance_ut_adress(current,
                                read_size,
                                klv_start_address,
                                klv_end_address);
    remaining -= read_size;
  }

  return true;
}

/***************************************************************************//**
 * Returns whether any object reachable from @p chain_anchor stores bytes inside
 * @p page.
 *
 * The walk mirrors @ref scan_from_anchor(): it follows @c next_addr from the
 * head and stops at the first erased slot or after a full wrap. Soft-deleted
 * objects count as occupants because the walk traverses them, so erasing one
 * would cut every later traversal from the persisted head.
 *
 * Anything undecodable is reported as occupied. That keeps this helper from
 * deciding a page is free based on bytes it could not interpret.
 ******************************************************************************/
static bool page_holds_chain_bytes(uint32_t page,
                                      uint32_t chain_anchor,
                                      uint32_t klv_start_address,
                                      uint32_t klv_end_address)
{
  uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  uint32_t current = chain_anchor;
  uint32_t max_steps = 0U;

  if ((total_region_size == 0U)
      || !is_address_in_range(chain_anchor, klv_start_address, klv_end_address)) {
    return true;
  }

  // One step per word bounds the walk even if a corrupt next_addr forms a cycle
  // that never returns to the anchor.
  max_steps = (total_region_size / sizeof(uint32_t)) + 1U;

  for (uint32_t step = 0U; step < max_steps; step++) {
    {
      sli_klv_ut_tail_record_t bound = { 0 };
      sl_status_t bound_status = ut_tail_read(&bound);
      if (bound_status != SL_STATUS_NOT_FOUND) {
        if (bound_status != SL_STATUS_OK
            || !ut_tail_record_valid(&bound, klv_start_address,
                                      klv_end_address)) {
          return true; // Never authorize an erase with bad NVM3 metadata.
        }
        if (current == bound.stable_tail) {
          return false;
        }
      }
    }
    sli_klv_ut_obj_info_t current_info = { 0 };
    uint32_t object_size = 0U;
    sl_status_t status = decode_object_from_anchor(current,
                                                   klv_start_address,
                                                   klv_end_address,
                                                   &current_info);

    if (status == SL_STATUS_NOT_FOUND) {
      // Erased slot: the reachable chain ends before this point.
      return false;
    } else if (status != SL_STATUS_OK) {
      return true;
    }

    object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(current_info.header.header_size,
                                                      current_info.header.payload_length);
    if (check_range_overlaps(current,
                             object_size,
                             page,
                             FLASH_PAGE_SIZE,
                             klv_start_address,
                             klv_end_address)) {
      return true;
    }

    current = current_info.next_addr;
    if (current == chain_anchor) {
      return false;
    }
  }

  return true;
}

/***************************************************************************//**
 * Makes sure a direct append can only ever program erased flash.
 *
 * Flash programming clears bits, so writing an object over a page that still
 * holds an older object stores the bitwise AND of the two. The program itself
 * reports success and the damage only surfaces as a CRC failure on the next
 * read. That is what a wrap onto a stale page produces once the head has moved
 * on but the page behind it was never reclaimed.
 *
 * Destination pages are cleaned lazily here, at the moment they are consumed,
 * rather than eagerly when the head vacates them. Erasing behind the head would
 * keep the ring permanently near-empty, and the reclaim pressure that drives
 * pending-extension activation would never build up.
 *
 * A page is only erased when the append enters it at its base and no object
 * reachable from @p chain_anchor stores bytes in it. A page entered at its
 * base that still holds chain bytes is reported as unavailable
 * (SL_STATUS_NO_MORE_RESOURCE) until the head has left it. Entering a page
 * mid-way requires the footprint to be blank already, anything else means the
 * chosen address overlaps data that is still in use, which is reported
 * (SL_STATUS_INVALID_STATE) instead of being overwritten.
 ******************************************************************************/
static sl_status_t check_or_prepare_append_destination(uint32_t write_addr,
                                                       uint32_t object_size,
                                                       uint32_t chain_anchor,
                                                       uint32_t klv_start_address,
                                                       uint32_t klv_end_address,
                                                       bool erase_obsolete_pages)
{
  uint32_t remaining = object_size;
  uint32_t current = write_addr;

  while (remaining > 0U) {
    uint32_t page = SLI_KLV_ANCHOR_PAGE_START(current);
    uint32_t chunk = FLASH_PAGE_SIZE - (current - page);
    bool footprint_blank;
    sl_status_t status = SL_STATUS_OK;

    if (chunk > remaining) {
      chunk = remaining;
    }

    // Bytes logically before the head on the head's own page are never
    // writable, blank or not. The page cannot be erased while the head is on
    // it, and once the head moves on the rest of the page is dead but still
    // unerasable: a tail left there is stranded mid-page next to stale bytes
    // and neither appends nor reclaim can make progress. Writes after the tail
    // on a page the head also occupies (small non-wrapped chain) are logically
    // after the head and stay allowed.
    if ((page == SLI_KLV_ANCHOR_PAGE_START(chain_anchor))
        && (compare_logical_address(current,
                                    chain_anchor,
                                    klv_start_address,
                                    klv_end_address) < 0)) {
      TOKENDBG(printf("UT append blocked by live head page: write_addr=0x%08lx page=0x%08lx anchor=0x%08lx\r\n",
                      (unsigned long)write_addr,
                      (unsigned long)page,
                      (unsigned long)chain_anchor); )
      return SL_STATUS_NO_MORE_RESOURCE;
    }

    footprint_blank = is_logical_range_blank(current,
                                             chunk,
                                             klv_start_address,
                                             klv_end_address);

    // Entering mid-page: the bytes below the cursor belong to an earlier
    // object, so the page cannot be erased and the footprint must already be
    // blank.
    if (current != page) {
      if (!footprint_blank) {
        TOKENDBG(printf("UT append blocked by occupied flash: write_addr=0x%08lx page=0x%08lx anchor=0x%08lx\r\n",
                        (unsigned long)write_addr,
                        (unsigned long)page,
                        (unsigned long)chain_anchor); )
        return SL_STATUS_INVALID_STATE;
      }
    } else if (!is_logical_range_blank(page,
                                       FLASH_PAGE_SIZE,
                                       klv_start_address,
                                       klv_end_address)) {
      // Entering a page at its base is the only moment it can be erased, so
      // judge the whole page rather than just this object's footprint. Stale
      // data beyond the object would otherwise survive, block the next append
      // that starts mid-page, and leave a later reclaim copy no blank room.
      //
      // A page that still carries live chain bytes (the head side of the ring)
      // must not be erased, and it must not be written into either, even when
      // this object's footprint happens to be blank: once the head moves on,
      // the rest of the page is dead but unerasable, the tail is stranded
      // mid-page, and neither appends nor reclaim can make progress. Treat the
      // page as unavailable until the head has left it.
      if (page_holds_chain_bytes(page,
                                 chain_anchor,
                                 klv_start_address,
                                 klv_end_address)) {
        TOKENDBG(printf("UT append blocked by live head page: write_addr=0x%08lx page=0x%08lx anchor=0x%08lx\r\n",
                        (unsigned long)write_addr,
                        (unsigned long)page,
                        (unsigned long)chain_anchor); )
        return SL_STATUS_NO_MORE_RESOURCE;
      } else {
        if (erase_obsolete_pages) {
          TOKENDBG(printf("UT erase stale append page: page=0x%08lx write_addr=0x%08lx anchor=0x%08lx\r\n",
                          (unsigned long)page,
                          (unsigned long)write_addr,
                          (unsigned long)chain_anchor); )
          // Erase once only when the destination page actually contains old
          // programmed bytes; empty pages are never erased redundantly.
          status = sli_token_manager_nvm_page_erase(page);
          if (status != SL_STATUS_OK) {
            return status;
          }
        }
      }
    }

    current = advance_ut_adress(current,
                                chunk,
                                klv_start_address,
                                klv_end_address);
    remaining -= chunk;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Actual append preparation after admission. The same implementation is used
 * for preflight (erase_obsolete_pages=false) so no flash page is erased just
 * because wrap was considered. All addresses are in logical extension order.
 ******************************************************************************/
static sl_status_t prepare_append_destination(uint32_t write_addr,
                                              uint32_t object_size,
                                              uint32_t chain_anchor,
                                              uint32_t klv_start_address,
                                              uint32_t klv_end_address)
{
  return check_or_prepare_append_destination(write_addr,
                                             object_size,
                                             chain_anchor,
                                             klv_start_address,
                                             klv_end_address,
                                             true);
}

/***************************************************************************//**
 * Makes sure a reclaim relocation or the final reclaim write lands on erased
 * flash.
 *
 * Reclaim derives its destinations from ring geometry (tail slack, reserve
 * window, free suffix) and from how far the erase cursor has swept. None of
 * that proves the destination pages are blank: with lazy page erase a free
 * suffix or gap can still hold objects from an earlier lap. Apply the same
 * rule as a direct append right before the bytes are programmed, with the
 * chain measured from the head reclaim is currently committed to, so a page
 * that still carries live or relocated objects is never erased.
 ******************************************************************************/
static sl_status_t prepare_reclaim_destination(uint32_t write_addr,
                                               uint32_t object_size,
                                               uint32_t klv_start_address,
                                               uint32_t klv_end_address)
{
  if (object_size == 0U) {
    return SL_STATUS_OK;
  }

  if (!active_ut_context_valid || (active_ut_context.chain_anchor == NULL)) {
    return SL_STATUS_INVALID_STATE;
  }

  return prepare_append_destination(write_addr,
                                    object_size,
                                    *active_ut_context.chain_anchor,
                                    klv_start_address,
                                    klv_end_address);
}

/***************************************************************************//**
 * Selects a direct-write address for a UT append while preserving reclaim
 * reserve requirements.
 *
 * The helper tries the cheapest valid option first:
 * 1. Append in place after the tail if enough space remains.
 * 2. If the chain is not wrapped, try wrapping to the region start, but only if
 *    doing so still leaves a future repack reserve gap.
 * 3. If the chain is already wrapped, validate that the tail-to-anchor gap can
 *    hold both the object and the required reserve.
 *
 * If none of those options work, the caller must fall back to reclaim.
 *
 * @param[in]  plan        Direct-append geometry and reserve requirements for
 *                         the pending UT write.
 * @param[in]  ut_context  Shared UT context providing logical storage bounds.
 * @param[out] write_addr  Logical destination selected for the append path.
 ******************************************************************************/
static sl_status_t select_write_address(const sli_klv_ut_write_plan_t *plan,
                                        const sli_klv_ut_context_t *ut_context,
                                        uint32_t *write_addr)
{
  uint32_t chain_anchor = 0U;
  uint32_t tail = 0U;
  uint32_t object_size = 0U;
  uint32_t repack_reserve_size = 0U;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  uint32_t repack_start = 0U;
  uint32_t candidate = 0U;
  uint32_t gap_repack_start = 0U;
  uint32_t future_tail = 0U;

  if ((plan == NULL) || (ut_context == NULL) || (write_addr == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  chain_anchor = plan->chain_anchor;
  tail = plan->tail;
  object_size = plan->object_size;
  repack_reserve_size = plan->repack_reserve_size;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;

  if (plan->chain_full) {
    return SL_STATUS_FULL;
  }

  if (plan->chain_empty) {
    uint32_t total_region_size = get_total_region_size(klv_start_address,
                                                       klv_end_address);
    uint32_t empty_heads[2];
    uint32_t empty_head_count = 1U;

    // Prefer the persisted empty head (first writable slot after recovery).
    // If that late head leaves too little suffix for object + reserve, fall
    // back to the region base: with no live chain the whole ring is free for
    // a fresh head.
    empty_heads[0] = chain_anchor;
    empty_heads[1] = klv_start_address;
    if (chain_anchor != klv_start_address) {
      empty_head_count = 2U;
    }

    for (uint32_t head_index = 0U; head_index < empty_head_count; head_index++) {
      uint32_t empty_head = empty_heads[head_index];

      repack_start = compute_repack_start(empty_head,
                                          empty_head,
                                          repack_reserve_size,
                                          klv_start_address,
                                          klv_end_address);
      candidate = skip_repack_window(empty_head,
                                     repack_start,
                                     repack_reserve_size,
                                     klv_start_address,
                                     klv_end_address);
      if (!check_range_overlaps_repack(candidate,
                                       object_size,
                                       repack_start,
                                       repack_reserve_size,
                                       klv_start_address,
                                       klv_end_address)
          && ((total_region_size
               - cal_distance_forward(klv_start_address,
                                      candidate,
                                      klv_start_address,
                                      klv_end_address))
              >= (object_size + repack_reserve_size))) {
        *write_addr = candidate;
        return SL_STATUS_OK;
      }
    }

    return SL_STATUS_NO_MORE_RESOURCE;
  }

  repack_start = compute_repack_start(chain_anchor,
                                      tail,
                                      repack_reserve_size,
                                      klv_start_address,
                                      klv_end_address);

  if (compare_logical_address(tail,
                              chain_anchor,
                              klv_start_address,
                              klv_end_address) >= 0) {
    // Non-wrapped layout: try to append after the current tail first.
    candidate = skip_repack_window(tail,
                                   repack_start,
                                   repack_reserve_size,
                                   klv_start_address,
                                   klv_end_address);
    // The prefix walk forbids direct wrap if it sees stale old objects.
    // This prohibition may be relaxed ONLY for a naturally contiguous wrap
    // whose *entire* destination footprint can be safely prepared. A dry
    // run checks all pages and never erases flash during write selection.
    bool safe_natural_wrap = false;
    if (!plan->wrap_start_allowed && candidate == tail
        && (compare_logical_address(advance_ut_adress(candidate, object_size,
                                                       klv_start_address,
                                                       klv_end_address),
                                    candidate,
                                    klv_start_address, klv_end_address) < 0)) {
      safe_natural_wrap =
        (check_or_prepare_append_destination(candidate,
                                             object_size,
                                             chain_anchor,
                                             klv_start_address,
                                             klv_end_address,
                                             false) == SL_STATUS_OK);
    }
    if ((compare_logical_address(candidate,
                                 tail,
                                 klv_start_address,
                                 klv_end_address) >= 0)
        && !check_range_overlaps_repack(candidate,
                                        object_size,
                                        repack_start,
                                        repack_reserve_size,
                                        klv_start_address,
                                        klv_end_address)) {
      future_tail = advance_ut_adress(candidate,
                                      object_size,
                                      klv_start_address,
                                      klv_end_address);
      if (compare_logical_address(future_tail,
                                  candidate,
                                  klv_start_address,
                                  klv_end_address) > 0) {
        *write_addr = candidate;
        return SL_STATUS_OK;
      }

      if ((plan->wrap_start_allowed || safe_natural_wrap)
          && try_compute_post_write_gap_repack_start(candidate,
                                                     object_size,
                                                     chain_anchor,
                                                     repack_reserve_size,
                                                     klv_start_address,
                                                     klv_end_address,
                                                     &gap_repack_start)) {
        *write_addr = candidate;
        return SL_STATUS_OK;
      }
    } else if ((plan->wrap_start_allowed || safe_natural_wrap)
               && try_compute_post_write_gap_repack_start(candidate,
                                                          object_size,
                                                          chain_anchor,
                                                          repack_reserve_size,
                                                          klv_start_address,
                                                          klv_end_address,
                                                          &gap_repack_start)) {
      // A physically wrapped object must stay contiguous with the current tail.
      // Leaving a reserved gap at the region start would break anchor traversal.
      *write_addr = candidate;
      return SL_STATUS_OK;
    }

    if (plan->wrap_start_allowed
        && (cal_distance_forward(klv_start_address,
                                 chain_anchor,
                                 klv_start_address,
                                 klv_end_address) >= object_size)) {
      // If appending at the end would collide with the reserve or region end,
      // try wrapping to the start, but only if the post-write tail still leaves
      // a valid gap for the next reclaim reserve.
      candidate = skip_repack_window(klv_start_address,
                                     repack_start,
                                     repack_reserve_size,
                                     klv_start_address,
                                     klv_end_address);
      if (!check_range_overlaps_repack(candidate,
                                       object_size,
                                       repack_start,
                                       repack_reserve_size,
                                       klv_start_address,
                                       klv_end_address)
          && try_compute_post_write_gap_repack_start(candidate,
                                                     object_size,
                                                     chain_anchor,
                                                     repack_reserve_size,
                                                     klv_start_address,
                                                     klv_end_address,
                                                     &gap_repack_start)
          && (cal_distance_forward(candidate,
                                   chain_anchor,
                                   klv_start_address,
                                   klv_end_address) >= object_size)) {
        *write_addr = candidate;
        return SL_STATUS_OK;
      }
    }
  } else {
    // Wrapped layout: the reserve is the reserve-sized span immediately before
    // the anchor. A direct append is the tail slack in front of that span,
    // including an object that ends exactly on the reserve start.
    if (!try_compute_gap_repack_start(tail,
                                      chain_anchor,
                                      repack_reserve_size,
                                      klv_start_address,
                                      klv_end_address,
                                      &repack_start)) {
      return SL_STATUS_NO_MORE_RESOURCE;
    }

    candidate = tail;
    if ((cal_distance_forward(candidate,
                              chain_anchor,
                              klv_start_address,
                              klv_end_address) >= object_size)
        && try_compute_post_write_gap_repack_start(candidate,
                                                   object_size,
                                                   chain_anchor,
                                                   repack_reserve_size,
                                                   klv_start_address,
                                                   klv_end_address,
                                                   &gap_repack_start)) {
      *write_addr = candidate;
      return SL_STATUS_OK;
    }
  }

  return SL_STATUS_NO_MORE_RESOURCE;
}

/***************************************************************************//**
 * Validates whether a direct UT append remains reclaim-safe after the candidate
 * write address has been selected.
 ******************************************************************************/
static sl_status_t try_direct_write_from_scan(const sli_klv_ut_context_t *ut_context,
                                              const sli_klv_ut_scan_result_t *scan_result,
                                              uint32_t object_size,
                                              uint32_t repack_reserve_size,
                                              uint32_t repack_guard_bytes,
                                              uint32_t *write_addr)
{
  sli_klv_ut_write_plan_t write_plan = { 0 };
  uint32_t chain_anchor = 0U;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  uint32_t total_region_size = 0U;
  uint32_t future_tail = 0U;
  uint32_t repack_start = 0U;
  uint32_t live_used_after_write = 0U;
  uint32_t matched_object_size = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL) || (ut_context->chain_anchor == NULL)
      || (scan_result == NULL) || (write_addr == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  chain_anchor = *ut_context->chain_anchor;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;
  total_region_size = get_total_region_size(klv_start_address, klv_end_address);

  write_plan.chain_anchor = chain_anchor;
  write_plan.tail = scan_result->tail;
  write_plan.object_size = object_size;
  write_plan.repack_reserve_size = repack_reserve_size;
  write_plan.repack_guard_bytes = repack_guard_bytes;
  write_plan.chain_empty = scan_result->chain_empty;
  write_plan.chain_full = scan_result->chain_full;
  write_plan.wrap_start_allowed = scan_result->wrap_start_allowed;

  // An erased head with prefix reclaimable bytes must go through reclaim so
  // stale prefix objects are erased. A direct region-start write would orphan
  // them outside the new live span.
  if (scan_result->chain_empty && (scan_result->reclaimable_bytes > 0U)) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  // Select the write address for the direct append.
  status = select_write_address(&write_plan, ut_context, write_addr);
  if ((status != SL_STATUS_OK) || scan_result->chain_empty) {
    return status;
  }

  future_tail = advance_ut_adress(*write_addr,
                                  object_size,
                                  klv_start_address,
                                  klv_end_address);

  if ((compare_logical_address(*write_addr,
                               chain_anchor,
                               klv_start_address,
                               klv_end_address) >= 0)
      && (compare_logical_address(future_tail,
                                  *write_addr,
                                  klv_start_address,
                                  klv_end_address) > 0)) {
    // Check if the write address is within the non-wrapped repack window.
    if (!has_valid_nonwrapped_repack_window(chain_anchor,
                                            future_tail,
                                            compute_repack_start(chain_anchor,
                                                                 scan_result->tail,
                                                                 repack_reserve_size,
                                                                 klv_start_address,
                                                                 klv_end_address),
                                            repack_reserve_size,
                                            klv_start_address,
                                            klv_end_address)) {
      // Calculate the used bytes after write: readable live + WIP reservations
      // (prefix reclaimable must not be subtracted from occupied_bytes).
      live_used_after_write = (scan_result->occupied_bytes
                               - scan_result->live_span_reclaimable_bytes)
                              + scan_result->live_span_wip_bytes
                              + object_size;
      if (scan_result->match_found) {
        // Calculate the matched object size.
        matched_object_size =
          SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(scan_result->match.header.header_size,
                                              scan_result->match.header.payload_length);
        live_used_after_write -= matched_object_size;
      }

      // Check if the used bytes after write is within the total region size.
      if ((((scan_result->reclaimable_bytes == 0U) && !scan_result->match_found))
          || ((live_used_after_write + repack_reserve_size) > total_region_size)) {
        return SL_STATUS_NO_MORE_RESOURCE;
      }
    }
  }

  if ((future_tail == chain_anchor)
      || ((compare_logical_address(future_tail,
                                   chain_anchor,
                                   klv_start_address,
                                   klv_end_address) < 0)
          && !try_compute_gap_repack_start(future_tail,
                                           chain_anchor,
                                           repack_reserve_size,
                                           klv_start_address,
                                           klv_end_address,
                                           &repack_start))) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  // Even when direct append finds physical space, keep one valid post-write
  // reserve window so a future reclaim can still relocate the largest live or
  // reserved WIP object without double-storing the full chain.
  repack_start = compute_repack_start(chain_anchor,
                                      scan_result->tail,
                                      repack_reserve_size,
                                      klv_start_address,
                                      klv_end_address);
  if ((scan_result->reclaimable_bytes > 0U)
      && check_range_overlaps_repack(scan_result->tail,
                                     object_size,
                                     repack_start,
                                     repack_reserve_size,
                                     klv_start_address,
                                     klv_end_address)) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns whether reclaim could make room for a new UT object of @p object_size.
 *
 * free_total is total - occupied. Prefix bytes sit outside occupied, so they are
 * already included in free_total. The size check therefore adds only
 * live_span_reclaimable_bytes (not full reclaimable_bytes, which also counts
 * prefix and would double-count). reclaimable_bytes > 0 still gates "something
 * to reclaim", including prefix-only stale data that reclaim may erase.
 ******************************************************************************/
static bool can_reclaim_write_from_scan(const sli_klv_ut_context_t *ut_context,
                                        const sli_klv_ut_scan_result_t *scan_result,
                                        uint32_t object_size,
                                        uint32_t repack_reserve_size)
{
  uint32_t total_region_size = 0U;
  uint32_t free_total_bytes = 0U;

  if ((ut_context == NULL) || (scan_result == NULL)) {
    return false;
  }

  total_region_size = get_total_region_size(ut_context->klv_start_address,
                                            ut_context->klv_end_address);
  free_total_bytes = total_region_size - scan_result->occupied_bytes;

  // Reclaim is worthwhile whenever something eraseable remains — including a
  // chain_empty head with only prefix reclaimable bytes (soft-deleted/stale
  // objects before an erased append point). Prefix bytes are already outside
  // occupied_bytes, so they are covered by free_total; live_span_reclaimable
  // still covers deleted bytes inside the committed live span.
  return ((scan_result->reclaimable_bytes > 0U)
          && ((free_total_bytes + scan_result->live_span_reclaimable_bytes)
              >= (object_size + repack_reserve_size)));
}

/***************************************************************************//**
 * Finds the first live object between @p start and @p tail.
 *
 * This is used when the current head object is deleted or otherwise skipped and
 * reclaim/delete needs the next persisted anchor candidate.
 ******************************************************************************/
static sl_status_t find_next_live_anchor(uint32_t start,
                                         uint32_t tail,
                                         uint32_t klv_start_address,
                                         uint32_t klv_end_address,
                                         uint32_t *live_anchor)
{
  sl_status_t status = SL_STATUS_OK;
  uint32_t current = start;

  if (live_anchor == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!is_address_in_range(start, klv_start_address, klv_end_address)
      || !is_address_in_range(tail, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  while (current != tail) {
    sli_klv_ut_obj_info_t current_info = { 0 };

    status = decode_object_from_anchor(current,
                                       klv_start_address,
                                       klv_end_address,
                                       &current_info);
    if (status == SL_STATUS_NOT_FOUND) {
      // Erased slot (or page-base torn header): no further objects exist in
      // the live span.
      *live_anchor = current;
      return SL_STATUS_OK;
    } else if (status != SL_STATUS_OK) {
      return status;
    }

    // Returns the next live anchor if found.
    if (current_info.header.is_readable) {
      *live_anchor = current;
      return SL_STATUS_OK;
    }

    // Move to the next object in the chain.
    current = current_info.next_addr;
  }

  // No live anchor found, so the anchor remains at the tail.
  *live_anchor = tail;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Chooses and persists the next logical UT chain anchor after the current head
 * has been consumed, deleted, or relocated.
 *
 * The anchor update is one of the critical durability points in reclaim: once a
 * page containing the old head becomes reclaimable, the persisted anchor must
 * already point somewhere else. During reclaim, power-fail safety depends on
 * keeping the remaining original live chain reachable for as long as possible,
 * so this helper first scans forward for the next live original object. Only
 * when no later original live object remains does it fall back to a relocated
 * replacement copy provided via @p relocated_head_addr. If neither exists, the
 * anchor falls back to @p tail.
 *
 * @param[in]     next_anchor         First address after the old head object.
 * @param[in]     tail                Current logical tail of the chain.
 * @param[in]     relocated_head_addr Relocated replacement for the old head, or
 *                                    0 when the old head should simply be
 *                                    skipped. Used only when no later original
 *                                    live object remains.
 * @param[in,out] chain_anchor        Persisted logical head to update.
 * @param[in]     ut_context          Shared UT context used to scan for the
 *                                    next live anchor and durably store the
 *                                    new anchor value.
 *
 * @return SL_STATUS_OK if the new anchor is selected and committed successfully,
 *         or an error if validation or persistence fails.
 ******************************************************************************/
static sl_status_t advance_chain_anchor_after_head(uint32_t next_anchor,
                                                   uint32_t tail,
                                                   uint32_t relocated_head_addr,
                                                   uint32_t *chain_anchor,
                                                   const sli_klv_ut_context_t *ut_context)
{
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  sli_klv_ut_anchor_commit_fn_t anchor_commit = NULL;
  void *anchor_context = NULL;
  sl_status_t status = SL_STATUS_OK;
  uint32_t new_anchor = 0U;
  uint32_t previous_anchor = 0U;

  if ((chain_anchor == NULL)
      || (ut_context == NULL)
      || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;
  anchor_commit = ut_context->anchor_commit;
  anchor_context = ut_context->anchor_context;

  // Prefer to keep traversing the original live chain until it is exhausted.
  status = find_next_live_anchor(next_anchor,
                                 tail,
                                 klv_start_address,
                                 klv_end_address,
                                 &new_anchor);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if ((new_anchor == tail) && (relocated_head_addr != 0U)) {
    // No later original live object remains, so the relocated copy becomes the
    // durable hand-off point for the logical chain.
    new_anchor = relocated_head_addr;
  }

  if (new_anchor == *chain_anchor) {
    return SL_STATUS_OK;
  }

  previous_anchor = *chain_anchor;
  *chain_anchor = new_anchor;
  // Persist before callers erase old pages, otherwise a reset could leave the
  // recovered anchor pointing into reclaimed flash.
  status = anchor_commit(*chain_anchor, anchor_context);
  if (status != SL_STATUS_OK) {
    // Rollback the previous anchor if the commit fails.
    *chain_anchor = previous_anchor;
  }

  return status;
}

/***************************************************************************//**
 * Advances the persisted chain anchor past the current head when that head
 * cannot be recovered in place. Stops on the next readable live object, or
 * leaves the anchor unchanged when no later live object exists.
 ******************************************************************************/
static sl_status_t advance_anchor_past_unrecoverable_head(uint32_t head_next_addr,
                                                          uint32_t *chain_anchor,
                                                          const sli_klv_ut_context_t *ut_context)
{
  uint32_t tail = 0U;
  uint32_t next_anchor = 0U;
  uint32_t previous_anchor = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((chain_anchor == NULL) || (ut_context == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  status = scan_tail_from_anchor(*chain_anchor,
                                 ut_context->klv_start_address,
                                 ut_context->klv_end_address,
                                 &tail);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = find_next_live_anchor(head_next_addr,
                                 tail,
                                 ut_context->klv_start_address,
                                 ut_context->klv_end_address,
                                 &next_anchor);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (next_anchor == tail) {
    // No readable live object remains before the logical tail. Persist the
    // anchor on that writable location so recovery does not spin on the stale
    // head when only reclaimable or corrupted objects precede erased flash.
    previous_anchor = *chain_anchor;
    if (*chain_anchor != tail) {
      *chain_anchor = tail;
      status = ut_context->anchor_commit(*chain_anchor, ut_context->anchor_context);
      if (status != SL_STATUS_OK) {
        *chain_anchor = previous_anchor;
      }
    }

    return status;
  }

  return advance_chain_anchor_after_head(head_next_addr,
                                         tail,
                                         0U,
                                         chain_anchor,
                                         ut_context);
}

/***************************************************************************//**
 * Copies a byte range within token storage using a small RAM scratch buffer.
 *
 * Reclaim frequently relocates objects from one flash location to another. This
 * helper centralizes the chunked read/write loop so callers do not need to
 * duplicate the backend-safe copy pattern.
 ******************************************************************************/
static sl_status_t copy_flash_range(uint32_t dst_addr,
                                    uint32_t src_addr,
                                    uint32_t size,
                                    uint32_t source_object_offset,
                                    uint32_t klv_start_address,
                                    uint32_t klv_end_address)
{
  uint32_t scratch_words[32] = { 0 };
  uint32_t copied = 0U;
  sl_status_t status = SL_STATUS_OK;

  while (copied < size) {
    uint32_t chunk_size = size - copied;
    uint32_t current_src = advance_ut_adress(src_addr,
                                             copied,
                                             klv_start_address,
                                             klv_end_address);
    uint32_t current_dst = advance_ut_adress(dst_addr,
                                             copied,
                                             klv_start_address,
                                             klv_end_address);

    if (chunk_size > sizeof(scratch_words)) {
      chunk_size = sizeof(scratch_words);
    }

    // Read into RAM first so the backend copy works for flash-like media that
    // cannot write directly from another flash-mapped source address.
    status = logical_read(current_src,
                          (void *)scratch_words,
                          chunk_size,
                          klv_start_address,
                          klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    // The source is readable, but the relocated copy MUST initially be WIP.
    // A power cut halfway through a relocation can then never expose a
    // half-copied destination as a committed token. CRC16 excludes flags.
    {
      const uint32_t flag_offset = (uint32_t)offsetof(sli_klv_obj_header_t, flags);
      uint32_t begin_offset = source_object_offset + copied;
      if (begin_offset <= flag_offset
          && (flag_offset - begin_offset) < chunk_size) {
        ((uint8_t *)scratch_words)[flag_offset - begin_offset] |=
          SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK;
      }
    }
    status = logical_write(current_dst,
                           scratch_words,
                           chunk_size,
                           klv_start_address,
                           klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    copied += chunk_size;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Finalizes an object whose payload is already present by clearing only the
 * write-in-progress bit in the final header word.
 *
 * Reclaim recovery uses this when power failed after the destination copy was
 * written but before the header was committed readable.
 ******************************************************************************/
static sl_status_t commit_object_wip_clear(uint32_t obj_addr,
                                           uint32_t klv_start_address,
                                           uint32_t klv_end_address)
{
  sli_klv_obj_header_t obj_hdr = { 0 };
  uint32_t final_header_word = 0U;
  uint32_t final_header_word_addr = 0U;
  sl_status_t status = SL_STATUS_OK;

  // Read the object header through the logical path.
  status = logical_read(obj_addr,
                        &obj_hdr,
                        sizeof(obj_hdr),
                        klv_start_address,
                        klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Build the final header word from the RAM header.
  obj_hdr.flags = (uint8_t)(obj_hdr.flags & ~SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK);
  memcpy(&final_header_word,
         ((const uint8_t *)&obj_hdr) + offsetof(sli_klv_obj_header_t, crc_data),
         sizeof(final_header_word));
  final_header_word &= ~(uint32_t)(SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK
                                   << 16U);

  final_header_word_addr = advance_ut_adress(obj_addr,
                                             offsetof(sli_klv_obj_header_t, crc_data),
                                             klv_start_address,
                                             klv_end_address);

  // Only the last header word is rewritten so the payload and earlier header
  // fields remain untouched; this is the smallest "commit" change possible.
  return logical_write(final_header_word_addr,
                       &final_header_word,
                       sizeof(final_header_word),
                       klv_start_address,
                       klv_end_address);
}

/***************************************************************************//**
 * Commits a recovered WIP head object only when both header and payload CRCs
 * validate. Streaming start leaves crc_data at the erased placeholder until
 * finish(), so incomplete sessions remain unreadable after recovery.
 ******************************************************************************/
static sl_status_t try_commit_recovered_wip_object(uint32_t obj_addr,
                                                   uint32_t klv_start_address,
                                                   uint32_t klv_end_address,
                                                   const sli_klv_ut_obj_info_t *obj_info)
{
  uint16_t computed_crc = 0U;
  uint32_t payload_addr = 0U;
  sl_status_t status = SL_STATUS_OK;

  if (obj_info == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // A torn header has no payload and nothing to commit; its magic stays as is.
  if (obj_info->header.is_torn || (obj_info->header.payload_crc == 0xFFFFU)) {
    return SL_STATUS_INVALID_SIGNATURE;
  }

  payload_addr = advance_ut_adress(obj_addr,
                                   obj_info->header.header_size,
                                   klv_start_address,
                                   klv_end_address);
  status = calculate_logical_checksum(payload_addr,
                                      obj_info->header.payload_length,
                                      klv_start_address,
                                      klv_end_address,
                                      &computed_crc);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (computed_crc != obj_info->header.payload_crc) {
    return SL_STATUS_INVALID_SIGNATURE;
  }

  return commit_object_wip_clear(obj_addr,
                                 klv_start_address,
                                 klv_end_address);
}

/***************************************************************************//**
 * Writes and commits a UT object at @p obj_addr.
 ******************************************************************************/
static sl_status_t write_committed_ut_object(uint32_t obj_addr,
                                             const sli_klv_obj_header_t *obj_hdr,
                                             const void *data,
                                             uint32_t data_length,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address)
{
  sl_status_t status = SL_STATUS_OK;

  if ((obj_hdr == NULL) || (data == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  status = ut_tail_prepare_header(obj_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = logical_write(obj_addr,
                         obj_hdr,
                         sizeof(*obj_hdr),
                         klv_start_address,
                         klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = ut_tail_publish_header(obj_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = write_ut_payload(advance_ut_adress(obj_addr,
                                              SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                              klv_start_address,
                                              klv_end_address),
                            data,
                            data_length,
                            klv_start_address,
                            klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return commit_object_wip_clear(obj_addr,
                                 klv_start_address,
                                 klv_end_address);
}

/***************************************************************************//**
 * Marks a UT object as deleted in place.
 ******************************************************************************/
static sl_status_t mark_ut_object_deleted(uint32_t obj_addr,
                                          uint32_t klv_start_address,
                                          uint32_t klv_end_address)
{
  uint32_t deleted_magic = SLI_TOKEN_KLV_MAGIC_KLV_DELETED;

  return logical_write(obj_addr,
                       &deleted_magic,
                       sizeof(deleted_magic),
                       klv_start_address,
                       klv_end_address);
}

/***************************************************************************//**
 * Fully validates a UT object at @p obj_addr for use as a live anchor/reclaim
 * source.
 *
 * Unlike raw header decoding, this helper also enforces that the object still
 * carries active magic and that its payload CRC matches the stored bytes.
 ******************************************************************************/
static sl_status_t validate_anchor_object(uint32_t obj_addr,
                                          uint32_t klv_start_address,
                                          uint32_t klv_end_address,
                                          sli_klv_ut_obj_info_t *obj_info)
{
  uint32_t magic_word = 0U;
  uint16_t payload_crc = 0U;
  uint32_t payload_addr = 0U;
  sl_status_t status = SL_STATUS_OK;

  if (obj_info == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!is_address_in_range(obj_addr, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = decode_object_from_anchor(obj_addr,
                                     klv_start_address,
                                     klv_end_address,
                                     obj_info);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = logical_read(obj_addr,
                        &magic_word,
                        sizeof(magic_word),
                        klv_start_address,
                        klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (magic_word != SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE) {
    return SL_STATUS_INVALID_SIGNATURE;
  }

  // A readable header alone is not enough for reclaim decisions, verify the
  // payload bytes too so a half-copied relocation cannot become the new head.
  payload_addr = advance_ut_adress(obj_addr,
                                   obj_info->header.header_size,
                                   klv_start_address,
                                   klv_end_address);
  // Calculate the payload checksum.
  status = calculate_logical_checksum(payload_addr,
                                      obj_info->header.payload_length,
                                      klv_start_address,
                                      klv_end_address,
                                      &payload_crc);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (payload_crc != obj_info->header.payload_crc) {
    return SL_STATUS_INVALID_SIGNATURE;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Walks the live UT chain and retires committed objects whose payload CRC
 * cannot be validated. Write-in-progress objects are left untouched here;
 * only the head recovery pass may commit a fully valid WIP object.
 ******************************************************************************/
static sl_status_t retire_corrupted_objects_in_ut_chain(const sli_klv_ut_context_t *ut_context)
{
  uint32_t *chain_anchor = NULL;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  uint32_t current = 0U;
  uint32_t magic_word = 0U;
  uint32_t total_region_size = 0U;
  uint32_t max_steps = 0U;
  uint32_t step = 0U;
  sli_klv_ut_obj_info_t obj_info = { 0 };
  sl_status_t status = SL_STATUS_OK;
  bool terminated = false;

  if ((ut_context == NULL) || (ut_context->chain_anchor == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  chain_anchor = ut_context->chain_anchor;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;
  total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  max_steps = (total_region_size / sizeof(uint32_t)) + 1U;

  current = *chain_anchor;
  while (step < max_steps) {
    if (ut_tail_is_frontier(current, klv_start_address, klv_end_address)) {
      terminated = true;
      break;
    }
    status = decode_object_from_anchor(current,
                                       klv_start_address,
                                       klv_end_address,
                                       &obj_info);
    if (status == SL_STATUS_NOT_FOUND) {
      terminated = true;
      break;
    } else if (status != SL_STATUS_OK) {
      return status;
    }

    status = logical_read(current,
                          &magic_word,
                          sizeof(magic_word),
                          klv_start_address,
                          klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if (obj_info.header.is_readable) {
      status = validate_anchor_object(current,
                                      klv_start_address,
                                      klv_end_address,
                                      &obj_info);
      if (status != SL_STATUS_OK) {
        stream_ctx_invalidate_recovered_ut_key(obj_info.header.key);
        if (magic_word == SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE) {
          (void)mark_ut_object_deleted(current,
                                       klv_start_address,
                                       klv_end_address);
        }
      }
    } else if ((magic_word == SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE)
               && obj_info.header.is_write_in_progress
               && !obj_info.header.is_torn) {
      // A VALID WIP header is never a torn header. A power failure can land
      // between programming crc_data and clearing the WIP flag, including in
      // an earlier concurrent stream. Check the payload; commit a valid CRC,
      // or retire the incomplete WIP copy without corrupting the chain walk.
      status = try_commit_recovered_wip_object(current,
                                               klv_start_address,
                                               klv_end_address,
                                               &obj_info);
      if (status == SL_STATUS_INVALID_SIGNATURE) {
        status = mark_ut_object_deleted(current,
                                        klv_start_address,
                                        klv_end_address);
      }
      if (status != SL_STATUS_OK) {
        return status;
      }
    }

    current = obj_info.next_addr;
    step++;
    if (current == *chain_anchor) {
      terminated = true;
      break;
    }
  }

  if (!terminated) {
    return SL_STATUS_INVALID_STATE;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Soft-deletes earlier readable objects that are superseded by a later
 * readable object with the same key, so each key keeps exactly one live
 * generation: the newest one.
 *
 * Both write paths publish the replacement first and retire the previous
 * generation afterwards. Finish / one-shot commit the new object's final header
 * word, then mark the old object deleted. Reclaim copies a live object to its
 * new location, then soft-deletes the source. A reset between those two flash
 * operations leaves two readable copies of one key. Reads already resolve the
 * newest generation; deleting it, however, would expose the stale one again,
 * and the stale bytes stay in the live span and inflate used storage.
 *
 * Chain order from the persisted anchor is write order, so "later" is always
 * the newer generation. Payload contents are not compared: an interrupted
 * overwrite may legitimately differ in length and data.
 *
 * Must run after @ref retire_corrupted_objects_in_ut_chain, which has already
 * retired torn or WIP copies, and after the head-relocation loop in
 * @ref sli_klv_recover_ut_chain_anchor, which has already resolved an
 * incomplete head move. The anchor itself can therefore be retired here when
 * a newer generation follows it; the anchor is then advanced and persisted
 * past the soft-deleted head.
 ******************************************************************************/
static sl_status_t retire_superseded_objects_in_ut_chain(const sli_klv_ut_context_t *ut_context)
{
  uint32_t *chain_anchor = NULL;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  uint32_t current = 0U;
  uint32_t total_region_size = 0U;
  uint32_t max_steps = 0U;
  uint32_t step = 0U;
  sl_status_t status = SL_STATUS_OK;
  bool terminated = false;
  bool head_retired = false;

  if ((ut_context == NULL)
      || (ut_context->chain_anchor == NULL)
      || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  chain_anchor = ut_context->chain_anchor;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;
  total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  max_steps = (total_region_size / sizeof(uint32_t)) + 1U;

  current = *chain_anchor;
  while (step < max_steps) {
    if (ut_tail_is_frontier(current, klv_start_address, klv_end_address)) {
      terminated = true;
      break;
    }
    sli_klv_ut_obj_info_t current_info = { 0 };
    uint32_t candidate = 0U;
    uint32_t candidate_steps = 0U;
    bool superseded = false;

    status = decode_object_from_anchor(current,
                                       klv_start_address,
                                       klv_end_address,
                                       &current_info);
    if (status == SL_STATUS_NOT_FOUND) {
      terminated = true;
      break;
    } else if (status != SL_STATUS_OK) {
      return status;
    }

    if (current_info.header.is_readable) {
      candidate = current_info.next_addr;
      while ((candidate != *chain_anchor)
             && (candidate_steps < max_steps)
             && !superseded
             && !ut_tail_is_frontier(candidate, klv_start_address,
                                      klv_end_address)) {
        sli_klv_ut_obj_info_t candidate_info = { 0 };

        status = decode_object_from_anchor(candidate,
                                           klv_start_address,
                                           klv_end_address,
                                           &candidate_info);
        if (status == SL_STATUS_NOT_FOUND) {
          break;
        } else if (status != SL_STATUS_OK) {
          return status;
        }

        if (candidate_info.header.is_readable
            && (candidate_info.header.key == current_info.header.key)) {
          superseded = true;
        }

        candidate = candidate_info.next_addr;
        candidate_steps++;
      }

      if (superseded) {
        TOKENDBG(printf("UT recover retire superseded: key=0x%04x addr=0x%08lx head=%u\r\n",
                        current_info.header.key,
                        (unsigned long)current,
                        (current == *chain_anchor) ? 1U : 0U); )
        status = mark_ut_object_deleted(current,
                                        klv_start_address,
                                        klv_end_address);
        if (status != SL_STATUS_OK) {
          return status;
        }

        if (current == *chain_anchor) {
          head_retired = true;
        }
      }
    }

    current = current_info.next_addr;
    step++;
    if (current == *chain_anchor) {
      terminated = true;
      break;
    }
  }

  if (!terminated) {
    return SL_STATUS_INVALID_STATE;
  }

  if (head_retired) {
    // The persisted head is now soft-deleted. Move it to the next readable or
    // WIP object and persist, as a normal head delete would.
    status = skip_unreadable_chain_head(chain_anchor, ut_context);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Copies a live UT object to a new location and verifies the relocated copy.
 *
 * Reclaim uses this helper to guarantee that the destination object is both
 * decodable and payload-identical before the source page becomes eligible for
 * erase.
 ******************************************************************************/
static sl_status_t copy_validated_live_object(uint32_t dst_addr,
                                              const sli_klv_ut_obj_info_t *src_info,
                                              uint32_t object_size,
                                              uint32_t klv_start_address,
                                              uint32_t klv_end_address)
{
  sli_klv_ut_obj_info_t copied_info = { 0 };
  uint32_t src_payload_addr = 0U;
  uint32_t dst_payload_addr = 0U;
  sl_status_t status = SL_STATUS_OK;

  if (src_info == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = ut_tail_prepare_header(dst_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = copy_flash_range(dst_addr,
                            src_info->obj_addr,
                            object_size,
                            0U,
                            klv_start_address,
                            klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Re-decode and validate the destination copy instead of assuming the backend
  // write succeeded perfectly, reclaim safety depends on this verification step.
  status = decode_object_from_anchor(dst_addr, klv_start_address,
                                      klv_end_address, &copied_info);
  if (status != SL_STATUS_OK || !copied_info.header.is_write_in_progress) {
    return SL_STATUS_INVALID_SIGNATURE;
  }

  if ((copied_info.header.key != src_info->header.key)
      || (copied_info.header.payload_length != src_info->header.payload_length)) {
    return SL_STATUS_INVALID_STATE;
  }

  // Final payload compare protects against accidental corruption even when the
  // destination header itself looks structurally valid.
  src_payload_addr = advance_ut_adress(src_info->obj_addr,
                                       src_info->header.header_size,
                                       klv_start_address,
                                       klv_end_address);
  dst_payload_addr = advance_ut_adress(dst_addr,
                                       copied_info.header.header_size,
                                       klv_start_address,
                                       klv_end_address);
  if (!logical_ranges_equal(src_payload_addr,
                            dst_payload_addr,
                            src_info->header.payload_length,
                            klv_start_address,
                            klv_end_address)) {
    return SL_STATUS_INVALID_STATE;
  }

  status = ut_tail_publish_header(dst_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = commit_object_wip_clear(dst_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  return validate_anchor_object(dst_addr, klv_start_address, klv_end_address,
                                &copied_info);
}

/***************************************************************************//**
 * Relocates a live UT object when the reclaim destination must wrap.
 *
 * The first physical segment is copied before any additional erase occurs. Once
 * that source prefix is no longer needed, reclaim can erase newly safe prefix
 * pages and write the wrapped remainder at the start of the logical region.
 *
 * The ordering is load bearing: copy first half, verify first half, optionally
 * erase pages strictly behind the source object, copy second half, verify
 * second half. Mid-relocation erase is bounded at the source object start so
 * the current chain-head page cannot be erased before reclaim_chain commits a
 * new anchor. If the wrap continuation needs more free room than that bound
 * allows, the helper returns SL_STATUS_NO_MORE_RESOURCE rather than violating
 * the power-fail invariant.
 ******************************************************************************/
static sl_status_t copy_validated_object_with_reclaim_wrap(uint32_t dst_addr,
                                                           const sli_klv_ut_obj_info_t *src_info,
                                                           uint32_t object_size,
                                                           sli_klv_ut_reclaim_window_t *reclaim_window,
                                                           uint32_t klv_start_address,
                                                           uint32_t klv_end_address)
{
  sli_klv_ut_obj_info_t copied_info = { 0 };
  uint32_t first_chunk_size = 0U;
  uint32_t remaining_size = 0U;
  uint32_t remaining_src_addr = 0U;
  uint32_t simulated_erase_cursor = 0U;
  uint32_t initial_anchor_page = 0U;
  uint32_t wrap_limit = 0U;
  uint32_t wrap_window_size = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((src_info == NULL) || (reclaim_window == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if (compare_logical_address(dst_addr,
                              reclaim_window->initial_tail,
                              klv_start_address,
                              klv_end_address) < 0) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  // Wrapped relocation copies the suffix from the current write address to the
  // logical ring end first, then places the remainder at the logical start.
  first_chunk_size = cal_distance_forward(dst_addr,
                                          klv_start_address,
                                          klv_start_address,
                                          klv_end_address);
  if ((first_chunk_size == 0U) || (first_chunk_size >= object_size)) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  remaining_size = object_size - first_chunk_size;
  remaining_src_addr = advance_ut_adress(src_info->obj_addr,
                                         first_chunk_size,
                                         klv_start_address,
                                         klv_end_address);

  // Bound mid-relocation erase at the source object start (not remaining_src).
  // reclaim_erase_span() stops at page_start(progress), so using remaining_src
  // would erase page(obj_addr) whenever the already-copied prefix crosses a page
  // boundary. That object is the current chain head; the anchor is only advanced
  // after this helper returns. Erasing the head page first leaves a power-fail
  // window where decode_object_from_anchor(anchor) returns NOT_FOUND and recovery
  // treats the ring as empty. prepare_reclaim_write_address() already passes the
  // object start for the same reason.
  simulated_erase_cursor = predict_reclaim_erase_cursor(reclaim_window->erase_cursor,
                                                        src_info->obj_addr,
                                                        reclaim_window->erase_stop_addr,
                                                        reclaim_window->initial_tail,
                                                        klv_start_address,
                                                        klv_end_address);
  initial_anchor_page = SLI_KLV_ANCHOR_PAGE_START(reclaim_window->initial_anchor);
  // Same page-aligned bound as prepare_reclaim_write_address(): the live-head
  // page is not erasable while the head remains there.
  wrap_limit = (simulated_erase_cursor == initial_anchor_page)
               ? initial_anchor_page
               : simulated_erase_cursor;
  wrap_window_size = cal_distance_forward(klv_start_address,
                                          wrap_limit,
                                          klv_start_address,
                                          klv_end_address);
  if (remaining_size > wrap_window_size) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  // Both halves are programmed below; make sure every destination page is
  // erased before the first byte goes out.
  status = prepare_reclaim_destination(dst_addr,
                                       object_size,
                                       klv_start_address,
                                       klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = ut_tail_prepare_header(dst_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = copy_flash_range(dst_addr,
                            src_info->obj_addr,
                            first_chunk_size,
                            0U,
                            klv_start_address,
                            klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Verify the first half here, not at the end of the function: if a later
  // failure aborts before the second half is written, the chain must still be
  // intact. Source pages at/after obj_addr are intentionally not erased below
  // (see progress bound), so this compare is not racing an erase of those bytes.
  {
    uint32_t flag_offset = (uint32_t)offsetof(sli_klv_obj_header_t, flags);
    uint32_t prefix = (first_chunk_size < flag_offset) ? first_chunk_size : flag_offset;
    if (!logical_ranges_equal(src_info->obj_addr, dst_addr, prefix,
                              klv_start_address, klv_end_address)) {
      return SL_STATUS_INVALID_STATE;
    }
    if (first_chunk_size > flag_offset + 1U) {
      uint32_t beyond_flag = first_chunk_size - flag_offset - 1U;
      if (!logical_ranges_equal(advance_ut_adress(src_info->obj_addr, flag_offset + 1U,
                                                   klv_start_address, klv_end_address),
                                advance_ut_adress(dst_addr, flag_offset + 1U,
                                                   klv_start_address, klv_end_address),
                                beyond_flag, klv_start_address, klv_end_address)) {
        return SL_STATUS_INVALID_STATE;
      }
    }
  }

  // Same bounds as the prediction above. Progress is the source object start so
  // this sweep cannot erase the current head page before reclaim_chain commits a
  // new anchor. erase_stop_addr still protects an overwrite target; initial_tail
  // keeps the cursor out of already-relocated objects.
  status = erase_reclaimed_pages(&reclaim_window->erase_cursor,
                                 src_info->obj_addr,
                                 reclaim_window->erase_stop_addr,
                                 reclaim_window->initial_tail,
                                 klv_start_address,
                                 klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = copy_flash_range(klv_start_address,
                            remaining_src_addr,
                            remaining_size,
                            first_chunk_size,
                            klv_start_address,
                            klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = decode_object_from_anchor(dst_addr, klv_start_address,
                                      klv_end_address, &copied_info);
  if (status != SL_STATUS_OK || !copied_info.header.is_write_in_progress) {
    return SL_STATUS_INVALID_SIGNATURE;
  }

  if ((copied_info.header.key != src_info->header.key)
      || (copied_info.header.payload_length != src_info->header.payload_length)) {
    return SL_STATUS_INVALID_STATE;
  }

  // Second half of the byte-for-byte check. Its source pages sit at or after
  // remaining_src_addr and were not erased above (progress was the object
  // start), so they are still readable. Together with the pre-erase compare
  // this covers the whole object.
  if (!logical_ranges_equal(remaining_src_addr,
                            klv_start_address,
                            remaining_size,
                            klv_start_address,
                            klv_end_address)) {
    return SL_STATUS_INVALID_STATE;
  }

  status = ut_tail_publish_header(dst_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = commit_object_wip_clear(dst_addr, klv_start_address, klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }
  return validate_anchor_object(dst_addr, klv_start_address, klv_end_address,
                                &copied_info);
}

/***************************************************************************//**
 * Finds a validated relocated copy of the head object in the active chain.
 *
 * Recovery uses this to detect the specific power-fail window where reclaim had
 * already written a relocated head candidate but had not yet fully advanced the
 * persisted anchor. The scan excludes @p head_addr and @p tail: the former is
 * the authoritative source and the latter is the erased append position after
 * the relocated copy.
 ******************************************************************************/
static sl_status_t find_matching_relocation(uint32_t head_addr,
                                            const sli_klv_ut_obj_info_t *head_info,
                                            uint32_t tail,
                                            uint32_t klv_start_address,
                                            uint32_t klv_end_address,
                                            uint32_t *match_addr)
{
  uint32_t current = 0U;
  uint32_t head_payload_addr = 0U;
  uint32_t total_region_size = 0U;
  uint32_t max_steps = 0U;
  uint32_t step = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((head_info == NULL) || (match_addr == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!is_address_in_range(head_addr, klv_start_address, klv_end_address)
      || !is_address_in_range(tail, klv_start_address, klv_end_address)
      || (tail == head_addr)) {
    return SL_STATUS_NOT_FOUND;
  }

  head_payload_addr = advance_ut_adress(head_addr,
                                        head_info->header.header_size,
                                        klv_start_address,
                                        klv_end_address);
  total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  max_steps = (total_region_size / sizeof(uint32_t)) + 1U;
  current = head_info->next_addr;

  while ((current != tail) && (step < max_steps)) {
    sli_klv_ut_obj_info_t candidate_info = { 0 };
    uint32_t candidate_next = 0U;

    status = decode_object_from_anchor(current,
                                       klv_start_address,
                                       klv_end_address,
                                       &candidate_info);
    if (status == SL_STATUS_NOT_FOUND) {
      return SL_STATUS_NOT_FOUND;
    } else if (status != SL_STATUS_OK) {
      return status;
    }
    candidate_next = candidate_info.next_addr;

    if (candidate_info.header.is_readable
        && (candidate_info.header.key == head_info->header.key)
        && (candidate_info.header.payload_length == head_info->header.payload_length)) {
      uint32_t candidate_payload_addr = 0U;

      status = validate_anchor_object(current,
                                      klv_start_address,
                                      klv_end_address,
                                      &candidate_info);
      if (status == SL_STATUS_OK) {
        candidate_payload_addr = advance_ut_adress(current,
                                                   candidate_info.header.header_size,
                                                   klv_start_address,
                                                   klv_end_address);
        if (logical_ranges_equal(head_payload_addr,
                                 candidate_payload_addr,
                                 head_info->header.payload_length,
                                 klv_start_address,
                                 klv_end_address)) {
          *match_addr = current;
          return SL_STATUS_OK;
        }
      }
    }

    current = candidate_next;
    step++;
  }

  return (current == tail) ? SL_STATUS_NOT_FOUND : SL_STATUS_INVALID_STATE;
}

/***************************************************************************//**
 * Returns the current tail of the UT ring by scanning from the logical anchor.
 *
 * This is a thin convenience wrapper around @ref scan_from_anchor used
 * by recovery code that needs only the tail location, not the full scan result.
 ******************************************************************************/
static sl_status_t scan_tail_from_anchor(uint32_t chain_anchor,
                                         uint32_t klv_start_address,
                                         uint32_t klv_end_address,
                                         uint32_t *tail)
{
  sli_klv_ut_scan_result_t scan_result = { 0 };
  sl_status_t status = SL_STATUS_OK;

  if (tail == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = scan_from_anchor(SLI_KLV_UT_FULL_CHAIN_SCAN_TOKEN,
                            chain_anchor,
                            klv_start_address,
                            klv_end_address,
                            &scan_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  *tail = scan_result.tail;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns how many bytes of whole pages an erase sweep may consume, starting at
 * @p erase_cursor and stopping at the nearest of the three bounds.
 *
 * Shared by @ref erase_reclaimed_pages() and @ref predict_reclaim_erase_cursor()
 * so the two can never disagree about where a sweep ends. See
 * @ref erase_reclaimed_pages() for what each bound protects.
 *
 * @param[in] erase_cursor          Page the sweep would start at.
 * @param[in] reclaim_progress_addr Object currently being reclaimed; its page and
 *                                  anything beyond must survive this sweep.
 * @param[in] write_base            Soft boundary the current reclaim phase needs
 *                                  kept readable.
 * @param[in] relocation_floor      Hard boundary; relocated objects start here.
 * @param[in] klv_start_address     Start of the UT region.
 * @param[in] klv_end_address       End of the UT region.
 *
 * @return Byte count, always a whole number of pages, possibly zero.
 ******************************************************************************/
static uint32_t reclaim_erase_span(uint32_t erase_cursor,
                                   uint32_t reclaim_progress_addr,
                                   uint32_t write_base,
                                   uint32_t relocation_floor,
                                   uint32_t klv_start_address,
                                   uint32_t klv_end_address)
{
  uint32_t span = 0U;
  uint32_t to_write_base = 0U;
  uint32_t to_floor = 0U;

  // Distances are measured forward from the cursor, so "nearest bound" is a
  // well-defined question on the ring even when the three addresses sit in
  // different physical regions.
  span = cal_distance_forward(erase_cursor,
                              SLI_KLV_ANCHOR_PAGE_START(reclaim_progress_addr),
                              klv_start_address,
                              klv_end_address);
  to_write_base = cal_distance_forward(erase_cursor,
                                       SLI_KLV_ANCHOR_PAGE_START(write_base),
                                       klv_start_address,
                                       klv_end_address);
  to_floor = cal_distance_forward(erase_cursor,
                                  SLI_KLV_ANCHOR_PAGE_START(relocation_floor),
                                  klv_start_address,
                                  klv_end_address);

  if (to_write_base < span) {
    span = to_write_base;
  }

  if (to_floor < span) {
    span = to_floor;
  }

  // cal_distance_forward() yields 0 for an address it cannot map to a logical
  // offset, which degrades to "erase nothing" rather than to a wild sweep.
  return span - (span % FLASH_PAGE_SIZE);
}

/***************************************************************************//**
 * Erases the pages reclaim has finished with, stopping at the first boundary it
 * must not cross.
 *
 * Three addresses bound the sweep and the nearest one travelling forward wins:
 *
 * - @p reclaim_progress_addr is the object reclaim is working on right now. Its
 *   page has to survive until the relocated copy is verified, so the sweep stops
 *   at the page below it.
 * - @p write_base is the soft boundary the current phase wants kept readable. It
 *   normally marks the relocated block, and moves to the overwrite target while
 *   that object still has to be marked deleted.
 * - @p relocation_floor is the hard boundary: where relocated objects start. No
 *   sweep may enter that span for the lifetime of the reclaim, whatever the
 *   other two arguments say.
 *
 * The cursor walks a ring, so comparing it to the bounds with `!=` only
 * terminates where intended while the cursor still trails all of them. Handing
 * over a boundary the cursor has already passed would otherwise let the sweep
 * continue to the next one and erase relocated objects on the way. Deciding the
 * span up front removes that failure mode: the page count is fixed before the
 * first erase, and @p relocation_floor still caps it when the other two
 * boundaries are wrong.
 ******************************************************************************/
static sl_status_t erase_reclaimed_pages(uint32_t *erase_cursor,
                                         uint32_t reclaim_progress_addr,
                                         uint32_t write_base,
                                         uint32_t relocation_floor,
                                         uint32_t klv_start_address,
                                         uint32_t klv_end_address)
{
  uint32_t cursor = 0U;
  uint32_t end = 0U;
  uint32_t steps = 0U;
  uint32_t max_steps = 0U;
  sl_status_t status = SL_STATUS_OK;

  if (erase_cursor == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  cursor = *erase_cursor;
  end = predict_reclaim_erase_cursor(cursor,
                                    reclaim_progress_addr,
                                    write_base,
                                    relocation_floor,
                                    klv_start_address,
                                    klv_end_address);
  if (cursor != SLI_KLV_ANCHOR_PAGE_START(cursor)) {
    uint32_t aligned = erase_page_start_at_or_after(cursor,
                                                    klv_start_address,
                                                    klv_end_address);
    uint32_t skip = cal_distance_forward(cursor,
                                         aligned,
                                         klv_start_address,
                                         klv_end_address);

    if ((skip == 0U) || (skip > FLASH_PAGE_SIZE)) {
      return SL_STATUS_OK;
    }

    cursor = aligned;
  }

  max_steps = (get_total_region_size(klv_start_address, klv_end_address)
               / FLASH_PAGE_SIZE) + 1U;
  while ((cursor != end) && (steps < max_steps)) {
    // Reclaim can revisit a page already erased during the relocation walk.
    // Skipping a verified blank page avoids an unnecessary physical P/E cycle
    // without changing the erase cursor or which live pages remain protected.
    if (!is_logical_range_blank(cursor, FLASH_PAGE_SIZE,
                                klv_start_address, klv_end_address)) {
      status = sli_token_manager_nvm_page_erase(cursor);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }

    cursor = advance_ut_adress(cursor,
                               FLASH_PAGE_SIZE,
                               klv_start_address,
                               klv_end_address);
    steps++;
  }

  if (cursor != end) {
    return SL_STATUS_INVALID_STATE;
  }

  *erase_cursor = end;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Predicts how far the reclaim erase cursor can advance.
 *
 * This mirrors @ref erase_reclaimed_pages() without touching flash so reclaim
 * can decide whether a wrapped relocation tail becomes writable after the
 * already-copied source prefix is no longer needed. The sweep starts on a
 * page boundary and stops before any page that still holds the live chain.
 ******************************************************************************/
static uint32_t predict_reclaim_erase_cursor(uint32_t erase_cursor,
                                             uint32_t reclaim_progress_addr,
                                             uint32_t write_base,
                                             uint32_t relocation_floor,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address)
{
  uint32_t cursor = erase_cursor;
  uint32_t page = SLI_KLV_ANCHOR_PAGE_START(cursor);
  uint32_t erase_span = 0U;
  uint32_t stepped = 0U;

  if (cursor != page) {
    uint32_t aligned = erase_page_start_at_or_after(cursor,
                                                    klv_start_address,
                                                    klv_end_address);
    uint32_t skip = cal_distance_forward(cursor,
                                         aligned,
                                         klv_start_address,
                                         klv_end_address);

    if ((skip == 0U) || (skip > FLASH_PAGE_SIZE)) {
      return erase_cursor;
    }

    cursor = aligned;
  }

  erase_span = reclaim_erase_span(cursor,
                                  reclaim_progress_addr,
                                  write_base,
                                  relocation_floor,
                                  klv_start_address,
                                  klv_end_address);
  while (stepped < erase_span) {
    page = SLI_KLV_ANCHOR_PAGE_START(cursor);
    if (page != cursor) {
      break;
    }

    if (active_ut_context_valid
        && (active_ut_context.chain_anchor != NULL)
        && page_holds_chain_bytes(page,
                                  *active_ut_context.chain_anchor,
                                  klv_start_address,
                                  klv_end_address)) {
      break;
    }

    cursor = advance_ut_adress(cursor,
                               FLASH_PAGE_SIZE,
                               klv_start_address,
                               klv_end_address);
    stepped += FLASH_PAGE_SIZE;
  }

  return cursor;
}

/***************************************************************************//**
 * Erases pages before the anchor.
 *
 * @param[in] chain_anchor          Anchor address.
 * @param[in] klv_start_address     Start of the UT region.
 * @param[in] klv_end_address       End of the UT region.
 *
 * @return SL_STATUS_OK when the pages are erased, otherwise an error.
 ******************************************************************************/
static sl_status_t erase_prefix_pages_before_anchor(uint32_t chain_anchor,
                                                    uint32_t klv_start_address,
                                                    uint32_t klv_end_address)
{
  uint32_t erase_cursor = klv_start_address;
  uint32_t erase_limit = SLI_KLV_ANCHOR_PAGE_START(chain_anchor);
  sl_status_t status = SL_STATUS_OK;

  while (erase_cursor != erase_limit) {
    // The prefix may already be erased by an earlier reclaim phase. Avoid
    // wearing flash with another P/E cycle on the same blank page.
    if (!is_logical_range_blank(erase_cursor, FLASH_PAGE_SIZE,
                                klv_start_address, klv_end_address)) {
      status = sli_token_manager_nvm_page_erase(erase_cursor);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }

    erase_cursor = advance_ut_adress(erase_cursor,
                                     FLASH_PAGE_SIZE,
                                     klv_start_address,
                                     klv_end_address);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns whether a candidate append/relocate footprint would enter the live head.
 *
 * Used by the head-overwrite append shortcut to compare a post-write span against
 * the free gap ending at @p gap_end. Not used as a general write-admission check
 * in @ref prepare_reclaim_write_address.
 ******************************************************************************/
static bool reclaim_destination_overlaps_live_head(uint32_t write_addr,
                                                   uint32_t required_size,
                                                   uint32_t gap_end,
                                                   uint32_t klv_start_address,
                                                   uint32_t klv_end_address)
{
  uint32_t gap_room = 0U;
  int32_t write_vs_gap = 0;

  if ((required_size == 0U)
      || (gap_end == 0U)
      || !is_address_in_range(write_addr, klv_start_address, klv_end_address)
      || !is_address_in_range(gap_end, klv_start_address, klv_end_address)) {
    return false;
  }

  write_vs_gap = compare_logical_address(write_addr,
                                         gap_end,
                                         klv_start_address,
                                         klv_end_address);

  // Destination starts at the live head.
  if (write_vs_gap == 0) {
    return true;
  }

  // The ring is circular: a non-wrapped reclaim writes at the tail, logically
  // after the head, but the room it has is the free suffix plus the prefix up
  // to @p gap_end once it wraps. Measure forward around the ring in both
  // geometries; a footprint longer than that reaches the live head.
  gap_room = cal_distance_forward(write_addr,
                                  gap_end,
                                  klv_start_address,
                                  klv_end_address);
  return required_size > gap_room;
}

/***************************************************************************//**
 * Returns whether appending @p object_size bytes at @p write_addr leaves at
 * least @p reserve_size of free gap in front of the committed head.
 *
 * The gap between the tail and the head is the only place a later reclaim can
 * relocate the head into, and the head's page offset can cost up to a page of
 * it. The repack reserve is sized to absorb that, so every durable state has
 * to keep a full reserve in the gap, a reclaim that consumes it to finish one
 * write leaves a ring that no later write can unlock. A frontier equal to the
 * head means the chain is empty and the whole ring is the gap.
 ******************************************************************************/
static bool reclaim_final_write_keeps_reserve(uint32_t write_addr,
                                              uint32_t object_size,
                                              uint32_t chain_anchor,
                                              uint32_t reserve_size,
                                              uint32_t klv_start_address,
                                              uint32_t klv_end_address)
{
  uint32_t gap = 0U;

  if (object_size == 0U) {
    return true;
  }

  if (!is_address_in_range(write_addr, klv_start_address, klv_end_address)
      || !is_address_in_range(chain_anchor, klv_start_address, klv_end_address)) {
    return false;
  }

  gap = (write_addr == chain_anchor)
        ? get_total_region_size(klv_start_address, klv_end_address)
        : cal_distance_forward(write_addr,
                               chain_anchor,
                               klv_start_address,
                               klv_end_address);

  if ((object_size + reserve_size) > gap) {
    TOKENDBG(printf("UT reclaim final write refused to keep reserve: write=0x%08lx anchor=0x%08lx gap=%lu object=%lu reserve=%lu\r\n",
                    (unsigned long)write_addr,
                    (unsigned long)chain_anchor,
                    (unsigned long)gap,
                    (unsigned long)object_size,
                    (unsigned long)reserve_size); )
    return false;
  }

  return true;
}

/***************************************************************************//**
 * Ensures @p write_addr points at an erased window large enough for the next
 * reclaim relocation or final appended object.
 *
 * Reclaim writes into a reserved "repack window" while older pages are erased
 * behind the moving reclaim cursor. This helper:
 * 1. Accepts the existing write address if it still falls inside the reserved
 *    repack window.
 * 2. Advances page erasure up to the current reclaim progress while preserving
 *    any still-authoritative overwrite target page tracked in the reclaim
 *    window.
 * 3. Validates whether the current segment or a wrapped segment at the start of
 *    the ring has enough erased space.
 * 4. Allows the object itself to continue across the logical ring end when the
 *    erased wrap window at the start is large enough.
 *
 * @param[in,out] write_addr            Candidate relocation/write address.
 * @param[in]     required_size         Number of bytes the next object needs.
 * @param[in]     reclaim_progress_addr Address of the object currently being
 *                                      reclaimed; pages at or beyond this point
 *                                      must not be erased yet.
 * @param[in,out] reclaim_window        Running reclaim state shared across the
 *                                      full reclaim operation.
 * @param[in]     klv_start_address     Start of the UT region.
 * @param[in]     klv_end_address       End of the UT region.
 *
 * @return SL_STATUS_OK when a valid erased write window is available; otherwise
 *         SL_STATUS_NO_MORE_RESOURCE or a backend erase/write error.
 ******************************************************************************/
static sl_status_t prepare_reclaim_write_address(uint32_t *write_addr,
                                                 uint32_t required_size,
                                                 uint32_t reclaim_progress_addr,
                                                 sli_klv_ut_reclaim_window_t *reclaim_window,
                                                 uint32_t klv_start_address,
                                                 uint32_t klv_end_address)

{
  uint32_t write_limit = 0U;
  uint32_t wrap_limit = 0U;
  uint32_t initial_anchor_page = 0U;
  uint32_t repack_used_size = 0U;
  uint32_t available_size = 0U;
  sl_status_t status = SL_STATUS_OK;
  bool using_end_segment = false;

  if ((write_addr == NULL) || (reclaim_window == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  // Calculate the initial anchor page.
  initial_anchor_page = SLI_KLV_ANCHOR_PAGE_START(reclaim_window->initial_anchor);
  // The reserve window follows the chain anchor, so measure how much of it the
  // write cursor has already consumed instead of relying on raw addresses.
  repack_used_size = cal_distance_forward(reclaim_window->repack_start,
                                          *write_addr,
                                          klv_start_address,
                                          klv_end_address);
  if (repack_used_size < reclaim_window->repack_reserve_size) {
    uint32_t reserve_room = reclaim_window->repack_reserve_size - repack_used_size;

    // A gap window ends at the live anchor. Once the frontier has entered the
    // reserve, the free tail of that window can be shorter than the full
    // reserve, so the object also has to fit before the anchor.
    if (reclaim_window->repack_uses_gap_window
        && (compare_logical_address(*write_addr,
                                    reclaim_window->reclaim_gap_end,
                                    klv_start_address,
                                    klv_end_address) < 0)) {
      uint32_t anchor_room = cal_distance_forward(*write_addr,
                                                  reclaim_window->reclaim_gap_end,
                                                  klv_start_address,
                                                  klv_end_address);

      if (anchor_room < reserve_room) {
        reserve_room = anchor_room;
      }
    }

    if (required_size <= reserve_room) {
      bool reserve_already_erased = true;

      if (reclaim_window->repack_uses_gap_window) {
        uint32_t object_end = advance_ut_adress(*write_addr,
                                               required_size,
                                               klv_start_address,
                                               klv_end_address);

        // The window slides with the anchor. Pages inside it stay unerased
        // until erase_cursor passes them, and erase itself starts on a page
        // boundary. A wrapped object, or one that still reaches the cursor,
        // has to take the erase path below.
        reserve_already_erased =
          (compare_logical_address(*write_addr,
                                   object_end,
                                   klv_start_address,
                                   klv_end_address) < 0)
          && (compare_logical_address(*write_addr,
                                      reclaim_window->erase_cursor,
                                      klv_start_address,
                                      klv_end_address) < 0)
          && (compare_logical_address(object_end,
                                      reclaim_window->erase_cursor,
                                      klv_start_address,
                                      klv_end_address) <= 0);
      }

      if (reserve_already_erased) {
        return prepare_reclaim_destination(*write_addr,
                                           required_size,
                                           klv_start_address,
                                           klv_end_address);
      }
    }
  }

  // Reclaim may only erase pages that are strictly behind the object currently
  // being processed, this preserves the source bytes until the relocation copy
  // has succeeded and the anchor can advance. The initial tail is passed as the
  // hard floor so the sweep can never reach into the objects relocated so far.
  status = erase_reclaimed_pages(&reclaim_window->erase_cursor,
                                 reclaim_progress_addr,
                                 reclaim_window->erase_stop_addr,
                                 reclaim_window->initial_tail,
                                 klv_start_address,
                                 klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // In a non-wrapped initial layout, addresses from the initial tail forward to
  // the logical ring end form the free suffix. That suffix may cross multiple
  // physically unrelated regions, so use logical ordering rather than raw
  // address comparisons.
  using_end_segment = (!reclaim_window->initial_chain_wrapped
                       && (compare_logical_address(*write_addr,
                                                   reclaim_window->initial_tail,
                                                   klv_start_address,
                                                   klv_end_address) >= 0));
  if (using_end_segment) {
    available_size = cal_distance_forward(*write_addr,
                                          klv_start_address,
                                          klv_start_address,
                                          klv_end_address);
  } else {
    // Gap writes stop at the erase cursor. When the cursor sits on the live-head
    // page, that page cannot be erased, so the usable limit is the page start —
    // not the mid-page head address (see prepare_append_destination()).
    write_limit = (reclaim_window->erase_cursor == initial_anchor_page)
                  ? initial_anchor_page
                  : reclaim_window->erase_cursor;
    // Check if the write address is before the write limit.
    if (compare_logical_address(*write_addr,
                                write_limit,
                                klv_start_address,
                                klv_end_address) < 0) {
      // Calculate the available size.
      available_size = cal_distance_forward(*write_addr,
                                            write_limit,
                                            klv_start_address,
                                            klv_end_address);
    }

    // The bytes before the head on its own page are deliberately not offered
    // here even when they read as erased: a copy that lands there strands the
    // tail mid-page next to dead bytes once the head moves on, and neither
    // appends nor the next reclaim can clear them (see prepare_append_destination()).
  }

  if (required_size <= available_size) {
    return prepare_reclaim_destination(*write_addr,
                                       required_size,
                                       klv_start_address,
                                       klv_end_address);
  }

  if (!using_end_segment) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  // Calculate the wrap limit (same page-aligned head-page bound as write_limit).
  wrap_limit = (reclaim_window->erase_cursor == initial_anchor_page)
               ? initial_anchor_page
               : reclaim_window->erase_cursor;

  // Allow the object itself to span the logical ring end as long as its
  // continuation still lands inside the erased wrap window.
  {
    uint32_t future_tail = advance_ut_adress(*write_addr,
                                             required_size,
                                             klv_start_address,
                                             klv_end_address);

    if ((compare_logical_address(future_tail,
                                 wrap_limit,
                                 klv_start_address,
                                 klv_end_address) <= 0)
        && is_address_in_range(future_tail,
                               klv_start_address,
                               klv_end_address)) {
      return prepare_reclaim_destination(*write_addr,
                                         required_size,
                                         klv_start_address,
                                         klv_end_address);
    }
  }

  return SL_STATUS_NO_MORE_RESOURCE;
}

/***************************************************************************//**
 * Refreshes reclaim-window boundaries that depend on the current chain anchor.
 *
 * The erase cursor is intentionally not recomputed here. It represents reclaim
 * progress through flash pages and must only move forward as pages become safe
 * to erase. The anchor-dependent logical limits can move when reclaim persists
 * a new head, so refresh them before subsequent space checks.
 *
 * The reserve is retargeted at @p chain_anchor. It is the moving window of
 * @c repack_reserve_size bytes that ends at the new head, not a region pinned
 * to the write frontier or to the erase cursor. @p relocation_frontier only
 * keeps that window from covering objects this reclaim has already written.
 *
 * The refresh is deliberately non-fatal. The new anchor is already durable in
 * NVM3 when this runs, so refusing to refresh must not abort the reclaim and
 * leave the persisted head inconsistent with the in-progress write. An
 * unmappable anchor or frontier keeps the previous window untouched, which
 * leaves @ref prepare_reclaim_write_address to fall back to its regular
 * erase-limit and wrap checks.
 *
 * @param[in,out] reclaim_window       Running reclaim state to refresh.
 * @param[in]     chain_anchor         Newly committed chain anchor.
 * @param[in]     relocation_frontier  Next free address reclaim will write to.
 * @param[in]     klv_start_address    Start of the UT region.
 * @param[in]     klv_end_address      End of the UT region.
 *
 * @return SL_STATUS_OK, or SL_STATUS_NULL_POINTER on a NULL window.
 ******************************************************************************/
static sl_status_t refresh_reclaim_window(sli_klv_ut_reclaim_window_t *reclaim_window,
                                          uint32_t chain_anchor,
                                          uint32_t relocation_frontier,
                                          uint32_t klv_start_address,
                                          uint32_t klv_end_address)
{
  sli_klv_ut_reclaim_window_t refreshed;

  if (reclaim_window == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  refreshed = *reclaim_window;
  // Repack and gap geometry follow the newly committed head. This is not the
  // walk-start head: reservation.initial_anchor preserves that for the post-
  // commit erase rewind in reclaim_final_erase_cursor().
  refreshed.initial_anchor = chain_anchor;
  refreshed.reclaim_gap_start = relocation_frontier;
  refreshed.reclaim_gap_end = chain_anchor;
  // Classify the remaining free room from where reclaim actually writes next: a
  // frontier still logically below the anchor means reclaim is filling a gap
  // that ends at the anchor, otherwise it is filling the suffix.
  refreshed.initial_chain_wrapped =
    (compare_logical_address(relocation_frontier,
                             chain_anchor,
                             klv_start_address,
                             klv_end_address) < 0);

  if (!try_compute_frontier_repack_start(relocation_frontier,
                                         chain_anchor,
                                         refreshed.repack_reserve_size,
                                         klv_start_address,
                                         klv_end_address,
                                         &refreshed.repack_start)) {
    return SL_STATUS_OK;
  }

  // Gap-window checks apply only while the free room still ends at the anchor.
  // A suffix fill uses the anchor-page window and the ordinary reserve fast path.
  refreshed.repack_uses_gap_window = refreshed.initial_chain_wrapped;
  *reclaim_window = refreshed;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Scans the live span of the UT ring and returns the largest readable object.
 *
 * Reclaim reserves enough page-aligned scratch space to relocate the largest
 * live object. This helper provides that sizing input from the current logical
 * chain contents.
 ******************************************************************************/
static sl_status_t scan_largest_live_object_size(uint32_t chain_anchor,
                                                 uint32_t tail,
                                                 uint32_t klv_start_address,
                                                 uint32_t klv_end_address)
{
  sl_status_t status = SL_STATUS_OK;
  uint32_t current = chain_anchor;
  uint32_t largest = 0U;

  while (current != tail) {
    sli_klv_ut_obj_info_t current_info = { 0 };
    uint32_t object_size = 0U;

    status = decode_object_from_anchor(current,
                                       klv_start_address,
                                       klv_end_address,
                                       &current_info);
    if (status == SL_STATUS_NOT_FOUND) {
      // Erased slot or page-base torn header before the expected tail: the
      // live span ends here.
      break;
    } else if (status != SL_STATUS_OK) {
      return status;
    }

    if (current_info.header.is_readable) {
      // Only live objects drive the reserve requirement; deleted/unreadable
      // entries will never need to be relocated during a future reclaim.
      object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(current_info.header.header_size,
                                                        current_info.header.payload_length);
      if (object_size > largest) {
        largest = object_size;
      }
    }

    current = current_info.next_addr;
    if (current == chain_anchor) {
      break;
    }
  }

  largest_live_object_size = largest;
  largest_live_object_cache_valid = true;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Updates the cached largest live-object size after a successful commit.
 ******************************************************************************/
static void ut_largest_live_object_cache_bump(uint32_t object_size)
{
  if (object_size > largest_live_object_size) {
    largest_live_object_size = object_size;
  }

  largest_live_object_cache_valid = true;
}

/***************************************************************************//**
 * Rebuilds the cached largest live-object size from the full UT chain.
 ******************************************************************************/
static sl_status_t ut_refresh_largest_live_object_cache(uint32_t chain_anchor,
                                                        uint32_t klv_start_address,
                                                        uint32_t klv_end_address)
{
  uint32_t tail = 0U;
  sl_status_t status = SL_STATUS_OK;

  if (!is_address_in_range(chain_anchor, klv_start_address, klv_end_address)) {
    sli_klv_ut_largest_live_object_cache_invalidate();
    return SL_STATUS_OK;
  }

  status = scan_tail_from_anchor(chain_anchor,
                                 klv_start_address,
                                 klv_end_address,
                                 &tail);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return scan_largest_live_object_size(chain_anchor,
                                       tail,
                                       klv_start_address,
                                       klv_end_address);
}

/***************************************************************************//**
 * Ensures the cache is populated, performing one full-chain scan when needed.
 ******************************************************************************/
static sl_status_t ut_largest_live_object_cache_ensure_valid(uint32_t chain_anchor,
                                                             uint32_t klv_start_address,
                                                             uint32_t klv_end_address)
{
  if (largest_live_object_cache_valid) {
    return SL_STATUS_OK;
  }

  return ut_refresh_largest_live_object_cache(chain_anchor,
                                              klv_start_address,
                                              klv_end_address);
}

/***************************************************************************//**
 * Updates the cache after a live object is removed from the chain.
 ******************************************************************************/
static sl_status_t ut_largest_live_object_cache_on_delete(uint32_t chain_anchor,
                                                          uint32_t klv_start_address,
                                                          uint32_t klv_end_address,
                                                          uint32_t deleted_object_size)
{
  if (largest_live_object_cache_valid
      && (deleted_object_size < largest_live_object_size)) {
    return SL_STATUS_OK;
  }

  return ut_refresh_largest_live_object_cache(chain_anchor,
                                              klv_start_address,
                                              klv_end_address);
}

/***************************************************************************//**
 * Updates the cache after a committed write or overwrite.
 ******************************************************************************/
static sl_status_t ut_largest_live_object_cache_on_write_commit(uint32_t chain_anchor,
                                                                uint32_t klv_start_address,
                                                                uint32_t klv_end_address,
                                                                uint32_t new_object_size,
                                                                uint32_t replaced_object_size,
                                                                bool replaced_was_live)
{
  sl_status_t status = SL_STATUS_OK;

  if (!largest_live_object_cache_valid) {
    return ut_refresh_largest_live_object_cache(chain_anchor,
                                                klv_start_address,
                                                klv_end_address);
  }

  if (replaced_was_live
      && (replaced_object_size >= largest_live_object_size)
      && (new_object_size < replaced_object_size)) {
    return ut_refresh_largest_live_object_cache(chain_anchor,
                                                klv_start_address,
                                                klv_end_address);
  }

  ut_largest_live_object_cache_bump(new_object_size);
  return status;
}

/***************************************************************************//**
 * Selects the chain head after a reclaim walk completes.
 *
 * Prefer @p first_relocated_addr when set: that is either the start of the
 * compacted relocated chain, or (head-overwrite append shortcut) the first
 * surviving in-place live object after the skipped head. Otherwise the newly
 * written object at @p write_addr becomes the head (empty-chain or head-only
 * overwrite cases).
 ******************************************************************************/
static uint32_t reclaim_final_anchor(const sli_klv_ut_stream_reservation_t *reservation,
                                     uint32_t chain_anchor)
{
  if (reservation == NULL) {
    return chain_anchor;
  }

  if (reservation->first_relocated_addr != 0U) {
    return reservation->first_relocated_addr;
  }

  return reservation->write_addr;
}

/***************************************************************************//**
 * Returns whether @p addr's flash page overlaps the committed live chain.
 *
 * @p live_bytes is the forward length of the half-open circular interval starting
 * at @p final_anchor (typically through the post-write tail). Any erase page that
 * shares even one byte with that interval must not be erased.
 ******************************************************************************/
static bool reclaim_page_overlaps_committed_live_chain(uint32_t addr,
                                                       uint32_t final_anchor,
                                                       uint32_t live_bytes,
                                                       uint32_t klv_start_address,
                                                       uint32_t klv_end_address)
{
  if ((live_bytes == 0U)
      || !is_address_in_range(addr, klv_start_address, klv_end_address)
      || !is_address_in_range(final_anchor, klv_start_address, klv_end_address)) {
    return false;
  }

  return check_range_overlaps(SLI_KLV_ANCHOR_PAGE_START(addr),
                              FLASH_PAGE_SIZE,
                              final_anchor,
                              live_bytes,
                              klv_start_address,
                              klv_end_address);
}

/***************************************************************************//**
 * Rewinds the final reclaim erase cursor when the walk advanced it past the
 * initial head page without erasing that page.
 *
 * That happens when the overwrite target still occupied the head page for most
 * of the walk: every interim sweep stops at the same page boundary, so the
 * cursor can leap ahead while the head page stays dirty. Before the post-commit
 * sweep, pull the cursor back to the initial head page when the new anchor has
 * moved to a later page.
 *
 * The rewind is suppressed when the walk-start page overlaps the committed live
 * chain of length @p live_bytes from @p final_anchor. A wrapped relocate can
 * reuse the old head page for live objects; erasing it would destroy valid data.
 * @p reservation.initial_anchor must still record the walk-start head (not a
 * mid-walk refresh) so the non-wrapped dirty-head case continues to rewind.
 ******************************************************************************/
static uint32_t reclaim_final_erase_cursor(uint32_t erase_cursor,
                                           uint32_t initial_anchor,
                                           uint32_t final_anchor,
                                           uint32_t live_bytes,
                                           uint32_t klv_start_address,
                                           uint32_t klv_end_address)
{
  uint32_t initial_anchor_page = SLI_KLV_ANCHOR_PAGE_START(initial_anchor);
  uint32_t new_anchor_page = SLI_KLV_ANCHOR_PAGE_START(final_anchor);

  if ((compare_logical_address(initial_anchor_page,
                               new_anchor_page,
                               klv_start_address,
                               klv_end_address) < 0)
      && (compare_logical_address(erase_cursor,
                                  initial_anchor_page,
                                  klv_start_address,
                                  klv_end_address) > 0)) {
    if (reclaim_page_overlaps_committed_live_chain(initial_anchor_page,
                                                   final_anchor,
                                                   live_bytes,
                                                   klv_start_address,
                                                   klv_end_address)) {
      return erase_cursor;
    }

    return initial_anchor_page;
  }

  return erase_cursor;
}

/***************************************************************************//**
 * Post-commit page sweep after a reclaiming write.
 *
 * Computes the post-write tail with @ref advance_ut_adress() so wrapped objects
 * and multi-segment layouts are handled correctly, then treats the committed
 * live chain as the circular half-open range
 * [@p final_anchor, final_write_tail). The erase cursor may rewind to a dirty
 * walk-start head page only when that page is outside the live range. Each
 * candidate erase page is checked for overlap with the live range before it is
 * erased, so a page that contains any live byte is never cleared even if the
 * cursor itself started outside the range.
 ******************************************************************************/
static sl_status_t reclaim_final_erase_sweep(uint32_t erase_cursor,
                                             uint32_t initial_anchor,
                                             uint32_t final_anchor,
                                             uint32_t write_addr,
                                             uint32_t written_object_size,
                                             uint32_t klv_start_address,
                                             uint32_t klv_end_address)
{
  uint32_t final_write_tail = 0U;
  uint32_t live_bytes = 0U;
  uint32_t total_size = 0U;
  uint32_t sweep_cursor = 0U;
  uint32_t erase_span = 0U;
  uint32_t erased_bytes = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((written_object_size == 0U)
      || !is_address_in_range(write_addr, klv_start_address, klv_end_address)
      || !is_address_in_range(final_anchor, klv_start_address, klv_end_address)) {
    return SL_STATUS_OK;
  }

  final_write_tail = advance_ut_adress(write_addr,
                                       written_object_size,
                                       klv_start_address,
                                       klv_end_address);
  total_size = get_total_region_size(klv_start_address, klv_end_address);
  live_bytes = cal_distance_forward(final_anchor,
                                    final_write_tail,
                                    klv_start_address,
                                    klv_end_address);

  // Endpoints coincide only when the live chain occupies the entire ring.
  if ((live_bytes == 0U) && (total_size > 0U)) {
    live_bytes = total_size;
  }

  sweep_cursor = reclaim_final_erase_cursor(erase_cursor,
                                            initial_anchor,
                                            final_anchor,
                                            live_bytes,
                                            klv_start_address,
                                            klv_end_address);

  // Soft stop and hard floor are the post-write tail: the dead gap runs from
  // final_write_tail forward to final_anchor. write_addr alone would leave the
  // newly written object's body unprotected.
  erase_span = reclaim_erase_span(sweep_cursor,
                                  final_anchor,
                                  final_write_tail,
                                  final_write_tail,
                                  klv_start_address,
                                  klv_end_address);

  while (erased_bytes < erase_span) {
    if (reclaim_page_overlaps_committed_live_chain(sweep_cursor,
                                                   final_anchor,
                                                   live_bytes,
                                                   klv_start_address,
                                                   klv_end_address)) {
      break;
    }

    // Final sweep may overlap pages that the normal reclaim walk already
    // erased. Do not consume flash endurance erasing an already blank page.
    if (!is_logical_range_blank(sweep_cursor, FLASH_PAGE_SIZE,
                                klv_start_address, klv_end_address)) {
      status = sli_token_manager_nvm_page_erase(sweep_cursor);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }

    sweep_cursor = advance_ut_adress(sweep_cursor,
                                     FLASH_PAGE_SIZE,
                                     klv_start_address,
                                     klv_end_address);
    erased_bytes += FLASH_PAGE_SIZE;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Measures readable survivors after a pinned head-overwrite target.
 *
 * One walk from @p head following @c next_addr until the chain returns to
 * @p head (or hits an erased write frontier). Records the first readable
 * survivor and its footprint, and sets @p sole_live_out when no readable
 * object other than the pinned head is linked. This replaces a separate
 * ring-count pass: the same walk answers both “any survivors?” and “sole?”.
 ******************************************************************************/
static sl_status_t measure_remaining_live_chain(uint32_t head,
                                                uint32_t klv_start_address,
                                                uint32_t klv_end_address,
                                                uint32_t *first_live_out,
                                                uint32_t *live_bytes_out,
                                                bool *sole_live_out)
{
  uint32_t current = head;
  uint32_t first_live = 0U;
  uint32_t live_bytes = 0U;
  uint32_t total_region_size = 0U;
  uint32_t max_steps = 0U;
  uint32_t step = 0U;
  sl_status_t status = SL_STATUS_OK;
  bool completed = false;

  if ((first_live_out == NULL) || (live_bytes_out == NULL) || (sole_live_out == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  *first_live_out = 0U;
  *live_bytes_out = 0U;
  *sole_live_out = false;

  if (!is_address_in_range(head, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  max_steps = (total_region_size / sizeof(uint32_t)) + 1U;

  do {
    sli_klv_ut_obj_info_t current_info = { 0 };
    uint32_t current_object_size = 0U;

    status = decode_object_from_anchor(current,
                                       klv_start_address,
                                       klv_end_address,
                                       &current_info);
    if (status == SL_STATUS_NOT_FOUND) {
      // Write frontier / erased slot ends the linked span. No further survivor
      // is reachable via next_addr, so zero survivors means sole-live.
      *first_live_out = first_live;
      *live_bytes_out = live_bytes;
      *sole_live_out = (live_bytes == 0U);
      return SL_STATUS_OK;
    }
    if (status != SL_STATUS_OK) {
      return status;
    }

    current_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(current_info.header.header_size,
                                                              current_info.header.payload_length);
    // The pinned head is the overwrite target; only later readable objects are
    // survivors that must be preserved if the append shortcut is taken.
    if ((current != head) && current_info.header.is_readable) {
      if (first_live == 0U) {
        first_live = current;
      }
      live_bytes += current_object_size;
    }

    current = current_info.next_addr;
    step++;
  } while ((current != head) && (step < max_steps));

  completed = (current == head);
  *first_live_out = first_live;
  *live_bytes_out = live_bytes;
  // Step-limit exit without returning to head is not a trustworthy sole ring.
  *sole_live_out = completed && (live_bytes == 0U);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Measures soft-deleted / unreadable bytes from @p start until the first live
 * object (or @p tail), and returns that first live address when present.
 *
 * Used to decide when a pinned head-overwrite target can be relocated: once the
 * dead gap behind it is at least one flash page, advancing the anchor past the
 * head unlocks page erase and reclaim can make forward progress.
 ******************************************************************************/
static sl_status_t measure_soft_deleted_prefix(uint32_t start,
                                               uint32_t tail,
                                               uint32_t klv_start_address,
                                               uint32_t klv_end_address,
                                               uint32_t *soft_deleted_bytes_out,
                                               uint32_t *first_live_out)
{
  uint32_t current = start;
  uint32_t soft_deleted_bytes = 0U;
  uint32_t first_live = 0U;
  uint32_t total_region_size = 0U;
  uint32_t max_steps = 0U;
  uint32_t step = 0U;
  sl_status_t status = SL_STATUS_OK;
  bool terminated = false;

  if ((soft_deleted_bytes_out == NULL) || (first_live_out == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  *soft_deleted_bytes_out = 0U;
  *first_live_out = 0U;

  if (!is_address_in_range(start, klv_start_address, klv_end_address)
      || !is_address_in_range(tail, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  total_region_size = get_total_region_size(klv_start_address, klv_end_address);
  max_steps = (total_region_size / sizeof(uint32_t)) + 1U;
  terminated = (current == tail);

  while ((current != tail) && (step < max_steps)) {
    sli_klv_ut_obj_info_t current_info = { 0 };
    uint32_t current_object_size = 0U;

    status = decode_object_from_anchor(current,
                                       klv_start_address,
                                       klv_end_address,
                                       &current_info);
    if (status == SL_STATUS_NOT_FOUND) {
      terminated = true;
      break;
    }
    if (status != SL_STATUS_OK) {
      return status;
    }

    current_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(current_info.header.header_size,
                                                              current_info.header.payload_length);
    if (current_info.header.is_readable) {
      first_live = current;
      terminated = true;
      break;
    }

    soft_deleted_bytes += current_object_size;
    current = current_info.next_addr;
    step++;
    if (current == tail) {
      terminated = true;
    }
  }

  if (!terminated) {
    return SL_STATUS_INVALID_STATE;
  }

  *soft_deleted_bytes_out = soft_deleted_bytes;
  *first_live_out = first_live;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Relocates a pinned head-overwrite target so reclaim can erase its page.
 *
 * While the overwrite target remains the committed chain head, erase_stop pins
 * that page and soft-deleted survivors after it cannot free memory space. Relocating
 * survivors alone then stalls: the same live suffix is copied again without
 * creating usable contiguous space for the final write.
 *
 * This only applies while a live object still stands in place behind the dead
 * gap, because that object is what becomes the new head. This helper moves the
 * old head value to the reclaim frontier (copy, validate, advance anchor,
 * soft-delete source), retargets @p relocate->target_delete_addr at the
 * relocated copy, and relaxes erase_stop so dead pages can be erased. The old
 * value stays reachable from the new anchor until the replacement is committed.
 * Finish/soft-delete therefore operates on the relocated copy,
 * while @p relocate->chain_anchor advances to the next live survivor. If the
 * gap is still too short afterwards, @ref reclaim_chain continues the normal
 * relocation walk from the new head.
 *
 * @param[in,out] relocate    Walk cursors and reclaim window to update.
 * @param[in]     ut_context  Shared UT context (region bounds and anchor commit).
 *
 * @return SL_STATUS_OK on success;
 *         SL_STATUS_IDLE when no live survivor remains in place;
 *         otherwise a copy/erase/anchor error.
 ******************************************************************************/
static sl_status_t try_relocate_pinned_overwrite_target(sli_klv_ut_pinned_head_relocate_t *relocate,
                                                        const sli_klv_ut_context_t *ut_context)
{
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  uint32_t soft_deleted_bytes = 0U;
  uint32_t first_live = 0U;
  uint32_t target_addr = 0U;
  uint32_t target_next = 0U;
  uint32_t target_object_size = 0U;
  uint32_t relocated_addr = 0U;
  uint32_t previous_anchor = 0U;
  bool wrapped_relocation = false;
  sli_klv_ut_obj_info_t target_info = { 0 };
  sli_klv_ut_reclaim_window_t *reclaim_window = NULL;
  sl_status_t status = SL_STATUS_OK;

  if ((relocate == NULL)
      || (relocate->write_addr == NULL)
      || (relocate->chain_anchor == NULL)
      || (relocate->target_delete_addr == NULL)
      || (relocate->reclaim_erase_stop_addr == NULL)
      || (relocate->first_relocated_addr == NULL)
      || (relocate->reclaim_window == NULL)
      || (ut_context == NULL)
      || (ut_context->chain_anchor == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  reclaim_window = relocate->reclaim_window;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;
  target_addr = *relocate->target_delete_addr;

  if ((target_addr == 0U)
      || (target_addr != *ut_context->chain_anchor)
      || !is_address_in_range(target_addr, klv_start_address, klv_end_address)) {
    return SL_STATUS_IDLE;
  }

  status = decode_object_from_anchor(target_addr,
                                     klv_start_address,
                                     klv_end_address,
                                     &target_info);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (!target_info.header.is_readable) {
    return SL_STATUS_IDLE;
  }

  target_next = target_info.next_addr;
  target_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(target_info.header.header_size,
                                                           target_info.header.payload_length);

  status = measure_soft_deleted_prefix(target_next,
                                       relocate->tail,
                                       klv_start_address,
                                       klv_end_address,
                                       &soft_deleted_bytes,
                                       &first_live);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Only a live object still standing in place can take over as the head. With
  // no such survivor the anchor would have to move to the relocated copy at the
  // frontier, which sits after everything this reclaim already relocated and
  // would drop those objects out of the persisted chain. The regular walk and
  // the append shortcut already handle a head that is the last live object, so
  // leave them to it.
  if (first_live == 0U) {
    return SL_STATUS_IDLE;
  }

  // No minimum on what the head move frees by itself: when the gap is still
  // short afterwards, reclaim_chain() keeps relocating the survivors, and the
  // soft-deleted bytes further along the span are what that walk releases.
  status = prepare_reclaim_write_address(relocate->write_addr,
                                         target_object_size,
                                         target_addr,
                                         reclaim_window,
                                         klv_start_address,
                                         klv_end_address);
  if (status != SL_STATUS_OK) {
    if (status != SL_STATUS_NO_MORE_RESOURCE) {
      return status;
    }

    status = copy_validated_object_with_reclaim_wrap(*relocate->write_addr,
                                                     &target_info,
                                                     target_object_size,
                                                     reclaim_window,
                                                     klv_start_address,
                                                     klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    wrapped_relocation = true;
  }

  relocated_addr = *relocate->write_addr;

  if (!wrapped_relocation) {
    status = copy_validated_live_object(relocated_addr,
                                        &target_info,
                                        target_object_size,
                                        klv_start_address,
                                        klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  *relocate->write_addr = advance_ut_adress(*relocate->write_addr,
                                            target_object_size,
                                            klv_start_address,
                                            klv_end_address);

  // first_live is live and in place, so the anchor always lands on it. Passing
  // no relocated fallback keeps it that way: an anchor at the relocated copy
  // would sit after the objects already relocated in this reclaim.
  previous_anchor = *relocate->chain_anchor;
  status = advance_chain_anchor_after_head(target_next,
                                           relocate->tail,
                                           0U,
                                           relocate->chain_anchor,
                                           ut_context);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (*relocate->chain_anchor != first_live) {
    return SL_STATUS_INVALID_STATE;
  }

  status = mark_ut_object_deleted(previous_anchor,
                                  klv_start_address,
                                  klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Finish must soft-delete the relocated old value, not the retired source.
  *relocate->target_delete_addr = relocated_addr;
  *relocate->reclaim_erase_stop_addr = relocated_addr;
  reclaim_window->erase_stop_addr = relocated_addr;

  // Post-write head is the first surviving live object after the old head.
  // It is logically ahead of anything relocated to the frontier,
  // so it also covers copies an earlier part of this reclaim already made.
  *relocate->first_relocated_addr = first_live;

  status = refresh_reclaim_window(reclaim_window,
                                  *relocate->chain_anchor,
                                  *relocate->write_addr,
                                  klv_start_address,
                                  klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Dead pages between the old head and the new live suffix can now be erased.
  // The floor is the live head, not initial_tail: the wide erase_stop (relocated
  // old value at the frontier) would otherwise let a sweep run through
  // on its way there.
  status = erase_reclaimed_pages(&reclaim_window->erase_cursor,
                                 *relocate->chain_anchor,
                                 *relocate->reclaim_erase_stop_addr,
                                 *relocate->chain_anchor,
                                 klv_start_address,
                                 klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT reclaim relocated pinned overwrite target: old=0x%08lx new=0x%08lx anchor=0x%08lx soft_deleted=%lu\r\n",
                  previous_anchor,
                  relocated_addr,
                  *relocate->chain_anchor,
                  soft_deleted_bytes); )
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Head-overwrite append shortcut when a full relocate would hit the pinned head.
 *
 * Why this path exists: while the overwrite target is still the committed head,
 * erase_stop keeps that object readable, so reclaim cannot free its pages yet.
 * Relocating every later live object into the free gap can therefore require
 * programming into the still-occupied head/extension region and would fail with
 * flash verify errors — or, if rejected naively, SL_STATUS_NO_MORE_RESOURCE even
 * though appending only the updated head value would fit.
 *
 * When that relocate footprint would overlap the live head, skip relocating the
 * survivors entirely: reserve space for the new object at the current tail, and
 * let @ref reclaim_final_anchor commit the head to the first surviving in-place
 * object after the skipped target. The contiguous chain from that survivor
 * through the old tail then includes the newly written value. Power-fail safe
 * because the old head remains committed until after the new object is written.
 *
 * This is not an early-exit after a partial relocate (which would orphan
 * not-yet-copied objects). No survivor is moved; the existing suffix stays the
 * authoritative chain.
 *
 * @param[in,out] write_addr            On success, destination for the new object.
 * @param[out]    first_surviving_out   First live object after the skipped head,
 *                                      or 0 when none remain (new object becomes head).
 * @param[out]    taken_out             True when the shortcut reserved @p write_addr.
 *
 * @return SL_STATUS_OK with @p taken_out false when a full relocate is safe,
 *         SL_STATUS_OK with @p taken_out true when only the new object was
 *         reserved, SL_STATUS_NO_MORE_RESOURCE when not even the append-only
 *         form leaves room to relocate the pinned head afterwards (the caller
 *         may still move the pinned head out via
 *         @ref try_relocate_pinned_overwrite_target), or a backend error.
 ******************************************************************************/
static sl_status_t try_reclaim_head_overwrite_append_shortcut(uint32_t *write_addr,
                                                              uint32_t object_size,
                                                              uint32_t next_after_target,
                                                              uint32_t tail,
                                                              sli_klv_ut_reclaim_window_t *reclaim_window,
                                                              uint32_t klv_start_address,
                                                              uint32_t klv_end_address,
                                                              uint32_t *first_surviving_out,
                                                              bool *taken_out)
{
  uint32_t first_live = 0U;
  uint32_t remaining_live_bytes = 0U;
  uint32_t gap_end = 0U;
  uint32_t append_gap_end = 0U;
  uint32_t relocate_footprint = 0U;
  bool sole_live = false;
  sl_status_t status = SL_STATUS_OK;

  if ((write_addr == NULL)
      || (reclaim_window == NULL)
      || (first_surviving_out == NULL)
      || (taken_out == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  *taken_out = false;
  *first_surviving_out = 0U;

  if (object_size == 0U) {
    return SL_STATUS_OK;
  }

  // Free reclaim space ends at the page that still holds the pinned head.
  // Mid-page bytes before the head are not erasable while that head remains
  // authoritative, so survivor admission must use the page-aligned bound
  // (same as the former reclaim_free_gap_end() helper).
  gap_end = (reclaim_window->reclaim_gap_end != 0U)
            ? reclaim_window->reclaim_gap_end
            : reclaim_window->initial_anchor;
  append_gap_end = SLI_KLV_ANCHOR_PAGE_START(gap_end);

  status = measure_remaining_live_chain(gap_end,
                                        klv_start_address,
                                        klv_end_address,
                                        &first_live,
                                        &remaining_live_bytes,
                                        &sole_live);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Sole-target reserve borrow: the suffix walk found no survivors and reached
  // the frontier (or wrapped to the pinned head). The replacement may occupy
  // the free gap that includes the retiring object's reserve.
  if (sole_live) {
    // Interrupted sole overwrite: erase whole pages of dead suffix that do
    // not share the pinned head, but only once at least one full flash page
    // of soft-deleted bytes sits after the head (same progress gate as
    // pinned-head relocate).
    if (next_after_target != tail) {
      uint32_t soft_deleted_bytes = 0U;
      uint32_t first_live_after = 0U;

      status = measure_soft_deleted_prefix(next_after_target,
                                           tail,
                                           klv_start_address,
                                           klv_end_address,
                                           &soft_deleted_bytes,
                                           &first_live_after);
      if (status != SL_STATUS_OK) {
        return status;
      }

      if ((first_live_after == 0U) && (soft_deleted_bytes >= FLASH_PAGE_SIZE)) {
        uint32_t candidate = SLI_KLV_ANCHOR_PAGE_START(next_after_target);
        uint32_t bytes_to_candidate = 0U;
        uint32_t erase_bytes = 0U;

        if (candidate != next_after_target) {
          candidate = advance_ut_adress(candidate,
                                        FLASH_PAGE_SIZE,
                                        klv_start_address,
                                        klv_end_address);
        }
        bytes_to_candidate = cal_distance_forward(next_after_target,
                                                  candidate,
                                                  klv_start_address,
                                                  klv_end_address);

        if ((bytes_to_candidate < soft_deleted_bytes)
            && (cal_distance_forward(candidate,
                                     append_gap_end,
                                     klv_start_address,
                                     klv_end_address) >= object_size)) {
          uint32_t erase_cursor = candidate;

          erase_bytes = cal_distance_forward(candidate,
                                             tail,
                                             klv_start_address,
                                             klv_end_address);
          while ((erase_bytes > 0U) && (erase_cursor != append_gap_end)) {
            status = sli_token_manager_nvm_page_erase(erase_cursor);
            if (status != SL_STATUS_OK) {
              return status;
            }

            erase_cursor = advance_ut_adress(erase_cursor,
                                             FLASH_PAGE_SIZE,
                                             klv_start_address,
                                             klv_end_address);
            erase_bytes = (erase_bytes > FLASH_PAGE_SIZE)
                          ? (erase_bytes - FLASH_PAGE_SIZE)
                          : 0U;
          }

          status = prepare_reclaim_destination(candidate,
                                               object_size,
                                               klv_start_address,
                                               klv_end_address);
          if (status == SL_STATUS_OK) {
            *write_addr = candidate;
            *first_surviving_out = 0U;
            *taken_out = true;
            return SL_STATUS_OK;
          }
          if (status != SL_STATUS_NO_MORE_RESOURCE) {
            return status;
          }
        }
      }
    }

    if (!reclaim_destination_overlaps_live_head(*write_addr,
                                                 object_size,
                                                 append_gap_end,
                                                 klv_start_address,
                                                 klv_end_address)) {
      status = prepare_reclaim_write_address(write_addr,
                                             object_size,
                                             tail,
                                             reclaim_window,
                                             klv_start_address,
                                             klv_end_address);
      if (status != SL_STATUS_OK) {
        return status;
      }

      *taken_out = true;
      return SL_STATUS_OK;
    }
  }

  // Multi-token path: historical page-aligned admission checks.
  relocate_footprint = remaining_live_bytes + object_size;
  if (!reclaim_destination_overlaps_live_head(*write_addr,
                                              relocate_footprint,
                                              append_gap_end,
                                              klv_start_address,
                                              klv_end_address)) {
    return SL_STATUS_OK;
  }

  if (reclaim_destination_overlaps_live_head(*write_addr,
                                             object_size,
                                             append_gap_end,
                                             klv_start_address,
                                             klv_end_address)) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  status = prepare_reclaim_write_address(write_addr,
                                         object_size,
                                         tail,
                                         reclaim_window,
                                         klv_start_address,
                                         klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  *first_surviving_out = first_live;
  *taken_out = true;
  TOKENDBG(printf("UT reclaim head-overwrite append shortcut: write=0x%08lx surviving=0x%08lx remaining_live=%lu object=%lu\r\n",
                  *write_addr,
                  first_live,
                  remaining_live_bytes,
                  object_size); )
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Relocates live UT objects and prepares the destination for a final write.
 *
 * Shared core of @ref reclaim_and_write and @ref reclaim_and_reserve.
 *
 * @param[in] klv_handle  Object the caller intends to write once the walk is
 *                        done, or NULL for a straighten-only pass. A NULL
 *                        handle reserves nothing for an incoming object, so the
 *                        window is sized from the live chain alone and the
 *                        returned reservation is meaningless to the caller.
 ******************************************************************************/
static sl_status_t reclaim_chain(const sl_klv_handle_t *klv_handle,
                                 uint32_t *chain_anchor,
                                 const sli_klv_ut_context_t *ut_context,
                                 uint32_t tail,
                                 const sli_klv_ut_obj_info_t *target_match,
                                 sli_klv_ut_reclaim_result_t *result)
{
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  sl_status_t status = SL_STATUS_OK;
  uint32_t current = 0U;
  uint32_t next_anchor = 0U;
  uint32_t write_addr = tail;
  uint32_t object_size = 0U;
  uint32_t initial_anchor = 0U;
  uint32_t initial_tail = tail;
  uint32_t reclaim_erase_stop_addr = tail;
  uint32_t first_relocated_addr = 0U;
  uint32_t repack_guard_bytes = 0U;
  uint32_t target_delete_addr = 0U;
  bool target_found = false;
  bool has_target = false;
  bool target_delete_pending = false;
  bool target_delete_was_anchor = false;
  bool append_shortcut_taken = false;
  // NULL @p klv_handle means straighten-only: relocate the live chain and stop.
  // Used before extension activation so the pass does not reserve flash for a
  // fake incoming object that will never be written.
  bool straighten_only = (klv_handle == NULL);
  sli_klv_ut_reclaim_window_t reclaim_window = { 0 };

  if ((chain_anchor == NULL)
      || (ut_context == NULL)
      || (result == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;

  if (!is_address_in_range(*ut_context->chain_anchor, klv_start_address, klv_end_address)
      || !is_address_in_range(tail, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_STATE;
  }

  if (target_match != NULL) {
    if (!is_address_in_range(target_match->obj_addr, klv_start_address, klv_end_address)
        || !is_address_in_range(target_match->next_addr, klv_start_address, klv_end_address)) {
      return SL_STATUS_INVALID_STATE;
    }

    has_target = true;
  }

  if (!straighten_only) {
    object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                      klv_handle->length);
  }

  status = ut_largest_live_object_cache_ensure_valid(*ut_context->chain_anchor,
                                                     klv_start_address,
                                                     klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Straighten-only sizes the window from live objects alone. A write-backed
  // reclaim still takes the larger of the incoming object and the largest live
  // entry so the final reservation always fits after the walk.
  reclaim_window.repack_reserve_size = object_size;
  if (largest_live_object_size > reclaim_window.repack_reserve_size) {
    reclaim_window.repack_reserve_size = largest_live_object_size;
  }
  repack_guard_bytes = reclaim_window.repack_reserve_size;
  reclaim_window.repack_reserve_size =
    compute_ut_repack_reserve_size(reclaim_window.repack_reserve_size);

  TOKENDBG(printf("UT reclaim start: key=0x%04lx anchor=0x%08lx tail=0x%08lx object=%lu largest=%lu repack=%lu target=%u straighten=%u\r\n",
                  straighten_only ? 0UL : (klv_handle->key & 0xFFFFU),
                  *ut_context->chain_anchor,
                  tail,
                  object_size,
                  largest_live_object_size,
                  reclaim_window.repack_reserve_size,
                  has_target,
                  straighten_only); )

  current = *ut_context->chain_anchor;
  initial_anchor = current;
  reclaim_erase_stop_addr = initial_tail;
  reclaim_window.initial_anchor = initial_anchor;
  reclaim_window.initial_tail = tail;
  reclaim_window.erase_stop_addr = initial_tail;
  reclaim_window.erase_cursor = SLI_KLV_ANCHOR_PAGE_START(initial_anchor);
  // The UT ring is ordered as base + ext1 + ext2 + ... regardless of where
  // those regions sit in physical flash. Use logical offsets here so reclaim
  // does not misclassify non-monotonic extension addresses as a wrapped chain.
  reclaim_window.initial_chain_wrapped =
    (compare_logical_address(tail,
                             initial_anchor,
                             klv_start_address,
                             klv_end_address) < 0);
  reclaim_window.reclaim_gap_start = tail;
  reclaim_window.reclaim_gap_end = initial_anchor;

  if (!reclaim_window.initial_chain_wrapped) {
    status = erase_prefix_pages_before_anchor(initial_anchor,
                                              klv_start_address,
                                              klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  // Logical reserve: the moving window that ends at the current anchor.
  // The erase cursor is a separate page-aligned sweep and must not replace it.
  reclaim_window.repack_start = compute_repack_start(*chain_anchor,
                                                     tail,
                                                     reclaim_window.repack_reserve_size,
                                                     klv_start_address,
                                                     klv_end_address);
  if (reclaim_window.initial_chain_wrapped) {
    uint32_t erase_page = 0U;

    // The erase cursor is the first page boundary at or after the tail. A gap
    // without a page of its own is not a reason to refuse the reclaim: the
    // capacity gate has already passed, and the first copy goes into blank
    // bytes at the tail. The cursor then stays parked on the anchor page,
    // which cannot be erased while the head is on it, and is released once
    // the walk moves past that page.
    if (try_compute_reclaim_gap_repack_start(reclaim_window.reclaim_gap_start,
                                             reclaim_window.reclaim_gap_end,
                                             klv_start_address,
                                             klv_end_address,
                                             &erase_page)) {
      reclaim_window.erase_cursor = erase_page;

      // A page-aligned tail puts the cursor on the relocation floor page. The
      // floor bound in reclaim_erase_span() is the tail page itself, so a
      // sweep starting there would measure a zero span and never move, and
      // the first relocation would find no erased room. That page holds no
      // chain bytes (the chain ends exactly at the tail), so erase it now and
      // start the sweep on the next page; the floor then only stops the
      // cursor once it has gone around the ring.
      if (erase_page == tail) {
        status = sli_token_manager_nvm_page_erase(erase_page);
        if (status != SL_STATUS_OK) {
          return status;
        }

        reclaim_window.erase_cursor = advance_ut_adress(erase_page,
                                                        FLASH_PAGE_SIZE,
                                                        klv_start_address,
                                                        klv_end_address);
      }
    }

    reclaim_window.repack_uses_gap_window = true;
  }

  write_addr = tail;

  {
    uint32_t total_region_size = get_total_region_size(klv_start_address, klv_end_address);
    uint32_t max_steps = (total_region_size / sizeof(uint32_t)) + 1U;
    uint32_t step = 0U;
    bool terminated = false;

    while (step < max_steps) {
      sli_klv_ut_obj_info_t current_info = { 0 };
      uint32_t current_object_size = 0U;

      if (current == tail) {
        terminated = true;
        break;
      }

      status = decode_object_from_anchor(current,
                                         klv_start_address,
                                         klv_end_address,
                                         &current_info);
      if (status == SL_STATUS_NOT_FOUND) {
        // Erased slot or page-base torn header before the expected tail:
        // nothing left to relocate.
        terminated = true;
        break;
      } else if (status != SL_STATUS_OK) {
        return status;
      }

      current_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(current_info.header.header_size,
                                                                current_info.header.payload_length);
      next_anchor = current_info.next_addr;
      step++;

      if (has_target && (current == target_match->obj_addr)) {
        target_found = true;
        target_delete_pending = true;
        target_delete_addr = current;
        target_delete_was_anchor = (current == *ut_context->chain_anchor);
        // The object being overwritten is not relocated, but it has to stay
        // readable until its delete marker is written after the new copy lands.
        // Pinning the erase stop to it protects it from every later sweep in this
        // reclaim, so it must be published before the first of those sweeps runs.
        reclaim_erase_stop_addr = target_delete_addr;
        reclaim_window.erase_stop_addr = reclaim_erase_stop_addr;

        status = erase_reclaimed_pages(&reclaim_window.erase_cursor,
                                       current,
                                       reclaim_erase_stop_addr,
                                       reclaim_window.initial_tail,
                                       klv_start_address,
                                       klv_end_address);
        if (status != SL_STATUS_OK) {
          return status;
        }
        current = next_anchor;
        if (current == initial_anchor) {
          return SL_STATUS_NO_MORE_RESOURCE;
        }

        // Head overwrite: if relocating every later live object plus the new value
        // would overlap the pinned live head, do not relocate the survivors.
        // Reserve the tail for the updated head token only; reclaim_final_anchor
        // will commit the chain to the first surviving in-place object so the
        // existing suffix (and the appended new value) stay reachable. See
        // try_reclaim_head_overwrite_append_shortcut() for why this is preferred
        // over SL_STATUS_NO_MORE_RESOURCE when capacity remains for the append.
        if (target_delete_was_anchor && !straighten_only) {
          uint32_t first_surviving = 0U;

          status = try_reclaim_head_overwrite_append_shortcut(&write_addr,
                                                              object_size,
                                                              current,
                                                              tail,
                                                              &reclaim_window,
                                                              klv_start_address,
                                                              klv_end_address,
                                                              &first_surviving,
                                                              &append_shortcut_taken);
          if (status != SL_STATUS_OK) {
            return status;
          }
          if (append_shortcut_taken) {
            first_relocated_addr = first_surviving;
            terminated = true;
            break;
          }

          // A prior reclaim may already have left >= one flash page of
          // soft-deleted objects after the pinned head. Relocate the old head
          // value now so erase can free that gap, instead of copying the live
          // survivor suffix again without making progress.
          {
            sli_klv_ut_pinned_head_relocate_t pinned_relocate = {
              .write_addr = &write_addr,
              .chain_anchor = chain_anchor,
              .target_delete_addr = &target_delete_addr,
              .reclaim_erase_stop_addr = &reclaim_erase_stop_addr,
              .first_relocated_addr = &first_relocated_addr,
              .tail = tail,
              .repack_guard_bytes = repack_guard_bytes,
              .reclaim_window = &reclaim_window,
            };

            status = try_relocate_pinned_overwrite_target(&pinned_relocate, ut_context);
          }
          if (status == SL_STATUS_OK) {
            // Survivors after the dead gap stay in place, reserve the final write.
            // target_delete_addr now names the relocated old value at the tail so
            // finish soft-deletes that copy after the replacement commits.
            //
            // Erase progress must be the new live head (*chain_anchor), not the
            // walk-start tail.
            status = prepare_reclaim_write_address(&write_addr,
                                                   object_size,
                                                   *chain_anchor,
                                                   &reclaim_window,
                                                   klv_start_address,
                                                   klv_end_address);
            if (status != SL_STATUS_OK) {
              return status;
            }
            append_shortcut_taken = true;
            terminated = true;
            break;
          }
          if (status != SL_STATUS_IDLE) {
            return status;
          }
        }
        continue;
      }

      if (current_info.header.is_readable) {
        uint32_t relocated_addr = 0U;
        bool wrapped_relocation = false;
        bool relocating_head = (current == *ut_context->chain_anchor);

        status = prepare_reclaim_write_address(&write_addr,
                                               current_object_size,
                                               current,
                                               &reclaim_window,
                                               klv_start_address,
                                               klv_end_address);
        if (status != SL_STATUS_OK) {
          if (status != SL_STATUS_NO_MORE_RESOURCE) {
            return status;
          }

          status = copy_validated_object_with_reclaim_wrap(write_addr,
                                                           &current_info,
                                                           current_object_size,
                                                           &reclaim_window,
                                                           klv_start_address,
                                                           klv_end_address);
          if (status != SL_STATUS_OK) {
            return status;
          }

          wrapped_relocation = true;
        }

        relocated_addr = write_addr;

        if (first_relocated_addr == 0U) {
          first_relocated_addr = relocated_addr;
        }

        if (!wrapped_relocation) {
          status = copy_validated_live_object(relocated_addr,
                                              &current_info,
                                              current_object_size,
                                              klv_start_address,
                                              klv_end_address);
          if (status != SL_STATUS_OK) {
            return status;
          }
        }

        // Retire the source as soon as the relocated copy is validated. When the
        // overwrite target pins erase_stop, page erase cannot clear these bytes
        // yet; without a soft-delete both copies stay ACTIVE in the live span
        // and used/reclaimable accounting (and recovery) see permanent duplicates.
        // Keep the head itself ACTIVE until its anchor advance commits so an
        // interrupted head move still has an authoritative original.
        if (!relocating_head) {
          status = mark_ut_object_deleted(current,
                                          klv_start_address,
                                          klv_end_address);
          if (status != SL_STATUS_OK) {
            return status;
          }
        }

        write_addr = advance_ut_adress(write_addr,
                                       current_object_size,
                                       klv_start_address,
                                       klv_end_address);

        if (relocating_head) {
          uint32_t previous_anchor = *chain_anchor;

          status = advance_chain_anchor_after_head(next_anchor,
                                                   tail,
                                                   first_relocated_addr,
                                                   chain_anchor,
                                                   ut_context);
          if (status != SL_STATUS_OK) {
            return status;
          }

          if (*chain_anchor != previous_anchor) {
            // Head move is durable: soft-delete the old head so it cannot remain a
            // second live copy if erase of its page is still deferred.
            status = mark_ut_object_deleted(previous_anchor,
                                            klv_start_address,
                                            klv_end_address);
            if (status != SL_STATUS_OK) {
              return status;
            }

            // Refresh the reclaim window after the chain anchor is advanced.
            // The reserve rotates with the new anchor. `write_addr` is only
            // the frontier, so the window does not cover objects already
            // relocated between the initial tail and it.
            status = refresh_reclaim_window(&reclaim_window,
                                            *chain_anchor,
                                            write_addr,
                                            klv_start_address,
                                            klv_end_address);
            if (status != SL_STATUS_OK) {
              return status;
            }
          }
        }
      } else if (current == *ut_context->chain_anchor) {
        uint32_t previous_anchor = *chain_anchor;

        status = advance_chain_anchor_after_head(next_anchor,
                                                 tail,
                                                 0U,
                                                 chain_anchor,
                                                 ut_context);
        if (status != SL_STATUS_OK) {
          return status;
        }

        if (*chain_anchor != previous_anchor) {
          // Refresh the reclaim window after the chain anchor is advanced.
          // The dead head was not relocated, so `write_addr` is still the
          // frontier, and the reserve rotates to end at the new anchor.
          status = refresh_reclaim_window(&reclaim_window,
                                          *chain_anchor,
                                          write_addr,
                                          klv_start_address,
                                          klv_end_address);
          if (status != SL_STATUS_OK) {
            return status;
          }
        }
      }

      // End of one walk step: everything below `next_anchor` has now been either
      // relocated or established as dead, so its pages can go. `reclaim_erase_stop_addr`
      // still pins the overwrite target if one was met, and the initial tail keeps
      // the sweep out of the objects relocated so far.
      status = erase_reclaimed_pages(&reclaim_window.erase_cursor,
                                     next_anchor,
                                     reclaim_erase_stop_addr,
                                     reclaim_window.initial_tail,
                                     klv_start_address,
                                     klv_end_address);
      if (status != SL_STATUS_OK) {
        return status;
      }

      current = next_anchor;

      if (current == initial_anchor) {
        return SL_STATUS_NO_MORE_RESOURCE;
      }
    }

    if (!terminated) {
      return SL_STATUS_INVALID_STATE;
    }
  }

  if (has_target && !target_found) {
    return SL_STATUS_NOT_FOUND;
  }

  if (!append_shortcut_taken) {
    // When this walk already relocated objects, reclaim_final_anchor() commits
    // that first copy as the head and the pinned overwrite target is deleted
    // once the new value lands. The reserve has to remain in front of the head
    // that will be committed, not in front of the target being replaced.
    uint32_t reserve_anchor = (first_relocated_addr != 0U)
                              ? first_relocated_addr
                              : *chain_anchor;

    if (!reclaim_final_write_keeps_reserve(write_addr,
                                           object_size,
                                           reserve_anchor,
                                           reclaim_window.repack_reserve_size,
                                           klv_start_address,
                                           klv_end_address)) {
      return SL_STATUS_NO_MORE_RESOURCE;
    }

    // A pinned head cannot be relocated from here: the walk has already
    // soft-deleted every source it copied, so no live object remains in place
    // to take over as the head. try_relocate_pinned_overwrite_target() runs
    // during the walk instead, while those survivors are still standing.
    status = prepare_reclaim_write_address(&write_addr,
                                           object_size,
                                           tail,
                                           &reclaim_window,
                                           klv_start_address,
                                           klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  memset(result, 0, sizeof(sli_klv_ut_reclaim_result_t));
  result->reclaim_window = reclaim_window;
  result->reservation.write_addr = write_addr;
  result->reservation.erase_cursor = reclaim_window.erase_cursor;
  result->reservation.first_relocated_addr = first_relocated_addr;
  result->reservation.target_delete_addr = target_delete_addr;
  result->reservation.initial_anchor = initial_anchor;
  result->reservation.target_delete_pending = target_delete_pending;
  result->reservation.target_delete_was_anchor = target_delete_was_anchor;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns whether the context has registered extensions outside the active ring.
 ******************************************************************************/
static bool ut_context_has_pending_extensions(const sli_klv_ut_context_t *ut_context)
{
  return (ut_context != NULL)
         && (ut_context->registered_extension_info != NULL)
         && (ut_context->registered_extension_info_count
             > ut_context->extension_info_count);
}

/***************************************************************************//**
 * Builds a context whose logical layout includes all registered extensions.
 ******************************************************************************/
static void build_registered_ut_context(const sli_klv_ut_context_t *ut_context,
                                        sli_klv_ut_context_t *registered_context)
{
  if ((ut_context == NULL) || (registered_context == NULL)) {
    return;
  }

  *registered_context = *ut_context;
  registered_context->extension_info = ut_context->registered_extension_info;
  registered_context->extension_info_count =
    ut_context->registered_extension_info_count;
  registered_context->total_region_size =
    ut_context->registered_total_region_size;
}

/***************************************************************************//**
 * After pending-extension activation, try a reclaim-safe direct append on the
 * expanded layout.
 *
 * The forced straighten during activation may already have left enough
 * writable space at the tail once pending extensions join the ring. Re-running
 * the same write-space checks used by the non-reclaim path avoids a second
 * @ref reclaim_chain walk that would relocate live objects again.
 *
 * Returns @ref SL_STATUS_OK only when @p write_addr_out is erased and safe to
 * program. Any other status means the caller must continue into reclaim; soft
 * failures such as @ref SL_STATUS_NO_MORE_RESOURCE or
 * @ref SL_STATUS_INVALID_STATE are expected when deleted/stale bytes still
 * require a reclaim walk.
 ******************************************************************************/
static sl_status_t try_direct_append_after_extension_activation(const sli_klv_ut_context_t *ut_context,
                                                                const sli_klv_ut_scan_result_t *scan_result,
                                                                uint32_t object_size,
                                                                uint32_t *write_addr_out)
{
  uint32_t repack_reserve_size = 0U;
  uint32_t repack_guard_bytes = 0U;
  uint32_t largest = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL) || (ut_context->chain_anchor == NULL)
      || (scan_result == NULL) || (write_addr_out == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  // Activation invalidates the largest-live cache; rebuild against the expanded
  // active geometry before sizing the reserve.
  status = ut_largest_live_object_cache_ensure_valid(*ut_context->chain_anchor,
                                                     ut_context->klv_start_address,
                                                     ut_context->klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  largest = largest_live_object_size;
  if (scan_result->largest_live_obj_size > largest) {
    largest = scan_result->largest_live_obj_size;
    ut_largest_live_object_cache_bump(largest);
  }

  // Size the reserve from live + WIP (same basis as the registered-capacity gate)
  // so a concurrent stream's provisional object cannot shrink the window.
  repack_guard_bytes = get_largest_object_size(scan_result, object_size);
  repack_reserve_size = compute_ut_repack_reserve_size(repack_guard_bytes);

  status = try_direct_write_from_scan(ut_context,
                                      scan_result,
                                      object_size,
                                      repack_reserve_size,
                                      repack_guard_bytes,
                                      write_addr_out);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return prepare_append_destination(*write_addr_out,
                                    object_size,
                                    *ut_context->chain_anchor,
                                    ut_context->klv_start_address,
                                    ut_context->klv_end_address);
}

/***************************************************************************//**
 * Completes a one-shot write via post-activation direct append when safe.
 *
 * Used by @ref reclaim_and_write after pending extensions are activated and the
 * chain has been rescanned on the expanded layout. On success, @p completed_out
 * is true and the write/soft-delete/cache update have finished. When the
 * expanded geometry still cannot admit a reclaim-safe append, @p completed_out
 * is false and the caller must continue into @ref reclaim_chain.
 ******************************************************************************/
static sl_status_t try_direct_write_after_extension_activation(const sl_klv_handle_t *klv_handle,
                                                               uint32_t *chain_anchor,
                                                               const sli_klv_ut_context_t *reclaim_context,
                                                               const sli_klv_ut_scan_result_t *scan_result,
                                                               uint32_t new_object_size,
                                                               uint32_t replaced_object_size,
                                                               bool replaced_was_live,
                                                               void const *data,
                                                               bool *completed_out)
{
  sli_klv_obj_header_t obj_hdr;
  sl_status_t status = SL_STATUS_OK;
  uint32_t write_addr = 0U;
  uint32_t anchor_before = 0U;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  uint16_t data_crc = 0U;

  if (completed_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *completed_out = false;

  if ((klv_handle == NULL)
      || (chain_anchor == NULL)
      || (reclaim_context == NULL)
      || (scan_result == NULL)
      || (data == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  klv_start_address = reclaim_context->klv_start_address;
  klv_end_address = reclaim_context->klv_end_address;
  anchor_before = *chain_anchor;

  status = try_direct_append_after_extension_activation(reclaim_context,
                                                        scan_result,
                                                        new_object_size,
                                                        &write_addr);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Expanded geometry already admits a reclaim-safe append. Complete the write
  // like the non-reclaim path so live objects are not relocated a second time
  // by reclaim_chain.
  *completed_out = true;

  data_crc = calculate_checksum(data, klv_handle->length);
  SLI_KLV_INIT_HDR_FOR_WRITE(obj_hdr, klv_handle, data_crc);

  status = write_committed_ut_object(write_addr,
                                     &obj_hdr,
                                     data,
                                     klv_handle->length,
                                     klv_start_address,
                                     klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (scan_result->chain_empty) {
    uint32_t previous_anchor = *chain_anchor;

    *chain_anchor = write_addr;
    if (*chain_anchor != previous_anchor) {
      status = reclaim_context->anchor_commit(*chain_anchor,
                                              reclaim_context->anchor_context);
      if (status != SL_STATUS_OK) {
        *chain_anchor = previous_anchor;
        return status;
      }
    }
  }

  if (scan_result->match_found) {
    sli_klv_ut_obj_info_t delete_info = { 0 };

    status = decode_object_from_anchor(scan_result->match.obj_addr,
                                       klv_start_address,
                                       klv_end_address,
                                       &delete_info);
    if (status == SL_STATUS_OK) {
      if (delete_info.header.is_readable) {
        status = mark_ut_object_deleted(scan_result->match.obj_addr,
                                        klv_start_address,
                                        klv_end_address);
        if (status != SL_STATUS_OK) {
          return status;
        }

        if (scan_result->match.obj_addr == anchor_before) {
          status = skip_unreadable_chain_head(chain_anchor, reclaim_context);
          if (status != SL_STATUS_OK) {
            return status;
          }
        }
      }
    } else if (status != SL_STATUS_NOT_FOUND) {
      return status;
    }
  }

  status = ut_largest_live_object_cache_on_write_commit(*chain_anchor,
                                                        klv_start_address,
                                                        klv_end_address,
                                                        new_object_size,
                                                        replaced_object_size,
                                                        replaced_was_live);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT post-activation direct write: key=0x%04lx write_addr=0x%08lx empty=%u match=%u\r\n",
                  (klv_handle->key & 0xFFFFU),
                  write_addr,
                  scan_result->chain_empty,
                  scan_result->match_found); )
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reclaims space for a UT stream write and leaves the destination reserved.
 *
 * This mirrors @ref reclaim_and_write but stops after the relocation and
 * erase planning phase so the payload can be streamed later in arbitrary chunks.
 * Returns @ref SL_STATUS_BUSY when another stream session is already
 * active, because relocation would invalidate that session's reservation.
 ******************************************************************************/
static sl_status_t reclaim_and_reserve(const sl_klv_handle_t *klv_handle,
                                       uint32_t *chain_anchor,
                                       const sli_klv_ut_context_t *ut_context,
                                       uint32_t tail,
                                       const sli_klv_ut_obj_info_t *target_match,
                                       sli_klv_ut_stream_reservation_t *reservation)
{
  sli_klv_ut_reclaim_result_t reclaim_result = { 0 };
  sli_klv_ut_context_t registered_context = { 0 };
  sli_klv_ut_scan_result_t rescan_result = { 0 };
  const sli_klv_ut_context_t *reclaim_context = ut_context;
  const sli_klv_ut_obj_info_t *effective_target_match = target_match;
  uint32_t effective_tail = tail;
  uint32_t object_size = 0U;
  uint32_t write_addr = 0U;
  bool activated_extensions = false;
  sl_status_t status = SL_STATUS_OK;

  if ((klv_handle == NULL)
      || (chain_anchor == NULL)
      || (ut_context == NULL)
      || (reservation == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  status = stream_reject_reclaim_with_peer_sessions(klv_handle->key);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = activate_pending_extensions_for_reclaim(ut_context,
                                                   &registered_context,
                                                   &activated_extensions);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (activated_extensions) {
    reclaim_context = &registered_context;
    status = scan_from_anchor(klv_handle->key,
                              *chain_anchor,
                              reclaim_context->klv_start_address,
                              reclaim_context->klv_end_address,
                              &rescan_result);
    if (status != SL_STATUS_OK) {
      return status;
    }

    effective_tail = rescan_result.tail;
    effective_target_match = rescan_result.match_found ? &rescan_result.match : NULL;

    object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                      klv_handle->length);
    status = try_direct_append_after_extension_activation(reclaim_context,
                                                          &rescan_result,
                                                          object_size,
                                                          &write_addr);
    if (status == SL_STATUS_OK) {
      // Expanded geometry already admits a reclaim-safe append; skip the second
      // live-object relocation walk. Stream finish treats this like a normal
      // direct reservation (used_reclaim = false).
      memset(reservation, 0, sizeof(*reservation));
      reservation->write_addr = write_addr;
      reservation->chain_empty = rescan_result.chain_empty;
      reservation->target_delete_pending = rescan_result.match_found;
      reservation->target_delete_was_anchor = rescan_result.match_found
                                              && (rescan_result.match.obj_addr == *chain_anchor);
      reservation->target_delete_addr = rescan_result.match_found
                                        ? rescan_result.match.obj_addr : 0U;
      reservation->used_reclaim = false;

      TOKENDBG(printf("UT post-activation direct reserve: key=0x%04lx write_addr=0x%08lx empty=%u match=%u\r\n",
                      (klv_handle->key & 0xFFFFU),
                      write_addr,
                      rescan_result.chain_empty,
                      rescan_result.match_found); )
      return SL_STATUS_OK;
    }
  }

  status = reclaim_chain(klv_handle,
                         chain_anchor,
                         reclaim_context,
                         effective_tail,
                         effective_target_match,
                         &reclaim_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  *reservation = reclaim_result.reservation;
  reservation->used_reclaim = true;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *  Checks whether the live chain reaches past the logical end of the region.
 *
 *  Activation appends the new extension behind the current logical end, so
 *  offsets at or beyond that end stop wrapping to the base and start addressing
 *  the extension instead. Every byte the chain stores past the end is therefore
 *  re-addressed by the commit, in two distinct ways:
 *
 *  - An object straddling the end loses its tail bytes, which the expanded
 *    layout reads out of the extension rather than the base.
 *  - A link crossing the end orphans the remainder of the chain. An object
 *    ending exactly at the end has its @c next_addr move from the base start to
 *    the extension start, so the walk hits erased flash and every later object
 *    silently disappears.
 *
 *  Measuring the span from the head covers both: the chain is safe only when it
 *  fits between its own head offset and the logical end. A chain ending exactly
 *  at the end is fine, because only its tail moves and the tail addresses free
 *  space in either layout.
 *
 * @param[in]  chain_anchor        Logical head of the chain.
 * @param[in]  tail                Logical tail returned by the scan.
 * @param[in]  chain_full          Whether the scan found the ring full, where
 *                                 the tail meets the head and the span cannot be
 *                                 derived from the two addresses alone.
 * @param[in]  klv_start_address   Start of the UT region.
 * @param[in]  klv_end_address     End of the UT region.
 *
 * @return true when the chain crosses the logical end, or when its extent
 *         cannot be determined.
 ******************************************************************************/
static bool chain_crosses_logical_end(uint32_t chain_anchor,
                                      uint32_t tail,
                                      bool chain_full,
                                      uint32_t klv_start_address,
                                      uint32_t klv_end_address)
{
  uint32_t total_region_size = get_total_region_size(klv_start_address,
                                                     klv_end_address);
  uint32_t anchor_offset = 0U;
  uint32_t span = 0U;

  if ((total_region_size == 0U)
      || !convert_address_to_logical_offset(chain_anchor,
                                            klv_start_address,
                                            klv_end_address,
                                            &anchor_offset)) {
    return true;
  }

  // A full ring puts the tail back on the head, so the distance between them
  // reads as zero and has to be replaced by the whole region.
  span = chain_full
         ? total_region_size
         : cal_distance_forward(chain_anchor,
                                tail,
                                klv_start_address,
                                klv_end_address);

  return ((uint64_t)anchor_offset + (uint64_t)span) > (uint64_t)total_region_size;
}

/***************************************************************************//**
 * @brief
 *  Forces a reclaim/repack pass before a new extension becomes active.
 *  This is used in a scenario where a new extension is added after the
 *  UT chain has already wrapped from the last active extension or within
 *  the base region back into the base allocation. Repacking pulls the chain
 *  back inside the logical end so the new extension can be appended behind it
 *  without re-addressing any stored byte, see @ref chain_crosses_logical_end().
 *
 *  Each pass is a straighten-only reclaim: it relocates the live chain without
 *  reserving flash for an incoming object, since no write follows it here.
 *
 * @param[in]  ut_context  UT context before the new extension is active.
 * @return @ref SL_STATUS_OK if no repack was needed or the repack succeeded,
 *         an error code otherwise.
 ******************************************************************************/
static sl_status_t repack_before_adding_extension(const sli_klv_ut_context_t *ut_context)
{
  sli_klv_ut_scan_result_t scan_result = { 0 };
  sli_klv_ut_reclaim_result_t reclaim_result = { 0 };
  uint32_t total_region_size = 0U;
  uint32_t max_attempts = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL) || (ut_context->chain_anchor == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);
  total_region_size = get_total_region_size(ut_context->klv_start_address,
                                            ut_context->klv_end_address);
  if (total_region_size == 0U) {
    return SL_STATUS_INVALID_STATE;
  }

  // Bound the loop by the smallest possible stored object. Each successful
  // straighten-only reclaim must make progress on the wrap condition, so this
  // is only a safety ceiling against a stuck chain.
  max_attempts = total_region_size
                 / SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                       1U);
  if (max_attempts == 0U) {
    max_attempts = 1U;
  }

  for (uint32_t attempt = 0U; attempt < max_attempts; attempt++) {
    // The scan supplies both the wrap verdict and the tail the repack starts
    // from, so the chain is walked once per attempt instead of twice.
    status = scan_from_anchor(SLI_KLV_UT_FULL_CHAIN_SCAN_TOKEN,
                              *ut_context->chain_anchor,
                              ut_context->klv_start_address,
                              ut_context->klv_end_address,
                              &scan_result);
    if (status != SL_STATUS_OK) {
      return status;
    }

    // An empty chain stores nothing the expanded layout could re-address.
    if (scan_result.chain_empty) {
      return SL_STATUS_OK;
    }

    if (!chain_crosses_logical_end(*ut_context->chain_anchor,
                                   scan_result.tail,
                                   scan_result.chain_full,
                                   ut_context->klv_start_address,
                                   ut_context->klv_end_address)) {
      return SL_STATUS_OK;
    }

    // Straighten-only: relocate the live chain under the old layout without
    // inventing a temporary write. Activation follows only once the chain no
    // longer reaches past the logical end; the caller's real reclaim then
    // continues on the expanded ring.
    status = reclaim_chain(NULL,
                           ut_context->chain_anchor,
                           ut_context,
                           scan_result.tail,
                           NULL,
                           &reclaim_result);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_NO_MORE_RESOURCE;
}

/***************************************************************************//**
 * Activates pending extensions at a reclaim boundary.
 *
 * Existing live objects are first repacked under the old layout so no object
 * still depends on the previous logical-end-to-base wrap. Only then is the
 * registered extension directory promoted and exposed to the reclaim caller.
 ******************************************************************************/
static sl_status_t activate_pending_extensions_for_reclaim(const sli_klv_ut_context_t *ut_context,
                                                           sli_klv_ut_context_t *registered_context,
                                                           bool *activated_out)
{
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL) || (registered_context == NULL)
      || (activated_out == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  *activated_out = false;
  *registered_context = *ut_context;

  if (!ut_context_has_pending_extensions(ut_context)) {
    return SL_STATUS_OK;
  }

  status = repack_before_adding_extension(ut_context);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Validate the expanded layout before the irreversible activation commit.
  // set_active_ut_context() silently falls back to base-only geometry on an
  // invalid snapshot; doing that after commit would leave NVM3 expanded while
  // this session continued on the old size.
  build_registered_ut_context(ut_context, registered_context);
  if (!ut_context_layout_is_valid(registered_context)
      || (registered_context->total_region_size
          == (registered_context->klv_end_address - registered_context->klv_start_address))) {
    return SL_STATUS_INVALID_CONFIGURATION;
  }

  // The old-layout repack above removes dependencies on the previous
  // logical-end-to-base wrap. If power fails before this metadata commit, the
  // extension remains inactive and recovery uses the old layout. If it fails
  // after the commit, the straightened chain is safe to recover with the
  // expanded layout.
  if (ut_context->extension_activation_commit != NULL) {
    status = ut_context->extension_activation_commit(ut_context->extension_activation_context);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  set_active_ut_context(registered_context);
  if (!ut_context_layout_is_valid(&active_ut_context)
      || (active_ut_context.total_region_size != registered_context->total_region_size)) {
    // Activation metadata is already durable; refuse to continue this session on
    // a silently truncated base-only layout.
    return SL_STATUS_INVALID_STATE;
  }

  *activated_out = true;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reclaims space in the UT ring and then writes the requested object.
 *
 * This is the core UT "repack" path. When pending extensions were just
 * activated, it first re-evaluates whether a reclaim-safe direct append fits on
 * the expanded layout; only if that fails does it walk the ring. When direct
 * append would violate the reserve rules, the algorithm walks the ring from the
 * logical head to the current tail, relocating live objects into a page-aligned
 * reserve window and erasing fully reclaimed pages behind it. If @p target_match
 * is supplied, that object is treated as the overwritten/deleted entry and is
 * intentionally not relocated.
 *
 * Important invariants:
 * - A live object is copied and its source soft-deleted before its source page
 *   is eligible for erase. Soft-delete is required because erase may be deferred
 *   while an overwrite target still pins the reclaim erase stop (especially when
 *   that target is the current chain head).
 * - The logical head (`chain_anchor`) is persisted before reclaim soft-deletes or
 *   erases a page that still contains the previous head.
 * - The reserve window is always large enough for the largest live object or
 *   the final write, whichever is bigger.
 *
 * @param[in]      klv_handle        Describes the new object being written.
 * @param[in,out]  chain_anchor      Current logical head of the UT ring.
 * @param[in]      ut_context        Shared UT context supplying region bounds
 *                                   and anchor persistence callback.
 * @param[in]      tail              Current logical tail returned by scanning.
 * @param[in]      target_match      Existing matching object, if this write is
 *                                   replacing an earlier live entry.
 * @param[in]      data              Payload bytes for the final object.
 *
 * @return SL_STATUS_OK if reclaim and the final write succeed,
 *         @ref SL_STATUS_BUSY if another stream session is already
 *         active, or an error if validation, copy, erase, or anchor
 *         persistence fails.
 ******************************************************************************/
static sl_status_t reclaim_and_write(const sl_klv_handle_t *klv_handle,
                                     uint32_t *chain_anchor,
                                     const sli_klv_ut_context_t *ut_context,
                                     uint32_t tail,
                                     const sli_klv_ut_obj_info_t *target_match,
                                     void const *data)
{
  sli_klv_ut_reclaim_result_t reclaim_result = { 0 };
  sli_klv_ut_context_t registered_context = { 0 };
  sli_klv_ut_scan_result_t rescan_result = { 0 };
  const sli_klv_ut_context_t *reclaim_context = ut_context;
  const sli_klv_ut_obj_info_t *effective_target_match = target_match;
  sli_klv_ut_anchor_commit_fn_t anchor_commit = NULL;
  void *anchor_context = NULL;
  sli_klv_obj_header_t obj_hdr;
  sl_status_t status = SL_STATUS_OK;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  uint32_t final_anchor = 0U;
  uint32_t previous_anchor = 0U;
  uint16_t data_crc = 0U;
  uint32_t new_object_size = 0U;
  uint32_t replaced_object_size = 0U;
  uint32_t effective_tail = tail;
  bool activated_extensions = false;
  bool replaced_was_live = false;

  if ((klv_handle == NULL)
      || (chain_anchor == NULL)
      || (ut_context == NULL)
      || (data == NULL)
      || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  status = stream_reject_reclaim_with_peer_sessions(klv_handle->key);
  if (status != SL_STATUS_OK) {
    return status;
  }

  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;
  anchor_commit = ut_context->anchor_commit;
  anchor_context = ut_context->anchor_context;
  new_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                        klv_handle->length);
  if (target_match != NULL) {
    replaced_was_live = true;
    replaced_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(target_match->header.header_size,
                                                               target_match->header.payload_length);
  }

  status = activate_pending_extensions_for_reclaim(ut_context,
                                                   &registered_context,
                                                   &activated_extensions);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (activated_extensions) {
    reclaim_context = &registered_context;
    status = scan_from_anchor(klv_handle->key,
                              *chain_anchor,
                              reclaim_context->klv_start_address,
                              reclaim_context->klv_end_address,
                              &rescan_result);
    if (status != SL_STATUS_OK) {
      return status;
    }

    effective_tail = rescan_result.tail;
    effective_target_match = rescan_result.match_found ? &rescan_result.match : NULL;
    replaced_was_live = rescan_result.match_found;
    replaced_object_size = rescan_result.match_found
                           ? SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(rescan_result.match.header.header_size,
                                                                 rescan_result.match.header.payload_length)
                           : 0U;

    {
      bool completed = false;

      status = try_direct_write_after_extension_activation(klv_handle,
                                                           chain_anchor,
                                                           reclaim_context,
                                                           &rescan_result,
                                                           new_object_size,
                                                           replaced_object_size,
                                                           replaced_was_live,
                                                           data,
                                                           &completed);
      if (completed) {
        return status;
      }
    }
  }

  status = reclaim_chain(klv_handle,
                         chain_anchor,
                         reclaim_context,
                         effective_tail,
                         effective_target_match,
                         &reclaim_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  data_crc = calculate_checksum(data, klv_handle->length);
  SLI_KLV_INIT_HDR_FOR_WRITE(obj_hdr, klv_handle, data_crc);

  status = write_committed_ut_object(reclaim_result.reservation.write_addr,
                                     &obj_hdr,
                                     data,
                                     klv_handle->length,
                                     klv_start_address,
                                     klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (reclaim_result.reservation.target_delete_pending) {
    sli_klv_ut_obj_info_t delete_info = { 0 };

    // Use logical decode so a header that wraps the ring end (or sits on a
    // non-memory-mapped backend) is still classified correctly. A raw physical
    // magic read can see unrelated flash past the logical end.
    status = decode_object_from_anchor(reclaim_result.reservation.target_delete_addr,
                                       klv_start_address,
                                       klv_end_address,
                                       &delete_info);
    if (status == SL_STATUS_OK) {
      if (delete_info.header.is_readable) {
        status = mark_ut_object_deleted(reclaim_result.reservation.target_delete_addr,
                                        klv_start_address,
                                        klv_end_address);
        if (status != SL_STATUS_OK) {
          return status;
        }
      }
    } else if (status != SL_STATUS_NOT_FOUND) {
      return status;
    }
  }

  final_anchor = reclaim_final_anchor(&reclaim_result.reservation, *chain_anchor);

  previous_anchor = *chain_anchor;
  if (*chain_anchor != final_anchor) {
    *chain_anchor = final_anchor;
    status = anchor_commit(*chain_anchor, anchor_context);
    if (status != SL_STATUS_OK) {
      *ut_context->chain_anchor = previous_anchor;
      return status;
    }
  }

  // Final sweep, run only after the anchor is durable so a reset in the middle
  // of it still recovers onto the new chain. Soft stop / hard floor are the
  // post-write tail (write_addr advanced by the stored object size).
  status = reclaim_final_erase_sweep(reclaim_result.reservation.erase_cursor,
                                     reclaim_result.reservation.initial_anchor,
                                     final_anchor,
                                     reclaim_result.reservation.write_addr,
                                     new_object_size,
                                     klv_start_address,
                                     klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // A head overwrite that relocated the whole chain leaves the relocated old
  // value as the head, and it was soft-deleted just above. Advance past it now
  // so the persisted head is a readable object.
  if (reclaim_result.reservation.target_delete_pending
      && (*chain_anchor == reclaim_result.reservation.target_delete_addr)) {
    status = skip_unreadable_chain_head(chain_anchor, reclaim_context);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  status = ut_largest_live_object_cache_on_write_commit(*chain_anchor,
                                                        klv_start_address,
                                                        klv_end_address,
                                                        new_object_size,
                                                        replaced_object_size,
                                                        replaced_was_live);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT reclaim complete: key=0x%04lx new_anchor=0x%08lx final_tail=0x%08lx write_addr=0x%08lx first_relocated=0x%08lx\r\n",
                  (klv_handle->key & 0xFFFFU),
                  *chain_anchor,
                  reclaim_result.reservation.write_addr,
                  reclaim_result.reservation.write_addr,
                  reclaim_result.reservation.first_relocated_addr); )

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Extracts plaintext UT payload bytes from a decoded anchor-based KLV object.
 *
 * UT objects are stored without AEAD metadata, so extraction is simply:
 * validate the stored CRC over the payload and then copy the caller-requested
 * slice.
 ******************************************************************************/
static sl_status_t extract_ut_payload(const sli_klv_hdr_parse_t *header,
                                      uint32_t obj_addr,
                                      uint32_t klv_start_address,
                                      uint32_t klv_end_address,
                                      const sl_klv_handle_t *klv_handle,
                                      void *data)
{
  uint32_t payload_addr = 0U;
  uint32_t read_addr = 0U;
  uint16_t calculated_checksum = 0U;
  sl_status_t status = SL_STATUS_OK;

  payload_addr = advance_ut_adress(obj_addr,
                                   header->header_size,
                                   klv_start_address,
                                   klv_end_address);
  status = calculate_logical_checksum(payload_addr,
                                      header->payload_length,
                                      klv_start_address,
                                      klv_end_address,
                                      &calculated_checksum);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (calculated_checksum != header->payload_crc) {
    TOKENDBG(printf("UT payload CRC check failed, calculated_checksum=0x%lx, header->payload_crc=0x%lx\n",
                    calculated_checksum,
                    header->payload_crc));
    return SL_STATUS_INVALID_SIGNATURE;
  }

  read_addr = advance_ut_adress(payload_addr,
                                klv_handle->offset,
                                klv_start_address,
                                klv_end_address);
  return logical_read(read_addr,
                      data,
                      klv_handle->length,
                      klv_start_address,
                      klv_end_address);
}

/***************************************************************************//**
 * Returns the current health and utilization summary of the UT circular buffer.
 ******************************************************************************/
sl_status_t sli_klv_get_ut_storage_info(const sli_klv_ut_context_t *ut_context,
                                        sl_token_manager_storage_info_t *info_out)
{
  sli_klv_ut_scan_result_t scan_result = { 0 };
  uint32_t chain_anchor = 0U;
  uint32_t total_region_size = 0U;
  uint32_t repack_reserve_size = 0U;
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL) || (ut_context->chain_anchor == NULL) || (info_out == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);

  chain_anchor = *ut_context->chain_anchor;
  total_region_size = get_total_region_size(ut_context->klv_start_address,
                                            ut_context->klv_end_address);

  if (!is_address_in_range(chain_anchor,
                           ut_context->klv_start_address,
                           ut_context->klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = scan_from_anchor(SLI_KLV_UT_FULL_CHAIN_SCAN_TOKEN,
                            chain_anchor,
                            ut_context->klv_start_address,
                            ut_context->klv_end_address,
                            &scan_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  memset(info_out, 0, sizeof(*info_out));

  // Public total_size is registered configured capacity (active + pending).
  // used/reclaimable still come from the active-ring scan; free_size is derived
  // from that same total_size so the reported fields stay consistent.
  info_out->total_size = ut_context->registered_total_region_size;
  if (info_out->total_size < total_region_size) {
    info_out->total_size = total_region_size;
  }
  // used_size is readable live-span bytes only. Subtract live-span reclaimable,
  // not the full reclaimable_bytes (which may include prefix and exceed occupied).
  info_out->used_size = scan_result.occupied_bytes - scan_result.live_span_reclaimable_bytes;
  info_out->reclaimable_size = scan_result.reclaimable_bytes;
  largest_live_object_size = scan_result.largest_live_obj_size;
  if (scan_result.largest_wip_obj_size > largest_live_object_size) {
    // WIP becomes live at finish; keep free_size / cache aligned with the
    // post-write reclaim window writers must preserve.
    largest_live_object_size = scan_result.largest_wip_obj_size;
  }
  largest_live_object_cache_valid = true;
  repack_reserve_size = compute_ut_repack_reserve_size(largest_live_object_size);
  // free_size = total - occupied - reserve, with occupied = used + reclaimable.
  // Guard on occupied + reserve (not used + reserve alone), otherwise a nearly
  // full ring with soft-deleted entries underflows uint32_t free_size.
  if ((scan_result.occupied_bytes + repack_reserve_size) <= info_out->total_size) {
    info_out->free_size = info_out->total_size
                          - scan_result.occupied_bytes
                          - repack_reserve_size;
  } else {
    info_out->free_size = 0U;
  }

  if (scan_result.torn_tail) {
    // The rest of the ring may contain stale or orphaned committed data.
    // Do not advertise writable capacity while repair is unavailable.
    info_out->free_size = 0U;
  }

  TOKENDBG(printf("UT storage info: total=%lu used=%lu free=%lu reclaimable=%lu repack=%lu\r\n",
                  info_out->total_size,
                  info_out->used_size,
                  info_out->free_size,
                  info_out->reclaimable_size,
                  repack_reserve_size); )

  return SL_STATUS_OK;
}

/***************************************************************************//**
* Start a streaming KLV object write for upgradable tokens. Not supported for 
* static tokens.
******************************************************************************/
sl_status_t sli_klv_set_ut_start(const sli_klv_ut_context_t *ut_context,
                                 uint32_t token,
                                 uint32_t total_length)
{
  sli_klv_ut_scan_result_t scan_result = { 0 };
  sli_klv_ut_stream_reservation_t reservation = { 0 };
  sl_klv_handle_t klv_handle = {
    .key = token,
    .offset = 0U,
    .length = total_length,
  };
  sli_stream_context_node_t *ctx = NULL;
  sl_status_t status = SL_STATUS_OK;
  uint32_t object_size = 0U;
  uint32_t repack_start = 0U;
  uint32_t repack_reserve_size = 0U;
  uint32_t repack_guard_bytes = 0U;

  if ((ut_context == NULL) || (ut_context->chain_anchor == NULL) || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);

  if ((total_length == 0U) || (ut_context->klv_start_address == 0U) || (ut_context->klv_end_address == 0U)
      || (ut_context->klv_start_address >= ut_context->klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!is_address_in_range(*ut_context->chain_anchor, ut_context->klv_start_address, ut_context->klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (stream_ctx_find(token) != NULL) {
    return SL_STATUS_IN_PROGRESS;
  }

  // A reclaiming stream still owns deferred erase/anchor/layout state. Starting
  // another writer would race those finish-time side effects.
  if (stream_ctx_any_used_reclaim()) {
    return SL_STATUS_BUSY;
  }

  ctx = stream_ctx_alloc(token);
  if (ctx == NULL) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  // Populate the context with the necessary information.
  ctx->chain_anchor = ut_context->chain_anchor;
  ctx->total_length = total_length;
  ctx->remaining_length = total_length;
  ctx->klv_start_address = ut_context->klv_start_address;
  ctx->klv_end_address = ut_context->klv_end_address;
  memset(ctx->pending_buf, 0xFF, sizeof(ctx->pending_buf));

  // Scan the chain from the anchor to find the first live object.
  status = scan_from_anchor(token,
                            *ut_context->chain_anchor,
                            ut_context->klv_start_address,
                            ut_context->klv_end_address,
                            &scan_result);
  if (status != SL_STATUS_OK) {
    stream_ctx_free(token);
    return status;
  }

  if (scan_result.torn_tail) {
    // The public UT API should have repaired a torn header first through the
    // NVM3 journal. An internal direct KLV caller must still fail closed.
    stream_ctx_free(token);
    return SL_STATUS_INVALID_STATE;
  }

  status = ut_largest_live_object_cache_ensure_valid(*ut_context->chain_anchor,
                                                     ut_context->klv_start_address,
                                                     ut_context->klv_end_address);
  if (status != SL_STATUS_OK) {
    stream_ctx_free(token);
    return status;
  }

  object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                    total_length);

  // Check if the object and its required repack reserve fit within the registered total capacity.
  if (!check_object_fits_registered_capacity(&scan_result,
                                             object_size,
                                             ut_context->registered_total_region_size)) {
    stream_ctx_free(token);
    return SL_STATUS_WOULD_OVERFLOW;
  }

  // Size repack reserve from readable live + ACTIVE+WIP (and the new object).
  // Matching the capacity gate prevents a concurrent stream whose WIP is the
  // largest object from leaving an undersized post-write reclaim window.
  repack_guard_bytes = get_largest_object_size(&scan_result, object_size);
  repack_reserve_size = compute_ut_repack_reserve_size(repack_guard_bytes);

  // Populate the reservation with the necessary information.
  reservation.chain_empty = scan_result.chain_empty;
  reservation.target_delete_pending = scan_result.match_found;
  reservation.target_delete_was_anchor = scan_result.match_found
                                         && (scan_result.match.obj_addr == *ut_context->chain_anchor);
  reservation.target_delete_addr = scan_result.match_found ? scan_result.match.obj_addr : 0U;

  {
    sli_klv_ut_write_plan_t write_plan = {
      .chain_anchor = *ut_context->chain_anchor,
      .tail = scan_result.tail,
      .object_size = object_size,
      .repack_reserve_size = repack_reserve_size,
      .repack_guard_bytes = repack_guard_bytes,
      .chain_empty = scan_result.chain_empty,
      .chain_full = scan_result.chain_full,
      .wrap_start_allowed = scan_result.wrap_start_allowed,
    };

    status = select_write_address(&write_plan,
                                  ut_context,
                                  &reservation.write_addr);
  }

  if ((status == SL_STATUS_OK) && !scan_result.chain_empty) {
    uint32_t future_tail = advance_ut_adress(reservation.write_addr,
                                             object_size,
                                             ut_context->klv_start_address,
                                             ut_context->klv_end_address);
    bool overlap_repack = false;
    bool missing_gap = false;

    repack_start = compute_repack_start(*ut_context->chain_anchor,
                                        scan_result.tail,
                                        repack_reserve_size,
                                        ut_context->klv_start_address,
                                        ut_context->klv_end_address);
    overlap_repack = ((scan_result.reclaimable_bytes > 0U)
                      && check_range_overlaps_repack(scan_result.tail,
                                                     object_size,
                                                     repack_start,
                                                     repack_reserve_size,
                                                     ut_context->klv_start_address,
                                                     ut_context->klv_end_address));
    missing_gap = ((future_tail == *ut_context->chain_anchor)
                   || ((compare_logical_address(future_tail,
                                                *ut_context->chain_anchor,
                                                ut_context->klv_start_address,
                                                ut_context->klv_end_address) < 0)
                       && !try_compute_gap_repack_start(future_tail,
                                                        *ut_context->chain_anchor,
                                                        repack_reserve_size,
                                                        ut_context->klv_start_address,
                                                        ut_context->klv_end_address,
                                                        &repack_start)));
    if (overlap_repack) {
      status = SL_STATUS_NO_MORE_RESOURCE;
    } else if (missing_gap) {
      status = SL_STATUS_NO_MORE_RESOURCE;
    }

  }

  if (status == SL_STATUS_OK) {
    // The stream programs into this window chunk by chunk, so it has to be
    // erased up front. An unusable window falls through to reclaim below.
    status = prepare_append_destination(reservation.write_addr,
                                        object_size,
                                        *ut_context->chain_anchor,
                                        ut_context->klv_start_address,
                                        ut_context->klv_end_address);
  }

  if (status != SL_STATUS_OK) {
    // Reclaim is worthwhile when a match / pending extension justifies the
    // path, or free + live-span reclaimable can cover the new object plus
    // reserve. chain_empty alone must not block this: a late erased head can
    // still have prefix reclaimable bytes that need a reclaim pass.
    if (scan_result.match_found
        || ut_context_has_pending_extensions(ut_context)
        || can_reclaim_write_from_scan(ut_context,
                                       &scan_result,
                                       object_size,
                                       repack_reserve_size)) {
      status = reclaim_and_reserve(&klv_handle,
                                   ut_context->chain_anchor,
                                   ut_context,
                                   scan_result.tail,
                                   scan_result.match_found ? &scan_result.match : NULL,
                                   &reservation);
    }

    if (status != SL_STATUS_OK) {
      stream_ctx_free(token);
      if (status == SL_STATUS_FULL) {
        return SL_STATUS_NO_MORE_RESOURCE;
      }

      return status;
    }
  }

  ctx->reservation.write_addr = reservation.write_addr;
  ctx->payload_addr = advance_ut_adress(reservation.write_addr,
                                        SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                        ut_context->klv_start_address,
                                        ut_context->klv_end_address);
  ctx->next_write_addr = ctx->payload_addr;
  ctx->reservation.erase_cursor = reservation.erase_cursor;
  ctx->reservation.first_relocated_addr = reservation.first_relocated_addr;
  ctx->reservation.target_delete_addr = reservation.target_delete_addr;
  ctx->reservation.initial_anchor = reservation.initial_anchor;
  ctx->reservation.chain_empty = reservation.chain_empty;
  ctx->reservation.used_reclaim = reservation.used_reclaim;
  ctx->reservation.target_delete_pending = reservation.target_delete_pending;
  ctx->reservation.target_delete_was_anchor = reservation.target_delete_was_anchor;

  // Capture the layout used for this reservation (may already include extensions
  // activated inside reclaim_and_reserve) so update/finish can restore it.
  stream_stamp_active_layout(ctx);

  status = stream_write_provisional_header(ctx);
  if (status != SL_STATUS_OK) {
    stream_ctx_free(token);
    return status;
  }

  ctx->is_active = true;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Update the streaming write with the next chunk of data.
 *
 * Does not call @ref stream_ctx_any_used_reclaim: this path belongs to the
 * session that already owns the reservation (including a reclaiming one).
 * Concurrent writers are rejected at @ref sli_klv_set_ut_start and
 * @ref sli_write_klv_object_from_anchor until this stream finishes.
 ******************************************************************************/
sl_status_t sli_klv_set_ut_update(uint32_t token,
                                  const void *data,
                                  uint32_t length)
{
  if (data == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (length == 0U) {
    TOKENDBG(printf("Stream write chunk length is 0\n"));
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_stream_context_node_t *context = stream_ctx_find(token);
  if (context == NULL) {
    TOKENDBG(printf("Stream write not found\n"));
    return SL_STATUS_NOT_FOUND;
  }

  if (!context->is_active) {
    TOKENDBG(printf("Stream write not active\n"));
    return SL_STATUS_NOT_READY;
  }

  if (length > context->remaining_length) {
    TOKENDBG(printf("Stream write chunk exceeds total_length\n"));
    return SL_STATUS_WOULD_OVERFLOW;
  }

  // Restore the layout stamped at start. An intervening public UT API may have
  // rebuilt active_ut_context from a pre-activation caller snapshot.
  stream_apply_stamped_layout(context);

  return stream_append_payload(context, (const uint8_t *)data, length);
}

/***************************************************************************//**
 * Finalize a streaming KLV object write for upgradable tokens. The start path
 * already wrote a valid header CRC, so finish() only commits crc_data and
 * clears the WIP bit to make the object readable.
 *
 * Does not call @ref stream_ctx_any_used_reclaim: finish must run for the
 * reclaiming stream itself to commit the anchor and deferred erase. Blocking
 * here would leave that session stuck. Other writers stay busy at start /
 * one-shot write until this context is freed.
 ******************************************************************************/
sl_status_t sli_klv_set_ut_finish(const sli_klv_ut_context_t *ut_context, uint32_t token)
{
  sli_stream_context_node_t *context = stream_ctx_find(token);
  sl_status_t status = SL_STATUS_OK;
  uint16_t data_crc = 0U;
  uint32_t previous_anchor = 0U;
  uint32_t final_anchor = 0U;
  uint32_t new_object_size = 0U;
  uint32_t replaced_object_size = 0U;
  bool replaced_was_live = false;
  uint32_t deleted_magic = SLI_TOKEN_KLV_MAGIC_KLV_DELETED;
  sli_klv_ut_obj_info_t replaced_info = { 0 };

  if ((ut_context == NULL) || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if (context == NULL) {
    TOKENDBG(printf("Stream write not found\n"));
    return SL_STATUS_NOT_FOUND;
  }

  if (!context->is_active) {
    TOKENDBG(printf("Stream write not active\n"));
    return SL_STATUS_NOT_READY;
  }

  // Prefer the layout stamped at start over a stale caller snapshot. Activation
  // during reclaim_and_reserve expands total_region_size; finish must checksum
  // and map addresses with that same logical ring.
  if ((ut_context->klv_start_address != context->klv_start_address)
      || (ut_context->klv_end_address != context->klv_end_address)
      || (ut_context->total_region_size != context->total_region_size)) {
    stream_apply_stamped_layout(context);
  } else {
    set_active_ut_context(ut_context);
    if (active_ut_context.total_region_size != context->total_region_size) {
      stream_apply_stamped_layout(context);
    }
  }

  if (context->bytes_written != context->total_length) {
    TOKENDBG(printf("Stream write incomplete: %lu of %lu bytes\n",
                    (unsigned long)context->bytes_written,
                    (unsigned long)context->total_length));
    status = SL_STATUS_INVALID_STATE;
    goto cleanup;
  }

  status = stream_flush_pending_word(context);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

  status = calculate_logical_checksum(context->payload_addr,
                                      context->total_length,
                                      context->klv_start_address,
                                      context->klv_end_address,
                                      &data_crc);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

  status = stream_commit_header(context, data_crc);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

  if (context->reservation.target_delete_pending) {
    status = decode_object_from_anchor(context->reservation.target_delete_addr,
                                       context->klv_start_address,
                                       context->klv_end_address,
                                       &replaced_info);
    if (status != SL_STATUS_OK) {
      goto cleanup;
    }

    if (replaced_info.header.is_readable) {
      replaced_was_live = true;
      replaced_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(replaced_info.header.header_size,
                                                                 replaced_info.header.payload_length);
    }

    status = logical_write(context->reservation.target_delete_addr,
                           &deleted_magic,
                           sizeof(deleted_magic),
                           context->klv_start_address,
                           context->klv_end_address);
    if (status != SL_STATUS_OK) {
      goto cleanup;
    }
  }

  if (context->reservation.used_reclaim) {
    final_anchor = reclaim_final_anchor(&context->reservation, *context->chain_anchor);

    previous_anchor = *context->chain_anchor;
    if (*context->chain_anchor != final_anchor) {
      *context->chain_anchor = final_anchor;
      status = ut_context->anchor_commit(*context->chain_anchor, ut_context->anchor_context);
      if (status != SL_STATUS_OK) {
        *context->chain_anchor = previous_anchor;
        goto cleanup;
      }
    }

    // Deferred until after the anchor commit for the same reason as the direct
    // write path: the old pages have to stay recoverable until the new chain
    // head is durable. Soft stop / hard floor are the post-write tail.
    new_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                          context->total_length);
    status = reclaim_final_erase_sweep(context->reservation.erase_cursor,
                                       context->reservation.initial_anchor,
                                       final_anchor,
                                       context->reservation.write_addr,
                                       new_object_size,
                                       context->klv_start_address,
                                       context->klv_end_address);
    if (status != SL_STATUS_OK) {
      goto cleanup;
    }

    // Same as reclaim_and_write: a relocated old value that became the head
    // was soft-deleted above, move the persisted head onto a readable object.
    if (context->reservation.target_delete_pending
        && (*context->chain_anchor == context->reservation.target_delete_addr)) {
      status = skip_unreadable_chain_head(context->chain_anchor, ut_context);
      if (status != SL_STATUS_OK) {
        goto cleanup;
      }
    }
  } else if (context->reservation.chain_empty) {
    previous_anchor = *context->chain_anchor;
    *context->chain_anchor = context->reservation.write_addr;
    status = ut_context->anchor_commit(*context->chain_anchor, ut_context->anchor_context);
    if (status != SL_STATUS_OK) {
      *context->chain_anchor = previous_anchor;
      goto cleanup;
    }
  } else if (context->reservation.target_delete_was_anchor) {
    // skip_unreadable_chain_head advances past soft-deleted heads only and
    // persists the new head (WIP heads are kept in the live span).
    status = skip_unreadable_chain_head(context->chain_anchor, ut_context);
    if (status != SL_STATUS_OK) {
      goto cleanup;
    }
  }

  new_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                        context->total_length);
  status = ut_largest_live_object_cache_on_write_commit(*context->chain_anchor,
                                                        context->klv_start_address,
                                                        context->klv_end_address,
                                                        new_object_size,
                                                        replaced_object_size,
                                                        replaced_was_live);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

cleanup:
  stream_ctx_free(token);
  return status;
}

/*
 * Keep GPCRC initialized for the WHOLE CRC message. Restarting GPCRC with
 * the previous DATA register value is not necessarily equivalent to feeding
 * consecutive bytes.
 *
 * Feeders below only operate while the caller holds the common token-manager
 * serialization. They do not need a full-page RAM buffer.
 */
static void ut_repair_crc16_begin(void)
{
#if defined(_CMU_CLKEN0_MASK)
  CMU->CLKEN0_SET = GPCRC_CLKEN;
#endif
  GPCRC_INSTANCE->EN = GPCRC_EN_EN;
  GPCRC_INSTANCE->CTRL = CRC16_POLY;
  GPCRC_INSTANCE->INIT = INIT_VALUE_CRC16;
  GPCRC_INSTANCE->CMD = GPCRC_CMD_INIT;
}

static void ut_repair_crc16_feed(const uint8_t *data, uint32_t len)
{
  while (len-- > 0U) {
    GPCRC_INSTANCE->INPUTDATABYTE = *data++;
  }
}

static uint16_t ut_repair_crc16_finish(void)
{
  return (uint16_t)GPCRC_INSTANCE->DATA;
}

/***************************************************************************//**
 * Power-fail-safe repair of an undecodable FINAL appended UT header.
 *
 * This facility does not handle valid WIP headers: they retain their normal
 * length/key/header-CRC and the existing WIP recovery path handles them.
 *
 * Only persistent state is ONE atomically replaced NVM3 item. If a source
 * page contains earlier chain bytes, we borrow ONE physical erase page from
 * the rotating UT repack window. We save/verify it BEFORE erasing the source.
 *
 * Recovery completes synchronously during UT initialization, before normal
 * KLV scans and before reads are served. No read-through overlay is required.
 *
 * For non-IDLE records, bind the checksum to BOTH the current persisted
 * anchor and the logical extension directory. This replaces separate NVM3
 * anchor/layout snapshot fields, and rejects destructive replay if either
 * changes before the repair completes. Extension addresses may be unordered.
 ******************************************************************************/
static uint16_t ut_repair_record_crc(const sli_klv_ut_context_t *ctx,
                                     const sli_klv_ut_repair_record_t *rec)
{
  uint32_t anchor = *ctx->chain_anchor;
  ut_repair_crc16_begin();
  ut_repair_crc16_feed((const uint8_t *)rec,
                       offsetof(sli_klv_ut_repair_record_t, record_crc16));
  ut_repair_crc16_feed((const uint8_t *)&anchor, sizeof(anchor));
  ut_repair_crc16_feed((const uint8_t *)&ctx->klv_start_address,
                       sizeof(ctx->klv_start_address));
  ut_repair_crc16_feed((const uint8_t *)&ctx->klv_end_address,
                       sizeof(ctx->klv_end_address));
  ut_repair_crc16_feed((const uint8_t *)&ctx->total_region_size,
                       sizeof(ctx->total_region_size));
  ut_repair_crc16_feed((const uint8_t *)&ctx->extension_info_count,
                       sizeof(ctx->extension_info_count));
  for (uint32_t i = 0U; i < ctx->extension_info_count; i++) {
    const sli_klv_extension_header_t *ext = &ctx->extension_info[i];
    ut_repair_crc16_feed((const uint8_t *)&ext->extension_addr,
                         sizeof(ext->extension_addr));
    ut_repair_crc16_feed((const uint8_t *)&ext->extension_size,
                         sizeof(ext->extension_size));
    ut_repair_crc16_feed((const uint8_t *)&ext->is_extension_active,
                         sizeof(ext->is_extension_active));
  }
  return ut_repair_crc16_finish();
}

/***************************************************************************//**
 * Calculates the CRC16 value of the data.
 *
 * @note The CRC16-CCITT is used.
 *
 * @param[in]  data  Pointer to data for which CRC needs to be calculated.
 * @param[in]  len   Length of the data.
 *
 * @return a valid CRC16 value. zero otherwise.
 ******************************************************************************/
static sl_status_t ut_repair_crc16_flash(uint32_t physical_addr,
                                         uint32_t length,
                                         const sli_klv_ut_context_t *ctx,
                                         uint16_t *crc_out)
{
  uint8_t buf[128];
  sl_status_t status;
  if (crc_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  ut_repair_crc16_begin();
  for (uint32_t offset = 0U; offset < length;) {
    uint32_t bytes = length - offset;
    if (bytes > sizeof(buf)) {
      bytes = sizeof(buf);
    }
    // All chunks belong to ONE CRC message, regardless of page size.
    status = logical_read(physical_addr + offset, buf, bytes,
                          ctx->klv_start_address, ctx->klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }
    ut_repair_crc16_feed(buf, bytes);
    offset += bytes;
  }
  *crc_out = ut_repair_crc16_finish();
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Publishes the repair record.
 *
 * @param[in]  ctx  The context of the UT.
 * @param[in]  rec  The repair record to publish.
 * @param[in]  new_phase  The new phase of the repair record.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
static sl_status_t ut_repair_record_publish(const sli_klv_ut_context_t *ctx,
                                            sli_klv_ut_repair_record_t *rec,
                                            uint16_t new_phase)
{
  if (new_phase == SLI_KLV_UT_REPAIR_IDLE) {
    // Canonical idle record must remain valid when the anchor/extension
    // directory changes after repair. Never bind IDLE to the old layout.
    memset(rec, 0, sizeof(*rec));
    rec->record_crc16 = sli_klv_compute_crc16((const uint8_t *)rec,
                                               offsetof(sli_klv_ut_repair_record_t,
                                                        record_crc16));
  } else {
    rec->phase = new_phase;
    rec->record_crc16 = ut_repair_record_crc(ctx, rec);
  }
  return ctx->repair_store(rec, ctx->repair_context);
}

static bool ut_repair_page_in_layout(uint32_t page,
                                     const sli_klv_ut_context_t *ctx)
{
  return ((page % FLASH_PAGE_SIZE) == 0U)
         && is_address_in_range(page, ctx->klv_start_address, ctx->klv_end_address)
         && (cal_bytes_until_segment_end(page,
                                         ctx->klv_start_address,
                                         ctx->klv_end_address) >= FLASH_PAGE_SIZE);
}

/***************************************************************************//**
 * Returns whether the source is good.
 *
 * @note Source is considered restored only if all bytes below the cut match the
 * original prefix checksum and the remaining bytes are completely erased.
 * This allows replay to recognize a completed restore after a crash in NVM3 
 * or cleanup.
 *
 * @param[in]  rec  The repair record.
 * @param[in]  ctx  The context of the UT.
 * @param[out] good  Whether the source is good.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
static sl_status_t ut_repair_source_good(const sli_klv_ut_repair_record_t *rec,
                                         const sli_klv_ut_context_t *ctx,
                                         bool *good)
{
  uint16_t crc = 0U;
  sl_status_t status;
  *good = false;
  status = ut_repair_crc16_flash(rec->source_page, rec->prefix_bytes, ctx, &crc);
  if (status != SL_STATUS_OK) {
    return status;
  }
  *good = (crc == rec->prefix_crc16)
          && is_logical_range_blank(rec->source_page + rec->prefix_bytes,
                                    FLASH_PAGE_SIZE - rec->prefix_bytes,
                                    ctx->klv_start_address, ctx->klv_end_address);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns whether the backup is good.
 *
 * @note Backup is considered good only if all bytes match the original source
 * checksum. This allows replay to recognize a completed restore after a crash
 * in NVM3 or cleanup.
 *
 * @param[in]  rec  The repair record.
 * @param[in]  ctx  The context of the UT.
 * @param[out] good  Whether the backup is good.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
static sl_status_t ut_repair_backup_good(const sli_klv_ut_repair_record_t *rec,
                                         const sli_klv_ut_context_t *ctx,
                                         bool *good)
{
  uint16_t crc = 0U;
  sl_status_t status;
  *good = false;
  status = ut_repair_crc16_flash(rec->scratch_page, FLASH_PAGE_SIZE, ctx, &crc);
  if (status != SL_STATUS_OK) {
    return status;
  }
  *good = (crc == rec->source_crc16);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Copies the physical page.
 *
 * @param[in]  to_page  The page to copy to.
 * @param[in]  from_page  The page to copy from.
 * @param[in]  bytes  The number of bytes to copy.
 * @param[in]  ctx  The context of the UT.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
static sl_status_t ut_repair_copy_physical(uint32_t to_page,
                                           uint32_t from_page,
                                           uint32_t bytes,
                                           const sli_klv_ut_context_t *ctx)
{
  uint32_t buffer[32];
  sl_status_t status;
  for (uint32_t offset = 0U; offset < bytes;) {
    uint32_t length = bytes - offset;
    if (length > sizeof(buffer)) {
      length = sizeof(buffer);
    }
    status = logical_read(from_page + offset, buffer, length,
                          ctx->klv_start_address, ctx->klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }
    status = logical_write(to_page + offset, buffer, length,
                           ctx->klv_start_address, ctx->klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }
    offset += length;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Replays the repair record.
 *
 * @param[in]  ctx  The context of the UT.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
sl_status_t sli_klv_replay_ut_page_repair(const sli_klv_ut_context_t *ctx)
{
  sli_klv_ut_repair_record_t rec = { 0 };
  sl_status_t status;
  bool good = false;

  if ((ctx == NULL) || (ctx->chain_anchor == NULL)
      || (ctx->repair_load == NULL) || (ctx->repair_store == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  set_active_ut_context(ctx);
  status = ctx->repair_load(&rec, ctx->repair_context);
  if (status == SL_STATUS_NOT_FOUND) {
    return SL_STATUS_OK;  // No journal exists on a freshly initialized UT.
  }
  if (status != SL_STATUS_OK) {
    return status;
  }
  if (rec.phase == SLI_KLV_UT_REPAIR_IDLE) {
    // Reject a damaged/corrupted idle record, but do not bind an already
    // completed repair to its former anchor or extension mapping.
    if ((rec.source_page != 0U) || (rec.scratch_page != 0U)
        || (rec.prefix_bytes != 0U) || (rec.source_crc16 != 0U)
        || (rec.prefix_crc16 != 0U)
        || (rec.record_crc16 != sli_klv_compute_crc16((const uint8_t *)&rec,
                                  offsetof(sli_klv_ut_repair_record_t,
                                           record_crc16)))) {
      return SL_STATUS_INVALID_SIGNATURE;
    }
    return SL_STATUS_OK;
  }
  if (rec.phase > SLI_KLV_UT_REPAIR_DONE) {
#if defined(SL_TOKEN_MANAGER_TEST)
    TOKENDBG(printf("UT_REPAIR_VERIFY invalid_phase=%u\n", (unsigned int)rec.phase));
#endif
    return SL_STATUS_INVALID_SIGNATURE;
  }
  {
    uint16_t computed_crc = ut_repair_record_crc(ctx, &rec);
    if (computed_crc != rec.record_crc16) {
#if defined(SL_TOKEN_MANAGER_TEST)
      TOKENDBG(printf("UT_REPAIR_VERIFY journal_crc phase=%u stored=0x%04x "
             "computed=0x%04x anchor=0x%08lx total=%lu extensions=%lu\n",
             (unsigned int)rec.phase, (unsigned int)rec.record_crc16,
             (unsigned int)computed_crc, (unsigned long)*ctx->chain_anchor,
             (unsigned long)ctx->total_region_size,
             (unsigned long)ctx->extension_info_count));
#endif
      return SL_STATUS_INVALID_SIGNATURE;
    }
  }
  if ((!ut_repair_page_in_layout(rec.source_page, ctx))
      || (rec.prefix_bytes >= FLASH_PAGE_SIZE)
      || ((rec.prefix_bytes % sizeof(uint32_t)) != 0U)
      || ((rec.prefix_bytes == 0U) && (rec.scratch_page != 0U))
      || (rec.prefix_bytes > 0U
          && (!ut_repair_page_in_layout(rec.scratch_page, ctx)
              || (rec.scratch_page == rec.source_page)))) {
    return SL_STATUS_INVALID_STATE; // Never erase an unvalidated flash address.
  }

  if (rec.phase == SLI_KLV_UT_REPAIR_COPY) {
    // This state is durable BEFORE any scratch mutation. The original page
    // must remain byte-for-byte intact until the backup is durably armed.
    uint16_t actual_source_crc = 0U;
    status = ut_repair_crc16_flash(rec.source_page, FLASH_PAGE_SIZE,
                                   ctx, &actual_source_crc);
    if (status != SL_STATUS_OK) {
      return status;
    }
    if (actual_source_crc != rec.source_crc16) {
#if defined(SL_TOKEN_MANAGER_TEST)
    TOKENDBG(printf("UT_REPAIR_VERIFY source_crc phase=COPY expected=0x%04x "
             "actual=0x%04x source=0x%08lx\n",
             (unsigned int)rec.source_crc16, (unsigned int)actual_source_crc,
             (unsigned long)rec.source_page));
#endif
      return SL_STATUS_INVALID_SIGNATURE;
    }
    if (rec.prefix_bytes != 0U) {
      status = sli_token_manager_nvm_page_erase(rec.scratch_page);
      if (status != SL_STATUS_OK) {
        return status;
      }
      status = ut_repair_copy_physical(rec.scratch_page, rec.source_page,
                                       FLASH_PAGE_SIZE, ctx);
      if (status != SL_STATUS_OK) {
        return status;
      }
      status = ut_repair_backup_good(&rec, ctx, &good);
      if ((status != SL_STATUS_OK) || !good) {
#if defined(SL_TOKEN_MANAGER_TEST)
      TOKENDBG(printf("UT_REPAIR_VERIFY COPY backup status=0x%08lx good=%u\n",
               (unsigned long)status, (unsigned int)good));
#endif
        return (status != SL_STATUS_OK) ? status : SL_STATUS_INVALID_SIGNATURE;
      }
      if (!logical_ranges_equal(rec.source_page, rec.scratch_page,
                                FLASH_PAGE_SIZE, ctx->klv_start_address,
                                ctx->klv_end_address)) {
        return SL_STATUS_INVALID_SIGNATURE;
      }
    }
    status = ut_repair_record_publish(ctx, &rec, SLI_KLV_UT_REPAIR_ARMED);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  if (rec.phase == SLI_KLV_UT_REPAIR_ARMED) {
    status = ut_repair_source_good(&rec, ctx, &good);
    if (status != SL_STATUS_OK) {
      return status;
    }
    if (!good) {
      if (rec.prefix_bytes > 0U) {
        status = ut_repair_backup_good(&rec, ctx, &good);
        if ((status != SL_STATUS_OK) || !good) {
          return (status != SL_STATUS_OK) ? status : SL_STATUS_INVALID_SIGNATURE;
        }
      }
      // This erase is legal ONLY after the backup has been verified and its
      // ownership durably published, or when the page has no live prefix.
      status = sli_token_manager_nvm_page_erase(rec.source_page);
      if (status != SL_STATUS_OK) {
        return status;
      }
      if (rec.prefix_bytes > 0U) {
        status = ut_repair_copy_physical(rec.source_page, rec.scratch_page,
                                         rec.prefix_bytes, ctx);
        if (status != SL_STATUS_OK) {
          return status;
        }
      }
      status = ut_repair_source_good(&rec, ctx, &good);
      if ((status != SL_STATUS_OK) || !good) {
        return (status != SL_STATUS_OK) ? status : SL_STATUS_INVALID_SIGNATURE;
      }
    }
    status = ut_repair_record_publish(ctx, &rec, SLI_KLV_UT_REPAIR_DONE);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  if (rec.phase == SLI_KLV_UT_REPAIR_DONE) {
    status = ut_repair_source_good(&rec, ctx, &good);
    if ((status != SL_STATUS_OK) || !good) {
      return (status != SL_STATUS_OK) ? status : SL_STATUS_INVALID_SIGNATURE;
    }
    // Cleanup FIRST, NVM3 IDLE LAST. DONE safely survives repeated partial
    // scratch erases; the verified source no longer depends on the backup.
    if (rec.prefix_bytes > 0U) {
      status = sli_token_manager_nvm_page_erase(rec.scratch_page);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }
    return ut_repair_record_publish(ctx, &rec, SLI_KLV_UT_REPAIR_IDLE);
  }
  return SL_STATUS_INVALID_STATE;
}

/***************************************************************************//**
 * Selects the scratch page.
 *
 * @param[in]  ctx  The context of the UT.
 * @param[in]  scan  The scan result.
 * @param[in]  source_page  The source page.
 * @param[out] selected  The selected scratch page.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
static sl_status_t ut_repair_select_scratch(const sli_klv_ut_context_t *ctx,
                                            const sli_klv_ut_scan_result_t *scan,
                                            uint32_t source_page,
                                            uint32_t *selected)
{
  uint32_t total = get_total_region_size(ctx->klv_start_address,
                                          ctx->klv_end_address);
  uint32_t guard = get_largest_object_size(scan,
                                            SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE);
  uint32_t reserve = compute_ut_repack_reserve_size(guard);
  uint32_t reserve_start = 0U;
  uint32_t free_gap = 0U;
  uint32_t anchor = *ctx->chain_anchor;
  uint32_t tail = scan->tail;
  uint32_t offset;

  if ((reserve < FLASH_PAGE_SIZE) || (reserve >= total)) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }
  reserve_start = compute_repack_start(anchor, tail, reserve,
                                        ctx->klv_start_address, ctx->klv_end_address);
  free_gap = cal_distance_forward(tail, anchor,
                                   ctx->klv_start_address, ctx->klv_end_address);
  // compute_repack_start() deliberately falls back to anchor on a wrapped
  // gap that cannot accommodate the reserve. Never interpret that fallback
  // as a real reserve window when authorizing a destructive scratch erase.
  if ((free_gap < reserve)
      || (cal_distance_forward(reserve_start, anchor,
                               ctx->klv_start_address,
                               ctx->klv_end_address) != reserve)) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }
  // A scratch page is safe only when its ENTIRE physical page is inside the
  // current free gap AND inside the moving reserve window. Just being in the
  // nominal reserve is not enough (e.g. a page shared with the live anchor).
  for (offset = 0U; offset + FLASH_PAGE_SIZE <= total; offset += FLASH_PAGE_SIZE) {
    uint32_t page = 0U;
    uint32_t to_page;
    uint32_t from_tail;
    if (!convert_logical_offset_to_address(offset,
                                            ctx->klv_start_address,
                                            ctx->klv_end_address, &page)
        || !ut_repair_page_in_layout(page, ctx)
        || (page == source_page)) {
      continue;
    }
    to_page = cal_distance_forward(reserve_start, page,
                                    ctx->klv_start_address, ctx->klv_end_address);
    from_tail = cal_distance_forward(tail, page,
                                      ctx->klv_start_address, ctx->klv_end_address);
    if ((to_page + FLASH_PAGE_SIZE <= reserve)
        && (from_tail + FLASH_PAGE_SIZE <= free_gap)) {
      *selected = page;
      return SL_STATUS_OK;
    }
  }
  return SL_STATUS_NO_MORE_RESOURCE;
}

/***************************************************************************//**
 * Heals the torn tail.
 *
 * @note A torn header is a write-in-progress that lost power before completing.
 * The physical page containing the header must be restored to the original
 * state and the header canceled to release the slot for a new write.
 *
 * The restored source must be durably verified before the header is canceled,
 * or else the new write would overwrite the restored source and corrupt it.
 *
 * @param[in]  ctx  The context of the UT.
 *
 * @return a valid status code. zero otherwise.
 ******************************************************************************/
sl_status_t sli_klv_heal_ut_torn_tail(const sli_klv_ut_context_t *ctx)
{
  sli_klv_ut_scan_result_t scan = { 0 };
  sli_klv_ut_repair_record_t rec = { 0 };
  sl_status_t status;
  uint32_t torn_page;

  if ((ctx == NULL) || (ctx->chain_anchor == NULL)
      || (ctx->repair_load == NULL) || (ctx->repair_store == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  set_active_ut_context(ctx);
  status = sli_klv_replay_ut_page_repair(ctx);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = scan_from_anchor(SLI_KLV_UT_FULL_CHAIN_SCAN_TOKEN,
                            *ctx->chain_anchor, ctx->klv_start_address,
                            ctx->klv_end_address, &scan);
  if (status != SL_STATUS_OK) {
    return status;
  }
  if (!scan.torn_tail) {
    sli_klv_ut_tail_record_t pending = { 0 };
    sli_klv_ut_obj_info_t header = { 0 };
    status = ut_tail_read(&pending);
    if (status == SL_STATUS_OK
        && pending.state == SLI_KLV_UT_TAIL_HEADER_PENDING
        && pending.stable_tail == *ctx->chain_anchor
        && pending.pending_header_addr != pending.stable_tail) {
      status = decode_object_from_anchor(pending.pending_header_addr,
                                          ctx->klv_start_address,
                                          ctx->klv_end_address, &header);
      if (status == SL_STATUS_NOT_FOUND) {
        bool programmed = false;
        status = ut_pending_header_is_programmed(pending.pending_header_addr,
                                                  ctx->klv_start_address,
                                                  ctx->klv_end_address,
                                                  &programmed);
        if (status != SL_STATUS_OK) {
          return status;
        }
        if (programmed) {
          scan.torn_tail = true;
          scan.tail = pending.pending_header_addr;
          scan.chain_empty = true;
        }
      }
    }
    if (!scan.torn_tail) {
      return (status == SL_STATUS_OK || status == SL_STATUS_NOT_FOUND)
             ? SL_STATUS_OK : status;
    }
  }
  // A live stream spans multiple public API calls. Only block when a real
  // torn header was found; ordinary concurrent streaming must remain legal.
  if (sli_stream_ctx_head != NULL) {
    return SL_STATUS_BUSY;
  }
  if ((scan.tail % sizeof(uint32_t)) != 0U) {
    return SL_STATUS_INVALID_STATE;
  }
  torn_page = SLI_KLV_ANCHOR_PAGE_START(scan.tail);
  if (!ut_repair_page_in_layout(torn_page, ctx)) {
    return SL_STATUS_INVALID_STATE;
  }
  rec.source_page = torn_page;
  // No torn address is needed for replay: only the preserved prefix cutoff
  // matters. An empty chain permits erasing the complete physical page.
  rec.prefix_bytes = scan.chain_empty ? 0U : (scan.tail - torn_page);
  if (rec.prefix_bytes > 0U) {
    status = ut_repair_select_scratch(ctx, &scan, torn_page, &rec.scratch_page);
    if (status != SL_STATUS_OK) {
      return status;  // Leave source and valid tokens entirely untouched.
    }
  }
  status = ut_repair_crc16_flash(rec.source_page, FLASH_PAGE_SIZE,
                                 ctx, &rec.source_crc16);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = ut_repair_crc16_flash(rec.source_page, rec.prefix_bytes,
                                 ctx, &rec.prefix_crc16);
  if (status != SL_STATUS_OK) {
    return status;
  }
  // One atomic persistent record BEFORE the first scratch or source erase.
  status = ut_repair_record_publish(ctx, &rec, SLI_KLV_UT_REPAIR_COPY);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = sli_klv_replay_ut_page_repair(ctx);
  if (status == SL_STATUS_OK) {
    // Only after the source page is restored and verified may the reserved
    // header be canceled in the same atomic NVM3 tail record.
    status = ut_tail_cancel_torn_header(ctx->klv_start_address,
                                         ctx->klv_end_address);
  }
  if (status == SL_STATUS_OK) {
    status = sli_klv_recover_ut_chain_anchor(ctx);
  }
  return status;
}

/***************************************************************************//**
 * Completes any interrupted UT reclaim by reconciling the persisted head object
 * with relocated copies that may already exist elsewhere in the ring.
 *
 * Reclaim is intentionally crash tolerant: a live head object is copied to a
 * new location first, then the logical chain anchor is advanced, and only after
 * that is the old head soft-deleted and old pages erased. If power is lost
 * between those steps, the old head remains readable and a relocated duplicate
 * may also be present. This routine detects that situation and finishes the
 * anchor move so later reads and writes resume from a single consistent head.
 *
 * Mid-chain relocations soft-delete their sources immediately after a validated
 * copy, and overwrites soft-delete the previous generation immediately after
 * the replacement is committed. If power is lost in either window, two
 * readable copies of one key remain; @ref retire_superseded_objects_in_ut_chain
 * keeps the newest and retires the earlier one so a later delete cannot expose
 * stale data and storage accounting stays correct.
 *
 * @param[in]     ut_context        Shared UT context carrying the persisted
 *                                  logical head, region bounds, and anchor
 *                                  persistence callback.
 *
 * @return SL_STATUS_OK if the chain is already consistent or recovery succeeds;
 *         otherwise an error describing the first failed validation/update step.
 ******************************************************************************/
sl_status_t sli_klv_recover_ut_chain_anchor(const sli_klv_ut_context_t *ut_context)
{
  uint32_t *chain_anchor = NULL;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  sli_klv_ut_obj_info_t head_info = { 0 };
  sli_klv_ut_obj_info_t pending_info = { 0 };
  uint32_t head_magic = 0U;
  uint32_t tail = 0U;
  uint32_t pending_addr = 0U;
  uint32_t next_anchor = 0U;
  uint32_t previous_anchor = 0U;
  bool progress = false;
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL)
      || (ut_context->chain_anchor == NULL)
      || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);

  chain_anchor = ut_context->chain_anchor;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;

  if (!is_address_in_range(*chain_anchor, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_klv_ut_largest_live_object_cache_invalidate();
  stream_ctx_free_all();

  // A torn append header is left untouched during routine init recovery.
  // The next mutating UT API repairs its page using the NVM3 journal and
  // validated rotating repack window. Boot still replays any interrupted
  // repair BEFORE reaching this function. Valid preceding objects stay readable.
  {
    sli_klv_ut_scan_result_t safety_scan = { 0 };
    status = scan_from_anchor(SLI_KLV_UT_FULL_CHAIN_SCAN_TOKEN,
                              *chain_anchor,
                              klv_start_address,
                              klv_end_address,
                              &safety_scan);
    if (status != SL_STATUS_OK) {
      return status;
    }
    if (safety_scan.torn_tail) {
      return ut_refresh_largest_live_object_cache(*chain_anchor,
                                                  klv_start_address,
                                                  klv_end_address);
    }
  }

  do {
    progress = false;

    // First decode the persisted anchor slot, even if it is currently deleted
    // or still marked write-in-progress. Recovery may need to either repair the
    // object in place or advance past it to the next live entry.
    if (ut_tail_is_frontier(*chain_anchor, klv_start_address,
                            klv_end_address)) {
      break;
    }
    status = decode_object_from_anchor(*chain_anchor,
                                       klv_start_address,
                                       klv_end_address,
                                       &head_info);
    if (status == SL_STATUS_NOT_FOUND) {
      break;
    } else if (status != SL_STATUS_OK) {
      return status;
    }

    status = logical_read(*chain_anchor,
                          &head_magic,
                          sizeof(head_magic),
                          klv_start_address,
                          klv_end_address);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if ((head_magic == SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE)
        && !head_info.header.is_readable) {
      // Only clear WIP when the stored payload CRC is valid. Incomplete stream
      // sessions still carry the erased crc_data placeholder and must remain
      // unreadable so normal scans return NOT_FOUND instead of INVALID_SIGNATURE.
      status = try_commit_recovered_wip_object(*chain_anchor,
                                               klv_start_address,
                                               klv_end_address,
                                               &head_info);
      if (status == SL_STATUS_OK) {
        progress = true;
        continue;
      }

      stream_ctx_invalidate_recovered_ut_key(head_info.header.key);
      previous_anchor = *chain_anchor;
      status = advance_anchor_past_unrecoverable_head(head_info.next_addr,
                                                      chain_anchor,
                                                      ut_context);
      if (status != SL_STATUS_OK) {
        return status;
      }

      if (*chain_anchor != previous_anchor) {
        progress = true;
        continue;
      }

      break;
    }

    if (head_magic != SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE) {
      // The stored anchor may still point at a head object that was already
      // soft-deleted before power was lost. Advance to the next live object so
      // normal scans restart from a readable active header.
      stream_ctx_invalidate_recovered_ut_key(head_info.header.key);

      previous_anchor = *chain_anchor;
      status = advance_anchor_past_unrecoverable_head(head_info.next_addr,
                                                      chain_anchor,
                                                      ut_context);
      if (status != SL_STATUS_OK) {
        return status;
      }

      if (*chain_anchor != previous_anchor) {
        progress = true;
        continue;
      }

      break;
    }

    status = validate_anchor_object(*chain_anchor,
                                    klv_start_address,
                                    klv_end_address,
                                    &head_info);
    if ((status != SL_STATUS_OK) || !head_info.header.is_readable) {
      stream_ctx_invalidate_recovered_ut_key(head_info.header.key);
      (void)mark_ut_object_deleted(*chain_anchor,
                                   klv_start_address,
                                   klv_end_address);
      previous_anchor = *chain_anchor;
      status = advance_anchor_past_unrecoverable_head(head_info.next_addr,
                                                      chain_anchor,
                                                      ut_context);
      if (status != SL_STATUS_OK) {
        return status;
      }

      if (*chain_anchor != previous_anchor) {
        progress = true;
        continue;
      }

      break;
    }

    // Reconstruct the current tail so relocation matching scans only the
    // active chain, not stale bytes beyond the logical end.
    status = scan_tail_from_anchor(*chain_anchor,
                                   klv_start_address,
                                   klv_end_address,
                                   &tail);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = find_matching_relocation(*chain_anchor,
                                      &head_info,
                                      tail,
                                      klv_start_address,
                                      klv_end_address,
                                      &pending_addr);
    if (status == SL_STATUS_NOT_FOUND) {
      break;
    } else if (status != SL_STATUS_OK) {
      return status;
    }

    // A matching relocated copy must validate before it can become the new
    // head. If it does not, mark it deleted so later scans ignore it.
    status = validate_anchor_object(pending_addr,
                                    klv_start_address,
                                    klv_end_address,
                                    &pending_info);
    if (status != SL_STATUS_OK) {
      (void)mark_ut_object_deleted(pending_addr,
                                   klv_start_address,
                                   klv_end_address);
      continue;
    }

    next_anchor = head_info.next_addr;
    // Persist the recovered head move before reclaim is allowed to erase any
    // page that still contains the previous head object.
    previous_anchor = *chain_anchor;
    status = advance_chain_anchor_after_head(next_anchor,
                                             tail,
                                             pending_addr,
                                             chain_anchor,
                                             ut_context);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if (*chain_anchor != previous_anchor) {
      (void)mark_ut_object_deleted(previous_anchor,
                                   klv_start_address,
                                   klv_end_address);
    }

    progress = true;
  } while (progress);

  status = retire_corrupted_objects_in_ut_chain(ut_context);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Keep one live generation per key. Covers a reclaim relocation interrupted
  // after the copy but before the source was soft-deleted, and an overwrite
  // interrupted after the new object was committed but before the old one was
  // soft-deleted. The newest copy (later in chain order) wins.
  status = retire_superseded_objects_in_ut_chain(ut_context);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return ut_refresh_largest_live_object_cache(*chain_anchor,
                                              klv_start_address,
                                              klv_end_address);
}

/***************************************************************************//**
 * Returns the current payload size of a token stored in the UT circular chain.
 *
 * The helper scans from the logical chain anchor rather than from a fixed
 * chain-start marker. The newest live object matching @p token determines the
 * size that is returned.
 ******************************************************************************/
sl_status_t sli_token_manager_get_size_from_klv_anchor(const sli_klv_ut_context_t *ut_context,
                                                       uint32_t token,
                                                       uint32_t *size_out)
{
  sli_klv_ut_scan_result_t scan_result = { 0 };
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL)
      || (ut_context->chain_anchor == NULL)
      || (size_out == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);

  if (is_invalid_klv_key(token)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Scan the UT KLV chain for the matching object.
  status = scan_from_anchor(token,
                            *ut_context->chain_anchor,
                            ut_context->klv_start_address,
                            ut_context->klv_end_address,
                            &scan_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT size scan: key=0x%04lx anchor=0x%08lx tail=0x%08lx used=%lu free=%lu reclaimable=%lu match=%u empty=%u full=%u\r\n",
                  (token & 0xFFFFU),
                  *ut_context->chain_anchor,
                  scan_result.tail,
                  (scan_result.occupied_bytes - scan_result.live_span_reclaimable_bytes),
                  (get_total_region_size(ut_context->klv_start_address,
                    ut_context->klv_end_address) - scan_result.occupied_bytes),
                  scan_result.reclaimable_bytes,
                  scan_result.match_found,
                  scan_result.chain_empty,
                  scan_result.chain_full); )

  if (!scan_result.match_found) {
    return SL_STATUS_NOT_FOUND;
  }

  // Return the payload length of the matching object.
  *size_out = scan_result.match.header.payload_length;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads a token from the anchor-based UT KLV chain.
 *
 * The circular UT chain is logically ordered by @p chain_anchor, not by the
 * lowest physical flash address. This API therefore scans from the logical head
 * to find the newest live match, validates the requested slice length, and then
 * extracts the plaintext payload.
 ******************************************************************************/
sl_status_t sli_read_klv_object_from_anchor(const sli_klv_ut_context_t *ut_context,
                                            sl_klv_handle_t const *klv_handle,
                                            void *data)
{
  sli_klv_ut_scan_result_t scan_result = { 0 };
  sl_status_t status = SL_STATUS_OK;

  if ((ut_context == NULL)
      || (ut_context->chain_anchor == NULL)
      || (klv_handle == NULL)
      || (data == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);

  if ((klv_handle->length == 0U) || is_invalid_klv_key(klv_handle->key)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Scan the UT KLV chain for the matching object.
  status = scan_from_anchor(klv_handle->key,
                            *ut_context->chain_anchor,
                            ut_context->klv_start_address,
                            ut_context->klv_end_address,
                            &scan_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT read scan: key=0x%04lx anchor=0x%08lx tail=0x%08lx used=%lu free=%lu reclaimable=%lu match=%u empty=%u full=%u\r\n",
                  (klv_handle->key & 0xFFFFU),
                  *ut_context->chain_anchor,
                  scan_result.tail,
                  (scan_result.occupied_bytes - scan_result.live_span_reclaimable_bytes),
                  (get_total_region_size(ut_context->klv_start_address,
                    ut_context->klv_end_address) - scan_result.occupied_bytes),
                  scan_result.reclaimable_bytes,
                  scan_result.match_found,
                  scan_result.chain_empty,
                  scan_result.chain_full); )

  if (!scan_result.match_found) {
    return SL_STATUS_NOT_FOUND;
  }

  if ((klv_handle->offset > scan_result.match.header.payload_length) 
      || ((klv_handle->offset + klv_handle->length) > scan_result.match.header.payload_length)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Extract the payload from the UT KLV object.
  return extract_ut_payload(&scan_result.match.header,
                            scan_result.match.obj_addr,
                            ut_context->klv_start_address,
                            ut_context->klv_end_address,
                            klv_handle,
                            data);
}

/***************************************************************************//**
 * Appends or replaces a token in the circular UT KLV chain.
 *
 * This is the main UT write entry point. The function first scans the current
 * chain to determine:
 * - whether a matching live token already exists,
 * - how much of the region is occupied or reclaimable,
 * - the largest live object size, which drives reserve calculations.
 *
 * It then either:
 * - selects a direct append address when enough space remains while preserving
 *   the repack reserve, or
 * - falls back to @ref reclaim_and_write to relocate live objects and
 *   reclaim pages safely.
 *
 * Reclaim is not started while another streaming session is active; the write
 * returns @ref SL_STATUS_BUSY instead. A reclaiming stream also rejects
 * later mutating writes until finish.
 *
 * The persisted anchor is managed by the caller-provided callback so reclaim can
 * publish head moves before old pages are erased.
 ******************************************************************************/
sl_status_t sli_write_klv_object_from_anchor(const sli_klv_ut_context_t *ut_context,
                                             sl_klv_handle_t const *klv_handle,
                                             void const *data)
{
  uint32_t *chain_anchor = NULL;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  sli_klv_ut_scan_result_t scan_result = { 0 };
  sl_status_t status = SL_STATUS_OK;
  uint32_t write_addr = 0U;
  uint32_t object_size = 0U;
  uint32_t repack_reserve_size = 0U;
  uint32_t repack_guard_bytes = 0U;
  uint32_t anchor_before = 0U;
  uint16_t data_crc = 0U;
  sli_klv_obj_header_t obj_hdr;
  uint32_t replaced_object_size = 0U;
  bool replaced_was_live = false;

  if ((ut_context == NULL)
      || (ut_context->chain_anchor == NULL)
      || (ut_context->anchor_commit == NULL)
      || (klv_handle == NULL)
      || (data == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);

  chain_anchor = ut_context->chain_anchor;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;

  if ((klv_handle->length == 0U) || is_invalid_klv_key(klv_handle->key)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!is_address_in_range(*chain_anchor, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // A one-shot write of a token that already has an active stream would append a
  // second live copy, reads/deletes resolve the first match from the anchor and
  // would ignore the newer one. Mirror set_start's same-token conflict.
  if (stream_ctx_find(klv_handle->key) != NULL) {
    return SL_STATUS_IN_PROGRESS;
  }

  anchor_before = *chain_anchor;

  status = scan_from_anchor(klv_handle->key,
                            *chain_anchor,
                            klv_start_address,
                            klv_end_address,
                            &scan_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (scan_result.torn_tail) {
    // A dirty, non-followable tail cannot be reused by direct append or
    // through reclaim: neither operation has a durable backup for its page.
    return SL_STATUS_INVALID_STATE;
  }

  status = ut_largest_live_object_cache_ensure_valid(*ut_context->chain_anchor,
                                                     klv_start_address,
                                                     klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (scan_result.match_found) {
    uint32_t payload_addr = advance_ut_adress(scan_result.match.obj_addr,
                                              scan_result.match.header.header_size,
                                              klv_start_address,
                                              klv_end_address);

    // If the latest live object already holds identical payload bytes, report
    // ALREADY_EXISTS instead of creating another duplicate history entry.
    if ((scan_result.match.header.payload_length == klv_handle->length)
        && logical_equals_buffer(payload_addr,
                                 data,
                                 klv_handle->length,
                                 klv_start_address,
                                 klv_end_address)) {
      return SL_STATUS_ALREADY_EXISTS;
    }
  }

  // Identical-payload checks are read-only. Any mutating write is unsafe while a
  // reclaiming stream still has deferred erase and layout side effects pending.
  if (stream_ctx_any_used_reclaim()) {
    return SL_STATUS_BUSY;
  }

  object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                    klv_handle->length);

  // Check if the object and its required repack reserve fit within the registered total capacity.
  if (!check_object_fits_registered_capacity(&scan_result,
                                             object_size,
                                             ut_context->registered_total_region_size)) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  // Size repack reserve from readable live + ACTIVE+WIP (and the new object).
  // Matching the capacity gate prevents a concurrent stream whose WIP is the
  // largest object from leaving an undersized post-write reclaim window.
  repack_guard_bytes = get_largest_object_size(&scan_result, object_size);
  repack_reserve_size = compute_ut_repack_reserve_size(repack_guard_bytes);

  // Try to write the object directly.
  status = try_direct_write_from_scan(ut_context,
                                      &scan_result,
                                      object_size,
                                      repack_reserve_size,
                                      repack_guard_bytes,
                                      &write_addr);

  TOKENDBG(printf("UT write scan: key=0x%04lx anchor=0x%08lx tail=0x%08lx used=%lu free=%lu reclaimable=%lu repack=%lu largest=%lu object=%lu direct_status=0x%0lx write_addr=0x%08lx match=%u empty=%u full=%u\r\n",
                  (klv_handle->key & 0xFFFFU),
                  *chain_anchor,
                  scan_result.tail,
                  (scan_result.occupied_bytes - scan_result.live_span_reclaimable_bytes),
                  (get_total_region_size(klv_start_address, klv_end_address) - scan_result.occupied_bytes),
                  scan_result.reclaimable_bytes,
                  repack_reserve_size,
                  largest_live_object_size,
                  object_size,
                  status,
                  write_addr,
                  scan_result.match_found,
                  scan_result.chain_empty,
                  scan_result.chain_full); )

  if (status == SL_STATUS_OK) {
    // The selected window has to be erased before it can hold the object. When
    // it cannot be cleaned, fall through to reclaim and let the repack rebuild
    // the layout instead of programming over stale bytes.
    status = prepare_append_destination(write_addr,
                                        object_size,
                                        *chain_anchor,
                                        klv_start_address,
                                        klv_end_address);
  }

  if (status != SL_STATUS_OK) {
    // Reclaim is worthwhile when the chain already contains data and either a
    // match / pending extension justifies the path, or free + live-span
    // reclaimable can cover the new object plus reserve.
    if (scan_result.match_found
        || ut_context_has_pending_extensions(ut_context)
        || can_reclaim_write_from_scan(ut_context,
                                       &scan_result,
                                       object_size,
                                       repack_reserve_size)) {
      TOKENDBG(printf("UT write reclaim: key=0x%04lx anchor=0x%08lx tail=0x%08lx object=%lu reclaimable=%lu repack=%lu match=%u\r\n",
                      (klv_handle->key & 0xFFFFU),
                      *chain_anchor,
                      scan_result.tail,
                      object_size,
                      scan_result.reclaimable_bytes,
                      repack_reserve_size,
                      scan_result.match_found); )

      // Reclaim and write the object.
      return reclaim_and_write(klv_handle,
                               chain_anchor,
                               ut_context,
                               scan_result.tail,
                               scan_result.match_found ? &scan_result.match : NULL,
                               data);
    }
    // Return the status of the scan operation.
    return status;
  }

  // Calculate the CRC for the data.
  data_crc = calculate_checksum(data, klv_handle->length);
  // Initialize the KLV header.
  SLI_KLV_INIT_HDR_FOR_WRITE(obj_hdr, klv_handle, data_crc);

  status = write_committed_ut_object(write_addr,
                                     &obj_hdr,
                                     data,
                                     klv_handle->length,
                                     klv_start_address,
                                     klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Update and persist the chain anchor if the chain is empty. skip_repack_window
  // may place the first object away from the prior NVM3 head; without an
  // immediate commit, power loss before the API-layer publish orphans that object.
  if (scan_result.chain_empty) {
    uint32_t previous_anchor = *chain_anchor;

    *chain_anchor = write_addr;
    if (*chain_anchor != previous_anchor) {
      status = ut_context->anchor_commit(*chain_anchor, ut_context->anchor_context);
      if (status != SL_STATUS_OK) {
        *chain_anchor = previous_anchor;
        return status;
      }
    }
  }

  if (scan_result.match_found) {
    sli_klv_ut_obj_info_t delete_info = { 0 };

    // Logical decode — same rule as reclaim_and_write / stream finish. A raw
    // physical magic read can miss a header that wraps the logical ring end.
    status = decode_object_from_anchor(scan_result.match.obj_addr,
                                       klv_start_address,
                                       klv_end_address,
                                       &delete_info);
    if (status == SL_STATUS_OK) {
      if (delete_info.header.is_readable) {
        status = mark_ut_object_deleted(scan_result.match.obj_addr,
                                        klv_start_address,
                                        klv_end_address);
        if (status != SL_STATUS_OK) {
          return status;
        }

        if (scan_result.match.obj_addr == anchor_before) {
          // Skip soft-deleted heads only; WIP remains a valid temporary head.
          status = skip_unreadable_chain_head(chain_anchor, ut_context);
          if (status != SL_STATUS_OK) {
            return status;
          }
        }
      }
    } else if (status != SL_STATUS_NOT_FOUND) {
      return status;
    }
  }

  if (scan_result.match_found) {
    replaced_was_live = true;
    replaced_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(scan_result.match.header.header_size,
                                                               scan_result.match.header.payload_length);
  }

  status = ut_largest_live_object_cache_on_write_commit(*chain_anchor,
                                                        klv_start_address,
                                                        klv_end_address,
                                                        object_size,
                                                        replaced_object_size,
                                                        replaced_was_live);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT write commit: key=0x%04lx new_anchor=0x%08lx write_addr=0x%08lx overwritten=%u\r\n",
                  (klv_handle->key & 0xFFFFU),
                  *chain_anchor,
                  write_addr,
                  scan_result.match_found); )

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Soft-deletes the newest live UT object that matches @p token.
 *
 * Delete does not reclaim space immediately. Instead it rewrites the object's
 * magic word to the deleted form so scans stop treating it as live, while the
 * bytes remain available for a later reclaim pass. If the deleted entry was the
 * logical head, the chain anchor is advanced past soft-deleted heads when a
 * live or WIP successor exists (WIP heads are kept) and persisted. If the only
 * remaining content is soft-deleted, the anchor stays on that deleted object
 * so reclaim can still account for those bytes.
 ******************************************************************************/
sl_status_t sli_delete_klv_object_from_anchor(const sli_klv_ut_context_t *ut_context,
                                              uint32_t token)
{
  uint32_t *chain_anchor = NULL;
  uint32_t klv_start_address = 0U;
  uint32_t klv_end_address = 0U;
  sli_klv_ut_scan_result_t scan_result = { 0 };
  sl_status_t status = SL_STATUS_OK;
  uint32_t anchor_before = 0U;
  uint32_t match_magic = 0U;
  uint32_t deleted_object_size = 0U;

  if ((ut_context == NULL)
      || (ut_context->chain_anchor == NULL)
      || (ut_context->anchor_commit == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  set_active_ut_context(ut_context);

  chain_anchor = ut_context->chain_anchor;
  klv_start_address = ut_context->klv_start_address;
  klv_end_address = ut_context->klv_end_address;

  if (is_invalid_klv_key(token)
      || !is_address_in_range(*chain_anchor, klv_start_address, klv_end_address)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  anchor_before = *chain_anchor;

  status = scan_from_anchor(token,
                            *chain_anchor,
                            klv_start_address,
                            klv_end_address,
                            &scan_result);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT delete scan: key=0x%04lx anchor=0x%08lx tail=0x%08lx used=%lu free=%lu reclaimable=%lu match=%u empty=%u full=%u\r\n",
                  (token & 0xFFFFU),
                  *chain_anchor,
                  scan_result.tail,
                  (scan_result.occupied_bytes - scan_result.live_span_reclaimable_bytes),
                  (get_total_region_size(klv_start_address, klv_end_address) - scan_result.occupied_bytes),
                  scan_result.reclaimable_bytes,
                  scan_result.match_found,
                  scan_result.chain_empty,
                  scan_result.chain_full); )

  if (scan_result.torn_tail) {
    // Even a soft-delete would change the only durable copy while the chain
    // contains an ambiguous discontinuity. Keep the entire chain read-only.
    return SL_STATUS_INVALID_STATE;
  }

  if (!scan_result.match_found) {
    return SL_STATUS_NOT_FOUND;
  }

  deleted_object_size = SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(scan_result.match.header.header_size,
                                                            scan_result.match.header.payload_length);

  // Read the magic word of the matched object.
  status = logical_read(scan_result.match.obj_addr,
                        &match_magic,
                        sizeof(match_magic),
                        klv_start_address,
                        klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (match_magic != SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE) {
    // The scan should only return live objects. If the header no longer matches
    // active magic, the chain changed unexpectedly underneath the caller.
    return SL_STATUS_INVALID_STATE;
  }

  // Write the deleted magic word to the matched object.
  status = mark_ut_object_deleted(scan_result.match.obj_addr,
                                  klv_start_address,
                                  klv_end_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (scan_result.match.obj_addr == anchor_before) {
    // Head deletion changes the logical start of the ring. Skip soft-deleted
    // heads only so an in-progress WIP object is not orphaned behind the head.
    status = skip_unreadable_chain_head(chain_anchor, ut_context);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  status = ut_largest_live_object_cache_on_delete(*chain_anchor,
                                                  klv_start_address,
                                                  klv_end_address,
                                                  deleted_object_size);
  if (status != SL_STATUS_OK) {
    return status;
  }

  TOKENDBG(printf("UT delete commit: key=0x%04lx new_anchor=0x%08lx deleted_addr=0x%08lx\r\n",
                  (token & 0xFFFFU),
                  *chain_anchor,
                  scan_result.match.obj_addr); )

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Returns the cached largest live UT object size tracked by the KLV backend.
 ******************************************************************************/
uint32_t sli_klv_ut_get_largest_live_object_cache_size(void)
{
  return largest_live_object_size;
}

/***************************************************************************//**
 * Clears the cached largest live-object size.
 ******************************************************************************/
void sli_klv_ut_largest_live_object_cache_invalidate(void)
{
  largest_live_object_size = 0U;
  largest_live_object_cache_valid = false;
}
#endif //SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS

#if !defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH)
/***************************************************************************//**
 * @brief
 *  Returns the size of a static token from the KLV header
 *
 * @param[in]  klv_start_address   The KLV start address of a KLV chain.
 * @param[in]  klv_end_address     The KLV end address of a KLV chain.
 * @param[in]  token               32-bit token identifier
 * @param[out] size_out            A pointer to where the token data size should be placed.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_token_manager_get_size_from_klv(uint32_t klv_start_address,
                                                uint32_t klv_end_address,
                                                uint32_t token,
                                                uint32_t *size_out)
{
  if (((uint32_t *)klv_start_address == NULL) || ((uint32_t *)klv_end_address == NULL) || (size_out == NULL)) {
    TOKENDBG(printf("Null pointer error in sli_token_manager_get_size_from_klv\n"));
    return SL_STATUS_NULL_POINTER;
  }

  uint32_t klv_main_start = *((uint32_t *)klv_start_address);

  // Validate the KLV_MAIN_START
  if ((klv_main_start != SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START)
      && (klv_main_start != SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START)) {
    TOKENDBG(printf("Invalid KLV chain start 0x%lx\n", klv_main_start));
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Start from the first KLV object in the chain.
  uint8_t *obj_adr = (uint8_t *)klv_start_address + sizeof(uint32_t);
  uint32_t security_offset = SLI_TOKEN_MANAGER_GET_SECURITY_OFFSET(klv_start_address);
  sl_status_t status = SL_STATUS_OK;

  while ((uint32_t *)obj_adr < (uint32_t *)klv_end_address) {
    sli_klv_hdr_parse_t klv_header_info;
    // Decode the KLV header
    status = decode_klv_header(obj_adr, &klv_header_info);
    if (status != SL_STATUS_OK) {
      return status;
    }

    // If the key matches, return the size of the object.
    if (klv_header_info.key == (token & 0xFFFFU)) {
      // Check if the object is write in-progress or not.
      if (!klv_header_info.is_readable) {
        // The object is not readable, skip it.
        obj_adr += SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES(klv_header_info.header_size,
                                                          security_offset,
                                                          klv_header_info.payload_length);
        continue;
      }
      // The object is valid, return the size of the object.
      *size_out = klv_header_info.payload_length;
      return SL_STATUS_OK;
    }

    // Move to the next KLV object in the chain.
    obj_adr += SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES(klv_header_info.header_size,
                                                      security_offset,
                                                      klv_header_info.payload_length);
  }

  // The requested token is not found in the KLV chain.
  return SL_STATUS_NOT_FOUND;
}

#if defined(SL_TOKEN_MANAGER_SECURITY)
/***************************************************************************//**
 * Constructs the full AEAD nonce used when encrypting a secure KLV object.
 *
 * @param[in]  obj_adr       Address to the klv object.
 * @param[in]  key           Key of the KLV object.
 * @param[in]  data_len      Length of the data in the KLV object.
 * @param[out] nonce_val     A pointer to nonce buffer which is updated by this function.
 *
 * @return SL_STATUS_OK if successful or SL_STATUS_SECURITY_RANDOM_NUM_GEN_ERROR on failure.
 ******************************************************************************/
static sl_status_t sli_token_manager_create_nonce(uint32_t obj_adr,
                                                  uint16_t key,
                                                  uint32_t data_len,
                                                  uint8_t *nonce_val)
{
  sl_status_t status;
  uint8_t nonce[SLI_CRYPTO_NONCE_SIZE];
  // Construct nonce from random number
  status = hal_crypto_handle.gen_rand_num(&nonce[0], SLI_CRYPTO_RANDOM_NUMBER_SIZE);
  if (status != SL_STATUS_OK) {
    TOKENDBG(printf("random number generation failed, status=0x%lx.\n", status));
    return status;
  }

  nonce[4] = (obj_adr & 0xFF000000) >> 24;
  nonce[5] = (obj_adr & 0x00FF0000) >> 16;
  nonce[6] = (obj_adr & 0x0000FF00) >> 8;
  nonce[7] = (obj_adr & 0x000000FF);

  nonce[8] = (key & 0xFF00) >> 8;
  nonce[9] = (key & 0x00FF);
  nonce[10] = (data_len & 0xFF00) >> 8;
  nonce[11] = (data_len & 0x00FF);

  memcpy(&nonce_val[0], &nonce[0], sizeof(nonce));

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reconstructs the full nonce needed to decrypt a secure KLV object.
 *
 * @param[in]  obj_adr       Address to the klv object.
 * @param[in]  key           Key of the KLV object.
 * @param[in]  data_len      Length of the data in the KLV object.
 * @param[out] nonce_val     A pointer to nonce buffer which is updated by this function.
 ******************************************************************************/
static void sli_token_manager_get_nonce(uint32_t obj_adr,
                                        uint16_t key,
                                        uint32_t data_len,
                                        uint8_t *nonce_val)
{
  uint8_t nonce[SLI_CRYPTO_NONCE_SIZE];

  // Read only the stored random prefix; the remaining bytes are reconstructed
  // deterministically so they do not need dedicated flash space.
  (void)sli_token_manager_nvm_read(obj_adr, &nonce[0], 4);

  nonce[4] = (obj_adr & 0xFF000000) >> 24;
  nonce[5] = (obj_adr & 0x00FF0000) >> 16;
  nonce[6] = (obj_adr & 0x0000FF00) >> 8;
  nonce[7] = (obj_adr & 0x000000FF);

  nonce[8] = (key & 0xFF00) >> 8;
  nonce[9] = (key & 0x00FF);
  nonce[10] = (data_len & 0xFF00) >> 8;
  nonce[11] = (data_len & 0x00FF);

  memcpy(&nonce_val[0], &nonce[0], sizeof(nonce));
}

/*****************************************************************************************************************//**
 * This function returns crypto algorithm w.r.t klv security type configured.
 *
 * @param[in]  sec_type     klv security type.
 *
 * @return Returns remapped crypto algorithm.
 ********************************************************************************************************************/
static hal_crypto_algo_t sli_token_manager_get_crypto_algo(sl_klv_security_type_t sec_type)
{
  hal_crypto_algo_t crypto_algo;

  switch (sec_type) {
    case SL_TOKEN_MANAGER_KLV_SECURITY_TYPE_NONE:
      crypto_algo = HAL_CRYPTO_ALGO_NONE;
      break;
    case SL_TOKEN_MANAGER_KLV_SECURITY_TYPE_AEAD:
      crypto_algo = HAL_CRYPTO_ALGO_AEAD;
      break;
    case SL_TOKEN_MANAGER_KLV_SECURITY_TYPE_ENC:
      crypto_algo = HAL_CRYPTO_ALGO_ENC;
      break;
    default:
      crypto_algo = HAL_CRYPTO_ALGO_AEAD;
      break;
  }

  return crypto_algo;
}

/***************************************************************************//**
 * Decrypts a KLV payload from flash into a allocated buffer.
 *
 * @param[in]  header         A pointer to the decoded KLV header (used as AAD).
 * @param[in]  src            Pointer to the on-flash payload (nonce + ciphertext + tag region).
 * @param[out] out_decrypted  On success, receives a buffer allocated with sl_malloc()
 *                            that the caller must release with sl_free().
 *
 * @return SL_STATUS_OK on success, an error code otherwise.
 ******************************************************************************/
static sl_status_t sli_token_manager_decrypt_klv_data(const sli_klv_hdr_parse_t *header,
                                                      const uint8_t *src,
                                                      uint8_t **out_decrypted)
{
  uint8_t nonce[SLI_CRYPTO_NONCE_SIZE] = { 0 };
  uint8_t tag[SLI_CRYPTO_TAG_SIZE];
  uint8_t *ptr_dec;

  // Allocate memory for decryption
  ptr_dec = (uint8_t *)sl_malloc(header->payload_length);
  if (ptr_dec == NULL) {
    TOKENDBG(printf("Memory allocation failed\n"));
    return SL_STATUS_ALLOCATION_FAILED;
  }

  sli_token_manager_get_nonce(SLI_KLV_SECURE_NONCE_STORE_ADDR_U32(header->header_size, src),
                              header->key,
                              header->payload_length,
                              nonce);
  memcpy(tag,
         (const void *)SLI_KLV_GET_SECURE_TAG_NVM_ADDR_U32(header->header_size,
                                                           (uint8_t *)src,
                                                           SL_CEILING(header->payload_length, sizeof(uint32_t))),
         SLI_CRYPTO_TAG_SIZE);

  sl_status_t status = hal_crypto_handle.decrypt(nonce,
                                                 src,
                                                 header->aad_length,
                                                 SLI_KLV_DATA_START_PTR(header->header_size, src),
                                                 header->payload_length,
                                                 ptr_dec,
                                                 tag,
                                                 sli_token_manager_get_crypto_algo(SL_TOKEN_MANAGER_KLV_SECURITY_TYPE));
  if (status != SL_STATUS_OK) {
    TOKENDBG(printf("Decryption failed, status=0x%lx\n", status));
    sl_free(ptr_dec);
    return status;
  }

  *out_decrypted = ptr_dec;
  return SL_STATUS_OK;
}
#endif

/***************************************************************************//**
 * Extracts the payload of a matched KLV object: decrypts (if secure), validates
 * CRC (device tokens), then copies the requested slice into the caller buffer.
 ******************************************************************************/
static sl_status_t sli_token_manager_extract_klv_payload(uint32_t klv_main_start,
                                                         const sli_klv_hdr_parse_t *header,
                                                         const uint8_t *obj_adr,
                                                         const sl_klv_handle_t *klv_handle,
                                                         void *data)
{
  const uint8_t *ptr_src = obj_adr;
  sl_status_t status = SL_STATUS_OK;

#if defined(SL_TOKEN_MANAGER_SECURITY)
  uint8_t *ptr_dec = NULL;

  if (klv_main_start == SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START) {
    status = sli_token_manager_decrypt_klv_data(header, ptr_src, &ptr_dec);
    if (status != SL_STATUS_OK) {
      return status;
    }
    ptr_src = ptr_dec;
  }
#endif

  // Verify CRC for only static device tokens. Static secure token data is
  // encrypted using authenticated encryption (AEAD) and therefore CRC is
  // not verified.
  if (klv_main_start == SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START) {
    ptr_src += header->header_size;
    uint16_t calculated_checksum = calculate_checksum(ptr_src, header->payload_length);
    if (calculated_checksum != header->payload_crc) {
      TOKENDBG(printf("CRC check failed\n"));
      status = SL_STATUS_INVALID_PARAMETER;
    }
  }

  if (status == SL_STATUS_OK) {
    memcpy(data, ptr_src + klv_handle->offset, klv_handle->length);
  }

#if defined(SL_TOKEN_MANAGER_SECURITY)
  if (ptr_dec != NULL) {
    sl_free(ptr_dec);
  }
#endif

  return status;
}

/***************************************************************************//**
 * Reads data from a klv object chain.
 *
 * @note
 *
 * @param[in]  klv_handle        A pointer to the sl_klv_handle_t,
 *                               containing length and key info
 * @param[in]  klv_start_address The KLV chain start address.
 * @param[in]  klv_end_address   The KLV chain end address.
 * @param[out] data              A pointer to where the token data should be placed.
 *
 * @return SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sli_read_klv_object(sl_klv_handle_t const *klv_handle,
                                uint32_t *klv_start_address,
                                uint32_t *klv_end_address,
                                void *data)
{
  if ((klv_handle == NULL) || (klv_start_address == NULL) || (klv_end_address == NULL) || (data == NULL)) {
    TOKENDBG(printf("Null pointer error in sli_read_klv_object\n"));
    return SL_STATUS_NULL_POINTER;
  }

  if ((klv_handle->length == 0) || (klv_handle->length == 0xFFFF) || (klv_handle->key == 0xFFFF)) {
    TOKENDBG(printf("Invalid KLV handle parameters\n"));
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint32_t klv_main_start = *klv_start_address;

  // Validate the KLV_MAIN_START
  if (klv_main_start != SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START
      && klv_main_start != SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START) {
    TOKENDBG(printf("Invalid KLV chain start\n"));
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Start from the first KLV object in the chain.
  uint8_t *obj_adr = (uint8_t *)klv_start_address + sizeof(uint32_t);
  uint32_t security_offset = SLI_TOKEN_MANAGER_GET_SECURITY_OFFSET(klv_start_address);

  while ((uint32_t *)obj_adr < klv_end_address) {
    sli_klv_hdr_parse_t klv_header_info;
    // Decode the KLV header
    sl_status_t status = decode_klv_header(obj_adr, &klv_header_info);
    if (status != SL_STATUS_OK) {
      TOKENDBG(printf("Failed to decode KLV header: %ld\n", status));
      return status;
    }

    // Check if the key matches
    if (klv_header_info.key == (klv_handle->key & 0xFFFFU)) {
      // Check if the object is valid or not.
      if (!klv_header_info.is_readable) {
        // The object is not readable, skip it.
        obj_adr += SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES(klv_header_info.header_size,
                                                          security_offset,
                                                          klv_header_info.payload_length);
        continue;
      }
      // A valid token is found.
      // Verify the requested length from the data offset doesn't exceed the total length.
      if ((klv_handle->offset > klv_header_info.payload_length) 
          || ((klv_handle->offset + klv_handle->length) > klv_header_info.payload_length)) {
        TOKENDBG(printf("Requested length exceeds available data length\n"));
        return SL_STATUS_INVALID_PARAMETER;
      }
      // Extract the payload.
      return sli_token_manager_extract_klv_payload(klv_main_start,
                                                   &klv_header_info,
                                                   obj_adr,
                                                   klv_handle,
                                                   data);
    }

    // Move to the next object.
    obj_adr += SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES(klv_header_info.header_size,
                                                      security_offset,
                                                      klv_header_info.payload_length);
  }

  // The requested key is not found in the KLV chain.
  return SL_STATUS_NOT_FOUND;
}

/***************************************************************************//**
 * Initializes a KLV chain start marker for an empty chain. The marker selected
 * (secure vs. device) depends on which chain the @p klv_start_address belongs to.
 *
 * @param[in]  klv_start_address  The KLV chain start address.
 * @param[out] klv_main_start     Updated with the initialized start marker value.
 *
 * @return SL_STATUS_OK on success, an error code otherwise.
 ******************************************************************************/
static sl_status_t sli_token_manager_init_klv_chain(uint32_t klv_start_address,
                                                    uint32_t *klv_main_start)
{
  if (klv_start_address == (uint32_t)SLI_TOKEN_STATIC_SECURE_TOKEN_BASE) {
    // Initialize KLV chain for secure token
    *klv_main_start = SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START;
    if (sli_token_manager_nvm_write(klv_start_address, klv_main_start, sizeof(*klv_main_start)) != SL_STATUS_OK) {
      TOKENDBG(printf("Failed to initialize KLV chain start\n"));
      return SL_STATUS_INITIALIZATION;
    }
  } else {
    // Initialize KLV chain for device token (stored in SE MTP via memcpy)
    *klv_main_start = SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START;
    memcpy((void *)klv_start_address, klv_main_start, sizeof(*klv_main_start));
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Decides what to do when @p current_pointer already points at an existing KLV
 * entry. Static device tokens may be overwritten in-place when the replacement
 * payload differs in content. The parsed header is returned so the caller can
 * preserve the existing header size during the overwrite.
 *
 * If this helper returns @ref SL_STATUS_NOT_FOUND, the caller must treat the
 * write as chain append and must not consume @p existing_header_out.
 * All other non-success cases are returned as errors.
 *
 * @return SL_STATUS_OK if the caller should proceed with the overwrite, otherwise
 *         an error status code.
 ******************************************************************************/
static sl_status_t sli_token_manager_check_klv_overwrite(uint32_t klv_main_start,
                                                         uint32_t *current_pointer,
                                                         const sl_klv_handle_t *klv_handle,
                                                         const void *data,
                                                         sli_klv_hdr_parse_t *existing_header_out)
{
  // Static secure token does not support overwrite.
  if (klv_main_start != SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START) {
    return SL_STATUS_ALREADY_EXISTS;
  }

  sli_klv_hdr_parse_t existing_header;
  // Decode the existing header
  sl_status_t status = decode_klv_header((uint8_t *)current_pointer, &existing_header);

  // If the status is NOT_OK, either the existing KLV object is not found or there is an error.
  if (status != SL_STATUS_OK) {
    return status;
  }

  // For an overwrite, the payload length must match.
  if (existing_header.payload_length != klv_handle->length) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // If the payload matches, return ALREADY_EXISTS.
  if (memcmp(data, ((uint8_t *)current_pointer + existing_header.header_size), existing_header.payload_length) == 0) {
    return SL_STATUS_ALREADY_EXISTS;
  }

  // Return the existing header if requested.
  if (existing_header_out != NULL) {
    *existing_header_out = existing_header;
  }

  // If the payload does not match, return OK.
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Rewrites an existing SDT entry in-place while preserving @c header_size.
 * This avoids expanding an 8-byte header into a 16-byte header, which would
 * otherwise overwrite the next chain entry.
 ******************************************************************************/
static sl_status_t sli_token_manager_rewrite_device_klv_inplace(uint8_t *current_pointer,
                                                                const sl_klv_handle_t *klv_handle,
                                                                const void *data,
                                                                uint32_t write_length_bytes)
{
  sl_klv_header_t obj_hdr = {
    .magic_number = SLI_TOKEN_KLV_MAGIC_NUMBER,
    .crc = calculate_checksum((const uint8_t *)data, klv_handle->length),
    .length = (uint16_t)klv_handle->length,
    .key = (uint16_t)(klv_handle->key & 0xFFFFU),
  };

  memcpy(current_pointer, &obj_hdr, sizeof(obj_hdr));

  // Rewrite the word-padded payload region while keeping the stored header size
  // unchanged, unused tail bytes remain erased.
  memset(current_pointer + sizeof(obj_hdr), 0xFF, write_length_bytes);
  memcpy(current_pointer + sizeof(obj_hdr), data, klv_handle->length);

  return SL_STATUS_OK;
}

#if defined(SL_TOKEN_MANAGER_SECURITY)
/***************************************************************************//**
 * Encrypts @p data and writes the resulting <header + nonce | ciphertext | tag>
 * layout to the secure KLV chain at @p current_pointer.
 *
 * The write follows the power-fail-safe pattern to ensure the integrity of the data:
 * write the object while the write-in-progress bit is set, then clear that bit
 * only after header, nonce, ciphertext, and tag have all been committed.
 ******************************************************************************/
static sl_status_t sli_token_manager_write_secure_klv(uint8_t *current_pointer,
                                                      const sli_klv_obj_header_t *obj_hdr,
                                                      const void *data,
                                                      uint32_t write_length_bytes)
{
  uint8_t buffer[SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE + SLI_CRYPTO_RANDOM_NUMBER_SIZE];
  uint8_t nonce_val[SLI_CRYPTO_NONCE_SIZE];
  uint8_t tag[SLI_CRYPTO_TAG_SIZE];
  sl_status_t status;
  uint8_t *ptr_enc;

  // Allocate memory for storing encrypted data
  ptr_enc = (uint8_t *)sl_malloc(obj_hdr->length);
  if (ptr_enc == NULL) {
    TOKENDBG(printf("Memory allocation failed\n"));
    return SL_STATUS_ALLOCATION_FAILED;
  }

  sli_token_manager_create_nonce(SLI_KLV_SECURE_NONCE_STORE_ADDR_U32(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                                     current_pointer),
                                 obj_hdr->key,
                                 obj_hdr->length,
                                 &nonce_val[0]);

  status = hal_crypto_handle.encrypt(&nonce_val[0],
                                     (const uint8_t *)obj_hdr,
                                     SLI_TOKEN_KLV_EXTENDED_HEADER_AAD_SIZE,
                                     data,
                                     obj_hdr->length,
                                     ptr_enc,
                                     tag,
                                     sli_token_manager_get_crypto_algo(SL_TOKEN_MANAGER_KLV_SECURITY_TYPE));
  if (status == SL_STATUS_OK) {
    // Write klv header and nonce
    memcpy(&buffer[SLI_CRYPTO_HEADER_OFFSET], obj_hdr, sizeof(*obj_hdr));
    memcpy(&buffer[SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE], &nonce_val[0], SLI_CRYPTO_RANDOM_NUMBER_SIZE);
    status = sli_token_manager_nvm_write((uint32_t)current_pointer,
                                         buffer,
                                         sizeof(*obj_hdr) + SLI_CRYPTO_RANDOM_NUMBER_SIZE);

    // Write payload and tag to flash
    if ((status == SL_STATUS_OK)
        && ((sli_token_manager_nvm_write(SLI_KLV_GET_SECURE_PAYLOAD_NVM_ADDR_U32(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                                                current_pointer),
                                         ptr_enc,
                                         write_length_bytes) != SL_STATUS_OK)
        || (sli_token_manager_nvm_write(SLI_KLV_GET_SECURE_TAG_NVM_ADDR_U32(SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE,
                                                                            current_pointer,
                                                                            write_length_bytes),
                                        tag,
                                        SLI_CRYPTO_TAG_SIZE) != SL_STATUS_OK))) {
      status = SL_STATUS_FAIL;
    }

    if (status == SL_STATUS_OK) {
      // Clear the write-in-progress bit (bit 7) and re-write the header to
      // mark the object as committed/readable.
      sli_klv_obj_header_t committed_hdr = *obj_hdr;
      committed_hdr.flags &= (uint8_t)(~SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK);
      status = sli_token_manager_nvm_write((uint32_t)current_pointer,
                                           &committed_hdr,
                                           sizeof(committed_hdr));
    }
  }

  // Free the memory allocated for the encrypted data.
  sl_free(ptr_enc);

  // Return the status.
  return status;
}
#endif

/***************************************************************************//**
 * Writes a klv object into the chain.
 *
 * @note This function checks for available space and then writes the KLV
 * object at the tail of the chain. When overwriting an existing SDT entry,
 * the decoded @c header_size is preserved so an 8-byte header is not expanded
 * in place.
 *
 * A KLV objects are stored in sequence as shown below:
 * KLV chain representation:
 * ------------------------------------------------------------------
 * KLV_CHAIN_START | KLV_OBJECT_1 | KLV_OBJECT_2 | KLV_OBJECT_2 |...
 * ------------------------------------------------------------------
 * KLV_CHAIN_START is a 4-byte unique value that indicates the start of the
 * respective KLV chain. The KLV chain start address is aligned to 4 bytes.
 *
 * Each KLV object is stored as follows:
 * Static device token - stored in the SE MTP and therefore are not encrypted.
 * used for storing small token data:
 * ------------------------------------------------------------------
 * KLV Header (8 bytes) | Data (length bytes) |
 * ------------------------------------------------------------------
 * KLV Header :
 *   The KLV header is a 8-byte structure @ref sl_klv_header_t
 *
 * When using @ref sli_klv_obj_header_t (@ref SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE),
 * the KLV header is 16 bytes.
 * ------------------------------------------------------------------
 * KLV Header (16 bytes) | Data (length bytes) |
 * ------------------------------------------------------------------
 * KLV Header :
 *   The KLV header is a 16-byte structure @ref sli_klv_obj_header_t
 *
 * Static secure token - the data is encrypted using authenticated encryption (AEAD):
 * -----------------------------------------------------------------------------
 * KLV Header (8 bytes) | Nonce (4 bytes) | Data (length bytes) | Tag (4 bytes) |
 * -----------------------------------------------------------------------------
 * KLV Header :
 *   The KLV header is a 8-byte structure @ref sl_klv_header_t
 *
 * When using @ref sli_klv_obj_header_t (@ref SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE),
 * the KLV header is 16 bytes.
 * -------------------------------------------------------------------------------
 * KLV Header (16 bytes) | Nonce (4 bytes) | Data (length bytes) | Tag (4 bytes) |
 * -------------------------------------------------------------------------------
 * KLV Header :
 *   The KLV header is a 16-byte structure @ref sli_klv_obj_header_t
 *
 * Nonce :
 *    The nonce is a 4-bytes value generated randomly during encryption.
 *    Even though the stored nonce is 4-bytes, the actual nonce used for
 *    encryption is 12 bytes.
 *
 * Data:
 *  The data is the actual content of the KLV object. The length of the data
 *  is specified in the KLV header.
 *
 * Tag :
 *    The tag is a 4-byte value that is generated during encryption.
 *
 * @param[in]  klv_handle        A pointer to the sl_klv_handle_t,
 *                               containing length and key info
 * @param[in]  klv_start_address The KLV chain start address.
 * @param[in]  klv_end_address   The KLV chain end address.
 * @param[out] data              A pointer to the data being written.
 *
 * @return SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sli_write_klv_object(sl_klv_handle_t const *klv_handle,
                                 uint32_t klv_start_address,
                                 uint32_t klv_end_address,
                                 void const *data)
{
  // Validate input parameters
  if ((klv_handle == NULL) || ((uint32_t *)klv_start_address == NULL) || ((uint32_t *)klv_end_address == NULL) || (data == NULL)) {
    TOKENDBG(printf("Null pointer error in sli_write_klv_object\n"));
    return SL_STATUS_NULL_POINTER;
  }

  if ((klv_handle->length == 0) || (klv_handle->length == 0xFFFF) || klv_handle->key == 0xFFFF) {
    TOKENDBG(printf("Invalid KLV handle parameters\n"));
    return SL_STATUS_INVALID_PARAMETER;
  }

  if ((klv_start_address % 4 != 0) || (klv_end_address % 4 != 0) || (klv_start_address >= klv_end_address)) {
    TOKENDBG(printf("Invalid address alignment or range\n"));
    return SL_STATUS_INVALID_RANGE;
  }

  uint32_t klv_main_start = *((uint32_t *)klv_start_address);
  if ((klv_main_start != SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START)
      && (klv_main_start != SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START)) {
    // KLV chain is not initialized, need to initialize it.
    TOKENDBG(printf("Initializing KLV chain\n"));
    sl_status_t init_status = sli_token_manager_init_klv_chain(klv_start_address, &klv_main_start);
    if (init_status != SL_STATUS_OK) {
      return init_status;
    }
  }

  // Walk the KLV chain to either find the existing KLV object or the next free space.
  uint32_t *current_pointer =
    (uint32_t *)check_token_in_klv_list((klv_handle->key & 0xFFFF),
                                        (uint32_t *)klv_start_address,
                                        (uint32_t *)klv_end_address);

  if (current_pointer >= (uint32_t *)klv_end_address) {
    // The KLV chain is full, return an error.
    TOKENDBG(printf("KLV chain is full, no more space\n"));
    return SL_STATUS_FULL;
  }

  bool preserve_header_size = false;
  bool has_existing_object = false;
  uint32_t write_header_size = (uint32_t)sizeof(sli_klv_obj_header_t);
  sli_klv_hdr_parse_t existing_header = { 0 };
  if (SLI_KLV_ADDRESS_HAS_PROGRAMMED_HEADER((const uint8_t *)current_pointer)) {
    // Check if the existing KLV object can be overwritten.
    sl_status_t overwrite_status = sli_token_manager_check_klv_overwrite(klv_main_start,
                                                                         current_pointer,
                                                                         klv_handle,
                                                                         data,
                                                                         &existing_header);

    // If the existing KLV object cannot be overwritten, return an error.
    if ((overwrite_status != SL_STATUS_OK) && (overwrite_status != SL_STATUS_NOT_FOUND)) {
      TOKENDBG(printf("Failed to check if the existing KLV object can be overwritten: %ld\n", overwrite_status));
      return overwrite_status;
    }

    // If the existing KLV object can be overwritten, set the flag.
    has_existing_object = (overwrite_status == SL_STATUS_OK);

    // If the existing KLV object can be overwritten and the header size is 8 bytes,
    // set the flag to preserve the header size.
    if (has_existing_object
        && (existing_header.header_size == SLI_TOKEN_KLV_HEADER_SIZE)) {
      preserve_header_size = true;
      write_header_size = (uint32_t)SLI_TOKEN_KLV_HEADER_SIZE;
    }
  }

  uint32_t security_offset = SLI_TOKEN_MANAGER_GET_SECURITY_OFFSET((uint32_t *)klv_start_address);
  // Must be a multiple of 4 bytes
  uint32_t write_length_bytes = SL_CEILING(klv_handle->length, sizeof(uint32_t));
  if ((uint32_t *)((uint8_t *)current_pointer + write_header_size + write_length_bytes + security_offset)
      >= (uint32_t *)klv_end_address) {
    TOKENDBG(printf("The token cannot fit in the available space in the KLV chain\n"));
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  uint16_t data_crc = 0;
  // Calculate CRC for only Static device token.
  // Static secure token data is encrypted using authenticated encryption (AEAD)
  // and therefore CRC is not calculated.
  if (klv_main_start == SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START) {
    data_crc = calculate_checksum(data, klv_handle->length);
  }

  sli_klv_obj_header_t obj_hdr;
  // Initialize the KLV header.
  SLI_KLV_INIT_HDR_FOR_WRITE(obj_hdr, klv_handle, data_crc);

  if (klv_main_start == SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START) {
#if defined(SL_TOKEN_MANAGER_SECURITY)
    // Write the secure KLV object to the chain.
    return sli_token_manager_write_secure_klv((uint8_t *)current_pointer, &obj_hdr, data, write_length_bytes);
#else
    if ((sli_token_manager_nvm_write((uint32_t)current_pointer,
                                     (const uint8_t *)&obj_hdr,
                                     sizeof(obj_hdr)) != SL_STATUS_OK)
        || (sli_token_manager_nvm_write((uint32_t)((uint8_t *)current_pointer + sizeof(obj_hdr)),
                                        (const uint8_t *)data,
                                        write_length_bytes) != SL_STATUS_OK)) {
      return SL_STATUS_FAIL;
    }
    // Clear the write-in-progress bit (bit 7) and re-write the header to
    // mark the object as committed/readable.
    sli_klv_obj_header_t committed_hdr = obj_hdr;
    committed_hdr.flags &= (uint8_t)(~SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK);
    return sli_token_manager_nvm_write((uint32_t)current_pointer,
                                       (const uint8_t *)&committed_hdr,
                                       sizeof(committed_hdr));
#endif
  } else {
    // Write the static device KLV object to the chain.
    if (preserve_header_size) {
      // Rewrite the device KLV object in-place while preserving the 8-byte header size.
      return sli_token_manager_rewrite_device_klv_inplace((uint8_t *)current_pointer,
                                                          klv_handle,
                                                          data,
                                                          write_length_bytes);
    }

    // Write the new KLV object to the chain.
    obj_hdr.flags = obj_hdr.flags & (~SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK);
    memcpy(current_pointer, &obj_hdr, sizeof(obj_hdr));
    memcpy((uint8_t *)current_pointer + sizeof(obj_hdr), data, write_length_bytes);
  }

  return SL_STATUS_OK;
}
#endif
#endif // SLI_TOKEN_MANAGER_KLV_COMMON_SUPPORT
