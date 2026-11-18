/***************************************************************************//**
 * @file sli_token_manager_upgradable.c
 * @brief Upgradable token API implementation. Handles upgradable token
 *        read/write operations.
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
#include "sl_token_manager_config.h"
#include "sl_token_manager_defines.h"
#include "em_device.h"
#include "sli_token_manager_internal.h"
#include "sli_token_manager_backend_nvm.h"
#include "sli_token_manager_format_klv.h"
#include <stdint.h>

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_DYNAMIC_TOKENS)
#include "nvm3.h"
#endif

// Minimum region size for upgradable token support
#if defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH)
#define SL_TOKEN_MANAGER_UT_REGION_SIZE_MIN 24576
#else
#define SL_TOKEN_MANAGER_UT_REGION_SIZE_MIN 16384
#endif

// NVM3 key for the upgradable token chain_anchor.
#define SLI_TOKEN_MANAGER_UT_CHAIN_ANCHOR_NVM3_KEY SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_PLATFORM + 1U), 0U)

// NVM3 key for the upgradable token extension header.
#define SLI_TOKEN_MANAGER_UT_EXT_INFO_NVM3_KEY SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_PLATFORM + 2U), 0U)

// A single atomically replaced NVM3 item, separate from UT flash and from
// the existing anchor and extension-directory items. No dedicated UT page.
#define SLI_TOKEN_MANAGER_UT_REPAIR_NVM3_KEY SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_PLATFORM + 3U), 0U)
// Tail metadata is a separate single-key atomic record; no new UT flash pages.
#define SLI_TOKEN_MANAGER_UT_TAIL_NVM3_KEY SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_PLATFORM + 4U), 0U)

// Cached copy of the persisted circular-buffer head. The value in NVM3 is the
// source of truth across resets; this copy avoids repeatedly threading the
// anchor through every API layer once it has been validated and recovered.
static uint32_t ut_chain_anchor = (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE;
static bool ut_chain_anchor_loaded = false;

// Cached registered UT extension directory. Active entries are compacted first,
// followed by inactive registered entries; ut_extension_info_count marks the
// active prefix used by ordinary UT traversal.
static sli_klv_extension_header_t ut_extension_info[SLI_TOKEN_MANAGER_EXT_INFO_MAX];
static uint32_t ut_extension_info_count = 0U;
static uint32_t ut_registered_extension_info_count = 0U;
static uint32_t ut_total_region_size = 0;
static uint32_t ut_registered_total_region_size = 0U;
static bool ut_extension_info_loaded = false;

static sli_klv_ut_context_t load_ut_klv_context(void);

/***************************************************************************//**
 * Returns whether @p entry is part of the active logical UT layout.
 ******************************************************************************/
static bool ut_extension_entry_is_active(const sli_klv_extension_header_t *entry)
{
  if (entry == NULL) {
    return false;
  }

  return (entry->is_extension_active == SLI_TOKEN_MANAGER_UT_EXTENSION_ACTIVE);
}

/***************************************************************************//**
 * Returns whether @p address is a valid UT chain anchor inside the configured
 * upgradable-token region.
 *
 * The UT implementation stores a moving "chain anchor" instead of a fixed
 * chain-start marker. Every public UT API therefore validates anchors before
 * trusting them, because a stale or corrupted anchor would cause the circular
 * KLV walk to treat arbitrary flash as the logical head of the chain.
 ******************************************************************************/
