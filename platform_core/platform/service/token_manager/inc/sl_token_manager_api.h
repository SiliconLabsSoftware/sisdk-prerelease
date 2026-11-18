/***************************************************************************//**
 * @file sl_token_manager_api.h
 * @brief Common token manager APIs.
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

#ifndef SL_TOKEN_MANAGER_API_H
#define SL_TOKEN_MANAGER_API_H

#if !defined(SL_ZIGBEE_TEST)
#include "sl_common.h"
#endif
#include <stdbool.h>
#include "sl_enum.h"
#include "sl_token_manager_config.h"
#include "sl_status.h"
#include "sl_token_manager_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup common_token_manager
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @brief Token type enumeration.
 ******************************************************************************/
SL_ENUM(sl_token_type_t) {
    SL_TOKEN_TYPE_DYNAMIC_TOKEN = 0,       ///< Default dynamic token type.
    SL_TOKEN_TYPE_DYNAMIC_TOKEN_SECONDARY, ///< Secondary dynamic token type. Used for Z-Wave.
    SL_TOKEN_TYPE_STATIC_DEVICE_TOKEN,     ///< Static device token type.
    SL_TOKEN_TYPE_STATIC_SECURE_TOKEN,     ///< Static secure token type.
    SL_TOKEN_TYPE_UPGRADABLE_TOKEN,        ///< Upgradable token type.
    SL_TOKEN_TYPE_MAX                      ///< Maximum token type index.
  };

/**
 * @brief Storage information structure
 *
 * Provides information about the storage, such as total size, used size, free size,
 * reclaimable size of the storage.
 *
 * @note: Currently the storage information is only supported for upgradable tokens.
 */
typedef struct {
  /** @brief Configured UT storage capacity in bytes (default UT region plus every
   *  registered extension, including inactive/pending ones). Grows as soon as
   *  @ref sl_token_manager_extend_storage succeeds. */
  uint32_t total_size;
  /** @brief Total size consumed by the live objects in the storage in bytes */
  uint32_t used_size;
  /** @brief Free size of the configured storage in bytes.
   * Computed as total_size - used_size - reclaimable_size - repack_reserve_size.
   * @note If the chain is empty, free_size equals total_size when the repack
   *       reserve is zero. Registered-but-inactive capacity is included in
   *       total_size/free_size accounting, but remains outside the active KLV
   *       ring until a reclaim activates it. */
  uint32_t free_size;
  /** @brief Reclaimable size in bytes, which can be reclaimed by the repacking operation.
   * This is the size of the objects that are marked as deleted in the chain. */
  uint32_t reclaimable_size;
} sl_token_manager_storage_info_t;

/***************************************************************************//**
 * Initialize the Token Manager.
 *
 * @note This function must be called before any other token manager
 * functions are called. The sl_token_manager_init() call will be automatically
 * added to your initialization.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sl_token_manager_init(void);

/***************************************************************************//**
 * Read token data.
 *
 * @param[in]  token       A 32-bit token identifier. Refer sl_token_manager_defines.h
 *                         on how to create a token.
 * @param[out] data        A pointer to where the token data should be placed.
 * @param[in]  length      The size of the data to be read, in bytes.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sl_token_manager_get_data(uint32_t token,
                                      void *data,
                                      uint32_t length);

/***************************************************************************//**
 * Write token data.
 *
 * @note Writing static tokens on Silicon Labs devices has different constraints
 * depending on the series:
 *
 * Silicon Labs Series 2 (EFR) devices: Only static token values that have not
 * been written since the last erase can be written. For areas of flash that
 * cannot be erased by user code, those static tokens are effectively write-once.
 *
 * Silicon Labs Series 3 (SixG3xx) devices: While static device tokens
 * support overwriting, the write operation is limited to a maximum of 100 writes.
 * The static secure tokens are effectively write-once.
 *
 * @param[in] token    A 32-bit token identifier. Refer sl_token_manager_defines.h
 *                     on how to create a token.
 * @param[in] data     A pointer to the data being written.
 * @param[in] length   The size of the token data in number of bytes.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 * @return SL_STATUS_IN_PROGRESS if this upgradable token already has an active
 *         streaming session (@ref sl_token_manager_set_start).
 * @return SL_STATUS_BUSY if an upgradable-token streaming write is already
 *         active and this write would reclaim/repack, or if an active stream
 *         already reserved its destination through reclaim.
 * @return SL_STATUS_WOULD_OVERFLOW if an upgradable-token object and its
 *         required repack reserve do not fit in registered capacity (base UT
 *         plus all registered extensions, including pending).
 ******************************************************************************/
sl_status_t sl_token_manager_set_data(uint32_t token,
                                      void *data,
                                      uint32_t length);

