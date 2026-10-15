/***************************************************************************//**
 * @file sli_bluetooth_common_init.h
 * @brief Bluetooth Common initialization
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef SLI_BLUETOOTH_COMMON_INIT_H
#define SLI_BLUETOOTH_COMMON_INIT_H

#include <stdint.h>
#include <sl_status.h>

#define SL_BLUETOOTH_COMMON_CONFIG_BUFFER_MEMORY_SIZE_UNUSED (0)

/**
 * @brief Configuration structure for Bluetooth Common initialization.
 */
struct sl_bluetooth_common_config {
  uint32_t buffer_memory_size;
};

/**
 * @brief Function prototype for object initialization.
 *
 * Functions of this type are used internally by Bluetooth components to
 * initialize created objects. Applications must not use the functions directly.
 *
 * @param[in] object Pointer to the object to initialize
 *
 * @return SL_STATUS_OK if successful, otherwise an error code
 */
typedef sl_status_t sli_btcommon_object_init_func_t(void *object);

/**
 * @brief Function prototype for object deinitialization.
 *
 * Functions of this type are used internally by Bluetooth components to
 * deinitialize created objects. Applications must not use the functions
 * directly.
 *
 * @param[in] object Pointer to the object to deinitialize
 */
typedef void sli_btcommon_object_deinit_func_t(void *object);

/**
 * @brief An opaque handle to object context specification.
 *
 * Data structures of this type are used internally by Bluetooth components to
 * define object contexts. Applications must not use the structures directly.
 */
typedef struct sli_btcommon_context sli_btcommon_context_t;

/**
 * @brief Structure to describe the context and functions for one object type.
 *
 * Data structures of this type are used internally by Bluetooth components to
 * define object types. Applications must not use the structures directly.
 */
typedef struct {
  sli_btcommon_context_t *const *contexts;
  sli_btcommon_object_init_func_t *const *init;
  sli_btcommon_object_deinit_func_t *const *deinit;
} sli_btcommon_object_type_t;

/**
 * @brief Initialize Bluetooth Common buffer memory.
 *
 * @param[in] config Pointer to the configuration structure. If NULL, the
 *   default configuration is used. If
 *   @ref sl_bluetooth_common_config::buffer_memory_size is set to
 *   @ref SL_BLUETOOTH_COMMON_CONFIG_BUFFER_MEMORY_SIZE_UNUSED, the default
 *   value from SL_BLUETOOTH_COMMON_BUFFER_MEMORY_SIZE is used.
 *
 * @return SL_STATUS_OK on success, otherwise error
 */
sl_status_t sli_bluetooth_common_init(const struct sl_bluetooth_common_config *config);

/**
 * @brief Deinitialize Bluetooth Common buffer memory.
 *
 * Releases the buffer pool allocated by @ref sli_bluetooth_common_init.
 *
 * @return SL_STATUS_OK on success, otherwise error
 */
sl_status_t sli_bluetooth_common_deinit(void);

#endif /* SLI_BLUETOOTH_COMMON_INIT_H */