static bool is_upgradable_token_address_in_range(uint32_t address)
{
  if ((address % sizeof(uint32_t)) != 0U) {
    return false;
  }

  if ((address >= (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE)
      && (address < (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_END)) {
    return true;
  }

  if (!ut_extension_info_loaded) {
    return false;
  }

  for (uint32_t i = 0U; i < ut_extension_info_count; i++) {
    uint32_t extension_end = 0U;

    extension_end = ut_extension_info[i].extension_addr
                    + ut_extension_info[i].extension_size;
    if ((address >= ut_extension_info[i].extension_addr)
        && (address < extension_end)) {
      return true;
    }
  }

  return false;
}

/***************************************************************************//**
 * Computes the CRC16 for a UT extension directory entry.
 ******************************************************************************/
static uint16_t ut_extension_info_entry_crc_compute(uint32_t extension_addr,
                                                    uint32_t extension_size)
{
  uint8_t crc_buf[8];

  memcpy(&crc_buf[0], &extension_addr, sizeof(extension_addr));
  memcpy(&crc_buf[4], &extension_size, sizeof(extension_size));
  return sli_klv_compute_crc16(crc_buf, sizeof(crc_buf));
}

/***************************************************************************//**
 * Returns whether @p entry contains a valid CRC for its address and size.
 ******************************************************************************/
static bool ut_extension_info_entry_is_valid(const sli_klv_extension_header_t *entry)
{
  if (entry == NULL) {
    return false;
  }

  return (entry->crc
          == ut_extension_info_entry_crc_compute(entry->extension_addr,
                                                 entry->extension_size));
}

/***************************************************************************//**
 * Returns whether two flash spans overlap.
 ******************************************************************************/
static bool ut_extension_flash_regions_overlap(uint32_t a_addr,
                                               uint32_t a_size,
                                               uint32_t b_addr,
                                               uint32_t b_size)
{
  uint64_t a_end = (uint64_t)a_addr + (uint64_t)a_size;
  uint64_t b_end = (uint64_t)b_addr + (uint64_t)b_size;

  if ((a_end > UINT32_MAX) || (b_end > UINT32_MAX)) {
    return true;
  }

  return ((a_addr < (uint32_t)b_end) && (b_addr < (uint32_t)a_end));
}

/***************************************************************************//**
 * Returns whether @p extension_addr/@p extension_size overlaps the default UT
 * region.
 ******************************************************************************/
static bool ut_extension_overlaps_main_region(uint32_t extension_addr,
                                              uint32_t extension_size)
{
  return ut_extension_flash_regions_overlap(extension_addr,
                                            extension_size,
                                            (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE,
                                            (uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE);
}

/***************************************************************************//**
 * Returns whether @p extension_addr/@p extension_size overlaps a reserved NVM
 * region that must not be used as UT extension storage.
 *
 * Rejects overlaps with the default UT region, static-secure storage, the
 * default NVM3 instance backing dynamic tokens, and (on internal-flash Series 2
 * builds) the lock-bits page used for static secure tokens.
 ******************************************************************************/
static bool ut_extension_overlaps_reserved_region(uint32_t extension_addr,
                                                  uint32_t extension_size)
{
  // Validate that the extension address and size do not overlap with the UT's main region.
  if (ut_extension_overlaps_main_region(extension_addr, extension_size)) {
    return true;
  }

#if defined(SL_TOKEN_MANAGER_FORMAT_LEGACY)
  // Validate that the extension address and size do not overlap with the lock-bits page.
  if (ut_extension_flash_regions_overlap(extension_addr,
                                         extension_size,
                                         (uint32_t)LOCKBITS_BASE,
                                         (uint32_t)LOCKBITS_DATA_SIZE)) {
    return true;
  }
#else
  // Validate that the extension address and size do not overlap with the static secure storage.
  if (ut_extension_flash_regions_overlap(extension_addr,
                                         extension_size,
                                         (uint32_t)SLI_TOKEN_STATIC_SECURE_TOKEN_BASE,
                                         (uint32_t)SL_TOKEN_MANAGER_STATIC_SECURE_TOKEN_SIZE)) {
    return true;
  }
#endif

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_DYNAMIC_TOKENS)
  // Validate that the extension address and size do not overlap with the default NVM3 instance.
  {
    uint32_t nvm3_base = (uint32_t)nvm3_defaultInit->nvmAdr;
    uint32_t nvm3_size = nvm3_defaultInit->nvmSize;

    if ((nvm3_size > 0U)
        && ut_extension_flash_regions_overlap(extension_addr,
                                              extension_size,
                                              nvm3_base,
                                              nvm3_size)) {
      return true;
    }
  }
#endif

  return false;
}

/***************************************************************************//**
 * Reads the persisted UT extension directory from NVM3.
 ******************************************************************************/
static sl_status_t ut_extension_info_read_nvm3(sli_klv_extension_header_t *extension_info)
{
  if (extension_info == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  return sli_token_manager_get_dynamic_data(SLI_TOKEN_MANAGER_UT_EXT_INFO_NVM3_KEY,
                                            (void *)extension_info,
                                            0U,
                                            sizeof(sli_klv_extension_header_t)
                                            * SLI_TOKEN_MANAGER_EXT_INFO_MAX);
}

/***************************************************************************//**
 * Writes the UT extension directory to NVM3.
 ******************************************************************************/
static sl_status_t ut_extension_info_write_nvm3(const sli_klv_extension_header_t *extension_info)
{
  if (extension_info == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  return sli_token_manager_set_dynamic_data(SLI_TOKEN_MANAGER_UT_EXT_INFO_NVM3_KEY,
                                            (void *)extension_info,
                                            sizeof(sli_klv_extension_header_t)
                                            * SLI_TOKEN_MANAGER_EXT_INFO_MAX);
}

/***************************************************************************//**
 * Loads the UT extension directory from NVM3 into the RAM cache.
 ******************************************************************************/
static sl_status_t ut_extension_info_load(void)
{
  sli_klv_extension_header_t persisted_info[SLI_TOKEN_MANAGER_EXT_INFO_MAX];
  sl_status_t status = ut_extension_info_read_nvm3(persisted_info);

  if (status == SL_STATUS_NOT_FOUND) {
    memset(ut_extension_info, 0, sizeof(ut_extension_info));
    ut_extension_info_count = 0U;
    ut_registered_extension_info_count = 0U;
    ut_total_region_size = (uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE;
    ut_registered_total_region_size = (uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE;
    ut_extension_info_loaded = true;
    return SL_STATUS_OK;
  } else if (status != SL_STATUS_OK) {
    return status;
  }

  memset(ut_extension_info, 0, sizeof(ut_extension_info));
  ut_extension_info_count = 0U;
  ut_registered_extension_info_count = 0U;
  ut_total_region_size = (uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE;
  ut_registered_total_region_size = (uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE;

  for (uint32_t i = 0U; i < SLI_TOKEN_MANAGER_EXT_INFO_MAX; i++) {
    if (!ut_extension_info_entry_is_valid(&persisted_info[i])) {
      continue;
    }

    ut_registered_total_region_size += persisted_info[i].extension_size;
    ut_registered_extension_info_count++;

    if (ut_extension_entry_is_active(&persisted_info[i])) {
      ut_extension_info[ut_extension_info_count] = persisted_info[i];
      ut_total_region_size += persisted_info[i].extension_size;
      ut_extension_info_count++;
    }
  }

  {
    uint32_t registered_write_index = ut_extension_info_count;

    for (uint32_t i = 0U; i < SLI_TOKEN_MANAGER_EXT_INFO_MAX; i++) {
      if (!ut_extension_info_entry_is_valid(&persisted_info[i])
          || ut_extension_entry_is_active(&persisted_info[i])) {
        continue;
      }

      ut_extension_info[registered_write_index] = persisted_info[i];
      registered_write_index++;
    }

    ut_registered_extension_info_count = registered_write_index;
  }

  ut_extension_info_loaded = true;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Persists the current UT chain anchor to NVM3 and updates the cached RAM copy
 * on success.
 *
 * The reclaim path moves the logical head before erasing pages. Persisting each
 * head move lets recovery resume safely after a power loss, because the on-flash
 * circular chain and the anchor stored in NVM3 stay synchronized at each reclaim
 * step boundary.
 *
 * @param[in] anchor  Word-aligned address of the first logical UT object.
 *
 * @return SL_STATUS_OK if the anchor was stored, or an error code if the anchor
 *         is invalid or the NVM3 write fails.
 ******************************************************************************/
static sl_status_t upgradable_token_anchor_store(uint32_t anchor)
{
  sl_status_t status = SL_STATUS_OK;

  // Reject corrupt anchors before publishing them to the persistent copy.
  if (!is_upgradable_token_address_in_range(anchor)) {
    return SL_STATUS_INVALID_RANGE;
  }

  status = sli_token_manager_set_dynamic_data(SLI_TOKEN_MANAGER_UT_CHAIN_ANCHOR_NVM3_KEY,
                                              (void *)&anchor,
                                              sizeof(anchor));
  if (status == SL_STATUS_OK) {
    ut_chain_anchor = anchor;
    ut_chain_anchor_loaded = true;
  }

  return status;
}

/***************************************************************************//**
 * Anchor-commit callback passed into the circular-KLV reclaim helpers.
 *
 * Reclaim advances the logical head incrementally as live objects are relocated.
 * The KLV layer does not know where the anchor is persisted, so it calls back
 * into this helper whenever it must durably publish a new head before more flash
 * is erased.
 ******************************************************************************/
static sl_status_t upgradable_token_commit_anchor(uint32_t anchor,
                                                  void *context)
{
  (void)context;
  return upgradable_token_anchor_store(anchor);
}

/***************************************************************************//**
 * @brief
 *  Loads the UT repair record from NVM3.
 ******************************************************************************/
static sl_status_t upgradable_token_repair_load(sli_klv_ut_repair_record_t *record,
                                                void *context)
{
  sl_status_t status;
  (void)context;
  if (record == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  memset(record, 0, sizeof(*record));
  status = sli_token_manager_get_dynamic_data(SLI_TOKEN_MANAGER_UT_REPAIR_NVM3_KEY,
                                               record, 0U, sizeof(*record));
  // If status is SL_STATUS_NOT_FOUND, preserve that distinction.
  return status;
}

/***************************************************************************//**
 * @brief
 *  Stores the UT repair record to NVM3.
 ******************************************************************************/
static sl_status_t upgradable_token_repair_store(const sli_klv_ut_repair_record_t *record,
                                                 void *context)
{
  (void)context;
  if (record == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  return sli_token_manager_set_dynamic_data(SLI_TOKEN_MANAGER_UT_REPAIR_NVM3_KEY,
                                             (void *)record, sizeof(*record));
}

/***************************************************************************//**
 * @brief
 *  Loads the UT tail record from NVM3.
 ******************************************************************************/
static sl_status_t upgradable_token_tail_load(sli_klv_ut_tail_record_t *record,
                                              void *context)
{
  (void)context;
  if (record == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  memset(record, 0, sizeof(*record));
  return sli_token_manager_get_dynamic_data(SLI_TOKEN_MANAGER_UT_TAIL_NVM3_KEY,
                                            record, 0U, sizeof(*record));
}

/***************************************************************************//**
 * @brief
 *  Stores the UT tail record to NVM3.
 ******************************************************************************/
static sl_status_t upgradable_token_tail_store(const sli_klv_ut_tail_record_t *record,
                                               void *context)
{
  (void)context;
  if (record == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  return sli_token_manager_set_dynamic_data(SLI_TOKEN_MANAGER_UT_TAIL_NVM3_KEY,
                                            (void *)record, sizeof(*record));
}

/***************************************************************************//**
 * Erases every page in a registered UT extension region.
 ******************************************************************************/
static sl_status_t ut_erase_extension_region(uint32_t extension_addr,
                                             uint32_t extension_size)
{
  sl_status_t status = SL_STATUS_OK;

  if ((extension_addr == 0U) || (extension_size == 0U)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if ((extension_size % FLASH_PAGE_SIZE) != 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  for (uint32_t offset = 0U; offset < extension_size; offset += FLASH_PAGE_SIZE) {
    status = sli_token_manager_nvm_page_erase(extension_addr + offset);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Marks all inactive registered extensions as active and refreshes the cache.
 *
 * Each extension that is still pending is erased page by page before its
 * @c is_active flag is promoted. Pending regions are not part of the logical
 * ring until activation, but they must be erased before writes can land there.
 ******************************************************************************/
static sl_status_t ut_extension_activation_commit(void *context)
{
  sli_klv_extension_header_t extension_info[SLI_TOKEN_MANAGER_EXT_INFO_MAX];
  sl_status_t status = ut_extension_info_read_nvm3(extension_info);
  bool changed = false;

  (void)context;

  if (status != SL_STATUS_OK) {
    return status;
  }

  // Persist a layout-remap intention while the old ring is authoritative.
  // This closes the crash window between activation NVM3 commit and a tail
  // rebase when the old tail was exactly at the logical end.
  for (uint32_t i = 0U; i < SLI_TOKEN_MANAGER_EXT_INFO_MAX; i++) {
    if (ut_extension_info_entry_is_valid(&extension_info[i])
        && !ut_extension_entry_is_active(&extension_info[i])) {
      sli_klv_ut_context_t old_ctx = load_ut_klv_context();
      status = sli_klv_ut_tail_prepare_extension(&old_ctx,
                                                 extension_info[i].extension_addr);
      if (status != SL_STATUS_OK) {
        return status;
      }
      break;
    }
  }

  for (uint32_t i = 0U; i < SLI_TOKEN_MANAGER_EXT_INFO_MAX; i++) {
    if (!ut_extension_info_entry_is_valid(&extension_info[i])) {
      continue;
    }

    if (!ut_extension_entry_is_active(&extension_info[i])) {
      status = ut_erase_extension_region(extension_info[i].extension_addr,
                                         extension_info[i].extension_size);
      if (status != SL_STATUS_OK) {
        return status;
      }

      extension_info[i].is_extension_active = SLI_TOKEN_MANAGER_UT_EXTENSION_ACTIVE;
      changed = true;
    }
  }

  if (changed) {
    status = ut_extension_info_write_nvm3(extension_info);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  ut_extension_info_loaded = false;
  status = ut_extension_info_load();
  if (status == SL_STATUS_OK) {
    sli_klv_ut_context_t expanded = load_ut_klv_context();
    status = sli_klv_ut_tail_recover(&expanded);
    if (status == SL_STATUS_OK) {
      sli_klv_ut_largest_live_object_cache_invalidate();
    }
  }
  return status;
}

/***************************************************************************//**
 * Builds the KLV context for the upgradable token.
 *
 * @return The KLV context for the upgradable token.
 ******************************************************************************/
 static sli_klv_ut_context_t load_ut_klv_context(void)
 {
   sli_klv_ut_context_t ut_context = {
     .chain_anchor = &ut_chain_anchor,
     .klv_start_address = (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE,
     .klv_end_address = (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_END,
     .extension_info = ut_extension_info,
     .extension_info_count = ut_extension_info_count,
     .total_region_size = ut_total_region_size,
     .registered_extension_info = ut_extension_info,
     .registered_extension_info_count = ut_registered_extension_info_count,
     .registered_total_region_size = ut_registered_total_region_size,
     .anchor_commit = upgradable_token_commit_anchor,
     .anchor_context = NULL,
     .extension_activation_commit = ut_extension_activation_commit,
     .extension_activation_context = NULL,
     .repair_load = upgradable_token_repair_load,
     .repair_store = upgradable_token_repair_store,
     .repair_context = NULL,
     .tail_load = upgradable_token_tail_load,
     .tail_store = upgradable_token_tail_store,
     .tail_context = NULL,
   };

   return ut_context;
 }

/***************************************************************************//**
 * Loads the persisted UT chain anchor, validates it, and completes any pending
 * reclaim recovery before exposing the chain to normal reads or writes.
 *
 * Flow:
 * 1. Ensure the UT extension directory is loaded so range checks include active
 *    extension regions (the logical head may live outside the UT base span).
 * 2. Read the last stored anchor from NVM3.
 * 3. If the anchor is missing or corrupt, reinitialize it to the start of the
 *    UT region so the chain is treated as empty.
 * 4. Ask the KLV recovery helper to detect partially completed reclaim moves
 *    and finish the anchor transition if a relocated head object was left behind.
 * 5. Re-persist the recovered anchor if recovery advanced it.
 *
 * This routine is intentionally called at the start of every public UT API so
 * that power-fail recovery is lazy and self-healing.
 ******************************************************************************/
static sl_status_t upgradable_token_anchor_load(void)
{
  sli_klv_ut_context_t ut_context;
  sl_status_t status = SL_STATUS_OK;
  uint32_t anchor = (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE;

  // Extension metadata must be available before any anchor range check. A valid
  // head can live in an active extension; validating without the directory would
  // treat that address as corrupt and reset the chain to the UT base.
  if (!ut_extension_info_loaded) {
    status = ut_extension_info_load();
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  ut_context = load_ut_klv_context();

  if (ut_chain_anchor_loaded && is_upgradable_token_address_in_range(ut_chain_anchor)) {
    // Read-only API calls must not observe a partially restored source page.
    status = sli_klv_replay_ut_page_repair(&ut_context);
    if (status != SL_STATUS_OK) {
      return status;
    }
    return sli_klv_ut_tail_recover(&ut_context);
  }

  status = sli_token_manager_get_dynamic_data(SLI_TOKEN_MANAGER_UT_CHAIN_ANCHOR_NVM3_KEY,
                                              (void *)&anchor,
                                              0U,
                                              sizeof(anchor));
  // A genuine NVM3 I/O failure is not an empty-chain indication. Never
  // overwrite a previously committed anchor because a read temporarily failed.
  if ((status != SL_STATUS_OK) && (status != SL_STATUS_NOT_FOUND)) {
    return status;
  }

  if ((status == SL_STATUS_NOT_FOUND)
      || !is_upgradable_token_address_in_range(anchor)) {
    // Missing/invalid metadata means the safest recoverable state is an empty
    // chain that starts at the base of the UT region.
    ut_chain_anchor_loaded = false;
    ut_chain_anchor = (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE;
    status = sli_klv_replay_ut_page_repair(&ut_context);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = upgradable_token_anchor_store((uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE);
    if (status != SL_STATUS_OK) {
      return status;
    }
    return sli_klv_ut_tail_recover(&ut_context);
  }

  ut_chain_anchor = anchor;

  // NVM3 repair state is committed before any destructive UT erase. Replay it
  // before scanning KLV headers or running the mutating anchor/reclaim cleanup.
  // If replay fails, init/read/write all fail closed, not with partial data.
  status = sli_klv_replay_ut_page_repair(&ut_context);
  if (status != SL_STATUS_OK) {
    ut_chain_anchor_loaded = false;
    return status;
  }

  // Reconcile pending header before applying the authoritative chain tail.
  // A valid WIP header advances the frontier. A torn header stays pending
  // until the existing no-dedicated-page repair runs on the next mutation.
  status = sli_klv_ut_tail_recover(&ut_context);
  if (status != SL_STATUS_OK) {
    ut_chain_anchor_loaded = false;
    return status;
  }

  // Recover any interrupted reclaim before serving the next public operation.
  status = sli_klv_recover_ut_chain_anchor(&ut_context);
  if (status != SL_STATUS_OK) {
    uint32_t persisted_anchor = (uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE;
    sl_status_t refresh_status =
    sli_token_manager_get_dynamic_data(SLI_TOKEN_MANAGER_UT_CHAIN_ANCHOR_NVM3_KEY,
                                       (void *)&persisted_anchor,
                                       0U,
                                       sizeof(persisted_anchor));

    // Recovery may already have advanced and persisted the logical head before a
    // later validation step failed. Never overwrite NVM3 with the pre-recovery
    // anchor here; instead refresh the RAM cache from whatever anchor is
    // currently persisted so later retries stay aligned with flash state.
    if ((refresh_status == SL_STATUS_OK)
        && is_upgradable_token_address_in_range(persisted_anchor)) {
      ut_chain_anchor = persisted_anchor;
    }

    ut_chain_anchor_loaded = false;
    return status;
  }

  ut_chain_anchor_loaded = true;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Publishes the final anchor after a successful UT mutation and restores the
 * caller-supplied rollback value if the final persist step fails.
 *
 * The KLV layer may already have committed intermediate head moves during
 * reclaim. This helper handles only the final "operation complete" publication
 * expected by the API layer.
 ******************************************************************************/
static sl_status_t upgradable_token_anchor_publish(uint32_t anchor,
                                                   uint32_t rollback_anchor)
{
  sl_status_t status = upgradable_token_anchor_store(anchor);

  if (status != SL_STATUS_OK) {
    // The flash mutation already completed successfully. Keep the newer anchor
    // in RAM for the current boot so subsequent UT operations can continue from
    // the committed KLV state even if the final NVM3 publish failed.
    ut_chain_anchor = anchor;
    ut_chain_anchor_loaded = true;
    (void)rollback_anchor;
  }

  return status;
}

/***************************************************************************//**
 * Initializes the upgradable token read and write operations.
 *
 * This opens the NVM backend over the dedicated UT NVM region and then loads
 * or reconstructs the current circular-chain anchor. The UT NVM region must be
 * explicitly configured; a zero-sized region is treated as a configuration
 * error even if UT support was compiled in.
 *
 * @return SL_STATUS_OK if the UT region is ready for use, or an error code if
 *         the region is misconfigured, the backend cannot be opened, or anchor
 *         recovery fails.
 ******************************************************************************/
sl_status_t sli_token_manager_init_upgradable_token(void)
{
  sl_status_t status = SL_STATUS_OK;

  ut_chain_anchor_loaded = false;

  // Validate the upgradable token region size.
  if (((uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE == 0U)
      || ((uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE < SL_TOKEN_MANAGER_UT_REGION_SIZE_MIN)
      || ((uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE % FLASH_PAGE_SIZE) != 0U) {
    TOKENDBG(printf("sli_token_manager_upgradable_token_init region size: 0x%0lx is not valid\r\n", SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE); )
    return SL_STATUS_INVALID_CONFIGURATION;
  }

  status = sli_token_manager_nvm_open((uint32_t)SLI_TOKEN_UPGRADABLE_TOKEN_BASE,
                                      (uint32_t)SL_TOKEN_MANAGER_UPGRADABLE_TOKEN_REGION_SIZE);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Initialize the NVM3 for upgradable token usage
  status = sli_token_manager_init_dynamic();
  TOKENDBG(printf("sli_token_manager_upgradable_token_init dynamic_init status: 0x%0lx\r\n", status); )
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Invalidate the cached largest live object size
  sli_klv_ut_largest_live_object_cache_invalidate();

  ut_extension_info_loaded = false;
  status = ut_extension_info_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  return upgradable_token_anchor_load();
}

#if defined(SL_TOKEN_MANAGER_TEST)
/***************************************************************************//**
 * Clears the in-RAM UT chain-anchor and extension-directory caches.
 *
 * Unit tests use this to force the next public UT API through the cold-cache
 * reload path without erasing NVM3 or the on-flash KLV chain.
 ******************************************************************************/
void sli_token_manager_ut_invalidate_ram_caches(void)
{
  ut_chain_anchor_loaded = false;
  ut_extension_info_loaded = false;
}

/***************************************************************************//**
 * Clears only the in-RAM UT extension-directory cache.
 *
 * Unit tests use this to reproduce the case where a valid extension-resident
 * chain anchor is still marked loaded while extension metadata is not.
 ******************************************************************************/
void sli_token_manager_ut_invalidate_extension_info_cache(void)
{
  ut_extension_info_loaded = false;
}
#endif // SL_TOKEN_MANAGER_TEST

/***************************************************************************//**
 * Reads an upgradable token from the circular UT KLV chain.
 *
 * The API first synchronizes the in-RAM anchor with the persisted metadata so
 * that reads see the post-recovery logical head after a reset or interrupted
 * reclaim. It then delegates the actual chain walk and payload extraction to
 * the anchor-based KLV reader.
 *
 * @param[in]  token   32-bit upgradable-token identifier.
 * @param[out] data    Destination buffer for the requested bytes.
 * @param[in]  offset  Byte offset inside the token payload.
 * @param[in]  length  Number of bytes to copy.
 *
 * @return SL_STATUS_OK if the token is present and readable, or an error code
 *         if the inputs are invalid, recovery fails, or the token is missing.
 ******************************************************************************/
sl_status_t sli_token_manager_get_upgradable_token_data(uint32_t token,
                                                        void *data,
                                                        uint32_t offset,
                                                        uint32_t length)
{
  sl_status_t status = SL_STATUS_OK;
  sl_klv_handle_t klv_handle = { token, offset, length };
  sli_klv_ut_context_t ut_context;

  if ((data == NULL) || (length == 0U)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = upgradable_token_anchor_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Rebuild the KLV context in RAM to ensure it is up to date with the latest anchor
  ut_context = load_ut_klv_context();

  return sli_read_klv_object_from_anchor(&ut_context, &klv_handle, data);
}

/***************************************************************************//**
 * Returns the current payload size of an upgradable token.
 *
 * This follows the same recovery-first pattern as reads: refresh the logical
 * chain anchor, then scan the anchor-based KLV chain for the latest live entry
 * that matches the requested token.
 ******************************************************************************/
sl_status_t sli_token_manager_get_upgradable_token_size(uint32_t token,
                                                        uint32_t *size_out)
{
  sl_status_t status = SL_STATUS_OK;
  sli_klv_ut_context_t ut_context;

  if (size_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = upgradable_token_anchor_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Rebuild the KLV context in RAM to ensure it is up to date with the latest anchor
  ut_context = load_ut_klv_context();

  return sli_token_manager_get_size_from_klv_anchor(&ut_context, token, size_out);
}

/***************************************************************************//**
 * @brief
 *  Torn header repair is a mutation, never performed by a read/size API.
 *  The KLV layer proves scratch ownership within its CURRENT logical ring.
 ******************************************************************************/
static sl_status_t upgradable_token_heal_before_mutation(void)
{
  uint32_t stored_anchor = 0U;
  sli_klv_ut_context_t context = load_ut_klv_context();
  // The journal binds its scratch proof to the durable NVM3 anchor. A prior
  // anchor-store error must not let RAM-only changes authorize page erasure.
  sl_status_t status = sli_token_manager_get_dynamic_data(SLI_TOKEN_MANAGER_UT_CHAIN_ANCHOR_NVM3_KEY,
                                                          &stored_anchor,
                                                          0U,
                                                          sizeof(stored_anchor));

  if (status != SL_STATUS_OK) {
    return status;
  }

  if (stored_anchor != ut_chain_anchor) {
    return SL_STATUS_INVALID_STATE;
  }
  return sli_klv_heal_ut_torn_tail(&context);
}

/***************************************************************************//**
 * Appends or overwrites an upgradable token in the circular UT KLV chain.
 *
 * Flow:
 * 1. Recover and validate the current chain anchor.
 * 2. Ask the anchor-based KLV writer to append the new object directly or, if
 *    required, reclaim space by relocating live objects.
 * 3. If reclaim or the final write moved the logical head, persist the new
 *    anchor. If a later step fails, preserve any intermediate anchors that
 *    reclaim already committed so recovery stays aligned with flash state.
 *
 * The KLV writer performs the detailed reserve calculations, wrap handling, and
 * page-erase sequencing; this API owns only the persisted anchor lifecycle.
 ******************************************************************************/
sl_status_t sli_token_manager_set_upgradable_token_data(uint32_t token,
                                                        const void *data,
                                                        uint32_t length)
{
  sl_status_t status = SL_STATUS_OK;
  sl_klv_handle_t klv_handle = { token, 0U, length };
  sli_klv_ut_context_t ut_context;
  uint32_t previous_anchor = 0U;

  if ((data == NULL) || (length == 0U)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = upgradable_token_anchor_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = upgradable_token_heal_before_mutation();
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Rebuild the KLV context in RAM to ensure it is up to date with the latest anchor
  ut_context = load_ut_klv_context();
  previous_anchor = ut_chain_anchor;

  status = sli_write_klv_object_from_anchor(&ut_context, &klv_handle, data);
  if (status != SL_STATUS_OK) {
    // Reclaim may already have relocated live objects and durably advanced the
    // anchor through anchor_commit(). Never roll NVM3 back to the pre-operation
    // head here, or the persisted chain anchor can point behind the live NVM
    // chain after a partial write failure.
    return status;
  }

  return upgradable_token_anchor_publish(ut_chain_anchor, previous_anchor);
}

/***************************************************************************//**
 * Soft-deletes an upgradable token from the circular UT KLV chain.
 *
 * Deletion marks the matching KLV object as deleted in place. If the deleted
 * object was the logical head, the anchor is advanced past soft-deleted heads
 * (WIP heads are kept) and persisted so future scans start from the correct
 * entry. Actual space recovery is deferred until a later reclaim-triggering
 * write.
 ******************************************************************************/
sl_status_t sli_token_manager_delete_upgradable_token(uint32_t token)
{
  sl_status_t status = SL_STATUS_OK;
  sli_klv_ut_context_t ut_context;
  uint32_t previous_anchor = 0U;

  status = upgradable_token_anchor_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Rebuild the KLV context in RAM to ensure it is up to date with the latest anchor
  ut_context = load_ut_klv_context();

  status = upgradable_token_heal_before_mutation();
  if (status != SL_STATUS_OK) {
    return status;
  }
  ut_context = load_ut_klv_context();

  previous_anchor = ut_chain_anchor;
  status = sli_delete_klv_object_from_anchor(&ut_context, token);
  if (status != SL_STATUS_OK) {
    // Delete can also fail after reclaim has already persisted a newer anchor.
    // Keep the latest committed head instead of restoring the stale pre-delete
    // anchor and desynchronizing NVM3 from the NVM circular chain.
    return status;
  }

  return upgradable_token_anchor_publish(ut_chain_anchor, previous_anchor);
}

/***************************************************************************//**
 * Returns the current health and utilization of the storage.
 *
 * @note Currently this call is only supported for upgradable tokens.
 *
 * @param[out] info_out  Destination structure for the storage summary.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sli_token_manager_get_ut_storage_info(sl_token_manager_storage_info_t *info_out)
{
  sli_klv_ut_context_t ut_context;
  sl_status_t status = SL_STATUS_OK;

  if (info_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = upgradable_token_anchor_load();
  if (status == SL_STATUS_OK) {
    // Rebuild the KLV context in RAM to ensure it is up to date with the latest
    // anchor and registered extension capacity (including pending).
    ut_context = load_ut_klv_context();

    status = sli_klv_get_ut_storage_info(&ut_context, info_out);
  }

  return status;
}

/***************************************************************************//**
 * Start streaming write: dispatch by token type to KLV (or future backends).
 ******************************************************************************/
sl_status_t sli_token_manager_set_ut_start(uint32_t token, uint32_t total_length)
{
  sli_klv_ut_context_t ut_context;
  sl_status_t status = SL_STATUS_OK;

  status = upgradable_token_anchor_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Rebuild the KLV context in RAM to ensure it is up to date with the latest anchor
  ut_context = load_ut_klv_context();

  status = upgradable_token_heal_before_mutation();
  if (status != SL_STATUS_OK) {
    return status;
  }
  ut_context = load_ut_klv_context();

  return sli_klv_set_ut_start(&ut_context, token, total_length);
}
 
/***************************************************************************//**
 * Write a chunk into an active streamed UT session.
 ******************************************************************************/
sl_status_t sli_token_manager_set_ut_update(uint32_t token,
                                            const void *data,
                                            uint32_t length)
{
  return sli_klv_set_ut_update(token, data, length);
}
 
/***************************************************************************//**
 * Finalize the streaming write session for @p token.
 ******************************************************************************/
sl_status_t sli_token_manager_set_ut_finish(uint32_t token)
{
  sl_status_t status = SL_STATUS_OK;
  sli_klv_ut_context_t ut_context;

  status = upgradable_token_anchor_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  ut_context = load_ut_klv_context();

  return sli_klv_set_ut_finish(&ut_context, token);
}

/***************************************************************************//**
 * Extends the upgradable token storage space.
 ******************************************************************************/
sl_status_t sli_token_manager_extend_ut_storage(uint32_t extension_addr,
                                                uint32_t extension_size)
{
  sl_status_t status = SL_STATUS_OK;
  sli_klv_extension_header_t extension_info[SLI_TOKEN_MANAGER_EXT_INFO_MAX];
  sli_klv_extension_header_t new_extension_info = { 0 };
  uint32_t free_slot = SLI_TOKEN_MANAGER_EXT_INFO_MAX;

  // Verify that the extension address and size are within the NVM's 
  // data region in case of series 3 devices and within the device main flash
  // array in case of series 2 devices.
  status = sli_token_manager_nvm_open(extension_addr, extension_size);
  if (status != SL_STATUS_OK) {
    return SL_STATUS_INVALID_RANGE;
  }

  // Validate that the provided extension address and size do not overlap
  // with the UT's main region or a reserved region.
  if (ut_extension_overlaps_reserved_region(extension_addr, extension_size)) {
    return SL_STATUS_INVALID_RANGE;
  }

  status = ut_extension_info_read_nvm3(extension_info);
  if (status == SL_STATUS_NOT_FOUND) {
    memset(extension_info, 0, sizeof(extension_info));
  } else if (status != SL_STATUS_OK) {
    return status;
  }

  for (uint32_t i = 0U; i < SLI_TOKEN_MANAGER_EXT_INFO_MAX; i++) {
    if (!ut_extension_info_entry_is_valid(&extension_info[i])) {
      if (free_slot == SLI_TOKEN_MANAGER_EXT_INFO_MAX) {
        free_slot = i;
      }
      continue;
    }

    // Validate that the provided extension address does not match an
    // existing extension address.
    if (extension_info[i].extension_addr == extension_addr) {
      return SL_STATUS_ALREADY_EXISTS;
    }

    // Validate that the provided extension address and size do not overlap
    // with an existing extension.
    if (ut_extension_flash_regions_overlap(extension_addr,
                                           extension_size,
                                           extension_info[i].extension_addr,
                                           extension_info[i].extension_size)) {
      return SL_STATUS_INVALID_RANGE;
    }
  }

  if (free_slot == SLI_TOKEN_MANAGER_EXT_INFO_MAX) {
    return SL_STATUS_FULL;
  }

  new_extension_info.extension_addr = extension_addr;
  new_extension_info.extension_size = extension_size;
  new_extension_info.crc =
    ut_extension_info_entry_crc_compute(extension_addr, extension_size);
  new_extension_info.is_extension_active = SLI_TOKEN_MANAGER_UT_EXTENSION_INACTIVE;
  new_extension_info.reserved = 0xFFU;

  extension_info[free_slot] = new_extension_info;

  // Write the new extension info to NVM3.
  status = ut_extension_info_write_nvm3(extension_info);
  if (status != SL_STATUS_OK) {
    return status;
  }

  ut_extension_info_loaded = false;
  status = ut_extension_info_load();
  if (status != SL_STATUS_OK) {
    return status;
  }

  return SL_STATUS_OK;
}