/***************************************************************************//**
 * This call supports reading both dynamic and static tokens partially.
 *
 * @param[in]  token       A 32-bit token identifier. Refer sl_token_manager_defines.h
 *                         on how to create a token.
 * @param[out] data        A pointer to where the token data should be placed.
 * @param[in]  offset      Indicates the number of bytes to skip from the beginning of the
 *                         token data before starting to read.
 * @param[in]  length      The size of the data (full or partial) to be read, in bytes.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sl_token_manager_get_partial_data(uint32_t token,
                                              void *data,
                                              uint32_t offset,
                                              uint32_t length);

/***************************************************************************//**
 * This call supports deleting a dynamic token.
 *
 * @note This call also supports deleting static tokens, if they have been
 * overridden with NVM3 override tokens. Only the override tokens are deleted.
 *
 * @param[in] token   A 32-bit token identifier. Refer sl_token_manager_defines.h
 *                    on how to create a token.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sl_token_manager_delete_dynamic_token(uint32_t token);

/***************************************************************************//**
 * This call deletes a dynamic or upgradable token identified by the 32-bit token
 * identifier.
 *
 * @note Dynamic tokens are deleted from NVM3 by reusing
 *       @ref sl_token_manager_delete_dynamic_token.
 *
 * @note Upgradable tokens use soft deletion. The matching KLV object is marked 
 *       as deleted in place and subsequent reads will no longer return the token.
 *
 * @param[in] token   A 32-bit token identifier. Refer sl_token_manager_defines.h
 *                    on how to create a token.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 *         @ref SL_STATUS_NOT_FOUND if no active object with that key exists.
 *         @ref SL_STATUS_NOT_SUPPORTED if @p delete operation is not supported
 *         for the token type.
 ******************************************************************************/
sl_status_t sl_token_manager_delete_token(uint32_t token);

/***************************************************************************//**
 * Increments the value of a counter object by 1. Counter tokens are stored in NVM3.
 *
 * @param[in] token  A 32-bit token identifier. Refer sl_token_manager_defines.h
 *                   on how to create a token.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sl_token_manager_increment_counter(uint32_t token);

/***************************************************************************//**
 * This call reads the token size.
 *
 * @note This call can be used to know the token size and user can allocate buffer
 * for holding the token data at runtime.
 *
 * @param[in]  token     A 32-bit token identifier. Refer sl_token_manager_defines.h
 *                       on how to create a token.
 * @param[out] size_out  output parameter to know the size of token.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 ******************************************************************************/
sl_status_t sl_token_manager_get_size(uint32_t token,
                                      uint32_t *size_out);

/***************************************************************************//**
 * Start a streaming write of a large token. Currently only supported for upgradable tokens.
 *
 * @param[in]  token         A 32-bit token identifier.
 * @param[in]  total_length  Total size of the token data in bytes.
 *
 * @return SL_STATUS_OK on success, an error code otherwise.
 * @return SL_STATUS_IN_PROGRESS if this token already has an active streaming
 *         session.
 * @return SL_STATUS_BUSY if another stream already reserved space through
 *         reclaim, or if starting this stream would reclaim/repack while
 *         another stream is still active.
 * @return SL_STATUS_WOULD_OVERFLOW if the object and its required repack reserve
 *         do not fit in registered capacity (base UT plus all registered 
 *         extensions, including pending).
 * @return SL_STATUS_NOT_SUPPORTED if the token type does not support streaming.
 ******************************************************************************/
sl_status_t sl_token_manager_set_start(uint32_t token,
                                       uint32_t total_length);

/***************************************************************************//**
 * Update the streaming write with the next chunk of data. Currently only 
 * supported for upgradable tokens.
 *
 * May be called repeatedly until all @p total_length bytes have been supplied.
 * Chunks are appended in call order; the stream implementation tracks the
 * current payload offset internally.
 *
 * @param[in]  token   The same 32-bit token identifier passed to
 *                     @ref sl_token_manager_set_start.
 * @param[in]  data    Pointer to the chunk data.
 * @param[in]  length  Size of this chunk in bytes.
 *
 * @return SL_STATUS_OK on success, an error code otherwise.
 * @return SL_STATUS_NOT_SUPPORTED if the token type does not support streaming.
 ******************************************************************************/
sl_status_t sl_token_manager_set_update(uint32_t token,
                                        const void *data,
                                        uint32_t length);

/***************************************************************************//**
 * Finalize a streaming write. Currently only supported for upgradable tokens.
 *
 * Clears the committed bit to mark the token as fully written, then frees
 * the internal streaming context.
 *
 * @param[in]  token  The same 32-bit token identifier passed to
 *                    @ref sl_token_manager_set_start.
 *
 * @return SL_STATUS_OK on success, an error code otherwise.
 * @return SL_STATUS_NOT_SUPPORTED if the token type does not support streaming.
 ******************************************************************************/
