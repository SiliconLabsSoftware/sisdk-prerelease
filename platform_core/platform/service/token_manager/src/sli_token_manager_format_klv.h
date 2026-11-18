/***************************************************************************//**
 * @file sli_token_manager_format_klv.h
 * @brief Header file for KLV (Key-Length-Value) format/type objects.
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

#ifndef SLI_TOKEN_MANAGER_FORMAT_KLV_H
#define SLI_TOKEN_MANAGER_FORMAT_KLV_H

#include "sl_status.h"
#include <stddef.h>
#include "sl_enum.h"
#include "sl_token_manager_config.h"
#include "sli_token_manager_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SLI_TOKEN_KLV_ERASE_STATE                        (0xFFFFFFFF)
// KLV chain identifier for static device token, start value is derived from Device MTP(DMTP)
#define SLI_TOKEN_KLV_STATIC_DEVICE_TOKEN_MAIN_START     (('D' << 24) | ('M' << 16) | ('T' << 8) | ('P' << 0))
// KLV chain identifier for static secure token, start value is derived from static secure token chain(SSTC)
#define SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START     (('S' << 24) | ('S' << 16) | ('T' << 8) | ('C' << 0))

// 16-bit KLV magic number for @ref sl_klv_header_t.
// 'K' is 0x4B and 'V' is 0x56, used for static device token and static secure token.
#define SLI_TOKEN_KLV_MAGIC_NUMBER                       (('K' << 8) | ('V' << 0))
#define SLI_TOKEN_KLV_HEADER_SIZE                        (8) //8bytes or 2 words

// 32-bit KLV tag magics for @ref sli_klv_obj_header_t.
// Used for static device token, static secure token and upgradable token.
// If the klv object is active, the magic number will be set to SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE.
#define SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE                   ((uint32_t)(('K' << 24) | ('L' << 16) | ('V' << 8) | (1 << 0)))
// If the klv object is deleted, the magic number will be set to SLI_TOKEN_KLV_MAGIC_KLV_DELETED,
// Not supported for static tokens.
#define SLI_TOKEN_KLV_MAGIC_KLV_DELETED                  ((uint32_t)(('K' << 24) | ('L' << 16) | ('V' << 8) | (0 << 0)))
// KLV header flags (@ref sli_klv_obj_header_t:flags).
// Bit 7: write-in-progress indicator.
#define SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS              (1)
#define SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_OFFSET       (7)
#define SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK     (SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS << SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_OFFSET)
// Bits 0-6 are reserved and must be 1.
#define SLI_TOKEN_KLV_HEADER_FLAGS_FIXED_LOW_MASK        (0x7FU)

// Total bytes one KLV object occupies in NVM: header + word-padded payload + tail.
#define SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES(hdr_size, sec_off, payload_len) \
  ((uint32_t)(hdr_size) + SL_CEILING((payload_len), sizeof(uint32_t)) + (uint32_t)(sec_off))

// UT plain-text layout: header + word-padded payload (no crypto nonce/tag).
#define SLI_KLV_UT_STORED_OBJECT_SIZE_BYTES(hdr_size, payload_len) \
  SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES((hdr_size), 0U, (payload_len))
// UT plain-text payload address immediately after the decoded header.
#define SLI_KLV_UT_DATA_START_PTR(header_size, obj_u8) \
  ((uint8_t *)(obj_u8) + (header_size))

// SST / crypto layout macros (not used for UTs).
#if defined(SL_TOKEN_MANAGER_SECURITY)
#define SLI_CRYPTO_TAG_SIZE                              4
#define SLI_CRYPTO_RANDOM_NUMBER_SIZE                    4
#define SLI_CRYPTO_NONCE_SIZE                            12
#define SLI_CRYPTO_HEADER_OFFSET                         0
#define SLI_CRYPTO_NONCE_OFFSET                          (SLI_CRYPTO_HEADER_OFFSET + SLI_TOKEN_KLV_HEADER_SIZE)

// Return the security offset for a given KLV start address.
#define SLI_TOKEN_MANAGER_GET_SECURITY_OFFSET(klv_start_address)                      \
  ((*(uint32_t *)(klv_start_address) == SLI_TOKEN_KLV_STATIC_SECURE_TOKEN_MAIN_START) \
   ? (SLI_CRYPTO_RANDOM_NUMBER_SIZE + SLI_CRYPTO_TAG_SIZE) : 0)

// Data start pointer after the header and nonce. Used for static secure token.
#define SLI_KLV_DATA_START_PTR(header_size, obj_u8) \
  ((uint8_t *)(obj_u8) + (header_size) + SLI_CRYPTO_RANDOM_NUMBER_SIZE)

// Return the payload NVM address for a given header size and object address. Used for static secure token.
#define SLI_KLV_GET_SECURE_PAYLOAD_NVM_ADDR_U32(header_size, obj_u8) \
  ((uint32_t)((uint8_t *)(obj_u8) + (header_size) + SLI_CRYPTO_RANDOM_NUMBER_SIZE))

// Return the nonce store address for a given header size and object address. Used for static secure token.
#define SLI_KLV_SECURE_NONCE_STORE_ADDR_U32(header_size, obj_u8) \
  ((uint32_t)((uint8_t *)(obj_u8) + (header_size)))

// Return the tag store address for a given header size, object address and payload size. Used for static secure token.
#define SLI_KLV_GET_SECURE_TAG_NVM_ADDR_U32(header_size, obj_u8, payload_padded_u32) \
  ((uint32_t)(SLI_KLV_DATA_START_PTR(header_size, obj_u8) + (uint32_t)(payload_padded_u32)))

// SST on-flash object size including crypto nonce/tag tail when applicable.
#define SLI_KLV_SST_STORED_OBJECT_SIZE_BYTES(hdr_size, payload_len, klv_start_address) \
  SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES((hdr_size), \
                                         SLI_TOKEN_MANAGER_GET_SECURITY_OFFSET(klv_start_address), \
                                         (payload_len))
#else
#define SLI_CRYPTO_RANDOM_NUMBER_SIZE 0
#define SLI_TOKEN_MANAGER_GET_SECURITY_OFFSET(klv_start_address) 0
// Data start pointer after the header, when the security is not enabled.
#define SLI_KLV_DATA_START_PTR(header_size, obj_u8) \
  ((uint8_t *)(obj_u8) + (header_size))

// Return the size of a static secure token from the KLV header when the security is not enabled.
#define SLI_KLV_SST_STORED_OBJECT_SIZE_BYTES(hdr_size, payload_len, klv_start_address) \
  SLI_TOKEN_KLV_STORED_OBJECT_SIZE_BYTES((hdr_size), 0U, (payload_len))
#endif

// Header is readable when the write-in-progress bit is cleared (write complete).
#define IS_KLV_HEADER_READABLE(hdr_flags) \
  (((hdr_flags) & SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK) == 0U)

// CRC covers the contiguous header fields length + key (excludes
// magic_number which changes on soft-delete, and excludes crc/crc_data).
#define SLI_TOKEN_KLV_EXTENDED_HEADER_CRC_PAYLOAD_LEN  (offsetof(sli_klv_obj_header_t, crc) - offsetof(sli_klv_obj_header_t, length))

// AAD for AEAD includes the full header prefix up to crc (magic_number + length + crc_data + key). Used for static secure token.
#define SLI_TOKEN_KLV_EXTENDED_HEADER_AAD_SIZE         ((uint32_t)offsetof(sli_klv_obj_header_t, crc))

// Check if the address has a programmed header.
#define SLI_KLV_ADDRESS_HAS_PROGRAMMED_HEADER(obj_adr) \
  ((*(const uint32_t *)(obj_adr) == SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE) \
   || (*(const uint32_t *)(obj_adr) == SLI_TOKEN_KLV_MAGIC_KLV_DELETED) \
   || (((const sl_klv_header_t *)(obj_adr))->magic_number == SLI_TOKEN_KLV_MAGIC_NUMBER))

// Build the write flags for the KLV header. Applicable when @ref sli_klv_obj_header_t:flags is used.
#define SLI_KLV_HEADER_BUILD_WRITE_FLAGS() \
  ((uint8_t)((SLI_TOKEN_KLV_HEADER_FLAGS_WRITE_IN_PROGRESS_BIT_MASK \
              | SLI_TOKEN_KLV_HEADER_FLAGS_FIXED_LOW_MASK)))

// Initialize the KLV header for write. Applicable when @ref sli_klv_obj_header_t is used.
#define SLI_KLV_INIT_HDR_FOR_WRITE(hdr, klv_h, crc_data_field) \
  do { \
    (hdr).magic_number = SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE; \
    (hdr).length = (klv_h)->length; \
    (hdr).key = (klv_h)->key & 0xFFFF; \
    (hdr).crc = calculate_checksum((uint8_t *)&(hdr).length, SLI_TOKEN_KLV_EXTENDED_HEADER_CRC_PAYLOAD_LEN); \
    (hdr).crc_data = (crc_data_field); \
    (hdr).flags = SLI_KLV_HEADER_BUILD_WRITE_FLAGS(); \
    (hdr).reserved = 0xFFU; \
  } while (0)

#if defined(SL_TOKEN_MANAGER_SECURITY)
SL_ENUM(sl_klv_security_type_t) {
  SL_TOKEN_MANAGER_KLV_SECURITY_TYPE_NONE = 0,
  SL_TOKEN_MANAGER_KLV_SECURITY_TYPE_AEAD = 1,
  SL_TOKEN_MANAGER_KLV_SECURITY_TYPE_ENC = 2,
  SL_TOKEN_MANAGER_KLV_SECURITY_TYPE_INVALID
};
#endif

// KLV header format for static device token and static secure token.
typedef struct {
  uint16_t magic_number;  // Must be @ref SLI_TOKEN_KLV_MAGIC_NUMBER.
  uint16_t crc;           // CRC value of the data
  uint16_t length;        // length of the data
  uint16_t key;           // unique key for a KLV object
} sl_klv_header_t;

// KLV handle format passed to the KLV APIs.
typedef struct {
  uint32_t key;           // unique key for a KLV object
  uint32_t offset;        // offset to begin reading token data (bytes)
  uint32_t length;        // length of the data
} sl_klv_handle_t;

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS)
// Anchor commit function type for UT chain anchor updates.
typedef sl_status_t (*sli_klv_ut_anchor_commit_fn_t)(uint32_t anchor, void *context);
typedef sl_status_t (*sli_klv_ut_extension_activation_commit_fn_t)(void *context);

// Repair uses borrowed physical repack page (no dedicated flash pages).
// Before altering flash, COPY must be stored atomically. Addresses are PHYSICAL.
#define SLI_KLV_UT_REPAIR_IDLE  (0U)
#define SLI_KLV_UT_REPAIR_COPY  (1U)
#define SLI_KLV_UT_REPAIR_ARMED (2U)
#define SLI_KLV_UT_REPAIR_DONE  (3U)

typedef struct {
  uint32_t source_page;     // Physical page containing the torn header.
  uint32_t scratch_page;    // Physical borrowed backup page; zero if not needed.
  uint32_t prefix_bytes;    // Number of original source-page bytes to preserve.
  uint16_t source_crc16;    // CRC16 of the original complete source page.
  uint16_t prefix_crc16;    // CRC16 of the prefix; survives backup cleanup.
  uint16_t phase;           // IDLE/COPY/ARMED/DONE (durable transaction state).
  uint16_t record_crc16;    // CRC16 of record fields, also bound to anchor/layout.
} sli_klv_ut_repair_record_t; // 20 bytes; all fields have distinct recovery uses.

// The append frontier and its single write-ahead header reservation live in
// ONE atomic NVM3 object. The tail counts valid WIP headers (not just committed
// payloads). Physical addresses are mapped through the active logical ring.
#define SLI_KLV_UT_TAIL_STABLE         (1U)
#define SLI_KLV_UT_TAIL_HEADER_PENDING (2U)
#define SLI_KLV_UT_TAIL_LAYOUT_PENDING (3U) // Extension activation remaps the ring end
typedef struct {
  uint32_t stable_tail;
  uint32_t pending_header_addr;
  uint8_t state;
  uint8_t reserved[3];
} sli_klv_ut_tail_record_t;

typedef sl_status_t (*sli_klv_ut_tail_load_fn_t)(sli_klv_ut_tail_record_t *record,
                                                void *context);
typedef sl_status_t (*sli_klv_ut_tail_store_fn_t)(const sli_klv_ut_tail_record_t *record,
                                                 void *context);

typedef sl_status_t (*sli_klv_ut_repair_load_fn_t)(sli_klv_ut_repair_record_t *record,
                                                  void *context);
typedef sl_status_t (*sli_klv_ut_repair_store_fn_t)(const sli_klv_ut_repair_record_t *record,
                                                   void *context);

// Shared context for UT anchor-based KLV operations.
typedef struct {
  uint32_t *chain_anchor;                              // Pointer to the chain anchor.
  uint32_t klv_start_address;                          // Start address of the region in bytes.
  uint32_t klv_end_address;                            // End address of the region in bytes.
  sli_klv_ut_anchor_commit_fn_t anchor_commit;         // Anchor commit function.
  void *anchor_context;                                // Anchor commit context.
  uint32_t total_region_size;                          // Total size of the region in bytes.
  uint32_t extension_info_count;                       // Number of active extension entries.
  const sli_klv_extension_header_t *extension_info;    // Pointer to the extension info.
  const sli_klv_extension_header_t *registered_extension_info; // Pointer to all registered extensions.
  uint32_t registered_extension_info_count;            // Number of registered extension entries.
  uint32_t registered_total_region_size;               // Total size including registered extensions.
  sli_klv_ut_extension_activation_commit_fn_t extension_activation_commit; // Marks pending extensions active.
  void *extension_activation_context;                  // Extension activation callback context.
  sli_klv_ut_repair_load_fn_t repair_load;             // Read atomic recovery metadata from NVM3.
  sli_klv_ut_repair_store_fn_t repair_store;           // Replace atomic recovery metadata in NVM3.
  void *repair_context;                                // Callback context.
  sli_klv_ut_tail_load_fn_t tail_load;                 // Read one atomic NVM3 tail record.
  sli_klv_ut_tail_store_fn_t tail_store;               // Replace one atomic NVM3 tail record.
  void *tail_context;
} sli_klv_ut_context_t;
#endif

// KLV header format for static device tokens, static secure tokens, and upgradable tokens.
// Going forward, the following header format will be used for all KLV objects written to NVM.
typedef struct {
  uint32_t magic_number;          // Must be @ref SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE for a live object; @ref SLI_TOKEN_KLV_MAGIC_KLV_DELETED for a soft-deleted object.
  uint32_t length;                // Length of the data payload in bytes.
  uint16_t key;                   // Unique key identifier for a KLV object (Token ID)
  uint16_t crc;                   // CRC16 over length, key (excludes magic_number, crc, crc_data, flags, reserved).
  uint16_t crc_data;              // CRC16 of the data payload.
  uint8_t  flags;                 // Flags for the KLV object. (bit 7 write-in-progress indicator: set=1 on write start, cleared=0 when complete).
  uint8_t  reserved;              // Reserved for future use.
} sli_klv_obj_header_t;

// Size of the extended header in bytes.
#define SLI_TOKEN_KLV_EXTENDED_HEADER_SIZE            (sizeof(sli_klv_obj_header_t))

#if !defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH)
/***************************************************************************//**
 * @brief
 *  Reads a KLV object: header layout is chosen from the magic
 *  @ref SLI_TOKEN_KLV_MAGIC_NUMBER -> @ref sl_klv_header_t,
 *  @ref SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE / @ref SLI_TOKEN_KLV_MAGIC_KLV_DELETED -> @ref sli_klv_obj_header_t.
 *
 * @param[in]  klv_handle        A pointer to the sl_klv_handle_t,
 *                               containing length and key info
 * @param[in]  klv_start_address A pointer to the KLV start address of a KLV chain.
 * @param[in]  klv_end_address   A pointer to the KLV end address of a KLV chain.
 * @param[out] data              A pointer to where the token data should be placed.
 *
 * @return
 *   The result of the read operation.
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_read_klv_object(sl_klv_handle_t const *klv_handle,
                                uint32_t *klv_start_address,
                                uint32_t *klv_end_address,
                                void *data);

/***************************************************************************//**
 * @brief
 *  Writes a KLV object using @ref sli_klv_obj_header_t /
 *  @ref SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE for chain append. When overwriting an
 *  existing SDT entry in place, the decoded @c header_size is preserved so the
 *  stored header is not expanded into a larger layout.
 *
 * @param[in] klv_handle        A pointer to the sl_klv_handle_t,
 *                              containing length and key info
 * @param[in] klv_start_address The KLV chain start address.
 * @param[in] klv_end_address   The KLV chain end address.
 * @param[in] data              A pointer to the data being written.
 *
 * @return
 *   The result of the write operation.
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_write_klv_object(sl_klv_handle_t const *klv_handle,
                                 uint32_t klv_start_address,
                                 uint32_t klv_end_address,
                                 void const *data);

/***************************************************************************//**
 * @brief
 *  Returns the size of a static token from the KLV header
 *
 * @param[in]  klv_start_address   The KLV chain start address.
 * @param[in]  klv_end_address     The KLV chain end address.
 * @param[in]  token               32-bit token identifier
 * @param[out] size_out            A pointer to where the token data size should be placed.
 *
 * @return
 * SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_token_manager_get_size_from_klv(uint32_t klv_start_address,
                                                uint32_t klv_end_address,
                                                uint32_t token,
                                                uint32_t *size_out);
#endif

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS)
// Calculate the page start address of the given address.
#define SLI_KLV_ANCHOR_PAGE_START(address) ((address) & ~(FLASH_PAGE_SIZE - 1U))

/***************************************************************************//**
 * @brief
 *  Reads a plaintext V1 KLV object chain that starts at a moving object anchor
 *  instead of a fixed 4-byte chain-start word.
 *
 * @param[in]  ut_context        A pointer to the sli_klv_ut_context_t,
 *                               containing the UT context.
 * @param[in]  klv_handle        A pointer to the sl_klv_handle_t,
 *                               containing length and key info.
 * @param[out] data              A pointer to where the token data should be placed.
 *
 * @return
 *   The result of the read operation.
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_read_klv_object_from_anchor(const sli_klv_ut_context_t *ut_context,
                                            sl_klv_handle_t const *klv_handle,
                                            void *data);

/***************************************************************************//**
 * @brief
 *  Writes a plaintext V1 KLV object into a circular UT chain that starts at a
 *  moving object anchor.
 *
 * @param[in]      ut_context        A pointer to the sli_klv_ut_context_t,
 *                                   containing the UT context.
 * @param[in]      klv_handle        A pointer to the sl_klv_handle_t,
 *                                   containing length and key info.
 * @param[in]      data              A pointer to the data being written.
 *
 * @return
 *   The result of the write operation.
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 *   @ref SL_STATUS_IN_PROGRESS if @p klv_handle->key already has an active
 *   streaming session (a concurrent one-shot write would create a duplicate
 *   live copy that reads/deletes would not see as the latest value).
 *   @ref SL_STATUS_BUSY if another stream reserved its destination through
 *   reclaim, or if this write would reclaim/repack while any stream is active.
 *   @ref SL_STATUS_WOULD_OVERFLOW if the post-write live set plus the required
 *   repack reserve cannot fit in @p ut_context->registered_total_region_size
 *   (base plus all registered extensions, including pending).
 *   @ref SL_STATUS_INVALID_STATE for unresolved pending torn-header recovery,
 *   an invalid tail record, or a destination whose unerasable mid-page flash
 *   remains programmed. STABLE-tail bytes outside the live chain are not
 *   classified as torn; safely obsolete destination pages are erased lazily.
 ******************************************************************************/
