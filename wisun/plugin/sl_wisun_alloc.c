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

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "sl_core.h"
#include "sl_memory_manager.h"
#include "sl_wisun_trace_api.h"
#include "sl_wisun_alloc.h"
#include "sl_wisun_alloc_config.h"

// Simple allocator configuration
#define SLI_ALLOC_ALIGNMENT 8
#define SLI_ALLOC_MIN_BLOCK_SIZE 16

// Block header structure
typedef struct sli_block_header {
    uint32_t size;                      // Size of the block (excluding header)
    struct sli_block_header *next;      // Next block in free list
    uint8_t magic;                      // Magic number to detect corruption
} sli_block_header_t;

#define SLI_BLOCK_MAGIC 0x5A
#define SLI_BLOCK_HEADER_SIZE (sizeof(sli_block_header_t))

// Simple allocator state
static uint8_t sli_pool[SL_WISUN_ALLOC_POOL_SIZE];
static sli_block_header_t *sli_free_list = NULL;
static bool sli_pool_initialized = false;
static uint32_t sli_used_memory = 0;  // Track used memory (maintained during operations)
static uint32_t sli_max_used_memory = 0;  // Track max used memory (maintained during operations)

/*****************************************************************************/
// Private functions
/*****************************************************************************/

// Initialize the allocator pool
static void sli_alloc_init(void)
{
    if (sli_pool_initialized) {
        return;
    }

    // Initialize the entire pool as one free block
    sli_block_header_t *block = (sli_block_header_t *)sli_pool;
    uint32_t pool_size = SL_WISUN_ALLOC_POOL_SIZE;
    uint32_t available_size = pool_size - SLI_BLOCK_HEADER_SIZE;

    block->size = available_size;
    block->next = NULL;
    block->magic = SLI_BLOCK_MAGIC;

    sli_free_list = block;
    sli_used_memory = 0;  // Initially all memory is free
    sli_max_used_memory = 0;
    sli_pool_initialized = true;
}

// Helper to check if ptr points into our pool (so we must handle it, not pass to sl_memory functions).
// Returns true for ANY address within pool bounds, including double-free and corrupted magic.
// Returning false for those error cases would cause undefined behavior.
static bool sli_is_pool_block(void *ptr)
{
    if (ptr == NULL) {
        return false;
    }

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    sli_block_header_t *block = (sli_block_header_t *)((uint8_t *)ptr - SLI_BLOCK_HEADER_SIZE);

    if ((uint8_t *)block < sli_pool || (uint8_t *)block >= sli_pool + SL_WISUN_ALLOC_POOL_SIZE) {
        CORE_EXIT_ATOMIC();
        return false;
    }

    // Corrupted magic: still a pool block
    if (block->magic != SLI_BLOCK_MAGIC) {
        CORE_EXIT_ATOMIC();
        return true;
    }

    // Double-free (already in free list): still a pool block
    sli_block_header_t *current = sli_free_list;
    while (current != NULL) {
        if (current == block) {
            CORE_EXIT_ATOMIC();
            return true;
        }
        current = current->next;
    }

    CORE_EXIT_ATOMIC();
    return true;
}

// Helper to check if ptr is a valid allocated block (not corrupted, not in free list).
// Used when sli_realloc returns NULL to distinguish OOM (try fallback) from invalid block (do not).
static bool sli_is_valid_allocated_block(void *ptr)
{
    if (ptr == NULL) {
        return false;
    }

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    sli_block_header_t *block = (sli_block_header_t *)((uint8_t *)ptr - SLI_BLOCK_HEADER_SIZE);

    if ((uint8_t *)block < sli_pool || (uint8_t *)block >= sli_pool + SL_WISUN_ALLOC_POOL_SIZE) {
        CORE_EXIT_ATOMIC();
        return false;
    }
    if (block->magic != SLI_BLOCK_MAGIC) {
        CORE_EXIT_ATOMIC();
        return false;
    }
    sli_block_header_t *current = sli_free_list;
    while (current != NULL) {
        if (current == block) {
            CORE_EXIT_ATOMIC();
            return false;
        }
        current = current->next;
    }

    CORE_EXIT_ATOMIC();
    return true;
}

