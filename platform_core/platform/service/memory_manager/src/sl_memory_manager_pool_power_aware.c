/***************************************************************************//**
 * @file
 * @brief Memory Manager Driver's Pool Power Aware Feature Implementation.
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

#include <string.h>

#include "sl_memory_manager.h"
#include "sli_memory_manager.h"
#include "sli_memory_manager_retention_control.h"
#include "sl_assert.h"
#include "sl_bit.h"
#include "em_device.h"
#include "sl_core.h"
#include "sl_common.h"
#include "sli_code_classification.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
#include "sli_memory_profiler.h"
#endif

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
#include <stdio.h>
#include "SEGGER_SYSVIEW.h"
#endif

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/

// Set LSB to indicate an odd block.
#define SET_ODD_FLAG(ptr) ((void *)((uintptr_t)(ptr) | 1))

// Clear LSB to indicate a bank block.
#define CLEAR_ODD_FLAG(ptr) ((void *)((uintptr_t)(ptr) & ~1))

// Check if a block is an odd block by checking the LSB bit.
#define IS_ODD_BLOCK(ptr) (((uintptr_t)(ptr) & 1) != 0)

// Set 2nd bit to indicate an allocated block.
#define SET_ALLOCATED_FLAG(ptr) ((void *)((uintptr_t)(ptr) | 2))

// Clear 2nd bit to indicate an unused block.
#define CLEAR_ALLOCATED_FLAG(ptr) ((void *)((uintptr_t)(ptr) & ~2))

// Check if a block is allocated by checking the 2nd bit.
#define IS_ALLOCATED_BLOCK(ptr) (((uintptr_t)(ptr) & 2) != 0)

/*******************************************************************************
 ********************************  UTILITY  ************************************
 ******************************************************************************/

static uint32_t count_blocks(sli_memory_pool_block_t *block_head);
static sli_memory_pool_free_cnt_entry_t *get_free_cnt_entry(sl_memory_pool_t *pool_handle);
static void free_free_cnt_entry(sl_memory_pool_t *pool_handle,
                                sli_memory_pool_free_cnt_entry_t *entry);
static void add_odd_block(void *addr,
                          sli_memory_pool_block_t *block_metadata,
                          sli_memory_pool_block_t **head);
static sl_status_t create_reservation(sl_memory_heap_t *heap,
                                      size_t block_size,
                                      uint32_t block_count,
                                      size_t align,
                                      sl_memory_reservation_t **handle);
static uint32_t calculate_block_size(size_t block_size, size_t align);
static sl_status_t create_pool(sl_memory_reservation_t *reservation_handle,
                               size_t block_size,
                               uint32_t block_count,
                               size_t align,
                               sl_memory_pool_t *pool_handle);

/*******************************************************************************
 ******************************  LOCAL VARIABLES   *****************************
 ******************************************************************************/
#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
static uint32_t sli_cmm_pool_id_available = 0;
#endif

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
 ******************************************************************************/
