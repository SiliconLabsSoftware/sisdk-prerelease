/***************************************************************************//**
 * @file
 * @brief SL Diagnostic record format
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

#ifndef SL_DIAG_FORMAT_H
#define SL_DIAG_FORMAT_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "sl_diag.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SL_DIAG_ENABLE

/***************************************************************************//**
 * @addtogroup sl_diag
 * @{
 ******************************************************************************/

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/***************************************************************************//**
 * @par Linking providers (build time) — stack integration guide
 *
 * Retained diagnostics RAM is sized entirely at **compile time**, matching the
 * architecture link-then-register model. When the application runs SLC generate,
 * contributions named `sl_diag_linked_provider` from the `sl_diag` component
 * and from stack components are merged into
 * `autogen/sl_diag_linked_providers_autogen.h`. That list defines
 * @c SLI_DIAG_LINKED_PROVIDER_LIST, which in turn drives linked-provider TLV
 * totals, @c SLI_DIAG_SLOT_MAX_BYTES, and @c sizeof(retained_region) placed in
 * @c .sl_diag_retained.noinit. If the product exceeds the linker RAM budget,
 * the link step fails.
 *
 * - **Link (SLC generate):** reserve retained TLV payload budget per provider ID.
 * - **Register (runtime):** call @ref sli_diag_register_provider() with the
 *   capture callback (descriptor @c max_output_bytes must not exceed the linked
 *   budget for that ID).
 *
 * In a stack repository, add the following to the stack component @c .slcc
 * (the project must include both @c sl_diag and that stack component). Use a
 * @c provider_id constant from this file. @c max_output_bytes is the worst-case
 * encoded **payload** size for that provider (TLV header is accounted for
 * separately in platform sizing).
 *
 * @code{.yaml}
 * requires:
 *   - name: sl_diag
 * template_contribution:
 *   - name: sl_diag_linked_provider
 *     value:
 *       provider_id: SL_DIAG_PROVIDER_HEAP
 *       max_output_bytes: 256
 * @endcode
 *
 * Do not contribute @c SL_DIAG_PROVIDER_CPU_CRASH from a stack @c .slcc;
 * platform autogen always links CPU Crash. Capture profiles may select
 * providers at runtime, but registration and retained space exist only for
 * linked IDs.
 *
 * @see sli_diag_register_provider()
 ******************************************************************************/

/* SL_DIAG TLV type and provider IDs */
/// TLV type for the Trigger metadata block in a retained record.
#define SL_DIAG_TLV_TRIGGER          0x0001u
/// TLV type for reset and boot metadata in a retained record.
#define SL_DIAG_TLV_RESET_BOOT       0x0002u
/// TLV type for firmware build metadata in a retained record.
#define SL_DIAG_TLV_BUILD            0x0003u
/// TLV type for a snapshot of Summary Ring entries in a retained record.
#define SL_DIAG_TLV_RECENT_EVENTS    0x0004u
/// TLV type for a provider capture outcome when evidence was not written.
#define SL_DIAG_TLV_PROVIDER_STATUS  0x0005u

/// Provider ID and TLV type for CPU crash registers and stack snapshot.
#define SL_DIAG_PROVIDER_CPU_CRASH   0x0010u
/// Provider ID and TLV type for system state captured at incident time.
#define SL_DIAG_PROVIDER_SYSTEM      0x0101u
/// Provider ID and TLV type for heap diagnostics.
#define SL_DIAG_PROVIDER_HEAP        0x0104u
/// Provider ID and TLV type for host interface diagnostics.
#define SL_DIAG_PROVIDER_HOST_IF     0x0105u
/// Provider ID and TLV type for network processor health.
#define SL_DIAG_PROVIDER_NWP_HEALTH  0x0106u

/*******************************************************************************
 ********************************   TYPES   ************************************
 ******************************************************************************/

/** @brief On-wire TLV header. The payload follows immediately in the record stream. */
typedef struct {
  uint16_t type;     ///< TLV type or provider ID.
  uint8_t version;   ///< Payload layout version. Provider TLVs use the provider version.
  uint8_t reserved;  ///< Reserved. Written as 0.
  uint16_t length;   ///< Payload length in bytes.
} sl_diag_tlv_header_t;