// Helper to check if an address points to a valid free block
static bool sli_is_free_block(sli_block_header_t *block)
{
    if (block == NULL) {
        return false;
    }

    // Check if within pool bounds
    if ((uint8_t *)block < sli_pool || (uint8_t *)block >= sli_pool + SL_WISUN_ALLOC_POOL_SIZE) {
        return false;
    }

    // Check magic number
    if (block->magic != SLI_BLOCK_MAGIC) {
        return false;
    }

    // Check if in free list
    sli_block_header_t *current = sli_free_list;
    while (current != NULL) {
        if (current == block) {
            return true;
        }
        current = current->next;
    }

    return false;
}

// Helper to remove a block from the free list
static void sli_remove_from_free_list(sli_block_header_t *block)
{
    if (block == NULL || sli_free_list == NULL) {
        return;
    }

    // If it's the first block
    if (sli_free_list == block) {
        sli_free_list = block->next;
        block->next = NULL;
        return;
    }

    // Find and remove from middle/end
    sli_block_header_t *current = sli_free_list;
    while (current != NULL && current->next != NULL) {
        if (current->next == block) {
            current->next = block->next;
            block->next = NULL;
            return;
        }
        current = current->next;
    }
}

// Helper to align size
static uint32_t sli_align_size(uint32_t size)
{
    return (size + SLI_ALLOC_ALIGNMENT - 1) & ~(SLI_ALLOC_ALIGNMENT - 1);
}

static uint32_t sli_get_block_size(void *ptr)
{
    sli_block_header_t *block = (sli_block_header_t *)((uint8_t *)ptr - SLI_BLOCK_HEADER_SIZE);
    return block->size;
}

static void *sli_calloc(uint32_t nmemb, uint32_t size)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    // Check for overflow
    uint32_t total_size = nmemb * size;
    if (total_size < nmemb || total_size < size) {
        return NULL;  // Overflow detected
    }

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    // Initialize pool if needed
    if (!sli_pool_initialized) {
        sli_alloc_init();
    }

    // Calculate required size (aligned)
    uint32_t aligned_size = sli_align_size(total_size);
    if (aligned_size < SLI_ALLOC_MIN_BLOCK_SIZE) {
        aligned_size = SLI_ALLOC_MIN_BLOCK_SIZE;
    }

    // Search free list for a block large enough
    sli_block_header_t **prev = &sli_free_list;
    sli_block_header_t *current = sli_free_list;

    while (current != NULL) {
        // Verify magic number
        if (current->magic != SLI_BLOCK_MAGIC) {
            // Corruption detected - return NULL
            CORE_EXIT_ATOMIC();
            return NULL;
        }

        if (current->size >= aligned_size) {
            // Found a block large enough
            void *user_ptr = (uint8_t *)current + SLI_BLOCK_HEADER_SIZE;
            uint32_t original_block_size = current->size;
            uint32_t allocated_memory;

            // If block is much larger, split it
            if (current->size >= aligned_size + SLI_ALLOC_MIN_BLOCK_SIZE + SLI_BLOCK_HEADER_SIZE) {
                // Create a new free block from the remainder
                sli_block_header_t *new_block = (sli_block_header_t *)((uint8_t *)current + SLI_BLOCK_HEADER_SIZE + aligned_size);
                new_block->size = current->size - aligned_size - SLI_BLOCK_HEADER_SIZE;
                new_block->next = current->next;
                new_block->magic = SLI_BLOCK_MAGIC;

                // Update current block size
                current->size = aligned_size;
                current->next = NULL;

                // Insert new block in free list
                *prev = new_block;

                // Account for the allocated portion only
                allocated_memory = aligned_size + SLI_BLOCK_HEADER_SIZE;
            } else {
                // Use entire block, remove from free list
                *prev = current->next;
                current->next = NULL;

                // Account for the entire block (we don't split, so we use it all)
                allocated_memory = original_block_size + SLI_BLOCK_HEADER_SIZE;
            }

            // Update used memory counter
            sli_used_memory += allocated_memory;
            if (sli_used_memory > sli_max_used_memory) {
                sli_max_used_memory = sli_used_memory;
            }

            // Zero the memory (calloc behavior)
            memset(user_ptr, 0, total_size);

            CORE_EXIT_ATOMIC();
            return user_ptr;
        }

        prev = &current->next;
        current = current->next;
    }

    CORE_EXIT_ATOMIC();

    // No suitable block found
    return NULL;
}