sl_status_t sli_write_klv_object_from_anchor(const sli_klv_ut_context_t *ut_context,
                                             sl_klv_handle_t const *klv_handle,
                                             void const *data);

/***************************************************************************//**
 * @brief
 *  Returns the size of a plaintext V1 KLV object chain that starts at a moving
 *  object anchor.
 *
 * @param[in]  ut_context        A pointer to the sli_klv_ut_context_t,
 *                               containing the UT context.
 * @param[in]  token             32-bit token identifier.
 * @param[out] size_out          A pointer to where the token data size should
 *                               be placed.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_token_manager_get_size_from_klv_anchor(const sli_klv_ut_context_t *ut_context,
                                                       uint32_t token,
                                                       uint32_t *size_out);

/***************************************************************************//**
 * @brief
 *  Soft-deletes a KLV object in a circular UT chain by rewriting its header
 *  magic from @ref SLI_TOKEN_KLV_MAGIC_KLV_ACTIVE to
 *  @ref SLI_TOKEN_KLV_MAGIC_KLV_DELETED. Payload and remaining header fields
 *  are left unchanged; space is reclaimed later through the normal repack path.
 *
 * @param[in]      ut_context        A pointer to the sli_klv_ut_context_t,
 *                                   containing the UT context.
 * @param[in]      token             32-bit token identifier.
 *
 * @return
 *   @ref SL_STATUS_OK if the object was soft-deleted,
 *   @ref SL_STATUS_NOT_FOUND if no active matching object exists,
 *   @ref SL_STATUS_INVALID_STATE if a suspected torn tail makes the chain
 *   read-only, or another error code otherwise.
 ******************************************************************************/