/** @brief Payload of the Trigger TLV (SL_DIAG_TLV_TRIGGER). */
typedef struct {
  uint8_t core_id;            ///< Core that captured the incident.
  sl_diag_trigger_t trigger;  ///< Capture trigger value.
  uint8_t reserved[2];        ///< Reserved. Written as 0.
  uint32_t reason_id;         ///< Product-specific reason identifier.
  uint32_t detail;            ///< Trigger-specific detail code or data.
  uint32_t uptime_ms;         ///< Device uptime at capture in milliseconds.
} sl_diag_trigger_tlv_payload_t;

/** @brief Payload of the Reset/Boot TLV (SL_DIAG_TLV_RESET_BOOT). */
typedef struct {
  uint32_t reset_reason;      ///< Primary reset reason latched at boot.
  uint32_t reset_reason_ext;  ///< Extended reset reason latched at boot.
  uint32_t boot_id;           ///< Boot counter at capture time.
} sl_diag_reset_boot_tlv_payload_t;

/** @brief Payload of the Build TLV (SL_DIAG_TLV_BUILD). */
typedef struct {
  uint8_t core_id;                         ///< Core that captured the incident.
  uint8_t build_id[SL_DIAG_BUILD_ID_LEN];  ///< Firmware build identifier bytes.
  uint8_t reserved[3];                     ///< Reserved. Written as 0.
  uint32_t dictionary_id;                  ///< Diagnostic dictionary identifier.
} sl_diag_build_tlv_payload_t;

/** @brief Payload of the Provider status TLV (SL_DIAG_TLV_PROVIDER_STATUS). */
typedef struct {
  uint16_t provider_id;              ///< Provider ID that was selected for capture.
  sl_diag_provider_status_t status;  ///< Outcome when evidence was gated or failed.
  uint8_t reserved;                  ///< Reserved. Written as 0.
} sl_diag_provider_status_tlv_payload_t;

#if SL_DIAG_SUMMARY_RING_ENABLE
/** @brief One Summary Ring entry. Also one element of the Recent-events TLV payload. */
typedef struct {
  uint32_t reason_id;  ///< Product-specific reason identifier.
  uint32_t detail;     ///< Reason-specific detail code or data.
} sl_diag_ring_entry_t;
#endif // SL_DIAG_SUMMARY_RING_ENABLE

/** @} (end addtogroup sl_diag) */

/*******************************************************************************
 ****************************   LAYOUT CHECKS   ********************************
 ******************************************************************************/

static_assert(sizeof(sl_diag_tlv_header_t) == 6u, "sl_diag_tlv_header_t must be 6 bytes");
static_assert(offsetof(sl_diag_tlv_header_t, length) == 4u, "sl_diag_tlv_header_t layout");

static_assert(sizeof(sl_diag_trigger_tlv_payload_t) == 16u, "sl_diag_trigger_tlv_payload_t must be 16 bytes");
static_assert(offsetof(sl_diag_trigger_tlv_payload_t, trigger) == 1u, "sl_diag_trigger_tlv_payload_t layout");
static_assert(offsetof(sl_diag_trigger_tlv_payload_t, reason_id) == 4u, "sl_diag_trigger_tlv_payload_t layout");
static_assert(offsetof(sl_diag_trigger_tlv_payload_t, uptime_ms) == 12u, "sl_diag_trigger_tlv_payload_t layout");

static_assert(sizeof(sl_diag_reset_boot_tlv_payload_t) == 12u, "sl_diag_reset_boot_tlv_payload_t must be 12 bytes");

static_assert(sizeof(sl_diag_build_tlv_payload_t) == 28u, "sl_diag_build_tlv_payload_t must be 28 bytes");
static_assert(offsetof(sl_diag_build_tlv_payload_t, build_id) == 1u, "sl_diag_build_tlv_payload_t layout");
static_assert(offsetof(sl_diag_build_tlv_payload_t, dictionary_id) == 24u, "sl_diag_build_tlv_payload_t layout");

static_assert(sizeof(sl_diag_provider_status_tlv_payload_t) == 4u, "sl_diag_provider_status_tlv_payload_t must be 4 bytes");
static_assert(offsetof(sl_diag_provider_status_tlv_payload_t, status) == 2u, "sl_diag_provider_status_tlv_payload_t layout");

#if SL_DIAG_SUMMARY_RING_ENABLE
static_assert(sizeof(sl_diag_ring_entry_t) == 8u, "sl_diag_ring_entry_t must be 8 bytes");
#endif

#endif // SL_DIAG_ENABLE

#ifdef __cplusplus
}
#endif

#endif // SL_DIAG_FORMAT_H