sl_status_t sl_token_manager_set_finish(uint32_t token);

/***************************************************************************//**
 * Returns the current health and utilization of the storage.
 *
 * @note Currently this call is only supported for upgradable tokens.
 * @note @p info_out->total_size is registered configured capacity (default UT
 *       region plus all registered extensions, including pending). @p free_size
 *       is derived from that same @c total_size so the reported fields stay
 *       internally consistent. Pending extensions still do not join the active
 *       KLV ring until reclaim activates them.
 *
 * @param[in]  token_type  The type of token to get the storage info for.
 * @param[out] info_out    Destination structure for the storage summary.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 *         @ref SL_STATUS_NOT_SUPPORTED if @p token_type is not an upgradable token.
 ******************************************************************************/
sl_status_t sl_token_manager_get_storage_info(sl_token_type_t token_type,
                                              sl_token_manager_storage_info_t *info_out);

/***************************************************************************//**
 * When the allocated default storage space for a token type is not enough,
 * the application can register additional flash for that token type. This call
 * records @p extension_addr and @p extension_size in the UT extension directory
 * (NVM3) as inactive/pending. The bytes are reserved for the UT ring and become
 * usable once a reclaim activates the extension; until then reads, deletes, and
 * direct writes continue on the current active layout.
 *
 * @note Successful registration immediately increases
 *       @ref sl_token_manager_get_storage_info() @c total_size (and therefore the
 *       derived @c free_size) by @p extension_size. The extension remains
 *       inactive for KLV addressing and writes until a reclaim activates it.
 *
 * @note This function does not activate the extension. Activation happens during
 *       reclaim when a write needs the extra space.
 *
 * @note Currently this call is only supported for upgradable tokens.
 *
 * @note The extension storage can support maximum of 10 entries.
 *
 * @param[in]  token_type      The type of token to set the extension for.
 * @param[in]  extension_addr  The address of the extension.
 * @param[in]  extension_size  The size of the extension in bytes.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise.
 *         @ref SL_STATUS_NOT_INITIALIZED if the token manager is not initialized.
 *         @ref SL_STATUS_NOT_SUPPORTED if @p extend_storage operation is not supported for the token type.
 *         @ref SL_STATUS_INVALID_PARAMETER if @p extension_addr or @p extension_size is invalid.
 *         @ref SL_STATUS_INVALID_RANGE if @p extension_addr is not aligned to FLASH_PAGE_SIZE,
 *              @p extension_addr overlaps the default UT region, overlaps a reserved region
 *              (static secure storage, default NVM3), or overlaps another registered extension,
 *              or the extension is outside programmable flash: on Series 3, outside the NVM
 *              data region. On Series 2, outside main flash (@c FLASH_BASE / @c FLASH_SIZE).
 *         @ref SL_STATUS_ALREADY_EXISTS if the extension already exists.
 *         @ref SL_STATUS_FULL if the extension storage is full.
 ******************************************************************************/
sl_status_t sl_token_manager_extend_storage(sl_token_type_t token_type,
                                            uint32_t extension_addr,
                                            uint32_t extension_size);
/** @} end token_manager */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/***************************************************************************//**
 * Internal assert failed function.
 *
 * @note Minimal assert functionality to support token manager asserts.  In
 * the future a fully formed assert component will become available and make
 * this assert code obsolete.
 *
 * @param filename
 * @param linenumber
 ******************************************************************************/
void halInternalAssertFailed(const char * filename, int linenumber);

#endif // DOXYGEN_SHOULD_SKIP_THIS

#if defined(SL_TOKEN_MANAGER_BACKWARD_COMPATIBILITY) || defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH)
// Note: Below header is for S2 backward compatibility.
// The legacy hal API support for silicon labs EFR series 2 devices
// are planned for deprecation. However the new API's listed above
// will be supported. Plan to migrate to new common token manager API's.
#include "sl_token_manager.h"
#endif
#ifdef __cplusplus
}
#endif

