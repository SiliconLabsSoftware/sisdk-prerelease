/***************************************************************************//**
 * @file
 * @brief DMA Manager APIs.
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

#ifndef SL_DMA_MANAGER_H
#define SL_DMA_MANAGER_H

#include <stdbool.h>
#include <stdlib.h>
#include "sl_status.h"
#include "sl_slist.h"
#include "sl_device_peripheral.h"
#include "sl_code_classification.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// DEFINES

#define SL_DMA_CHANNEL_HIGH_PRIORITY                (1 << 0)
#define SL_DMA_CHANNEL_USES_ROUND_ROBIN             (1 << 1)
#define SL_DMA_CHANNEL_SUPPORTS_INTERLEAVING        (1 << 2)
#define SL_DMA_CHANNEL_SUPPORTS_DUAL_DESTINATION    (1 << 3)

// -----------------------------------------------------------------------------
// TYPE DEFINITIONS

typedef struct sl_dma_handle sl_dma_handle_t;

typedef void (*sl_dma_manager_channel_irq_callback_t)(void);

/// A DMA manager instance handle data structure.
/// Allocated by the application or by the DMA manager.
/// Several concurrent DMA instances may exist. The application must
/// not modify the contents of this handle and should not depend on its values.
struct sl_dma_handle {
  /// @cond DO_NOT_INCLUDE_WITH_DOXYGEN
  sl_peripheral_dma_t dma_peripheral;  ///< DMA peripheral.
  uint32_t dma_channels_bitmap;        ///< Bitmap of allocated channels.
  uint32_t sync_bit_bitmap;            ///< Bitmap of allocated sync bits.
  uint8_t  round_robin_channel_number; ///< Number of round robin channels.
  sl_dma_manager_channel_irq_callback_t* channel_irq_callbacks_table; ///< Table of IRQ callbacks per channel.
  void** channel_user_data_table;      ///< Table of user data pointers per channel.
  sl_slist_node_t node;                ///< Linked list node for DMA handle management.
  /// @endcond
};

// -----------------------------------------------------------------------------
// PROTOTYPES

/***************************************************************************//**
 * Initializes the DMA manager.
 *
 * @param[in]  dma_handle Pointer to DMA handle. Can be NULL, will be allocated
 *                        internally.
 *
 * @param[in]  dma_peripheral DMA peripheral. Can be NULL if only one DMA
 *                            instance on device.
 ******************************************************************************/
sl_status_t sl_dma_manager_init(sl_dma_handle_t *dma_handle,
                                sl_peripheral_dma_t dma_peripheral);

