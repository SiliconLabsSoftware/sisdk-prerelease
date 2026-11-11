/***************************************************************************//**
 * @file
 * @brief SL Diagnostic internal definitions
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

#ifndef SLI_DIAG_INTERNAL_H
#define SLI_DIAG_INTERNAL_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sl_status.h"
#include "sl_diag.h"
#include "sl_diag_format.h"
#include "sli_diag.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SL_DIAG_ENABLE

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/// ID of the core that owns the retained region. Platform-owned, not user
/// configuration. 0 because supported platforms have a single participating
/// host core.
#define SLI_DIAG_CORE_ID  0u

/// Control block magic.
#define SLI_DIAG_CTRL_MAGIC       0x534C4447u
/// Record slot magic.
#define SLI_DIAG_RECORD_MAGIC     0x52454344u
#if SL_DIAG_SUMMARY_RING_ENABLE
/// Summary Ring magic.
#define SLI_DIAG_SUMMARY_MAGIC    0x53554D4Du
#endif
#if SL_DIAG_EMERGENCY_ENABLE
/// Mini Emergency Tail magic.
#define SLI_DIAG_EMERGENCY_MAGIC  0x454D4552u
#endif

/// Commit token, written last to commit a record.
#define SLI_DIAG_COMMIT_TOKEN    0x434D4954u

/// Record format version.
#define SLI_DIAG_FORMAT_VERSION  1u

/// Payload version of the core TLVs (0x0001 to 0x0005).
#define SLI_DIAG_CORE_TLV_VERSION  1u

/// CRC-32 initial state.
#define SLI_DIAG_CRC32_INIT    0xFFFFFFFFu
/// CRC-32 final XOR value.
#define SLI_DIAG_CRC32_XOROUT  0xFFFFFFFFu

/// Writer priority of administrative mutations (boot validation and clear).
/// Lower-priority than every capture trigger. Numerically greater than all trigger priorities.
#define SLI_DIAG_PRIORITY_ADMIN  UINT8_MAX

/// Writer priority when no capture holds retained-region ownership.
#define SLI_DIAG_WRITER_PRIORITY_IDLE  SLI_DIAG_PRIORITY_ADMIN

/// No slot selected.
#define SLI_DIAG_SLOT_NONE  UINT32_MAX

#if SL_DIAG_SUMMARY_RING_ENABLE
/// Number of Summary Ring entries.
#define SLI_DIAG_SUMMARY_RING_CAPACITY  32u

/// Summary Ring entries copied by a normal capture.
#define SLI_DIAG_SUMMARY_RING_TAIL_NORMAL   8u
/// Summary Ring entries copied by a minimal or watchdog capture.
#define SLI_DIAG_SUMMARY_RING_TAIL_MINIMAL  3u
#endif // SL_DIAG_SUMMARY_RING_ENABLE

/// TLV header size in bytes.
#define SLI_DIAG_TLV_HDR_BYTES  6u

/// Linked providers, one X(provider ID, budgeted max_output_bytes) entry each.
/// Only these providers can register, and none may declare more output than
/// its entry. The count, table and byte total below are derived from this
/// list, so they cannot diverge. Adding a provider changes the slot size and
/// therefore the retained layout signature.
///
/// The list is defined in sl_diag_linked_providers_autogen.h (SLC project
/// generate) or sl_diag_linked_providers_default.h (non-SLC builds). Stack
/// teams add providers via template_contribution sl_diag_linked_provider in
/// their component .slcc; platform does not edit this header per provider.
#if defined(__has_include)
  #if __has_include("sl_diag_linked_providers_autogen.h")
    #include "sl_diag_linked_providers_autogen.h"
  #else
    #include "sl_diag_linked_providers_default.h"
  #endif
#else
  #include "sl_diag_linked_providers_default.h"
#endif

/// Adds 1 per linked provider.
#define SLI_DIAG_LINKED_PROVIDER_ADD_ONE(id_, bytes_)        + 1u
/// Adds the encoded size of a linked provider TLV, header included.
#define SLI_DIAG_LINKED_PROVIDER_ADD_TLV_BYTES(id_, bytes_)  + (SLI_DIAG_TLV_HDR_BYTES + (bytes_))
/// Table entry of linked provider. Provider ID and budgeted max_output_bytes.
#define SLI_DIAG_LINKED_PROVIDER_ENTRY(id_, bytes_)          { (id_), (bytes_) },

/// Number of linked providers.
#define SLI_DIAG_LINKED_PROVIDER_COUNT \
  (0u SLI_DIAG_LINKED_PROVIDER_LIST(SLI_DIAG_LINKED_PROVIDER_ADD_ONE))

/// Linked provider table entries, { provider ID, budgeted max_output_bytes }.
#define SLI_DIAG_LINKED_PROVIDER_ENTRIES \
  SLI_DIAG_LINKED_PROVIDER_LIST(SLI_DIAG_LINKED_PROVIDER_ENTRY)

/// Maximum encoded size of all linked provider TLVs, headers included.
#define SLI_DIAG_LINKED_PROVIDER_TLV_BYTES \
  (0u SLI_DIAG_LINKED_PROVIDER_LIST(SLI_DIAG_LINKED_PROVIDER_ADD_TLV_BYTES))

/// Provider registry capacity. Only providers in SLI_DIAG_LINKED_PROVIDER_ENTRIES
/// can register, so every registered provider has retained space and an
/// unlinked provider cannot take a linked provider's entry.
#define SLI_DIAG_PROVIDER_REGISTRY_CAPACITY  SLI_DIAG_LINKED_PROVIDER_COUNT

#if SL_DIAG_INCIDENT_METADATA_ENABLE
/// Trigger, Reset/Boot and Build TLV sizes, headers included.
#define SLI_DIAG_METADATA_TLV_BYTES                                      \
  ((SLI_DIAG_TLV_HDR_BYTES + sizeof(sl_diag_trigger_tlv_payload_t))      \
   + (SLI_DIAG_TLV_HDR_BYTES + sizeof(sl_diag_reset_boot_tlv_payload_t)) \
   + (SLI_DIAG_TLV_HDR_BYTES + sizeof(sl_diag_build_tlv_payload_t)))
#else
#define SLI_DIAG_METADATA_TLV_BYTES  0u
#endif

#if SL_DIAG_SUMMARY_RING_ENABLE
/// Recent-events TLV size for a normal capture, header included.
#define SLI_DIAG_RECENT_EVENTS_TLV_BYTES \
  (SLI_DIAG_TLV_HDR_BYTES                \
   + (SLI_DIAG_SUMMARY_RING_TAIL_NORMAL * sizeof(sl_diag_ring_entry_t)))
#else
#define SLI_DIAG_RECENT_EVENTS_TLV_BYTES  0u
#endif

/// Provider status TLV size, header included.
#define SLI_DIAG_PROVIDER_STATUS_TLV_BYTES \
  (SLI_DIAG_TLV_HDR_BYTES + sizeof(sl_diag_provider_status_tlv_payload_t))

/// Core TLV bytes per record: metadata, recent events and provider status.
#define SLI_DIAG_CORE_TLV_OVERHEAD_BYTES \
  (SLI_DIAG_METADATA_TLV_BYTES           \
   + SLI_DIAG_RECENT_EVENTS_TLV_BYTES    \
   + (SLI_DIAG_LINKED_PROVIDER_COUNT * SLI_DIAG_PROVIDER_STATUS_TLV_BYTES))

/// Rounds a byte count up to a multiple of 4.
#define SLI_DIAG_ALIGN_UP_4(value)   (((value) + 3u) & ~3u)

/// TLV bytes one record needs, excluding the slot header.
#define SLI_DIAG_SLOT_REQUIRED_BYTES \
  (SLI_DIAG_CORE_TLV_OVERHEAD_BYTES + SLI_DIAG_LINKED_PROVIDER_TLV_BYTES)

/// TLV capacity of one record slot, excluding the slot header. Rounded up so
/// the slot has no compiler tail padding.
#define SLI_DIAG_SLOT_MAX_BYTES   SLI_DIAG_ALIGN_UP_4(SLI_DIAG_SLOT_REQUIRED_BYTES)

/// Record slot header size in bytes. The exported record starts with it.
#define SLI_DIAG_RECORD_HEADER_BYTES  32u

/// Retained layout signature stored in the control block profile_flags.
/// A mismatch at boot means the retained layout changed and the region is
/// re-initialized.
#define SLI_DIAG_PROFILE_FLAGS                     \
  ((uint32_t)SL_DIAG_PROFILE                       \
   | ((uint32_t)SL_DIAG_SLOT_COUNT << 8)           \
   | ((uint32_t)SL_DIAG_SUMMARY_RING_ENABLE << 12) \
   | ((uint32_t)SL_DIAG_EMERGENCY_ENABLE << 13)    \
   | (((uint32_t)SLI_DIAG_SLOT_MAX_BYTES & 0xFFFFu) << 16))

/*******************************************************************************
 ********************************   TYPES   ************************************
 ******************************************************************************/

