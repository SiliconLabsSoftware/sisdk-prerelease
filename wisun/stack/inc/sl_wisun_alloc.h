/*
 * SPDX-License-Identifier: LicenseRef-MSLA
 * Copyright (c) 2024 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */

#ifndef SL_WISUN_ALLOC_H
#define SL_WISUN_ALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**************************************************************************//**
 * @addtogroup SL_WISUN_ALLOC
 *
 * @{
 *****************************************************************************/

/**************************************************************************//**
 * Allocate zero-initialized memory from the Wi-SUN heap.
 *
 * If allocation from the Wi-SUN heap fails, this function falls back to the
 * normal heap.
 *
 * @param[in] nmemb Number of elements to allocate.
 * @param[in] size Size of each element in bytes.
 * @return Pointer to allocated memory on success, NULL on failure.
 ******************************************************************************/
void *sl_wisun_calloc(size_t nmemb, size_t size);

/**************************************************************************//**
 * Reallocate a Wi-SUN heap allocation.
 *
 * If reallocation from the Wi-SUN heap fails, this function falls back to the
 * normal heap.
 *
 * @param[in] ptr Pointer to a previously allocated heap block, or NULL.
 * @param[in] size New requested size in bytes.
 * @return Pointer to reallocated memory on success, NULL on failure.
 ******************************************************************************/
void *sl_wisun_realloc(void *ptr, size_t size);

/**************************************************************************//**
 * Free a previously allocated block from either the Wi-SUN or the normal heap.
 *
 * @param[in] ptr Pointer to the heap block to free.
 ******************************************************************************/
void sl_wisun_free(void *ptr);

/**************************************************************************//**
 * Check whether a pointer belongs to the Wi-SUN heap.
 *
 * @param[in] ptr Pointer to check.
 * @return true if the pointer references a Wi-SUN heap block, false otherwise.
 ******************************************************************************/
bool sl_wisun_is_heap_block(void *ptr);

/**************************************************************************//**
 * Get current Wi-SUN heap usage statistics.
 *
 * @param[out] used_size Current used heap size in bytes.
 * @param[out] high_watermark Highest observed used heap size in bytes.
 ******************************************************************************/
void sl_wisun_heap_get_stats(uint32_t *used_size, uint32_t *high_watermark);

/** @} (end SL_WISUN_ALLOC) */

#endif // SL_WISUN_ALLOC_H