sl_status_t sli_delete_klv_object_from_anchor(const sli_klv_ut_context_t *ut_context,
                                              uint32_t token);

/***************************************************************************//**
 * @brief
 *  Summarizes the current health and utilization of the UT circular buffer.
 *
 * @param[in]  ut_context  A pointer to the sli_klv_ut_context_t,
 *                         containing the UT context.
 * @param[out] info_out    Destination structure for the computed storage state.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_klv_get_ut_storage_info(const sli_klv_ut_context_t *ut_context,
                                        sl_token_manager_storage_info_t *info_out);

/***************************************************************************//**
 * @brief
 *  Returns the cached largest live UT object size tracked by the KLV backend.
 *
 * @return Largest stored live-object size in bytes when the cache is valid,
 *         otherwise 0.
 ******************************************************************************/
uint32_t sli_klv_ut_get_largest_live_object_cache_size(void);

/***************************************************************************//**
 * @brief
 *  Clears the cached largest live-object size.
 ******************************************************************************/
void sli_klv_ut_largest_live_object_cache_invalidate(void);

/***************************************************************************//**
 * @brief
 *  Completes interrupted UT reclaim relocations after a power failure.
 *
 *  Relocation is a two-step process: a validated copy at the tail, then a
 *  chain-anchor update. The original object remains authoritative until the
 *  anchor advances. On recovery this function detects a validated relocated
 *  duplicate and completes the pending anchor move.
 *
 * @param[in]  ut_context  A pointer to the sli_klv_ut_context_t,
 *                         containing the UT context.
 *
 * @return
 *   @ref SL_STATUS_OK if recovery completed or nothing was pending,
 *   including a torn tail intentionally left untouched for deferred repair;
 *   an error code otherwise.
 ******************************************************************************/
 sl_status_t sli_klv_recover_ut_chain_anchor(const sli_klv_ut_context_t *ut_context);