/// Action for a new incident against the committed record.
SL_ENUM(sli_diag_record_action_t) {
  SLI_DIAG_WRITE_NEW = 0,  ///< Build a new record.
  SLI_DIAG_REPEAT    = 1,  ///< Increment repeat_count of the committed record.
  SLI_DIAG_PRESERVE  = 2   ///< Keep the committed record unchanged.
};

/// TLV writer. Providers only see sli_diag_writer_t.
struct sli_diag_writer {
  volatile uint8_t *cursor;  ///< Next write position in the retained slot.
  volatile uint8_t *end;     ///< End of the writable area.
  uint16_t blocks_written;   ///< TLV blocks written.
  uint32_t bytes_written;    ///< Bytes written.
  uint32_t owner_token;      ///< Writer ownership token.
};

/// Summary of a provider run, for the capture entry point that decides the
/// final record state.
typedef struct {
  uint16_t evaluated_count;     ///< Selected, linked and trigger-eligible providers.
  uint16_t not_ok_count;        ///< Evaluated providers with a non-OK outcome (excludes REDACTED).
  bool budget_limited;          ///< The byte or time budget withheld evidence or was exceeded.
  bool status_tlv_dropped;      ///< A Provider status TLV did not fit.
  bool latency_overrun;         ///< A callback measured past its max_latency_us.
  bool time_unmeasured;         ///< A callback's elapsed time was not measured reliably.
} sli_diag_provider_run_summary_t;

/// Decoded TLV header.
typedef struct {
  uint16_t type;            ///< TLV type.
  uint8_t version;          ///< Payload version.
  uint16_t length;          ///< Payload length in bytes.
  uint32_t payload_offset;  ///< Payload offset in the TLV stream.
} sli_diag_tlv_info_t;

/// Non-retained copy of a record slot header. Same layout as the first
/// SLI_DIAG_RECORD_HEADER_BYTES bytes of sli_diag_record_slot_t.
typedef struct {
  uint32_t magic;                ///< SLI_DIAG_RECORD_MAGIC.
  uint16_t format_version;       ///< Record format version.
  sl_diag_record_state_t state;  ///< Record state.
  uint8_t reserved;              ///< Reserved, written as 0.
  uint32_t generation;           ///< Record generation.
  uint32_t total_size;           ///< Valid bytes in tlv_data.
  uint16_t block_count;          ///< TLV blocks in tlv_data.
  uint16_t repeat_count;         ///< Repeat count, or the reserved field.
  uint32_t header_crc;           ///< Header CRC.
  uint32_t payload_crc;          ///< Payload CRC.
  uint32_t commit_token;         ///< Commit token.
} sli_diag_record_header_t;

#if SL_DIAG_SUMMARY_RING_ENABLE
/// Summary Ring.
typedef struct {
  uint32_t magic;                                               ///< SLI_DIAG_SUMMARY_MAGIC.
  uint16_t head;                                                ///< Next write index.
  uint16_t count;                                               ///< Number of valid entries.
  sl_diag_ring_entry_t entries[SLI_DIAG_SUMMARY_RING_CAPACITY]; ///< Entries.
} sli_diag_summary_ring_t;
#endif // SL_DIAG_SUMMARY_RING_ENABLE

#if SL_DIAG_EMERGENCY_ENABLE
/// Mini Emergency Tail. Minimal crash evidence written before the record
/// commit completes. Not a TLV.
typedef struct {
  uint8_t core_id;            ///< Core ID.
  uint8_t reserved0[3];       ///< Reserved, written as 0.
  uint32_t magic;             ///< SLI_DIAG_EMERGENCY_MAGIC.
  sl_diag_trigger_t trigger;  ///< Capture trigger.
  uint8_t reserved1[3];       ///< Reserved, written as 0.
  uint32_t reason_id;         ///< Trigger-specific reason ID.
  uint32_t detail;            ///< Trigger-specific detail.
  uint32_t pc;                ///< Program counter.
  uint32_t lr;                ///< Link register.
  uint32_t sp;                ///< Stack pointer.
  uint32_t uptime_ms;         ///< Uptime at capture in milliseconds.
  uint32_t boot_id;           ///< Boot ID.
  uint32_t emergency_crc;     ///< CRC of this structure.
} sli_diag_mini_emergency_tail_t;
#endif // SL_DIAG_EMERGENCY_ENABLE