// ******** THE REST OF THE FILE IS DOCUMENTATION ONLY !***********************
/// @addtogroup common_token_manager Common Token Manager
/// @{
/// @brief Routines for working with tokens using the Common Token Manager.
///
///   @details
///
///
/// @n @section common_token_manager Introduction
///
/// The Common Token Manager (CTM) provides a means to read and write manufacturing
/// (static device and static secure) and dynamic tokens such as manufacturing ID,
/// channel number, transmit power, and various pieces of information that the application
/// needs to be persistent between device power cycles. The token system is designed to
/// abstract implementation details and simplify interacting with differing non-volatile
/// systems. The majority of tokens are stored in NVM3 where they can be rewritten.
/// By default the CTM uses NVM3 component for storage.
///
/// The CTM API is designed to be used across different platforms, providing a unified
/// interface for token management. It supports both dynamic tokens and static tokens.
/// - The dynamic tokens are stored in NVM3, which can be modified at runtime,
/// allowing for read and write operations.
/// - The static tokens are typically used for manufacturing data and are not
///   intended to be modified after the initial write.
///   The static tokens are further divided into
///   - Static device tokens, which are stored in secure memory regions, has limited
///     storage capacity, and are mass erase protected.
///   - Static secure tokens, which are stored in a dedicated flash region, with
///     additional security measures when flash is external (uses 128-bit AES-GCM
///     mode to secure the data). The static secure region has a reserved 8k space
///     at the end of flash. This space is used for storing larger size tokens.
///
/// In CTM, tokens are identified by a 32-bit identifier that includes the token type,
/// size, and a unique key. The token type can be one of the following:
/// - SL_TOKEN_TYPE_DYNAMIC_TOKEN: For dynamic tokens stored in NVM3.
/// - SL_TOKEN_TYPE_DYNAMIC_TOKEN_SECONDARY: For dynamic tokens stored in a secondary NVM3 instance.
/// - SL_TOKEN_TYPE_STATIC_DEVICE_TOKEN: For static device tokens stored in secure memory.
/// - SL_TOKEN_TYPE_STATIC_SECURE_TOKEN: For static secure tokens stored in a dedicated flash region.
/// - SL_TOKEN_TYPE_UPGRADABLE_TOKEN: For upgradable tokens stored in the UT region.
/// The token size is embedded in the identifier for only static tokens,
/// allowing the CTM to manage tokens of varying sizes.
///
/// To create a 32-bit token identifier, the CTM provides helper macros such as:
/// - SL_TOKEN_GET_STATIC_DEVICE_TOKEN(token): Creates a static device token.
/// - SL_TOKEN_GET_STATIC_SECURE_TOKEN(token): Creates a static secure token.
/// - SL_TOKEN_GET_DYNAMIC_TOKEN(token, IsCounterObj): Creates a dynamic token for
///   default NVM3 instance.
/// - SL_TOKEN_GET_DYNAMIC_SECONDARY_INSTANCE_TOKEN(token, IsCounterObj): Creates a
///   dynamic token for a secondary NVM3 instance.
/// - SL_TOKEN_GET_DYNAMIC_OVER_RIDE_TOKEN(token): Creates a dynamic override token
///   for static tokens. Phase-1 keys (bits 15:12 are 0x1000 or 0x3000) map to
///   0x8E000 / 0x8F000. New 16-bit keys map to 0xE0000 / 0xF0000 from bits 31:29.
///   Returns @ref SL_TOKEN_NVM3_DYNAMIC_OVERRIDE_TOKEN_INVALID when the type cannot
///   be identified. Pass the 32-bit static token for new keys.
/// For more information refer to @ref `sl_token_manager_defines.h` file.
///
/// The CTM API provides functions to initialize the token manager, read and write token data,
/// increment counter tokens, and manage token sizes. It also supports partial reads of
/// token data, allowing applications to access specific portions of token data without
/// needing to read the entire token.
///
/// ## Override Tokens
/// This feature enables applications to store copies of static manufacturing
/// tokens in NVM3, allowing read/write operations on the NVM3 version rather
/// than relying exclusively on the immutable or restricted static token regions.
/// It offers a flexible way to override factory-programmed values when necessary,
/// while preserving the integrity of the original static data.
///
/// To enable NVM3 override functionality, the override config flag
/// `SL_TOKEN_MANAGER_ENABLE_OVERRIDE_TOKENS` in your project sl_token_manager_config.h
/// configuration file should be `enabled` or set to `1`, by enabling this feature
/// the default token retrieval mechanisms can be overridden by custom implementations.
///
/// The search order for tokens read request (when the override flag is enabled) is as follows:
///
/// - By default search a static token in NVM3, when NVM3 override config flag is
/// active, if found return the token data read from NVM3 region.
///   The NVM3 key is built from the full 32-bit static token so bits 31:27 and
///   bits 15:12 select the Phase-1 override range or the 16-bit override range.
/// - If the token is not found in NVM3, search in manufacturing region.
///
/// Note: Enabling the override flag may introduce latency compared to direct reads
/// from static region, particularly in time-sensitive scenarios such as device boot-up
/// where rapid access to token data is critical. Furthermore, storing override token
/// data in NVM3 consumes NVM3 memory, which can be a limiting factor on
/// resource-constrained devices.
///
/// @} end common_token_manager ******************************************************/

#endif /* SL_TOKEN_MANAGER_API_H */
