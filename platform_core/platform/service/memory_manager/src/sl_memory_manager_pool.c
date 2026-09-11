/***************************************************************************//**
 * @file
 * @brief Memory Manager Driver's Memory Pool Lightweight Feature Implementation.
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_memory_manager.h"
#include "sli_memory_manager.h"
#include "sli_memory_manager_log.h"
#include "sl_memory_manager_config.h"

#include "sl_assert.h"
#include "sl_core.h"
#include "sl_bit.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
#include <stdio.h>
#include "SEGGER_SYSVIEW.h"
#endif

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/

#define SLI_MEM_POOL_OUT_OF_MEMORY     UINTPTR_MAX

/*******************************************************************************
 ******************************  LOCAL VARIABLES   *****************************
 ******************************************************************************/

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
static uint32_t sli_cmm_pool_id_available = 0;
#endif

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

static sl_status_t delete_pool(sl_memory_pool_t *pool_handle,
                               bool force);

#if (defined(SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE) && (SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE == 1))
static inline bool value_looks_like_free_list_ptr(const sl_memory_pool_t *pool_handle,
                                                  size_t value);
static bool block_is_in_free_list(const sl_memory_pool_t *pool_handle,
                                  const void *block);
#endif // (defined(SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE) && (SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE == 1))

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Creates a memory pool in the general-purpose heap.
 ******************************************************************************/
sl_status_t sl_memory_create_pool(size_t block_size,
                                  uint32_t block_count,
                                  sl_memory_pool_t *pool_handle)
{
  return sl_memory_heap_create_pool_advanced(&sli_general_purpose_heap, block_size, block_count, SL_MEMORY_BLOCK_ALIGN_DEFAULT, pool_handle);
}

/***************************************************************************//**
 * Creates a memory pool.
 * Advanced version that allows to specify reservation handle and block alignment.
 *
 * @note Custom reservation is not supported in this implementation,
 *       use the power aware version.
 ******************************************************************************/
sl_status_t sl_memory_create_pool_advanced(sl_memory_reservation_t *reservation,
                                           size_t block_size,
                                           uint32_t block_count,
                                           size_t align,
                                           sl_memory_pool_t *pool_handle)
{
  // Reservations are not supported in this implementation.
  EFM_ASSERT(reservation == NULL);
  return sl_memory_heap_create_pool_advanced(&sli_general_purpose_heap, block_size, block_count, align, pool_handle);
}

/***************************************************************************//**
 * Deletes a memory pool.
 ******************************************************************************/
sl_status_t sl_memory_delete_pool(sl_memory_pool_t *pool_handle)
{
  return delete_pool(pool_handle, false);
}

/***************************************************************************//**
 * Force-deletes a memory pool regardless of outstanding block allocations.
 ******************************************************************************/
sl_status_t sl_memory_delete_pool_force(sl_memory_pool_t *pool_handle)
{
  return delete_pool(pool_handle, true);
}

/***************************************************************************//**
 * Deletes a memory pool, but keeps the reservation.
 *
 * @note Not supported in this implementation. Use the power aware version for
 *       reservations support.
 ******************************************************************************/
sl_status_t sl_memory_delete_pool_no_unreserve(sl_memory_pool_t *pool_handle)
{
  (void) pool_handle;
  EFM_ASSERT(false); // This is not supported in this implementation.
                     // In the case that asserts are disabled, return an error.
  return SL_STATUS_NOT_SUPPORTED;
}

/***************************************************************************//**
 * Allocates a block from a memory pool.
 ******************************************************************************/