/// Record slot: header followed by the TLV stream.
typedef struct {
  uint32_t magic;                            ///< SLI_DIAG_RECORD_MAGIC.
  uint16_t format_version;                   ///< Record format version.
  sl_diag_record_state_t state;              ///< Record state.
  uint8_t reserved;                          ///< Reserved, written as 0.
  uint32_t generation;                       ///< Record generation.
  uint32_t total_size;                       ///< Valid bytes in tlv_data.
  uint16_t block_count;                      ///< TLV blocks in tlv_data.
#if SL_DIAG_REPEAT_COUNT_ENABLE
  uint16_t repeat_count;                     ///< Repeats of the same incident.
#else
  uint16_t reserved0;                        ///< Reserved.
#endif
  uint32_t header_crc;                       ///< Header CRC.
  uint32_t payload_crc;                      ///< CRC of tlv_data[0..total_size).
  uint32_t commit_token;                     ///< Written last to commit the record.
  uint8_t tlv_data[SLI_DIAG_SLOT_MAX_BYTES]; ///< TLV stream.
} sli_diag_record_slot_t;

/// Control block.
typedef struct {
  uint8_t core_id;         ///< Core ID.
  uint8_t reserved[3];     ///< Reserved, written as 0.
  uint32_t magic;          ///< SLI_DIAG_CTRL_MAGIC.
  uint32_t profile_flags;  ///< Profile and feature flags of the region.
  uint32_t active_slot;    ///< Active slot index.
  uint32_t generation;     ///< Generation of the last committed record.
  uint32_t boot_id;        ///< Boot ID.
  uint32_t ctrl_crc;       ///< Control block CRC.
} sli_diag_control_t;

/// Retained region. Placed in .sl_diag_retained.noinit.
typedef struct {
  sli_diag_control_t ctrl;                             ///< Control block.
  sli_diag_record_slot_t slot[SL_DIAG_SLOT_COUNT];     ///< Record slots.
#if SL_DIAG_SUMMARY_RING_ENABLE
  sli_diag_summary_ring_t summary_ring;                ///< Summary Ring.
#endif
#if SL_DIAG_EMERGENCY_ENABLE
  sli_diag_mini_emergency_tail_t mini_emergency_tail;  ///< Mini Emergency Tail.
#endif
} sli_diag_retained_region_t;

/// Retained region size in bytes.
#define SLI_DIAG_REGION_BYTES  sizeof(sli_diag_retained_region_t)

/*******************************************************************************
 ****************************   LAYOUT CHECKS   ********************************
 *
 * Compile-time checks that retained structures have no hidden padding and
 * that member offsets match the on-wire format. The retained object in
 * sl_diag.c is placed in .sl_diag_retained.noinit; its size is
 * SLI_DIAG_REGION_BYTES. The linker script reserves one object in that section,
 * so the region cannot grow past the allotted RAM without a link failure.
 ******************************************************************************/

static_assert(SLI_DIAG_TLV_HDR_BYTES == sizeof(sl_diag_tlv_header_t), "SLI_DIAG_TLV_HDR_BYTES mismatch");

#if SL_DIAG_SUMMARY_RING_ENABLE
static_assert(offsetof(sli_diag_summary_ring_t, head) == 4u, "sli_diag_summary_ring_t layout");
static_assert(offsetof(sli_diag_summary_ring_t, count) == 6u, "sli_diag_summary_ring_t layout");
static_assert(offsetof(sli_diag_summary_ring_t, entries) == 8u, "sli_diag_summary_ring_t header must be 8 bytes");
static_assert(sizeof(sli_diag_summary_ring_t)
              == (8u + (SLI_DIAG_SUMMARY_RING_CAPACITY * sizeof(sl_diag_ring_entry_t))),
              "sli_diag_summary_ring_t size");
#endif

#if SL_DIAG_EMERGENCY_ENABLE
static_assert(sizeof(sli_diag_mini_emergency_tail_t) == 44u, "sli_diag_mini_emergency_tail_t must be 44 bytes");
static_assert(offsetof(sli_diag_mini_emergency_tail_t, magic) == 4u, "sli_diag_mini_emergency_tail_t layout");
static_assert(offsetof(sli_diag_mini_emergency_tail_t, trigger) == 8u, "sli_diag_mini_emergency_tail_t layout");
static_assert(offsetof(sli_diag_mini_emergency_tail_t, reason_id) == 12u, "sli_diag_mini_emergency_tail_t layout");
static_assert(offsetof(sli_diag_mini_emergency_tail_t, emergency_crc) == 40u, "sli_diag_mini_emergency_tail_t layout");
#endif

static_assert(offsetof(sli_diag_record_slot_t, state) == 6u, "sli_diag_record_slot_t layout");
static_assert(offsetof(sli_diag_record_slot_t, generation) == 8u, "sli_diag_record_slot_t layout");
static_assert(offsetof(sli_diag_record_slot_t, block_count) == 16u, "sli_diag_record_slot_t layout");
static_assert(offsetof(sli_diag_record_slot_t, header_crc) == 20u, "sli_diag_record_slot_t layout");
static_assert(offsetof(sli_diag_record_slot_t, payload_crc) == 24u, "sli_diag_record_slot_t layout");
static_assert(offsetof(sli_diag_record_slot_t, commit_token) == 28u, "sli_diag_record_slot_t layout");
static_assert(offsetof(sli_diag_record_slot_t, tlv_data) == 32u, "sli_diag_record_slot_t header must be 32 bytes");
static_assert(sizeof(sli_diag_record_slot_t) == (32u + SLI_DIAG_SLOT_MAX_BYTES),
              "Unexpected padding in sli_diag_record_slot_t");
static_assert(offsetof(sli_diag_record_slot_t, tlv_data) == SLI_DIAG_RECORD_HEADER_BYTES,
              "SLI_DIAG_RECORD_HEADER_BYTES mismatch");

static_assert(sizeof(sli_diag_record_header_t) == SLI_DIAG_RECORD_HEADER_BYTES,
              "sli_diag_record_header_t must match the slot header");
static_assert(offsetof(sli_diag_record_header_t, format_version) == offsetof(sli_diag_record_slot_t, format_version),
              "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, state) == offsetof(sli_diag_record_slot_t, state),
              "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, generation) == offsetof(sli_diag_record_slot_t, generation),
              "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, total_size) == offsetof(sli_diag_record_slot_t, total_size),
              "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, block_count) == offsetof(sli_diag_record_slot_t, block_count),
              "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, repeat_count) == 18u, "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, header_crc) == offsetof(sli_diag_record_slot_t, header_crc),
              "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, payload_crc) == offsetof(sli_diag_record_slot_t, payload_crc),
              "sli_diag_record_header_t layout");