/***************************************************************************//**
 * @brief
 *  Must be called after loading NVM3 anchor/extensions and before KLV recovery.
 *  Completes a power-failed page restore before any object is read or scanned.
 *
 * @param[in]  ut_context  A pointer to the sli_klv_ut_context_t,
 *                         containing the UT context.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_klv_replay_ut_page_repair(const sli_klv_ut_context_t *ut_context);

/***************************************************************************//**
 * @brief
 *  Detects an unreadable tail header and repairs the containing physical page,
 *  borrowing at most one safe page from the existing UT repack window.
 *  Call under the existing public CTM mutex before a UT mutation.
 *  Resolve a durable header reservation before any anchor walk or normal read.
 *
 * @param[in]  ctx  A pointer to the sli_klv_ut_context_t,
 *                  containing the UT context.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_klv_ut_tail_recover(const sli_klv_ut_context_t *ctx);

/***************************************************************************//**
 * @brief
 *  Persist a layout-remap intent before activating any new extensions.
 *
 * @param[in]  ut_context           A pointer to the sli_klv_ut_context_t,
 *                                  containing the UT context.
 * @param[in]  first_new_extension  The first new extension to activate.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_klv_ut_tail_prepare_extension(const sli_klv_ut_context_t *ctx,
                                             uint32_t first_new_extension);

/***************************************************************************//**
 * @brief
 *  Heals a torn tail by repairing the containing physical page and borrowing
 *  at most one safe page from the existing UT repack window.
 *
 * @param[in]  ut_context        A pointer to the sli_klv_ut_context_t,
 *                               containing the UT context.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_klv_heal_ut_torn_tail(const sli_klv_ut_context_t *ut_context);

/***************************************************************************//**
 * @brief
 *  Starts a streamed UT write by reserving space and writing a provisional
 *  header with the WIP bit set.
 *
 * @param[in]     ut_context        A pointer to the sli_klv_ut_context_t,
 *                                  containing the UT context.
 * @param[in]     token             32-bit token identifier.
 * @param[in]     total_length      Total length of the token data in bytes.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 *   @ref SL_STATUS_IN_PROGRESS if @p token already has an active session.
 *   @ref SL_STATUS_BUSY if another stream reserved space through reclaim, or if
 *   this start would reclaim/repack while another stream is still active.
 *   @ref SL_STATUS_WOULD_OVERFLOW if the post-write live set plus the required
 *   repack reserve cannot fit in @p ut_context->registered_total_region_size
 *   (base plus all registered extensions, including pending).
 *   @ref SL_STATUS_INVALID_STATE for unresolved pending torn-header recovery,
 *   an invalid tail record, or a destination whose unerasable mid-page flash
 *   remains programmed. STABLE-tail bytes outside the live chain are not
 *   classified as torn; safely obsolete destination pages are erased lazily.
 ******************************************************************************/