sl_status_t sl_memory_create_pool_advanced(sl_memory_reservation_t *reservation_handle,
                                           size_t block_size,
                                           uint32_t block_count,
                                           size_t align,
                                           sl_memory_pool_t *pool_handle)
{
  sl_status_t status;
  bool user_reservation = true;

  // Validate arguments.
  EFM_ASSERT(block_size > 0);
  EFM_ASSERT(block_count > 0);
  // Check proper alignment characteristics.
  EFM_ASSERT((align == SL_MEMORY_BLOCK_ALIGN_DEFAULT)
             || (SL_MATH_IS_PWR2(align)
                 && (align <= SL_MEMORY_BLOCK_ALIGN_512_BYTES)));

  // Verify that the handle pointers aren't NULL.
  if (pool_handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  pool_handle->reservation = reservation_handle;
  // If reservation is null, create a new one in the general purpose heap.
  if (pool_handle->reservation == NULL) {
    user_reservation = false;
    // Create reservation for the pool.
    status = create_reservation(&sli_general_purpose_heap,
                                block_size,
                                block_count,
                                align,
                                &pool_handle->reservation);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  // Create the memory pool.
  status = create_pool(pool_handle->reservation, block_size, block_count, align, pool_handle);
  if (status != SL_STATUS_OK  && !user_reservation) {
    (void)sl_memory_release_block(pool_handle->reservation);
    (void)sl_memory_reservation_handle_free(pool_handle->reservation);
    return status;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Deletes a memory pool.
 *
 * @note The pool_handle provided is neither freed or invalidated. It can be
 *       reused in a new call to sl_memory_create_pool() to create another pool.
 ******************************************************************************/
sl_status_t sl_memory_delete_pool(sl_memory_pool_t *pool_handle)
{
  sl_status_t status;
  sl_memory_reservation_t *reservation;

  // Verify that the handle pointer isn't NULL.
  if (pool_handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  reservation = pool_handle->reservation;

  status = sl_memory_delete_pool_no_unreserve(pool_handle);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Release block.
  status = sl_memory_release_block(reservation);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Free reservation pool_handle.
  status = sl_memory_reservation_handle_free(reservation);

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  if (status == SL_STATUS_OK) {
    SEGGER_SYSVIEW_PrintfHost("Pool @0x%08lX deleted", (unsigned long)(uintptr_t)pool_handle);
  }
#endif

  return status;
}

/***************************************************************************//**
 * Deletes a memory pool, but keeps the reservation.
 *
 * @note The pool_handle provided is neither freed or invalidated. It can be
 *       reused in a new call to sl_memory_create_pool() to create another pool.
 ******************************************************************************/
sl_status_t sl_memory_delete_pool_no_unreserve(sl_memory_pool_t *pool_handle)
{
  // Verify that the handle pointer isn't NULL.
  if ((pool_handle == NULL) || (pool_handle->reservation == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  // Verify that no blocks are allocated.
  if (pool_handle->free_blk_cnt != (uint32_t)(pool_handle->block_count)) {
    return SL_STATUS_INVALID_STATE;
  }

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  // Delete the memory tracker.
  sli_memory_profiler_delete_tracker(pool_handle);
#endif

  // Free free_count/bank/block lists.
  (void)sl_memory_free(pool_handle->free_cnt_tbl);
  (void)sl_memory_free(pool_handle->bank_tbl);
  (void)sl_memory_free(pool_handle->block_tbl);

  // Reset the handle.
  memset(pool_handle, 0, sizeof(sl_memory_pool_t));

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Allocates a block from a memory pool.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_MEMORY_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sl_memory_pool_alloc(sl_memory_pool_t *pool_handle,
                                 void **block)
{
#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  void *volatile return_address = sli_memory_profiler_get_return_address();
#endif
  sli_memory_pool_free_cnt_entry_t *cur_free_cnt;
  sli_memory_pool_free_cnt_entry_t *new_free_cnt;
  sli_memory_pool_bank_t *cur_bank;
  sli_memory_pool_block_t *cur_block;
  sl_memory_heap_t *heap;

  if ((pool_handle == NULL) || (block == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  heap = pool_handle->heap;

  // No block allocated yet.
  *block = NULL;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  if (pool_handle->free_cnt_head == NULL) {
    if (pool_handle->odd_blocks_head != NULL) {
      // If no more regular blocks, pick one from the odd blocks list.
      cur_block = pool_handle->odd_blocks_head;
      pool_handle->odd_blocks_head = cur_block->next;
      *block = CLEAR_ODD_FLAG(cur_block->block_addr);
      pool_handle->free_blk_cnt--;

      // Increment bank counter for this new block (retained size is updated inside the macro).
      SLI_MEMORY_INCREMENT_BANK_COUNTER(heap, (uint8_t *)(*block), (uint8_t *)(*block) + pool_handle->block_size - 1);

      // Indicate block as allocated.
      cur_block->block_addr = SET_ALLOCATED_FLAG(cur_block->block_addr);

      CORE_EXIT_ATOMIC();

      #if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
      SEGGER_SYSVIEW_HeapAllocEx(pool_handle, *block, pool_handle->block_size, 0);
      #endif

      return SL_STATUS_OK;
    }

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
    sli_memory_profiler_track_alloc_with_ownership(pool_handle, NULL, pool_handle->block_size, return_address);
#endif

    block = NULL;

    CORE_EXIT_ATOMIC();

    return SL_STATUS_EMPTY;
  }

  // Retrieve block to allocate.
  cur_free_cnt = pool_handle->free_cnt_head;
  cur_bank = cur_free_cnt->bank_head;

  // We should have at least one bank.
  EFM_ASSERT(cur_bank != NULL);
  cur_block = cur_bank->block_head;

  // We should have at least one free block.
  EFM_ASSERT(cur_block != NULL);
  *block = CLEAR_ODD_FLAG(cur_block->block_addr);

  // Increment bank counter for this new block (retained size is updated inside the macro).
  SLI_MEMORY_INCREMENT_BANK_COUNTER(heap, (uint8_t *)(*block), (uint8_t *)(*block) + pool_handle->block_size - 1);

  // Indicate block as allocated.
  cur_block->block_addr = SET_ALLOCATED_FLAG(cur_block->block_addr);

  // Remove block from bank list.
  cur_bank->block_head = cur_block->next;

  // Re-organize free list.
  // Are there other free block in the bank?
  if (cur_free_cnt->free_cnt != 1) {
    // Move bank one step left.
    if (cur_bank->next != NULL) {
      // There are other banks in this free_cnt entry.
      // Remove current bank from the list.
      cur_free_cnt->bank_head = cur_bank->next;
      cur_free_cnt->bank_head->prev = NULL;

      // Alloc new free_cnt entry,
      new_free_cnt = get_free_cnt_entry(pool_handle);
      new_free_cnt->free_cnt = cur_free_cnt->free_cnt - 1;

      // Insert cur_bank to it.
      new_free_cnt->bank_head = cur_bank;
      cur_bank->free_cnt_entry = new_free_cnt;
      cur_bank->prev = NULL;
      cur_bank->next = NULL;

      // Insert new free_cnt entry to list.
      new_free_cnt->next = pool_handle->free_cnt_head;
      pool_handle->free_cnt_head->prev = new_free_cnt;
      new_free_cnt->prev = NULL;
      pool_handle->free_cnt_head = new_free_cnt;
    } else {
      // There was no other bank in this free cnt entry.
      cur_free_cnt->free_cnt--;
    }
  } else {
    // That was the only one.
    // Remove bank from free cnt entry.
    cur_bank->prev = NULL;
    cur_free_cnt->bank_head = cur_bank->next;
    cur_bank->next = NULL;
    cur_bank->free_cnt_entry = NULL;

    if (cur_free_cnt->bank_head != NULL) {
      cur_free_cnt->bank_head->prev = NULL;
    } else {
      // There were no other banks. Delete free cnt entry.
      pool_handle->free_cnt_head = cur_free_cnt->next;
      if (pool_handle->free_cnt_head != NULL) {
        pool_handle->free_cnt_head->prev = NULL;
      }

      free_free_cnt_entry(pool_handle, cur_free_cnt);
    }
  }

  pool_handle->free_blk_cnt--;

  CORE_EXIT_ATOMIC();

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  sli_memory_profiler_track_alloc_with_ownership(pool_handle, *block, pool_handle->block_size, return_address);
#endif
#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  SEGGER_SYSVIEW_HeapAllocEx(pool_handle, *block, pool_handle->block_size, 0);
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Frees a block from a memory pool.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_MEMORY_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sl_memory_pool_free(sl_memory_pool_t *pool_handle,
                                void *block)
{
  sli_memory_pool_free_cnt_entry_t *cur_free_cnt;
  sli_memory_pool_free_cnt_entry_t *new_free_cnt = NULL;
  sli_memory_pool_bank_t *cur_bank;
  sli_memory_pool_block_t *cur_block;
  sli_bank_coverage_t coverage;
  sl_memory_heap_t *heap;
  const sli_retention_control_t *retention_control;

  if ((pool_handle == NULL) || (block == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  heap = pool_handle->heap;

  retention_control = (sli_retention_control_t *)heap->retention_control;

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  sli_memory_profiler_track_free(pool_handle, block);
#endif

  // Validate that the provided address is in the pool payload range.
  if (((uintptr_t)block < (uintptr_t)pool_handle->reservation->block_address)
      || ((uintptr_t)block >= ((uintptr_t)pool_handle->reservation->block_address + pool_handle->reservation->block_size))) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (((uintptr_t)block < (uintptr_t)pool_handle->reservation->block_address)
      || ((uintptr_t)block >= ((uintptr_t)pool_handle->reservation->block_address + pool_handle->reservation->block_size))) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  // Retrieve associated block entry.
  uint32_t blk_index = ((uintptr_t)block - (uintptr_t)(pool_handle->reservation->block_address)) / pool_handle->block_size;
  cur_block = &pool_handle->block_tbl[blk_index];

  // Check if block is allocated.
  if (!IS_ALLOCATED_BLOCK(cur_block->block_addr)) {
    CORE_EXIT_ATOMIC();

    return SL_STATUS_INVALID_PARAMETER;
  }

  cur_block->block_addr = CLEAR_ALLOCATED_FLAG(cur_block->block_addr);

  // Decrement bank counter for freed memory pool block (retained size is updated inside the macro).
  SLI_MEMORY_DECREMENT_BANK_COUNTER(heap, (uint8_t *) block, (uint8_t *)block + pool_handle->block_size - 1);

  // Check if block belongs to odd blocks list.
  if (IS_ODD_BLOCK(cur_block->block_addr)) {
    cur_block->next = pool_handle->odd_blocks_head;
    pool_handle->odd_blocks_head = cur_block;
    pool_handle->free_blk_cnt++;

    CORE_EXIT_ATOMIC();

    #if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
    SEGGER_SYSVIEW_HeapFree(pool_handle, block);
    #endif

    return SL_STATUS_OK;
  }

  // Retrieve the bank entry where block belongs.
  coverage = retention_control->get_block_bank_coverage(CLEAR_ODD_FLAG(block), pool_handle->block_size);

  // Insert block to bank entry.
  cur_bank = &pool_handle->bank_tbl[coverage.start - pool_handle->banks_full.start];
  cur_block->next = cur_bank->block_head;
  cur_bank->block_head = cur_block;
  cur_free_cnt = cur_bank->free_cnt_entry;

  // cur_bank now needs to be moved away from the head of the free count list
  // because it contains one more block. Banks with more free blocks have a lower
  // priority for the next allocation, as they are less likely to be shut down.

  // Are we alone in the free_cnt entry?
  if (cur_bank->next != NULL || cur_bank->prev != NULL) {
    // There are other banks.

    // Remove bank from the free_cnt entry list.
    if (cur_bank->prev != NULL) {
      cur_bank->prev->next = cur_bank->next;
    } else {
      // We were at head.
      cur_free_cnt->bank_head = cur_bank->next;
      cur_free_cnt->bank_head->prev = NULL;
    }

    if (cur_bank->next != NULL) {
      cur_bank->next->prev = cur_bank->prev;
    }

    // Now needs to find a new home for cur_bank.
    // Is free_cnt_entry neighbour to the right suitable?
    if ((cur_free_cnt->next != NULL) && (cur_free_cnt->next->free_cnt == cur_free_cnt->free_cnt + 1)) {
      // Neighbour to the right is suitable. Will be the new home for cur_bank. Insert it there.
      new_free_cnt = cur_free_cnt->next;
    } else {
      // No suitable free_cnt entry neighbour. Create one.
      new_free_cnt = get_free_cnt_entry(pool_handle);
      new_free_cnt->free_cnt = cur_free_cnt->free_cnt + 1;
      new_free_cnt->bank_head = NULL;

      // Insert new free_cnt entry to list next to cur_free_cnt.
      if (cur_free_cnt->next != NULL) {
        cur_free_cnt->next->prev = new_free_cnt;
      }

      new_free_cnt->prev = cur_free_cnt;
      new_free_cnt->next = cur_free_cnt->next;
      cur_free_cnt->next = new_free_cnt;
    }
  } else {
    // The bank was alone in the free_cnt entry, or was empty.
    // Was the bank entry empty (all bank blocks allocated)?
    if (cur_free_cnt == NULL) {
      if (pool_handle->free_cnt_head != NULL && pool_handle->free_cnt_head->free_cnt == 1) {
        // Insert it in existing free cnt entry at head.
        new_free_cnt = pool_handle->free_cnt_head;
      } else {
        // Create a new free cnt entry with free_cnt = 1.
        new_free_cnt = get_free_cnt_entry(pool_handle);
        new_free_cnt->free_cnt = 1;
        new_free_cnt->bank_head = NULL;

        // Insert new_free_cnt at head.
        new_free_cnt->next = pool_handle->free_cnt_head;
        new_free_cnt->prev = NULL;

        if (pool_handle->free_cnt_head != NULL) {
          pool_handle->free_cnt_head->prev = new_free_cnt;
        }

        pool_handle->free_cnt_head = new_free_cnt;
      }
      // Is there a neighbour to the right and if yes does it have the free_cnt we need?
    } else if (cur_free_cnt->next != NULL && cur_free_cnt->next->free_cnt == cur_free_cnt->free_cnt + 1) {
      // The neighbour is appropriate. Move the bank there.
      new_free_cnt = cur_free_cnt->next;

      // Remove cur_free_cnt from the list.
      new_free_cnt->prev = cur_free_cnt->prev;

      if (cur_free_cnt->prev != NULL) {
        cur_free_cnt->prev->next = new_free_cnt;
      }

      if (pool_handle->free_cnt_head == cur_free_cnt) {
        pool_handle->free_cnt_head = new_free_cnt;
      }

      // Free the now useless free_cnt entry.
      free_free_cnt_entry(pool_handle, cur_free_cnt);
    } else {
      // No appropriate neighbour.
      cur_free_cnt->free_cnt++;
    }
  }

  // If a new free cnt was assigned, add the bank to it.
  if (new_free_cnt != NULL) {
    cur_bank->next = new_free_cnt->bank_head;
    cur_bank->prev = NULL;

    if (new_free_cnt->bank_head != NULL) {
      new_free_cnt->bank_head->prev = cur_bank;
    }

    new_free_cnt->bank_head = cur_bank;
    cur_bank->free_cnt_entry = new_free_cnt;
  }

  pool_handle->free_blk_cnt++;

  CORE_EXIT_ATOMIC();

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  SEGGER_SYSVIEW_HeapFree(pool_handle, block);
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the count of free blocks in a memory pool.
 ******************************************************************************/
uint32_t sl_memory_pool_get_free_block_count(const sl_memory_pool_t *pool_handle)
{
  return pool_handle->free_blk_cnt;
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
  sl_status_t status;

  // Validate arguments.
  EFM_ASSERT(block_size > 0);
  EFM_ASSERT(block_count > 0);
  // Check proper alignment characteristics.
  EFM_ASSERT((align == SL_MEMORY_BLOCK_ALIGN_DEFAULT)
             || (SL_MATH_IS_PWR2(align)
                 && (align <= SL_MEMORY_BLOCK_ALIGN_512_BYTES)));

  // Verify that the handle pointers aren't NULL.
  if (pool_handle == NULL || heap == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Create reservation for the pool.
  status = create_reservation(heap,
                              block_size,
                              block_count,
                              align,
                              &pool_handle->reservation);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Create the memory pool.
  status = create_pool(pool_handle->reservation, block_size, block_count, align, pool_handle);
  if (status != SL_STATUS_OK) {
    (void)sl_memory_release_block(pool_handle->reservation);
    (void)sl_memory_reservation_handle_free(pool_handle->reservation);
    return status;
  }

  return SL_STATUS_OK;
}

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

#if defined(SLI_MEMORY_MANAGER_ENABLE_TEST_UTILITIES)
/***************************************************************************//**
 * Go through the pool metadata lists and check some facts. For example check that
 * the free count entries are correct and that the total block count is correct.
 * Also check that the free block count is correct.
 *
 * @param[in] pool_handle   Pointer to a pool handle.
 * @param[in] allocated_count   The block count of already allocated blocks.
 *
 * @return true if the heap integrity check is successful. False otherwise.
 ******************************************************************************/
bool sli_check_pool_integrity(sl_memory_pool_t *pool_handle,
                              uint32_t allocated_count)
{
  struct sli_memory_pool_free_cnt_entry *current_free_count_entry = pool_handle->free_cnt_head;
  sli_memory_pool_bank_t *bank;
  uint32_t count = 0;
  uint32_t total_count = 0;

  while (current_free_count_entry != NULL) {
    bank = current_free_count_entry->bank_head;

    while (bank != NULL) {
      if (bank->free_cnt_entry != current_free_count_entry) {
        return false;
      }
      count = count_blocks(bank->block_head);
      if (current_free_count_entry->free_cnt != count) {
        return false;
      }
      bank = bank->next;
      total_count += count;
    }

    current_free_count_entry = current_free_count_entry->next;
  }

// Add blocks of odd list.
  total_count += count_blocks(pool_handle->odd_blocks_head);

// Check that the total count of blocks is correct.
  if (((allocated_count + total_count) != pool_handle->block_count) || (pool_handle->free_blk_cnt != total_count)) {
    return false;
  }

  if (allocated_count == pool_handle->block_count) {
    if ((pool_handle->free_cnt_head != NULL) || (pool_handle->odd_blocks_head != NULL)) {
      return false;
    }
  }

  return true;
}
#endif

/***************************************************************************//**
 * Get a free count entry from the free count table. This function also updates
 * the free count list head.
 *
 * @param[in]  pool_handle  Pointer to the pool handle.
 *
 * @return     The next free count entry.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_MEMORY_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
static sli_memory_pool_free_cnt_entry_t * get_free_cnt_entry(sl_memory_pool_t * pool_handle)
{
  sli_memory_pool_free_cnt_entry_t *entry = pool_handle->empty_free_cnt_head;
  pool_handle->empty_free_cnt_head = entry->next;

  return entry;
}

/***************************************************************************//**
 * Free a free count entry. This function also updates.
 * the free count list head.
 *
 * @param[in]  pool_handle  Pointer to the pool handle.
 * @param[in]  entry  Pointer to the pool free count entre to free.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_MEMORY_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
static void free_free_cnt_entry(sl_memory_pool_t *pool_handle,
                                sli_memory_pool_free_cnt_entry_t * entry)
{
  entry->next = pool_handle->empty_free_cnt_head;
  pool_handle->empty_free_cnt_head = entry;
}

/***************************************************************************//**
 * Count the number of blocks in a bank.
 *
 * @param[in]  block_head  Pointer to the block list.
 *
 * @return     Block count.
 ******************************************************************************/
static uint32_t count_blocks(sli_memory_pool_block_t *block_head)
{
  uint32_t count = 0;
  sli_memory_pool_block_t *current = block_head;
  while (current != NULL) {
    count++;
    current = current->next;
  }
  return count;
}

/***************************************************************************//**
 * Add a pool block to a odd blocks list.
 *
 * @param[in]  addr  Block address.
 * @param[in]  block_metadata  Pointer to the block metadata.
 * @param[in]  head  Pointer to the list head.
 ******************************************************************************/
static void add_odd_block(void *addr,
                          sli_memory_pool_block_t *block_metadata,
                          sli_memory_pool_block_t **head)
{
  block_metadata->block_addr = SET_ODD_FLAG(addr);
  block_metadata->next = *head;
  *head = block_metadata;
}

/***************************************************************************//**
 * Creates a reservation
 *
 * @param[in]  heap  Heap to create the reservation in.
 * @param[in]  block_size  Size of each block.
 * @param[in]  block_count  Number of blocks.
 * @param[in]  align  Alignment of blocks.
 * @param[in]  handle  Reservation handle.
 *
 * @return     status of the reservation creation.
 ******************************************************************************/
static sl_status_t create_reservation(sl_memory_heap_t *heap,
                                      size_t block_size,
                                      uint32_t block_count,
                                      size_t align,
                                      sl_memory_reservation_t **handle)
{
#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT) && !defined(DMEMCACHE_PRESENT)
#define MEMORY_POOL_METADATA_HEAP  sl_memory_manager_get_dtcm_heap()
#else
#define MEMORY_POOL_METADATA_HEAP  heap
#endif

  sl_status_t status;
  size_t pool_size;
  void *pool_data = NULL;

  // Allocate reservation handle as a long-term block.
  status = sl_memory_heap_reservation_handle_alloc(MEMORY_POOL_METADATA_HEAP, handle);
#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  sli_memory_profiler_track_ownership(SLI_INVALID_MEMORY_TRACKER_HANDLE,
                                      *handle,
                                      return_address);
#endif
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Align block size to 'align' bytes. To ensure the first bit of the block metadata
  // can be used to determine if a block belongs to the odd list, the minimum align is 8.
  pool_size = calculate_block_size(block_size, align) * block_count;

  // Reserve a block in which the entire pool will reside.
  status = sl_memory_heap_reserve_block(heap,
                                        pool_size,
                                        align,
                                        *handle,
                                        &pool_data);
#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  sli_memory_profiler_track_ownership(SLI_INVALID_MEMORY_TRACKER_HANDLE, pool_data, return_address);
#endif
  if (status != SL_STATUS_OK) {
    (void)sl_memory_reservation_handle_free(*handle);
    return status;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Calculates the real block size according to the desired block size and the alignment.
 *
 * @param[in]  block_size  Size of each block.
 * @param[in]  align  Alignment of blocks.
 *
 * @return     Real block size
 ******************************************************************************/
static uint32_t calculate_block_size(size_t block_size,
                                     size_t align)
{
  size_t block_align = (align == SL_MEMORY_BLOCK_ALIGN_DEFAULT) ? SLI_BLOCK_ALLOC_MIN_ALIGN : align;

  return SLI_ALIGN_ROUND_UP(block_size, block_align);
}

/***************************************************************************//**
 * Creates a memory pool
 *
 * @param[in]  reservation_handle  Reservation handle to use for the pool.
 * @param[in]  block_size  Size of each block.
 * @param[in]  block_count  Number of blocks.
 * @param[in]  align  Alignment of blocks.
 * @param[in]  pool_handle  Reservation handle.
 *
 * @return     status of the reservation creation.
 ******************************************************************************/
static sl_status_t create_pool(sl_memory_reservation_t *reservation_handle,
                               size_t block_size,
                               uint32_t block_count,
                               size_t align,
                               sl_memory_pool_t *pool_handle)
{
#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT) && !defined(DMEMCACHE_PRESENT)
#define MEMORY_POOL_METADATA_HEAP  sl_memory_manager_get_dtcm_heap()
#else
#define MEMORY_POOL_METADATA_HEAP  heap
#endif

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  void *volatile return_address = sli_memory_profiler_get_return_address();
#endif
  sl_status_t status;
  size_t pool_size;
  uint32_t nb_banks_full = 0;
  uint32_t nb_free_cnt_entry;
  void *pool_data = NULL;
  uint32_t block_size_real;
  uint32_t blocks_per_bank;
  sl_memory_heap_t *heap;
  const sli_retention_control_t *retention_control;

  // Retrieve the heap where the block was allocated.
  heap = sli_memory_get_heap_handle(reservation_handle->block_address);
  pool_handle->reservation = reservation_handle;

  // Align block size to 'align' bytes. To ensure the first bit of the block metadata
  // can be used to determine if a block belongs to the odd list, the minimum align is 2.
  block_size_real = calculate_block_size(block_size, align);
  block_size_real = SLI_ALIGN_ROUND_UP(block_size_real, 2);

  pool_size = block_size_real * block_count;

  pool_handle->free_cnt_head = NULL;
  pool_handle->block_size = block_size_real;
  pool_handle->block_count = block_count;
  pool_handle->odd_blocks_head = NULL;
  pool_handle->block_tbl = NULL;
  pool_handle->bank_tbl = NULL;
  pool_handle->free_cnt_tbl = NULL;
  pool_handle->heap = heap;

  pool_data = pool_handle->reservation->block_address;

  retention_control = (sli_retention_control_t *) heap->retention_control;
  sli_bank_coverage_t coverage_pool = retention_control->get_block_bank_coverage(CLEAR_ODD_FLAG(pool_data), pool_size);

  // Get the coverage of the next/previous address of start/end address of the pool to check if the pool is aligned with the bank.
  sli_bank_coverage_t coverage_align = retention_control->get_block_bank_coverage((void *)((uintptr_t)CLEAR_ODD_FLAG(pool_data) - 1), pool_size + 2);
  pool_handle->banks_full.start = (coverage_pool.start == coverage_align.start) ? coverage_pool.start + 1 : coverage_pool.start;
  pool_handle->banks_full.end =  (coverage_pool.end == coverage_align.end) ? coverage_pool.end - 1 : coverage_pool.end;

  status = sl_memory_heap_calloc(MEMORY_POOL_METADATA_HEAP,
                                 block_count,
                                 sizeof(sli_memory_pool_block_t),
                                 BLOCK_TYPE_LONG_TERM,
                                 (void *)&pool_handle->block_tbl);

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  sli_memory_profiler_track_ownership(SLI_INVALID_MEMORY_TRACKER_HANDLE,
                                      pool_handle->block_tbl,
                                      return_address);
#endif
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Browse all blocks to find odd/bank blocks and calculate the required bank number.
  sli_bank_coverage_t coverage_prev = { 0xFF, 0xFF };
  for (uint32_t i = 0; i < block_count; i++) {
    sli_memory_pool_block_t *block_metadata = &pool_handle->block_tbl[i];
    void *block_start_addr = &((uint8_t *)pool_data)[i * block_size_real];
    sli_bank_coverage_t coverage = retention_control->get_block_bank_coverage(CLEAR_ODD_FLAG(block_start_addr), block_size_real);

    if (coverage.start < pool_handle->banks_full.start || coverage.end > pool_handle->banks_full.end) {
      // Odd block resides in not fully owned bank.
      add_odd_block(block_start_addr, block_metadata, &pool_handle->odd_blocks_head);
      continue;
    }

    if (coverage.start != coverage.end) {
      // Odd block spans to 2 banks.
      add_odd_block(block_start_addr, block_metadata, &pool_handle->odd_blocks_head);
      continue;
    } else {
      // Bank block, clear LSB bit.
      block_metadata->block_addr = CLEAR_ODD_FLAG(block_start_addr);
    }

    // Count the number of banks fully covered by the pool.
    if (coverage.end != coverage_prev.end) {
      // Increment the number of banks.
      nb_banks_full++;

      // Update previous bank index.
      coverage_prev = coverage;
    }
  }

  // Calculate the number of blocks per bank.
  blocks_per_bank = retention_control->bank_size / block_size_real;

  // If the number of banks FULLY covered by the pool <= 1 or the max number of blocks per bank <= 1,
  // just put everything in odd_list and make everything more efficient.
  // We don't need all this complexity when not enough banks are covered, not worth it.
  if ((nb_banks_full <= 1) || (blocks_per_bank <= 1)) {
    // Add remaining blocks to odd list.
    for (uint32_t i = 0; i < block_count; i++) {
      sli_memory_pool_block_t *block_metadata = &pool_handle->block_tbl[i];
      if (!IS_ODD_BLOCK(block_metadata->block_addr)) {
        add_odd_block(block_metadata->block_addr, block_metadata, &pool_handle->odd_blocks_head);
      }
    }

    pool_handle->free_blk_cnt = block_count;

    #if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
    uint32_t pool_id0 = __atomic_fetch_add(&sli_cmm_pool_id_available, 1, __ATOMIC_RELAXED);
    char pool_name0[sizeof("Pool 4294967295")];
    snprintf(pool_name0, sizeof(pool_name0), "Pool %lu", (unsigned long)pool_id0);
    SEGGER_SYSVIEW_HeapDefine(pool_handle,
                              pool_handle->reservation->block_address,
                              block_count * block_size_real,
                              0);
    SEGGER_SYSVIEW_NameResource((uint32_t) pool_handle, pool_name0);
    SEGGER_SYSVIEW_PrintfHost("Pool %lu created", (unsigned long)pool_id0);
    #endif

    return SL_STATUS_OK;
  }

  // From this point, we start building the 3d linked list.

  nb_free_cnt_entry = SL_MIN(blocks_per_bank, nb_banks_full);

  status = sl_memory_heap_calloc(MEMORY_POOL_METADATA_HEAP,
                                 nb_free_cnt_entry,
                                 sizeof(sli_memory_pool_free_cnt_entry_t),
                                 BLOCK_TYPE_LONG_TERM,
                                 (void *)&pool_handle->free_cnt_tbl);

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  sli_memory_profiler_track_ownership(SLI_INVALID_MEMORY_TRACKER_HANDLE,
                                      pool_handle->free_cnt_tbl,
                                      return_address);
#endif
  if (status != SL_STATUS_OK) {
    (void)sl_memory_free(pool_handle->block_tbl);
    return status;
  }

  status = sl_memory_heap_calloc(MEMORY_POOL_METADATA_HEAP,
                                 nb_banks_full,
                                 sizeof(sli_memory_pool_bank_t),
                                 BLOCK_TYPE_LONG_TERM,
                                 (void *)&pool_handle->bank_tbl);

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  sli_memory_profiler_track_ownership(SLI_INVALID_MEMORY_TRACKER_HANDLE,
                                      pool_handle->bank_tbl,
                                      return_address);
#endif
  if (status != SL_STATUS_OK) {
    (void)sl_memory_free(pool_handle->block_tbl);
    (void)sl_memory_free(pool_handle->free_cnt_tbl);
    return status;
  }

  // Assign each block to its associated bank.
  sli_memory_pool_block_t *block_metadata;
  for (uint32_t i = 0; i < block_count; i++) {
    block_metadata = &pool_handle->block_tbl[i];
    if (!IS_ODD_BLOCK(block_metadata->block_addr)) {
      sli_bank_coverage_t coverage = retention_control->get_block_bank_coverage(CLEAR_ODD_FLAG(block_metadata->block_addr), pool_handle->block_size);

      // Add block to bank list.
      block_metadata->next = pool_handle->bank_tbl[coverage.start - pool_handle->banks_full.start].block_head;
      pool_handle->bank_tbl[coverage.start - pool_handle->banks_full.start].block_head = block_metadata;
    }
  }

  // Initialize free_count free pool list.
  for (uint32_t i = 0; i < (nb_free_cnt_entry - 1); i++) {
    sli_memory_pool_free_cnt_entry_t *free_count_cur = &pool_handle->free_cnt_tbl[i];
    sli_memory_pool_free_cnt_entry_t *free_count_next = &pool_handle->free_cnt_tbl[i + 1];
    free_count_cur->free_cnt = 0;

    free_count_cur->next = free_count_next;
    free_count_next->prev = free_count_cur;

    if (i == 0) {
      free_count_cur->prev = NULL;
    }
    if (i == (nb_free_cnt_entry - 1)) {
      free_count_next->next = NULL;
    }
  }

  pool_handle->empty_free_cnt_head = &pool_handle->free_cnt_tbl[0];

  // Populate the free count table with banks.
  // Iterate through the banks and populate the free count list by counting the number of blocks in each bank.
  // Group banks with the same block count into the same free count entry.
  sli_memory_pool_free_cnt_entry_t *free_cnt_entry_cur = NULL;
  sli_memory_pool_free_cnt_entry_t *free_cnt_entry_next = NULL;
  pool_handle->free_cnt_head = NULL;
  sli_memory_pool_bank_t *bank = NULL;
  uint32_t bank_block_count;

  for (uint32_t i = 0; i < nb_banks_full; i++) {
    bank = &pool_handle->bank_tbl[i];
    bank_block_count = count_blocks(bank->block_head);

    // Check if free count entry exist.
    free_cnt_entry_cur = pool_handle->free_cnt_head;
    while (free_cnt_entry_cur != NULL) {
      if (free_cnt_entry_cur->free_cnt == bank_block_count) {
        break;
      }
      free_cnt_entry_cur = free_cnt_entry_cur->next;
    }

    if (free_cnt_entry_cur == NULL) {
      free_cnt_entry_cur = get_free_cnt_entry(pool_handle);
      free_cnt_entry_cur->free_cnt = (uint16_t)bank_block_count;
      if (pool_handle->free_cnt_head != NULL) {
        pool_handle->free_cnt_head->prev = free_cnt_entry_cur;
      }
      free_cnt_entry_cur->next = pool_handle->free_cnt_head;
      free_cnt_entry_cur->prev = NULL;
      pool_handle->free_cnt_head = free_cnt_entry_cur;
      bank->next = NULL;
    } else {
      bank->next = free_cnt_entry_cur->bank_head;
    }

    free_cnt_entry_cur->bank_head = bank;
    bank->prev = NULL;
    bank->free_cnt_entry = free_cnt_entry_cur;
  }

  // Sort free count entries so that the entry with the least block count is first.
  // At initialization, there can be at most two free count entries due to the limited number of bank block sizes.
  if (pool_handle->free_cnt_head->next != NULL) {
    free_cnt_entry_cur = pool_handle->free_cnt_head;
    free_cnt_entry_next = free_cnt_entry_cur->next;

    if (free_cnt_entry_cur->free_cnt > free_cnt_entry_next->free_cnt) {
      free_cnt_entry_next->next = free_cnt_entry_cur;
      free_cnt_entry_next->prev = NULL;
      free_cnt_entry_cur->prev = free_cnt_entry_next;
      free_cnt_entry_cur->next = NULL;
      pool_handle->free_cnt_head = free_cnt_entry_next;
    }
  }

  pool_handle->free_blk_cnt = block_count;

#if defined(SL_CATALOG_MEMORY_PROFILER_PRESENT)
  // Create the tracker for the pool with no description. The code that created
  // the pool can add the tracker description if relevant.
  sli_memory_profiler_create_pool_tracker(pool_handle,
                                          NULL,
                                          pool_handle->reservation->block_address,
                                          block_count * block_size_real);
#endif
#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)
  uint32_t pool_id1 = __atomic_fetch_add(&sli_cmm_pool_id_available, 1, __ATOMIC_RELAXED);
  char pool_name1[sizeof("Pool 4294967295")];
  snprintf(pool_name1, sizeof(pool_name1), "Pool %lu", (unsigned long)pool_id1);
  SEGGER_SYSVIEW_HeapDefine(pool_handle,
                            pool_handle->reservation->block_address,
                            block_count * block_size_real,
                            0);
  SEGGER_SYSVIEW_NameResource((uint32_t) pool_handle, pool_name1);
  SEGGER_SYSVIEW_PrintfHost("Pool %lu created", (unsigned long)pool_id1);
#endif

  return SL_STATUS_OK;
}