static_assert(offsetof(sli_diag_record_header_t, commit_token) == offsetof(sli_diag_record_slot_t, commit_token),
              "sli_diag_record_header_t layout");

// Field ranges of SLI_DIAG_PROFILE_FLAGS.
static_assert(SL_DIAG_PROFILE <= 0xFFu, "SL_DIAG_PROFILE does not fit profile_flags");
static_assert(SL_DIAG_SLOT_COUNT <= 0xFu, "SL_DIAG_SLOT_COUNT does not fit profile_flags");
static_assert((SL_DIAG_EMERGENCY_ENABLE == 0) || (SL_DIAG_EMERGENCY_ENABLE == 1),
              "SL_DIAG_EMERGENCY_ENABLE must be 0 or 1");
static_assert(SLI_DIAG_SLOT_MAX_BYTES <= 0xFFFFu, "SLI_DIAG_SLOT_MAX_BYTES does not fit profile_flags");

static_assert(sizeof(sli_diag_control_t) == 28u, "sli_diag_control_t must be 28 bytes");
static_assert(offsetof(sli_diag_control_t, magic) == 4u, "sli_diag_control_t layout");
static_assert(offsetof(sli_diag_control_t, ctrl_crc) == 24u, "sli_diag_control_t layout");

// The retained region must not contain padding between or after its members.
static_assert(offsetof(sli_diag_retained_region_t, slot) == sizeof(sli_diag_control_t),
              "sli_diag_retained_region_t layout");
#if SL_DIAG_SUMMARY_RING_ENABLE
static_assert(offsetof(sli_diag_retained_region_t, summary_ring)
              == (offsetof(sli_diag_retained_region_t, slot) + (SL_DIAG_SLOT_COUNT * sizeof(sli_diag_record_slot_t))),
              "sli_diag_retained_region_t layout");
#endif
#if SL_DIAG_EMERGENCY_ENABLE && SL_DIAG_SUMMARY_RING_ENABLE
static_assert(offsetof(sli_diag_retained_region_t, mini_emergency_tail)
              == (offsetof(sli_diag_retained_region_t, summary_ring) + sizeof(sli_diag_summary_ring_t)),
              "sli_diag_retained_region_t layout");
#elif SL_DIAG_EMERGENCY_ENABLE
static_assert(offsetof(sli_diag_retained_region_t, mini_emergency_tail)
              == (offsetof(sli_diag_retained_region_t, slot) + (SL_DIAG_SLOT_COUNT * sizeof(sli_diag_record_slot_t))),
              "sli_diag_retained_region_t layout");
#endif
#if SL_DIAG_EMERGENCY_ENABLE
static_assert((offsetof(sli_diag_retained_region_t, mini_emergency_tail) + sizeof(sli_diag_mini_emergency_tail_t))
              == SLI_DIAG_REGION_BYTES,
              "sli_diag_retained_region_t layout");
#elif SL_DIAG_SUMMARY_RING_ENABLE
static_assert((offsetof(sli_diag_retained_region_t, summary_ring) + sizeof(sli_diag_summary_ring_t))
              == SLI_DIAG_REGION_BYTES,
              "sli_diag_retained_region_t layout");
#else
static_assert((offsetof(sli_diag_retained_region_t, slot) + (SL_DIAG_SLOT_COUNT * sizeof(sli_diag_record_slot_t)))
              == SLI_DIAG_REGION_BYTES,
              "sli_diag_retained_region_t layout");
#endif

/*******************************************************************************
 ******************************   PROTOTYPES   *********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Gets the retained region.
 *
 * @return Retained region.
 ******************************************************************************/
volatile sli_diag_retained_region_t *sli_diag_get_retained_region(void);

/***************************************************************************//**
 * @brief Validates the record slots and selects the active one.
 *
 * @param[in] owner_token Writer ownership token.
 * @param[in] preferred_slot Slot preferred when generations are equal, or
 *            SLI_DIAG_SLOT_NONE.
 * @param[out] selected_slot Newest readable slot, or SLI_DIAG_SLOT_NONE.
 * @param[out] selected_generation Generation of the selected slot.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 *
 * @note Slots that fail validation are marked CORRUPT. Their header CRC is
 *       not recomputed.
 ******************************************************************************/
sl_status_t sli_diag_validate_and_select_active_slot(uint32_t owner_token,
                                                     uint32_t preferred_slot,
                                                     uint32_t *selected_slot,
                                                     uint32_t *selected_generation);

/***************************************************************************//**
 * @brief Checks whether a new incident may replace the current record.
 *
 * @param[in] req Capture request.
 *
 * @return true if the action is SLI_DIAG_WRITE_NEW, false otherwise.
 ******************************************************************************/
bool sli_diag_may_overwrite(const sli_diag_capture_request_t *req);

/***************************************************************************//**
 * @brief Gets the action for a new incident against the committed record.
 *
 * @param[in] req Capture request.
 *
 * @return SLI_DIAG_WRITE_NEW, SLI_DIAG_REPEAT or SLI_DIAG_PRESERVE.
 *
 * @note Call while owning the writer, so the committed record cannot change.
 ******************************************************************************/
sli_diag_record_action_t sli_diag_get_record_action(const sli_diag_capture_request_t *req);

/***************************************************************************//**
 * @brief Selects the slot for a new record.
 *
 * @return Slot 0 with one slot, otherwise the inactive slot.
 ******************************************************************************/
volatile sli_diag_record_slot_t *sli_diag_select_target_slot(void);

/***************************************************************************//**
 * @brief Gets the capture profile of a trigger.
 *
 * @param[in] trigger Capture trigger.
 *
 * @return Capture profile, or NULL if none is defined, including for
 *         SL_DIAG_TRIGGER_NONE and out-of-range values.
 *
 * @note Profiles are static and constant. Each lists provider IDs in
 *       execution order, the maximum capture depth and the total time budget.
 *       A listed provider that is not registered is not run and emits no TLV.
 ******************************************************************************/
const sli_diag_capture_profile_t *sli_diag_lookup_profile(sl_diag_trigger_t trigger);