static void sli_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    if (!sli_pool_initialized) {
        return;
    }

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    // Get block header
    sli_block_header_t *block = (sli_block_header_t *)((uint8_t *)ptr - SLI_BLOCK_HEADER_SIZE);

    // Verify the pointer is within our pool
    if ((uint8_t *)block < sli_pool || (uint8_t *)block >= sli_pool + SL_WISUN_ALLOC_POOL_SIZE) {
        CORE_EXIT_ATOMIC();
        return;
    }

    // Verify magic number
    if (block->magic != SLI_BLOCK_MAGIC) {
        CORE_EXIT_ATOMIC();
        return;  // Invalid block or double free
    }

    // Check if already in free list (double free detection)
    sli_block_header_t *current = sli_free_list;
    while (current != NULL) {
        if (current == block) {
            CORE_EXIT_ATOMIC();
            return;  // Already freed
        }
        current = current->next;
    }

    // Calculate memory being freed (block size + header)
    uint32_t freed_memory = block->size + SLI_BLOCK_HEADER_SIZE;

    // Coalesce with adjacent free blocks
    sli_block_header_t *prev_block = NULL;
    sli_block_header_t *next_block = NULL;

    // Find the previous block in memory (if any)
    // A previous block would end right before this block starts
    uint8_t *block_start = (uint8_t *)block;
    current = sli_free_list;
    while (current != NULL) {
        uint8_t *current_end = (uint8_t *)current + SLI_BLOCK_HEADER_SIZE + current->size;
        if (current_end == block_start) {
            prev_block = current;
            break;
        }
        current = current->next;
    }

    // Find the next block in memory (if any)
    // The next block would start right after this block ends
    uint8_t *block_end = (uint8_t *)block + SLI_BLOCK_HEADER_SIZE + block->size;
    if (block_end < sli_pool + SL_WISUN_ALLOC_POOL_SIZE) {
        sli_block_header_t *candidate = (sli_block_header_t *)block_end;
        if (sli_is_free_block(candidate)) {
            next_block = candidate;
        }
    }

    // Coalesce with previous block
    if (prev_block != NULL) {
        // Remove previous block from free list
        sli_remove_from_free_list(prev_block);

        // Merge: extend previous block to include current block
        // The previous block was already free, so we only account for the current block
        prev_block->size += SLI_BLOCK_HEADER_SIZE + block->size;
        block = prev_block;  // Use previous block as the merged block
    }

    // Coalesce with next block
    if (next_block != NULL) {
        // Remove next block from free list
        sli_remove_from_free_list(next_block);

        // Merge: extend current block to include next block
        // The next block was already free, so we only account for the current block
        block->size += SLI_BLOCK_HEADER_SIZE + next_block->size;
    }

    // Update used memory counter (subtract the freed memory)
    // Note: We only subtract the current block, not coalesced blocks (they were already free)
    if (sli_used_memory >= freed_memory) {
        sli_used_memory -= freed_memory;
    } else {
        // Should not happen, but handle gracefully
        sli_used_memory = 0;
    }

    // Add merged block to free list (at head for simplicity)
    block->next = sli_free_list;
    sli_free_list = block;

    CORE_EXIT_ATOMIC();
}