sl_status_t sl_memory_pool_alloc(sl_memory_pool_t *pool_handle,
                                 void **block)
{
  CORE_DECLARE_IRQ_STATE;

  if ((pool_handle == NULL) || (block == NULL)) {
    SLI_MEMORY_MANAGER_LOG_ERROR("pool_alloc() failed: handle or block=NULL");
    return SL_STATUS_NULL_POINTER;
  }

  // No block allocated yet.
  *block = NULL;

  CORE_ENTER_ATOMIC();

  if ((size_t)pool_handle->block_free == SLI_MEM_POOL_OUT_OF_MEMORY) {
    CORE_EXIT_ATOMIC();
    SLI_MEMORY_MANAGER_LOG_WARN("pool_alloc() failed: pool empty handle=%p", (uint32_t)pool_handle);
    return SL_STATUS_EMPTY;
  }

  // Get the next free block.
  void *block_addr = pool_handle->block_free;

  // Update the next free block using the address saved in that block.
  pool_handle->block_free = (void *)*(size_t *)block_addr;

#if defined(MEMORY_MANAGER_TEST_CONDITIONS)
  // Clear the first word of the allocated block so it doesn't look like
  // a free list pointer. This enables O(1) double-free detection in most cases.
  // Without this, unwritten blocks would trigger the O(n) verification path
  // because they still contain the old "next" pointer from the free list.
  // This is only done in test conditions to avoid performance impact.
  *(size_t *)block_addr = 0;
#endif

  CORE_EXIT_ATOMIC();

  *block = block_addr;

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  SEGGER_SYSVIEW_HeapAllocEx(pool_handle, *block, pool_handle->block_size, 0);
#endif

  SLI_MEMORY_MANAGER_LOG_DEBUG("pool_alloc(): handle=%p addr=%p",
                               (uint32_t)pool_handle, (uint32_t)*block);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Frees a block from a memory pool.
 ******************************************************************************/
sl_status_t sl_memory_pool_free(sl_memory_pool_t *pool_handle,
                                void *block)
{
  CORE_DECLARE_IRQ_STATE;

  if ((pool_handle == NULL) || (block == NULL)) {
    SLI_MEMORY_MANAGER_LOG_ERROR("pool_free() failed: handle or block=NULL");
    return SL_STATUS_NULL_POINTER;
  }

  // Validate that the provided address is in the pool payload range.
  if ((block < pool_handle->block_address)
      || ((size_t)block >= ((size_t)pool_handle->block_address + (pool_handle->block_size * pool_handle->block_count)))) {
    SLI_MEMORY_MANAGER_LOG_ERROR("pool_free() failed: addr=%p out of range", (uint32_t)block);
    return SL_STATUS_INVALID_PARAMETER;
  }

  CORE_ENTER_ATOMIC();

#if (defined(SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE) && (SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE == 1))
  // Double-free detection using hybrid approach:
  //
  // O(1) check: does block content look like a free list pointer?
  // After allocation, sl_memory_pool_alloc() clears the first word to 0.
  // After a legitimate free, the block contains a pointer to the next free block
  // (a valid block address) or the end marker. If the block was allocated and the
  // user didn't overwrite the first word with a valid block address (rare), the
  // content will be 0 and this check will pass quickly.
  size_t stored_value = *(size_t *)block;
  if (value_looks_like_free_list_ptr(pool_handle, stored_value)) {
    // Content looks suspicious - verify by walking the free list.
    if (block_is_in_free_list(pool_handle, block)) {
      // Block is actually in the free list - this is a double-free.
      CORE_EXIT_ATOMIC();
      SLI_MEMORY_MANAGER_LOG_ERROR("pool_free() failed: double free addr=%p", (uint32_t)block);
      return SL_STATUS_INVALID_PARAMETER;
    }
    // False positive: user data happened to look like a free list pointer.
    // Proceed with normal free.
  }
#endif // (defined(SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE) && (SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE == 1))

  // Save the current free block address in this block and update free list head.
  *(size_t *)block = (size_t)pool_handle->block_free;
  pool_handle->block_free = block;

  CORE_EXIT_ATOMIC();

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  SEGGER_SYSVIEW_HeapFree(pool_handle, block);
#endif

  SLI_MEMORY_MANAGER_LOG_DEBUG("pool_free(): handle=%p addr=%p",
                               (uint32_t)pool_handle, (uint32_t)block);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the count of free blocks in a memory pool.
 ******************************************************************************/
uint32_t sl_memory_pool_get_free_block_count(const sl_memory_pool_t *pool_handle)
{
  uint32_t free_block_count = 0;
  void *free_block;

  if (pool_handle == NULL) {
    return 0;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  free_block = pool_handle->block_free;

  // Go through the free block list and count the number of free blocks remaining.
  while ((size_t)free_block != SLI_MEM_POOL_OUT_OF_MEMORY) {
    free_block = (void *)*(size_t *)free_block;
    free_block_count++;
  }

  CORE_EXIT_ATOMIC();

  return free_block_count;
}

/***************************************************************************//**
 * Creates a memory pool from a specific heap instance.
 ******************************************************************************/
sl_status_t sl_memory_heap_create_pool(sl_memory_heap_t *heap,
                                       size_t block_size,
                                       uint32_t block_count,
                                       sl_memory_pool_t *pool_handle)
{
  return sl_memory_heap_create_pool_advanced(heap, block_size, block_count, SL_MEMORY_BLOCK_ALIGN_DEFAULT, pool_handle);
}

/***************************************************************************//**
 * Creates a memory pool from a specific heap instance.
 * Advanced version that allows to specify block alignment
 ******************************************************************************/
sl_status_t sl_memory_heap_create_pool_advanced(sl_memory_heap_t *heap,
                                                size_t block_size,
                                                uint32_t block_count,
                                                size_t align,
                                                sl_memory_pool_t *pool_handle)
{
  // Check proper alignment characteristics.
  EFM_ASSERT((align == SL_MEMORY_BLOCK_ALIGN_DEFAULT)
             || (SL_MATH_IS_PWR2(align)
                 && (align <= SL_MEMORY_BLOCK_ALIGN_512_BYTES)));

  // Check that the parameters are valid.
  EFM_ASSERT(heap != NULL);
  EFM_ASSERT(block_count > 0);
  EFM_ASSERT(block_size > 0);

  sl_status_t status = SL_STATUS_OK;
  uint8_t *block = NULL;
  size_t block_addr;
  size_t pool_size;
  size_t block_align = (align == SL_MEMORY_BLOCK_ALIGN_DEFAULT) ? SLI_BLOCK_ALLOC_MIN_ALIGN : align;

  if (pool_handle == NULL) {
    SLI_MEMORY_MANAGER_LOG_ERROR("create_pool_advanced() failed: handle=NULL");
    return SL_STATUS_NULL_POINTER;
  }

  // SLI_ALIGN_ROUND_UP Rounds up to the nearest alignment boundary.
  // E.g. with align=8, 60 bytes will be rounded up to 64 bytes.
  pool_handle->block_size = SLI_ALIGN_ROUND_UP(block_size, block_align);
  pool_handle->block_count = block_count;

  // Reserve a block in which the entire pool will reside. Uses a long term allocation to keep
  // behavior similar to dynamic reservation.
  pool_size = pool_handle->block_size * pool_handle->block_count;
  status = sl_memory_heap_alloc_advanced(heap, pool_size, align, BLOCK_TYPE_LONG_TERM, (void **)&block);

  if (status != SL_STATUS_OK) {
    return status;
  }

  pool_handle->block_address = (void *)block;

  // Returned block pointer not used because its reference is already stored in block_address.
  (void)&block;

  pool_handle->block_free = (uint32_t *)pool_handle->block_address;

  block_addr = (size_t)pool_handle->block_address;

  // Populate the list of free blocks except the last block.
  for (uint16_t i = 0; i < (block_count - 1); i++) {
    *(size_t *)block_addr = block_addr + pool_handle->block_size;
    block_addr += pool_handle->block_size;
  }

  // Last element will indicate out of memory.
  *(size_t *)block_addr = SLI_MEM_POOL_OUT_OF_MEMORY;

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  uint32_t pool_id = __atomic_fetch_add(&sli_cmm_pool_id_available, 1, __ATOMIC_RELAXED);
  char pool_name[sizeof("Pool 4294967295")];
  snprintf(pool_name, sizeof(pool_name), "Pool %lu", (unsigned long)pool_id);
  SEGGER_SYSVIEW_HeapDefine(pool_handle,
                            pool_handle->block_address,
                            pool_size,
                            0);
  SEGGER_SYSVIEW_NameResource((uint32_t)pool_handle, pool_name);
  SEGGER_SYSVIEW_PrintfHost("Pool %lu created", (unsigned long)pool_id);
#endif

  SLI_MEMORY_MANAGER_LOG_INFO("create_pool_advanced(): blk_size=%u blk_cnt=%u",
                              (uint32_t)block_size,
                              (uint32_t)block_count);

  return status;
}

/***************************************************************************//**
 * Deletes a memory pool. Shared implementation for sl_memory_delete_pool() and
 * sl_memory_delete_pool_force().
 *
 * @param[in] pool_handle  Pointer to the memory pool handle.
 * @param[in] force        When false, the pool is deleted only if all blocks
 *                         have been freed. When true, the pool is deleted
 *                         regardless of outstanding block allocations.
 *
 * @return  SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
static sl_status_t delete_pool(sl_memory_pool_t *pool_handle,
                               bool force)
{
  sl_status_t status;

  if (pool_handle == NULL) {
    SLI_MEMORY_MANAGER_LOG_ERROR("delete_pool() failed: handle=NULL");
    return SL_STATUS_NULL_POINTER;
  }

  // Verify that the pool was properly initialized.
  if (pool_handle->block_address == NULL) {
    SLI_MEMORY_MANAGER_LOG_ERROR("delete_pool() failed: pool not init");
    return SL_STATUS_NULL_POINTER;
  }

  // Verify that no blocks are allocated, unless a force-delete was requested.
  if (!force) {
    uint32_t free_block_count = sl_memory_pool_get_free_block_count(pool_handle);
    if (free_block_count != pool_handle->block_count) {
      SLI_MEMORY_MANAGER_LOG_ERROR("delete_pool() failed: blocks in use");
      return SL_STATUS_INVALID_STATE;
    }
  }

  // Free block.
  status = sl_memory_free(pool_handle->block_address);

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  if (status == SL_STATUS_OK) {
    SEGGER_SYSVIEW_PrintfHost("Pool @0x%08lX deleted", (unsigned long)(uintptr_t)pool_handle);
  }
#endif

  if (status == SL_STATUS_OK) {
    SLI_MEMORY_MANAGER_LOG_INFO("delete_pool(): pool=%p", (uint32_t)pool_handle);
  }

  return status;
}

#if (defined(SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE) && (SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE == 1))
/***************************************************************************//**
 * Checks if a value looks like a valid free list pointer.
 * Used as a quick O(1) pre-check for double-free detection.
 *
 * A value looks like a free list pointer if it's either:
 * - The end-of-list marker (SLI_MEM_POOL_OUT_OF_MEMORY), OR
 * - A block-aligned address within the pool's memory range
 *
 * @param[in] pool_handle  Pointer to the memory pool handle.
 * @param[in] value        The value to check.
 *
 * @return true if value looks like it could be a free list pointer.
 ******************************************************************************/
static inline bool value_looks_like_free_list_ptr(const sl_memory_pool_t *pool_handle,
                                                  size_t value)
{
  // Check for end-of-list marker.
  if (value == SLI_MEM_POOL_OUT_OF_MEMORY) {
    return true;
  }

  // Check if it points within the pool's block range.
  size_t pool_start = (size_t)pool_handle->block_address;
  size_t pool_end = pool_start + (pool_handle->block_size * pool_handle->block_count);

  if ((value >= pool_start) && (value < pool_end)) {
    // Must be aligned to a block boundary.
    size_t offset = value - pool_start;
    return (offset % pool_handle->block_size) == 0;
  }

  return false;
}

/***************************************************************************//**
 * Checks if a block is in the free list by walking the list.
 * Used as O(n) verification when the quick check is suspicious.
 *
 * @param[in] pool_handle  Pointer to the memory pool handle.
 * @param[in] block        The block address to search for.
 *
 * @return true if block is found in the free list (already freed).
 *
 * @note Must be called within an atomic section.
 ******************************************************************************/
static bool block_is_in_free_list(const sl_memory_pool_t *pool_handle,
                                  const void *block)
{
  void *current = pool_handle->block_free;

  while ((size_t)current != SLI_MEM_POOL_OUT_OF_MEMORY) {
    if (current == block) {
      return true;
    }

    // Follow the free list to the next block.
    current = (void *)*(size_t *)current;
  }

  return false;
}
#endif // (defined(SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE) && (SL_MEMORY_MANAGER_POOL_DOUBLE_FREE_PROTECTION_ENABLE == 1))