/***************************************************************************//**
 * @brief Runs the providers of a capture profile into a record.
 *
 * @param[in,out] writer TLV writer of the record being built.
 * @param[in] profile Capture profile from sli_diag_lookup_profile().
 * @param[in] depth Requested capture depth. Clamped to profile->max_depth.
 * @param[in] context_flags SLI_DIAG_CONTEXT_* flags of the calling context.
 * @param[in] scratch Passed to providers as ctx->scratch, or NULL.
 * @param[out] result Run summary. Set unless bad input is returned.
 *
 * @return SL_STATUS_OK when orchestration completed: every listed provider
 *         was evaluated and ownership was kept. It does not mean that any
 *         provider captured successfully; provider outcomes are reported in
 *         result and in Provider status TLVs.
 *         SL_STATUS_ABORT if writer ownership was lost; result covers the
 *         providers evaluated before that,
 *         SL_STATUS_NULL_POINTER or SL_STATUS_INVALID_PARAMETER on bad input.
 *
 * @note Each listed provider is handled in profile order:
 *       1. Not registered, not eligible for the trigger, or already
 *          evaluated in this capture: skipped, no TLV.
 *       2. Context: SKIPPED unless the provider supports every flag in
 *          context_flags.
 *       3. Dependency: SKIPPED unless every dependency's callback ran
 *          earlier in this capture and did not return FAILED.
 *       4. Privacy: REDACTED if privacy_class is above
 *          SL_DIAG_MAX_PRIVACY_CLASS.
 *       5. Bytes: TRUNCATED if one TLV header plus max_output_bytes does not
 *          fit the record.
 *       6. Time: SKIPPED if max_latency_us exceeds the remaining time budget.
 *       7. The callback runs. A value outside sl_diag_provider_status_t, or
 *          OK without exactly one TLV of the provider's ID and version, is
 *          reported as FAILED.
 *       8. Any non-OK outcome emits a Provider status TLV. If it does not
 *          fit, result->status_tlv_dropped is set.
 *       A provider that fails a gate is not called.
 * @note Differences from the HLD orchestrator pseudocode, kept for safety:
 *       the context gate requires every caller flag rather than any one, so
 *       a provider that is not fault-safe never runs in fault context; the
 *       byte gate also counts the TLV header; and trigger eligibility is
 *       checked first, as in the Architecture provider descriptor.
 * @note A provider writes through a writer bounded to one TLV header plus
 *       max_output_bytes, so it cannot use another provider's space. The
 *       framing of an OK result is checked; the payload is the provider's
 *       responsibility. Bytes written before a non-OK result are kept.
 * @note Time accounting. The time budget only admits provider callbacks; it
 *       is not a deadline for the whole capture. Gate checks, ownership
 *       checks, core TLVs and Provider status TLVs are not charged. Each
 *       callback is charged one of:
 *       - Elapsed time, measured: the platform time counter is readable,
 *         enabled and advancing, no configurable interrupt can preempt the
 *         callback (PRIMASK set, or the NMI or HardFault handler), and the
 *         counter rate is the same before and after. Ticks are converted at
 *         that rate, rounded up. A callback longer than one counter wrap, or
 *         a debugger halt, is not detected.
 *       - Conservative: the counter advances but the callback could be
 *         preempted or the clock changed, so a handler may have slowed the
 *         clock or let the core sleep. The count at the slower endpoint
 *         clock is charged if it exceeds max_latency_us. This is not a
 *         guaranteed bound.
 *       - Declared, elapsed time unknown: max_latency_us. Used when there is
 *         no readable, advancing counter, and when the conservative count
 *         does not exceed max_latency_us.
 *       result->time_unmeasured is set if any charge was not measured. The
 *       HLD charges measured elapsed time; that holds only for measured
 *       charges. On DWT targets the cycle_counter driver supplies the counter
 *       and enables hardware at driver_init. SL Diagnostic does not modify
 *       debug registers during init or capture.
 * @note A callback that runs past its max_latency_us or the remaining budget
 *       is not interrupted and its outcome is kept. result->latency_overrun
 *       is set only for a measured overrun of the provider's own
 *       max_latency_us. result->budget_limited is set when a measured or
 *       conservative charge exceeds the remaining budget, so the record is
 *       INCOMPLETE even if no provider follows. A declared charge cannot
 *       show this. The remaining budget saturates at 0, so every later
 *       provider fails the time gate, because max_latency_us is at least 1.
 * @todo Measure callback time on cores without a DWT cycle counter, and in
 *       preemptible contexts, once a fault-safe time base is defined for
 *       them.
 * @todo Have the capture entry points bound their own time outside
 *       provider callbacks.
 * @note Does not commit the record or release ownership. Ownership is
 *       checked before each provider and at the end; after it is lost,
 *       nothing more is written.
 * @note Called by the crash and non-crash capture entry points, which own
 *       the record, write the core TLVs first, and choose VALID or
 *       INCOMPLETE from result when they finalize.
 ******************************************************************************/
sl_status_t sli_diag_run_providers(sli_diag_writer_t *writer,
                                   const sli_diag_capture_profile_t *profile,
                                   sli_diag_capture_depth_t depth,
                                   uint32_t context_flags,
                                   const void *scratch,
                                   sli_diag_provider_run_summary_t *result);

#if SL_DIAG_INCIDENT_METADATA_ENABLE
/***************************************************************************//**
 * @brief Writes the Trigger TLV.
 *
 * @details The Trigger TLV identifies the incident: trigger, reason_id,
 *          detail and the uptime at capture. The overwrite policy and repeat
 *          counting read trigger and reason_id back from it.
 *
 * @param[in,out] writer TLV writer.
 * @param[in] req Capture request.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_INVALID_PARAMETER if req->trigger is not a capture trigger,
 *         SL_STATUS_WOULD_OVERFLOW if the TLV does not fit, the record is
 *         unchanged,
 *         SL_STATUS_ABORT if writer ownership was lost,
 *         SL_STATUS_NULL_POINTER if writer or req is NULL.
 *
 * @note uptime_ms is read from sl_diag_get_uptime_ms() at write time.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_trigger(sli_diag_writer_t *writer,
                                       const sli_diag_capture_request_t *req);

/***************************************************************************//**
 * @brief Writes the Reset/Boot TLV.
 *
 * @details The Reset/Boot TLV holds the reset reasons and the boot ID of the
 *          boot in which the incident was captured, so the host can tell it
 *          apart from the boot in which the record is read.
 *
 * @param[in,out] writer TLV writer.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_WOULD_OVERFLOW if the TLV does not fit, the record is
 *         unchanged,
 *         SL_STATUS_ABORT if writer ownership was lost,
 *         SL_STATUS_NULL_POINTER if writer is NULL.
 *
 * @note The reset reasons describe the reset that started this boot, not the
 *       reset that follows the capture. They come from the boot latch, never
 *       from the reset-cause hardware.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_reset_boot(sli_diag_writer_t *writer);

/***************************************************************************//**
 * @brief Writes the Build TLV.
 *
 * @details The Build TLV identifies the firmware that captured the record,
 *          so the host decodes it with the matching image and dictionary.
 *
 * @param[in,out] writer TLV writer.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_WOULD_OVERFLOW if the TLV does not fit, the record is
 *         unchanged,
 *         SL_STATUS_ABORT if writer ownership was lost,
 *         SL_STATUS_NULL_POINTER if writer is NULL.
 *
 * @note build_id and dictionary_id come from sl_diag_get_build_info(), called
 *       at write time, in the capture context. reserved is 0.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_build(sli_diag_writer_t *writer);
#endif // SL_DIAG_INCIDENT_METADATA_ENABLE

#if SL_DIAG_SUMMARY_RING_ENABLE
/***************************************************************************//**
 * @brief Validates the Summary Ring at boot and adds the boot breadcrumbs.
 *
 * @param[in] owner_token Ownership token.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_INVALID_STATE if a breadcrumb could not be added,
 *         SL_STATUS_ABORT if ownership was lost.
 *         On failure, no breadcrumb was added.
 *
 * @note A valid ring keeps its entries across the reset. A ring with a wrong
 *       magic, or head or count out of range, is emptied. Then
 *       SL_DIAG_REASON_BOOT_RESET_REASON is added with the latched primary
 *       reset reason, and SL_DIAG_REASON_BOOT_RESET_REASON_EXT with the
 *       extended reason when it is not 0. Called once per boot, by
 *       sl_diag_init().
 ******************************************************************************/