// Reallocate a block (ptr must be an allocated block from our pool)
static void *sli_realloc(void *ptr, uint32_t size)
{
    if (ptr == NULL) {
        return sli_calloc(1, size);
    }

    if (size == 0) {
        sli_free(ptr);
        return NULL;
    }

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    sli_block_header_t *block = (sli_block_header_t *)((uint8_t *)ptr - SLI_BLOCK_HEADER_SIZE);

    // Validate block: corrupted magic or double-free cannot be safely reallocated
    if (block->magic != SLI_BLOCK_MAGIC) {
        CORE_EXIT_ATOMIC();
        return NULL;
    }
    sli_block_header_t *current = sli_free_list;
    while (current != NULL) {
        if (current == block) {
            CORE_EXIT_ATOMIC();
            return NULL;  // Double-free
        }
        current = current->next;
    }

    uint32_t aligned_new = sli_align_size(size);
    if (aligned_new < SLI_ALLOC_MIN_BLOCK_SIZE) {
        aligned_new = SLI_ALLOC_MIN_BLOCK_SIZE;
    }

    // Shrink or same size
    if (aligned_new <= block->size) {
        if (block->size >= aligned_new + SLI_ALLOC_MIN_BLOCK_SIZE + SLI_BLOCK_HEADER_SIZE) {
            // Split: create free block from remainder
            sli_block_header_t *new_free = (sli_block_header_t *)((uint8_t *)block + SLI_BLOCK_HEADER_SIZE + aligned_new);
            new_free->size = block->size - aligned_new - SLI_BLOCK_HEADER_SIZE;
            new_free->next = sli_free_list;
            new_free->magic = SLI_BLOCK_MAGIC;
            sli_free_list = new_free;

            uint32_t freed_memory = new_free->size + SLI_BLOCK_HEADER_SIZE;
            sli_used_memory -= freed_memory;

            block->size = aligned_new;
        }
        CORE_EXIT_ATOMIC();
        return ptr;
    }

    // Grow: try in-place by merging with next free block
    uint8_t *block_end = (uint8_t *)block + SLI_BLOCK_HEADER_SIZE + block->size;
    if (block_end < sli_pool + SL_WISUN_ALLOC_POOL_SIZE) {
        sli_block_header_t *next_block = (sli_block_header_t *)block_end;
        if (sli_is_free_block(next_block)) {
            uint32_t merged_size = block->size + SLI_BLOCK_HEADER_SIZE + next_block->size;
            if (merged_size >= aligned_new) {
                sli_remove_from_free_list(next_block);
                sli_used_memory += next_block->size + SLI_BLOCK_HEADER_SIZE;

                block->size = merged_size;

                // If we have excess, split off a free block
                if (merged_size >= aligned_new + SLI_ALLOC_MIN_BLOCK_SIZE + SLI_BLOCK_HEADER_SIZE) {
                    sli_block_header_t *new_free = (sli_block_header_t *)((uint8_t *)block + SLI_BLOCK_HEADER_SIZE + aligned_new);
                    new_free->size = merged_size - aligned_new - SLI_BLOCK_HEADER_SIZE;
                    new_free->next = sli_free_list;
                    new_free->magic = SLI_BLOCK_MAGIC;
                    sli_free_list = new_free;

                    sli_used_memory -= new_free->size + SLI_BLOCK_HEADER_SIZE;
                    block->size = aligned_new;
                }

                if (sli_used_memory > sli_max_used_memory) {
                    sli_max_used_memory = sli_used_memory;
                }

                CORE_EXIT_ATOMIC();
                return ptr;
            }
        }
    }

    CORE_EXIT_ATOMIC();

    void *new_ptr = sli_calloc(1, size);
    if (new_ptr == NULL) {
        return NULL;
    }
    memcpy(new_ptr, ptr, block->size);
    sli_free(ptr);
    return new_ptr;
}

/*****************************************************************************/
// Public APIs
// These APIs are used by the security components to allocate and free memory
/*****************************************************************************/

void *sl_wisun_calloc(size_t nmemb, size_t size)
{
    void *ptr = sli_calloc(nmemb, size);
    if (!ptr) {
        sl_wisun_trace_debug("wisun_alloc(%"PRIu32"): OOM", (uint32_t)(nmemb * size));
        ptr = sl_calloc(nmemb, size);
    }
    return ptr;
}

void *sl_wisun_realloc(void *ptr, size_t size)
{
    void *new_ptr;

    if (ptr == NULL) {
        return sl_wisun_calloc(1, size);
    }
    if (size == 0) {
        sl_wisun_free(ptr);
        return NULL;
    }
    if (!sli_is_pool_block(ptr)) {
        return sl_realloc(ptr, size);
    }

    new_ptr = sli_realloc(ptr, (uint32_t)size);
    if (!new_ptr) {
        sl_wisun_trace_debug("wisun_realloc(%"PRIu32"): OOM", (uint32_t)size);
        /* Only try fallback for OOM; invalid block (corrupted/double-free) must not be copied */
        if (!sli_is_valid_allocated_block(ptr)) {
            return NULL;
        }
        new_ptr = sl_malloc(size);
        if (new_ptr) {
            uint32_t old_size = sli_get_block_size(ptr);
            memcpy(new_ptr, ptr, (size < old_size) ? size : old_size);
            sli_free(ptr);
        }
    }
    return new_ptr;
}

void sl_wisun_free(void *ptr)
{
    if (!sli_is_pool_block(ptr)) {
        sl_free(ptr);
    } else {
        sli_free(ptr);
    }
}

bool sl_wisun_is_pool_block(void *ptr)
{
    if (!sli_pool_initialized) {
        return false;
    }
    return sli_is_pool_block(ptr);
}

void sl_wisun_alloc_get_stats(uint32_t *alloc_size, uint32_t *max_size)
{
    *alloc_size = sli_used_memory;
    *max_size = sli_max_used_memory;
}