sl_status_t sli_klv_set_ut_start(const sli_klv_ut_context_t *ut_context,
                                 uint32_t token,
                                 uint32_t total_length);

/***************************************************************************//**
 * @brief
 *  Writes a UT payload chunk at the supplied logical payload offset.
 *
 * @param[in]     token             32-bit token identifier.
 * @param[in]     data              A pointer to the data being written.
 * @param[in]     length            Size of the data in bytes.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_klv_set_ut_update(uint32_t token,
                                  const void *data,
                                  uint32_t length);

/***************************************************************************//**
 * @brief
 *  Finalizes a streamed UT write by validating coverage, updating CRC metadata,
 *  and clearing the WIP bit.
 *
 * @param[in]     ut_context        A pointer to the sli_klv_ut_context_t,
 *                                  containing the UT context.
 * @param[in]     token             32-bit token identifier.
 *
 * @return
 *   @ref SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_klv_set_ut_finish(const sli_klv_ut_context_t *ut_context,
                                  uint32_t token);

/***************************************************************************//**
 * @brief
 *  Computes a CRC16-CCITT checksum over @p data.
 *
 * @param[in]  data  Pointer to the data buffer.
 * @param[in]  len   Length of the data in bytes.
 *
 * @return CRC16-CCITT value.
 ******************************************************************************/
 uint16_t sli_klv_compute_crc16(const uint8_t *data, uint32_t len);

#endif //SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS

#ifdef __cplusplus
}
#endif

#endif //SLI_TOKEN_MANAGER_FORMAT_KLV_H