sl_status_t sli_diag_init_summary_ring(uint32_t owner_token);

/***************************************************************************//**
 * @brief Empties the Summary Ring.
 *
 * @param[in] owner_token Ownership token.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 *
 * @note The ring stays valid and empty. The record is not changed.
 ******************************************************************************/
sl_status_t sli_diag_clear_summary_ring(uint32_t owner_token);

/***************************************************************************//**
 * @brief Writes the latest Summary Ring entries as a Recent-events TLV.
 *
 * @param[in,out] writer TLV writer.
 * @param[in] tail_entries Number of entries requested, at most
 *            SLI_DIAG_SUMMARY_RING_TAIL_NORMAL.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_WOULD_OVERFLOW if not all entries fit; the newest entries
 *         that fit are written, or nothing when no entry fits,
 *         SL_STATUS_INVALID_STATE if the ring is damaged, nothing is written,
 *         SL_STATUS_ABORT if writer ownership was lost,
 *         SL_STATUS_NULL_POINTER if writer is NULL.
 *
 * @note The payload is entry_count = length / 8 sl_diag_ring_entry_t
 *       entries, oldest first, without padding. An empty payload means the
 *       ring had no entries, or tail_entries was 0. The Summary Ring is not
 *       modified and keeps recording.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_recent_events(sli_diag_writer_t *writer,
                                             uint16_t tail_entries);
#endif // SL_DIAG_SUMMARY_RING_ENABLE

#if SL_DIAG_EMERGENCY_ENABLE
/***************************************************************************//**
 * @brief Writes the Mini Emergency Tail.
 *
 * @param[in] req Capture request of a crash trigger.
 * @param[in] scratch CPU crash context, or NULL. Source of pc, lr and sp,
 *            which are 0 without it.
 *
 * @note Not written if a valid tail of this boot with a higher or
 *       equal-priority trigger is kept. SLI_DIAG_CAPTURE_FORCE_OVERWRITE does
 *       not apply to the tail. A tail of an earlier boot is always replaced.
 *       A misaligned scratch is treated as NULL. magic is
 *       written last, so a tail cut by a reset or a nested capture is never
 *       valid. Does not need writer ownership. Never blocks or allocates.
 ******************************************************************************/
void sli_diag_stash_mini_emergency_tail(const sli_diag_capture_request_t *req,
                                        const sli_diag_cpu_crash_scratch_t *scratch);

/***************************************************************************//**
 * @brief Validates the Mini Emergency Tail at boot.
 *
 * @param[in] owner_token Ownership token.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost,
 *         SL_STATUS_INVALID_STATE if the tail cannot be read.
 *
 * @note A valid tail is kept. A damaged tail, such as one cut by a reset, is
 *       zeroed.
 ******************************************************************************/
sl_status_t sli_diag_validate_mini_emergency_tail(uint32_t owner_token);

/***************************************************************************//**
 * @brief Clears the Mini Emergency Tail.
 *
 * @param[in] owner_token Ownership token.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 ******************************************************************************/
sl_status_t sli_diag_clear_mini_emergency_tail(uint32_t owner_token);
#endif // SL_DIAG_EMERGENCY_ENABLE

/***************************************************************************//**
 * @brief Writes a Provider status TLV.
 *
 * @param[in,out] writer TLV writer.
 * @param[in] provider_id Provider ID.
 * @param[in] status Capture result.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_INVALID_PARAMETER if status is not a provider status,
 *         SL_STATUS_WOULD_OVERFLOW if the TLV does not fit, the record is
 *         unchanged,
 *         SL_STATUS_ABORT if writer ownership was lost,
 *         SL_STATUS_NULL_POINTER if writer is NULL.
 *
 * @note Only for providers selected by the active capture profile. Providers
 *       absent from the profile emit nothing.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_provider_status(sli_diag_writer_t *writer,
                                               uint16_t provider_id,
                                               sl_diag_provider_status_t status);

/***************************************************************************//**
 * @brief Finalizes and commits a record, then publishes it.
 *
 * @param[in] writer TLV writer used to build the record.
 * @param[in,out] slot Record slot prepared by sli_diag_prepare_record().
 * @param[in] incomplete true if not all requested evidence was captured.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost,
 *         SL_STATUS_NULL_POINTER or SL_STATUS_INVALID_PARAMETER on bad input.
 *
 * @note Releases writer ownership before returning.
 ******************************************************************************/
sl_status_t sli_diag_finalize_record(sli_diag_writer_t *writer,
                                     volatile sli_diag_record_slot_t *slot,
                                     bool incomplete);

/***************************************************************************//**
 * @brief Gets the priority of a trigger.
 *
 * @param[in] trigger Capture trigger.
 *
 * @return Trigger priority. Lower values have higher priority. UINT8_MAX for
 *         SL_DIAG_TRIGGER_NONE and out-of-range triggers.
 ******************************************************************************/
uint8_t sli_diag_get_trigger_priority(sl_diag_trigger_t trigger);

/*******************************************************************************
 ***************************   RESET AND BOOT   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Latches the reset cause of this boot.
 *
 * @details Keeps a RAM copy of the reset cause, so captures and boot
 *          breadcrumbs report it even after the application clears the
 *          reset-cause hardware.
 *
 * @note Calls sli_platform_latch_reset_boot() on the first call of the boot
 *       only. Later calls do nothing. Interrupts are masked during the call,
 *       so the value and the latched flag are published together.
 ******************************************************************************/