/***************************************************************************//**
 * Gets the default DMA handle.
 *
 * @param[out] dma_handle Pointer to variable that will receive the default DMA handle.
 *
 * @return 0 if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_dma_manager_get_default_handle(sl_dma_handle_t **dma_handle);

/***************************************************************************//**
 * Allocates a DMA channel.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[out] channel_nbr Pointer to variable that will receive the allocated channel number.
 *
 * @return 0 if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_dma_manager_allocate_channel(sl_dma_handle_t *dma_handle,
                                            uint8_t *channel_nbr);

/***************************************************************************//**
 * Allocates a DMA channel. This function allows to describe desired properties
 * and features for the allocated channel.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[in]  channel_properties Channel properties. Possible values are
 *                                (can be OR-ed):
 *                                   - SL_DMA_CHANNEL_HIGH_PRIORITY
 *                                   - SL_DMA_CHANNEL_USES_ROUND_ROBIN
 *                                   - SL_DMA_CHANNEL_SUPPORTS_INTERLEAVING
 *                                   - SL_DMA_CHANNEL_SUPPORTS_DUAL_DESTINATION
 *
 * @param[out] channel_nbr Pointer to variable that will receive the allocated channel number.
 *
 * @return 0 if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_dma_manager_allocate_channel_with_properties(sl_dma_handle_t *dma_handle,
                                                            uint32_t channel_properties,
                                                            uint8_t *channel_nbr);

/***************************************************************************//**
 * Reserves a specific channel.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[in]  channel_nbr Channel number to reserve.
 *
 * @return 0 if successful. SL_STATUS_NOT_AVAILABLE if channel is already
 *         reserved/allocated. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_dma_manager_reserve_channel(sl_dma_handle_t *dma_handle,
                                           uint8_t channel_nbr);

/***************************************************************************//**
 * Frees a previously allocated/reserved channel.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[in]  channel_nbr Channel number to free.
 *
 * @return 0 if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_dma_manager_free_channel(sl_dma_handle_t *dma_handle,
                                        uint8_t channel_nbr);

/***************************************************************************//**
 * Allocates a SYNC bit/channel.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[out] sync_nbr Pointer to variable that will receive the allocated sync number.
 *
 * @return 0 if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_dma_manager_allocate_sync(sl_dma_handle_t *dma_handle,
                                         uint8_t *sync_nbr);

/***************************************************************************//**
 * Frees a previously allocated SYNC bit/channel.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[in]  sync_nbr Sync number to free.
 *
 * @return 0 if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_dma_manager_free_sync(sl_dma_handle_t *dma_handle,
                                     uint8_t sync_nbr);

/***************************************************************************//**
 * Registers an IRQ callback.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[in]  channel_nbr Channel number.
 *
 * @param[in]  callback Pointer to function to be called on channel IRQ.
 *
 * @return 0 if successful. Error code otherwise.
 *
 * @note This function should only be used with series 2 devices, where there is a
 *       single IRQ line for all channels and the DMA Manager offers a central
 *       IRQ dispatcher. For series 3 devices, there is an IRQ line per channel.
 *       Therefore IRQ should be registered directly through the Interrupt Manager
 *       module. Using this function for series 3 devices, will only call
 *       the Interrupt Manager under the hood.
 *
 * @note This function is ONLY for users that do not use DMADRV or DMA Channel
 *       Driver. There is no need to register an IRQ callback when using either
 *       of these drivers.
 *
 * @note To remove a registered IRQ callback, call this function with a NULL
 *       callback.
 ******************************************************************************/
sl_status_t sl_dma_manager_register_channel_irq_callback(sl_dma_handle_t *dma_handle,
                                                         uint8_t channel_nbr,
                                                         sl_dma_manager_channel_irq_callback_t callback);

/***************************************************************************//**
 * Registers user data for a specific DMA channel.
 *
 * @param[in]  dma_handle Pointer to DMA handle. NULL will take the default
 *                        DMA instance.
 *
 * @param[in]  channel_nbr Channel number.
 *
 * @param[in]  user_data Pointer to user data to associate with the channel.
 *
 * @return 0 if successful. Error code otherwise.
 *
 * @note To remove previously registered user data, call this function with a
 *       NULL user_data.
 ******************************************************************************/
sl_status_t sl_dma_manager_register_channel_user_data(sl_dma_handle_t *dma_handle,
                                                      uint8_t channel_nbr,
                                                      void *user_data);

/***************************************************************************//**
 * Retrieves the user data and channel number for the DMA channel currently
 * being serviced in the interrupt dispatch context.
 *
 * @param[out] channel_nbr Pointer to variable that will receive the channel
 *                         number currently being serviced.
 *
 * @param[out] user_data Pointer to variable that will receive the user data
 *                       associated with the channel.
 *
 * @return 0 if successful. SL_STATUS_INVALID_STATE if called outside of the
 *         DMA Manager interrupt dispatch context. Error code otherwise.
 *
 * @note This function is designed to be called from within a DMA channel IRQ
 *       callback registered via sl_dma_manager_register_channel_irq_callback().
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_MANAGER, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
sl_status_t sl_dma_manager_retrieve_current_channel_user_data(uint8_t *channel_nbr,
                                                              void **user_data);

/***************************************************************************//**
 * Get the pending errors for the DMA manager.
 * @param[in]  channel_nbr Channel number.
 * @return The pending errors bitmap.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_MANAGER, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
uint32_t sl_dma_manager_get_pending_errors(uint8_t channel_nbr);

/***************************************************************************//**
 * Clear the pending errors for the DMA manager.
 * This function clears the pending error flag for the specified channel.
 * It is idempotent and safe to call multiple times.
 *
 * @param[in]  channel_nbr Channel number.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_MANAGER, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
void sl_dma_manager_clear_pending_errors(uint8_t channel_nbr);

#ifdef __cplusplus
}
#endif

#endif // SL_DMA_MANAGER_H
