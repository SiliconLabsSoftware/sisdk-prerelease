/***************************************************************************/ /**
 * @file sli_crash_manager_ctm.c
 * @brief Implementation of the Silicon Labs Crash Manager CTM storage
 * @version 1.0.0
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

/*******************************************************************************
 ********************************   INCLUDES   ********************************
 ******************************************************************************/

#include "sli_crash_manager_flash.h"
#include "sli_crash_manager.h"

#if defined(SL_CATALOG_CRASH_MANAGER_BACKEND_CTM_PRESENT)

#include "sl_token_manager_defines.h"
#include "sl_token_manager_api.h"

/** NVM3 domain/key for crash log dynamic token (platform NVM3 region; see sl_token_manager_defines.h). */
#define SLI_CRASH_MANAGER_CTM_NVM3_KEY SL_TOKEN_NVM3_REGION_PLATFORM
/** Full CTM token id for persisted crash log. */
#define SLI_CRASH_MANAGER_CTM_TOKEN \
  SL_TOKEN_GET_DYNAMIC_TOKEN(SLI_CRASH_MANAGER_CTM_NVM3_KEY, 0)

/*******************************************************************************
 **************************   EXTERNALS   **************************************
 ******************************************************************************/

/** @brief Set true after sl_token_manager_init() succeeds (sl_token_manager_api.c). */
extern bool sl_token_manager_init_success;

/*******************************************************************************
 **************************   INITIALIZATION   *********************************
 ******************************************************************************/

/**
 * @brief Ensure token manager is ready for crash log tokens.
 *
 * @return SL_STATUS_OK if already initialized or init succeeds;
 *         token manager status on failure
 */
sl_status_t sli_crash_manager_flash_init(void)
{
  if (!sl_token_manager_init_success) {
    return sl_token_manager_init();
  }
  return SL_STATUS_OK;
}

/*******************************************************************************
 **************************   READ / WRITE / DELETE   **************************
 ******************************************************************************/

/**
 * @brief Persist crash log blob to the CTM dynamic token.
 *
 * @param[in] data  Full crash record; must not be NULL
 * @return SL_STATUS_OK on success; SL_STATUS_INVALID_PARAMETER if NULL;
 *         SL_STATUS_NOT_INITIALIZED if token manager not ready;
 *         token manager status on failure
 */
sl_status_t sli_crash_manager_flash_write(const volatile sl_crash_manager_data_t *data)
{
  if (data == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (!sl_token_manager_init_success) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  sl_crash_manager_data_t copy = *data;
  return sl_token_manager_set_data(SLI_CRASH_MANAGER_CTM_TOKEN,
                                   &copy,
                                   sizeof(sl_crash_manager_data_t));
}

/**
 * @brief Remove persisted crash log token from storage.
 *
 * @return SL_STATUS_OK on success or if token was already absent;
 *         SL_STATUS_NOT_INITIALIZED if token manager not ready;
 *         token manager status on failure
 */
sl_status_t sli_crash_manager_flash_delete(void)
{
  if (!sl_token_manager_init_success) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  sl_status_t delete_status = sl_token_manager_delete_dynamic_token(SLI_CRASH_MANAGER_CTM_TOKEN);
  if (delete_status == SL_STATUS_NOT_FOUND) {
    return SL_STATUS_OK;
  }
  return delete_status;
}

/*******************************************************************************
 **************************   LOG RETRIEVAL   ***********************************
 ******************************************************************************/

/**
 * @brief Load crash log from token storage and validate layout and CRC.
 *
 * @param[out] data  Receives full record on success; must not be NULL
 * @return SL_STATUS_OK if valid crash; SL_STATUS_EMPTY if erased/empty slot;
 *         SL_STATUS_NOT_FOUND if no token; SL_STATUS_INVALID_PARAMETER if NULL;
 *         SL_STATUS_INVALID_STATE if size or content invalid;
 *         SL_STATUS_NOT_INITIALIZED if token manager not ready;
 *         SL_STATUS_FAIL on read error
 */
sl_status_t sli_crash_manager_flash_get_log(sl_crash_manager_data_t *data)
{
  uint32_t token_size = 0U;
  sl_status_t status;

  if (data == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (!sl_token_manager_init_success) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  status = sl_token_manager_get_size(SLI_CRASH_MANAGER_CTM_TOKEN, &token_size);
  if (status == SL_STATUS_NOT_FOUND) {
    return SL_STATUS_NOT_FOUND;
  }
  if (status != SL_STATUS_OK) {
    return status;
  }
  if (token_size != sizeof(sl_crash_manager_data_t)) {
    return SL_STATUS_INVALID_STATE;
  }

  status = sl_token_manager_get_data(SLI_CRASH_MANAGER_CTM_TOKEN,
                                     data,
                                     sizeof(sl_crash_manager_data_t));
  if (status != SL_STATUS_OK) {
    if (status == SL_STATUS_NOT_FOUND) {
      return SL_STATUS_NOT_FOUND;
    }
    return SL_STATUS_FAIL;
  }
  if (!sli_crash_log_is_valid(data)) {
    if (data->magic == 0U && data->type == SL_CRASH_TYPE_NONE) {
      return SL_STATUS_EMPTY;
    }
    return SL_STATUS_INVALID_STATE;
  }
  return SL_STATUS_OK;
}

/*******************************************************************************
 **************************   STORAGE VTABLE (PLUGIN)   *************************
 ******************************************************************************/

/** @brief Storage backend init: delegates to sli_crash_manager_flash_init */
static sl_status_t ctm_storage_init(void *ctx)
{
  (void)ctx;
  return sli_crash_manager_flash_init();
}

/** @brief Storage backend write: delegates to sli_crash_manager_flash_write */
static sl_status_t ctm_storage_write(void *ctx, const volatile sl_crash_manager_data_t *data)
{
  (void)ctx;
  return sli_crash_manager_flash_write(data);
}

/** @brief Storage backend erase: delegates to sli_crash_manager_flash_delete */
static sl_status_t ctm_storage_erase(void *ctx)
{
  (void)ctx;
  return sli_crash_manager_flash_delete();
}

/** @brief Storage backend get_log: delegates to sli_crash_manager_flash_get_log */
static sl_status_t ctm_storage_get_log(void *ctx, sl_crash_manager_data_t *data)
{
  (void)ctx;
  return sli_crash_manager_flash_get_log(data);
}

static const sli_crash_manager_storage_ops_t s_ctm_storage_ops = {
  .init = ctm_storage_init,
  .write = ctm_storage_write,
  .erase = ctm_storage_erase,
  .get_log = ctm_storage_get_log,
};

/**
 * @brief Register CTM token storage with the crash manager (service_init).
 *
 * Installs the storage vtable so the crash manager can init NVM, read, erase, and persist via @ref sl_crash_manager_persist_pending_record.
 */
void sli_crash_manager_ctm_plugin_register(void)
{
  sli_crash_manager_register_storage(&s_ctm_storage_ops, NULL);
}

#endif /* SL_CATALOG_CRASH_MANAGER_BACKEND_CTM_PRESENT */