void sli_diag_latch_reset_boot(void);

/***************************************************************************//**
 * @brief Gets the reset cause of this boot.
 *
 * @param[out] out Latched reset causes. Both are 0 if nothing was latched.
 *
 * @note Never reads the reset-cause hardware, so it is safe in fault context.
 ******************************************************************************/
void sli_diag_get_reset_boot(sli_diag_reset_boot_t *out);

/***************************************************************************//**
 * @brief Gets the trigger assigned to this boot from its reset cause.
 *
 * @return SL_DIAG_TRIGGER_WATCHDOG_RESET if the latched reset cause reports a
 *         watchdog reset, SL_DIAG_TRIGGER_NONE otherwise or before
 *         sl_diag_init() completed.
 *
 * @note Classification only: no record is written. Devices without a known
 *       watchdog reset bit always return SL_DIAG_TRIGGER_NONE.
 * @note Reset-cause bits accumulate until the application clears them. A
 *       watchdog bit left from an earlier boot classifies a later boot as
 *       SL_DIAG_TRIGGER_WATCHDOG_RESET. Clearing after the latch does not
 *       change the latched value.
 * @note On devices with hard and soft reset-cause registers, no platform
 *       function is documented to clear them, so a stale watchdog cause can
 *       persist until the platform defines one.
 ******************************************************************************/
sl_diag_trigger_t sli_diag_get_boot_trigger(void);

/*******************************************************************************
 *************************   RECORD LIFECYCLE   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Checks whether sl_diag_init() completed in this boot.
 *
 * @return true if initialized, false otherwise.
 ******************************************************************************/
bool sli_diag_is_initialized(void);

/***************************************************************************//**
 * @brief Marks SL Diagnostic as initialized for this boot.
 ******************************************************************************/
void sli_diag_set_initialized(void);

/***************************************************************************//**
 * @brief Gets the boot ID of this boot.
 *
 * @return Boot ID from the control block, 0 before sl_diag_init() completed.
 *
 * @note The boot ID counts boots with a valid control block, modulo 2^32. It
 *       restarts at 1 when the retained region is rebuilt.
 ******************************************************************************/
uint32_t sli_diag_get_boot_id(void);

/***************************************************************************//**
 * @brief Acquires retained-region mutation ownership.
 *
 * @param[in] priority Writer priority. Lower values have higher priority.
 * @param[in] caller_context_flags SLI_DIAG_CONTEXT_* flags of the caller. Used
 *            so fault-context capture can preempt an equal-priority writer.
 * @param[out] owner_token Ownership token.
 *
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_BUSY if a writer of equal or higher priority is active,
 *         SL_STATUS_NOT_INITIALIZED if a capture is requested before
 *         sl_diag_init().
 *
 * @note A strictly higher priority takes ownership from the active writer.
 *       Fault context can preempt at equal priority. The preempted writer's
 *       next retained store returns SL_STATUS_ABORT.
 ******************************************************************************/
sl_status_t sli_diag_owner_acquire(uint8_t priority,
                                   uint32_t caller_context_flags,
                                   uint32_t *owner_token);

/***************************************************************************//**
 * @brief Releases retained-region mutation ownership.
 *
 * @param[in] owner_token Ownership token.
 *
 * @note Does nothing if the token is no longer the current owner.
 ******************************************************************************/
void sli_diag_owner_release(uint32_t owner_token);

/***************************************************************************//**
 * @brief Checks whether a token is still the current owner.
 *
 * @param[in] owner_token Ownership token.
 *
 * @return true if the token owns the writer, false otherwise.
 *
 * @note Ownership can be lost right after the call. Retained stores still
 *       check ownership themselves.
 ******************************************************************************/
bool sli_diag_owner_is_current(uint32_t owner_token);

/***************************************************************************//**
 * @brief Copies bytes into retained memory while still owning the writer.
 *
 * @param[in] owner_token Ownership token.
 * @param[out] dst Retained destination.
 * @param[in] src Source bytes.
 * @param[in] len Number of bytes.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 *         No byte is written after ownership is lost.
 *
 * @note Each byte is stored in its own critical section, so interrupt latency
 *       does not grow with len. Bytes stored before ownership was lost stay
 *       written. Use only for data that is not readable yet, such as the TLV
 *       stream of a WRITING record.
 ******************************************************************************/
sl_status_t sli_diag_owned_copy(uint32_t owner_token,
                                volatile uint8_t *dst,
                                const uint8_t *src,
                                uint32_t len);

/***************************************************************************//**
 * @brief Zeroes retained memory while still owning the writer.
 *
 * @param[in] owner_token Ownership token.
 * @param[out] dst Retained destination.
 * @param[in] len Number of bytes.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 *
 * @note Each byte is stored in its own critical section, like
 *       sli_diag_owned_copy().
 ******************************************************************************/
sl_status_t sli_diag_owned_zero(uint32_t owner_token,
                                volatile uint8_t *dst,
                                uint32_t len);

/***************************************************************************//**
 * @brief Copies retained bytes to normal memory.
 *
 * @param[out] dst Destination.
 * @param[in] src Retained source.
 * @param[in] len Number of bytes.
 *
 * @return SL_STATUS_OK if len bytes were copied, or if len is 0.
 *         SL_STATUS_NULL_POINTER if dst is NULL.
 *         SL_STATUS_INVALID_RANGE if [src, src + len) is not inside the
 *         retained region of the active profile (SLI_DIAG_REGION_BYTES).
 *
 * @note On error, dst is left unchanged.
 ******************************************************************************/
sl_status_t sli_diag_retained_read(uint8_t *dst,
                                   const volatile uint8_t *src,
                                   uint32_t len);

/***************************************************************************//**
 * @brief Reads the control block.
 *
 * @param[out] ctrl Control block copy.
 ******************************************************************************/
void sli_diag_read_control(sli_diag_control_t *ctrl);

/***************************************************************************//**
 * @brief Computes the control block CRC.
 *
 * @param[in] ctrl Control block copy.
 *
 * @return CRC over the control block, excluding ctrl_crc.
 ******************************************************************************/
uint32_t sli_diag_compute_control_crc(const sli_diag_control_t *ctrl);

/***************************************************************************//**
 * @brief Writes the control block with a recomputed ctrl_crc.
 *
 * @param[in] owner_token Ownership token.
 * @param[in] ctrl New control block content. ctrl_crc is ignored.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 ******************************************************************************/
sl_status_t sli_diag_write_control(uint32_t owner_token,
                                   const sli_diag_control_t *ctrl);

/***************************************************************************//**
 * @brief Reads a record slot header.
 *
 * @param[in] slot Record slot.
 * @param[out] header Header copy.
 ******************************************************************************/
void sli_diag_read_record_header(const volatile sli_diag_record_slot_t *slot,
                                 sli_diag_record_header_t *header);

/***************************************************************************//**
 * @brief Computes the record header CRC.
 *
 * @param[in] header Header copy.
 *
 * @return CRC over the header up to header_crc, then over payload_crc.
 ******************************************************************************/
uint32_t sli_diag_compute_header_crc(const sli_diag_record_header_t *header);

/***************************************************************************//**
 * @brief Validates a record slot.
 *
 * @param[in] slot Record slot.
 * @param[out] header Header copy used for the validation.
 *
 * @return SL_DIAG_STATE_EMPTY if magic is 0,
 *         SL_DIAG_STATE_VALID or SL_DIAG_STATE_INCOMPLETE if the slot is a
 *         readable committed record,
 *         SL_DIAG_STATE_WRITING if the slot is being written or changed
 *         during the validation,
 *         SL_DIAG_STATE_CORRUPT otherwise.
 ******************************************************************************/
sl_diag_record_state_t sli_diag_validate_slot(const volatile sli_diag_record_slot_t *slot,
                                              sli_diag_record_header_t *header);

/***************************************************************************//**
 * @brief Gets the published record if it is readable.
 *
 * @param[out] slot Published record slot.
 * @param[out] header Header copy of the published record.
 *
 * @return SL_STATUS_OK if the record is VALID or INCOMPLETE,
 *         SL_STATUS_NOT_FOUND if there is no record,
 *         SL_STATUS_INVALID_STATE if the record is being written, corrupt or
 *         changed during the call.
 ******************************************************************************/
sl_status_t sli_diag_get_readable_record(volatile sli_diag_record_slot_t **slot,
                                         sli_diag_record_header_t *header);

/***************************************************************************//**
 * @brief Prepares a slot as a WRITING record with the next generation.
 *
 * @param[in] owner_token Ownership token.
 * @param[out] slot Target slot from sli_diag_select_target_slot().
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 ******************************************************************************/
sl_status_t sli_diag_prepare_record(uint32_t owner_token,
                                    volatile sli_diag_record_slot_t *slot);

/***************************************************************************//**
 * @brief Initializes a TLV writer on the TLV area of a slot.
 *
 * @param[out] writer TLV writer.
 * @param[in] slot Record slot prepared by sli_diag_prepare_record().
 * @param[in] owner_token Ownership token checked on every retained store.
 ******************************************************************************/
void sli_diag_writer_init(sli_diag_writer_t *writer,
                          volatile sli_diag_record_slot_t *slot,
                          uint32_t owner_token);

#if SL_DIAG_REPEAT_COUNT_ENABLE
/***************************************************************************//**
 * @brief Increments repeat_count of the published record.
 *
 * @param[in] owner_token Ownership token.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost,
 *         SL_STATUS_INVALID_STATE if the published record is not VALID.
 *
 * @note Only repeat_count and header_crc change. Releases writer ownership
 *       before returning.
 ******************************************************************************/
sl_status_t sli_diag_increment_repeat_count(uint32_t owner_token);
#endif // SL_DIAG_REPEAT_COUNT_ENABLE

/***************************************************************************//**
 * @brief Invalidates every record slot.
 *
 * @param[in] owner_token Ownership token.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_ABORT if ownership was lost.
 *
 * @note Payload bytes are not erased. An empty header makes the slot read as
 *       EMPTY, so sl_diag_read() never exports the old bytes.
 * @note The control block is not changed. The caller sets active_slot to 0
 *       and publishes the control block with sli_diag_write_control(), as
 *       sl_diag_init() and sl_diag_clear() do.
 ******************************************************************************/
sl_status_t sli_diag_invalidate_records(uint32_t owner_token);

/*******************************************************************************
 ******************************   TLV STREAM   *********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Decodes the next TLV of a TLV stream.
 *
 * @param[in] data TLV stream.
 * @param[in] total_size Stream size in bytes.
 * @param[in,out] offset Offset of the next TLV. Advanced past the TLV.
 * @param[out] tlv Decoded TLV.
 *
 * @return SL_STATUS_OK if a TLV was decoded,
 *         SL_STATUS_NOT_FOUND at the end of the stream,
 *         SL_STATUS_INVALID_STATE if the framing is malformed.
 ******************************************************************************/
sl_status_t sli_diag_tlv_next(const volatile uint8_t *data,
                              uint32_t total_size,
                              uint32_t *offset,
                              sli_diag_tlv_info_t *tlv);

/***************************************************************************//**
 * @brief Finds the first TLV of a type.
 *
 * @param[in] data TLV stream.
 * @param[in] total_size Stream size in bytes.
 * @param[in] type TLV type.
 * @param[out] tlv Decoded TLV.
 *
 * @return SL_STATUS_OK if found, SL_STATUS_NOT_FOUND if absent,
 *         SL_STATUS_INVALID_STATE if the framing is malformed.
 ******************************************************************************/
sl_status_t sli_diag_tlv_find(const volatile uint8_t *data,
                              uint32_t total_size,
                              uint16_t type,
                              sli_diag_tlv_info_t *tlv);

/*******************************************************************************
 *********************************   CRC   *************************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Updates a CRC-32 state.
 *
 * @param[in] crc State before the final XOR. Start with SLI_DIAG_CRC32_INIT.
 * @param[in] data Bytes.
 * @param[in] len Number of bytes.
 *
 * @return Updated state, before the final XOR.
 ******************************************************************************/
uint32_t sli_diag_crc32_update(uint32_t crc,
                               const void *data,
                               uint32_t len);

/***************************************************************************//**
 * @brief Updates a CRC-32 state from retained memory.
 *
 * @param[in] crc State before the final XOR.
 * @param[in] data Retained bytes.
 * @param[in] len Number of bytes.
 *
 * @return Updated state, before the final XOR.
 ******************************************************************************/
uint32_t sli_diag_crc32_update_retained(uint32_t crc,
                                        const volatile uint8_t *data,
                                        uint32_t len);

/***************************************************************************//**
 * @brief Computes a CRC-32 (ISO-HDLC: reflected 0xEDB88320, initial and final
 *        XOR 0xFFFFFFFF).
 *
 * @param[in] data Bytes.
 * @param[in] len Number of bytes.
 *
 * @return CRC-32.
 ******************************************************************************/
uint32_t sli_diag_crc32(const void *data,
                        uint32_t len);

#endif // SL_DIAG_ENABLE

#ifdef __cplusplus
}
#endif

#endif // SLI_DIAG_INTERNAL_H
