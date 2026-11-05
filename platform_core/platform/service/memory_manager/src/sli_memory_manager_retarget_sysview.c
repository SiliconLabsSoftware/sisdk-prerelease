/***************************************************************************//**
 * @file
 * @brief Memory Manager SystemView Retarget via Link-Time Function Wrapping.
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

#include "sli_memory_manager.h"

#if defined(SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW)

/***************************************************************************//**
 * Emits an ownership event using the established SystemView protocol.
 ******************************************************************************/
static inline void sli_mm_sv_record_ownership(uint32_t ptr,
                                              uint32_t owner_pc)
{
  SEGGER_SYSVIEW_RecordU32x2(SLI_SYSTEMVIEW_EVENT_ID_OWNERSHIP_SITE,
                             ptr,
                             owner_pc);
}

/***************************************************************************//**
 * Records an explicit owner for an object without interpreting either payload.
 ******************************************************************************/
void sli_memory_track_ownership(const void *ptr,
                                const void *owner_pc)
{
  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)owner_pc);
}

// Link-time wrapping for GCC:
//   --wrap=symbol  makes any external reference to "symbol" resolve to
//   "__wrap_symbol", and any reference to "__real_symbol" resolve to the
//   original "symbol". Internal calls within sl_memory_manager.c are NOT
//   intercepted, so only the true external caller is captured.
//
// The GNU gold linker can strip wrapped symbols under LTO; marking them
// externally_visible prevents that.
#if !defined(__clang__)
#define ATTR_EXT_VIS    __attribute__((externally_visible))
#else
#define ATTR_EXT_VIS
#endif

// Prototypes for the original (real) functions resolved by the linker.

// Dynamic allocation / free.
extern void *__real_sl_malloc(size_t size);
extern sl_status_t __real_sl_memory_alloc(size_t size,
                                          uint8_t type,
                                          void **block);
extern sl_status_t __real_sl_memory_alloc_advanced(size_t size,
                                                   size_t align,
                                                   uint8_t type,
                                                   void **block);
extern void __real_sl_free(void *ptr);
extern sl_status_t __real_sl_memory_free(void *block);
extern void *__real_sl_calloc(size_t item_count,
                              size_t size);
extern sl_status_t __real_sl_memory_calloc(size_t item_count,
                                           size_t size,
                                           uint8_t type,
                                           void **block);
extern void *__real_sl_realloc(void *ptr,
                               size_t size);
extern sl_status_t __real_sl_memory_realloc(void *ptr,
                                            size_t size,
                                            void **block);

// Heap-specific dynamic allocation / free.
extern sl_status_t __real_sl_memory_heap_alloc(sl_memory_heap_t *heap,
                                               size_t size,
                                               uint8_t type,
                                               void **block);
extern sl_status_t __real_sl_memory_heap_alloc_advanced(sl_memory_heap_t *heap,
                                                        size_t size,
                                                        size_t align,
                                                        uint8_t type,
                                                        void **block);
extern sl_status_t __real_sl_memory_heap_free(sl_memory_heap_t *heap,
                                              void *block);
extern sl_status_t __real_sl_memory_heap_calloc(sl_memory_heap_t *heap,
                                                size_t item_count,
                                                size_t size,
                                                uint8_t type,
                                                void **block);
extern sl_status_t __real_sl_memory_heap_realloc(sl_memory_heap_t *heap,
                                                 void *ptr,
                                                 size_t size,
                                                 void **block);

// Dynamic reservation.
extern sl_status_t __real_sl_memory_reserve_block(size_t size,
                                                  size_t align,
                                                  sl_memory_reservation_t *handle,
                                                  void **block);
extern sl_status_t __real_sl_memory_release_block(sl_memory_reservation_t *handle);
extern sl_status_t __real_sl_memory_reservation_handle_alloc(sl_memory_reservation_t **handle);
extern sl_status_t __real_sl_memory_reservation_handle_free(sl_memory_reservation_t *handle);
extern sl_status_t __real_sl_memory_reservation_add_retention(const sl_memory_reservation_t *handle);
extern sl_status_t __real_sl_memory_reservation_remove_retention(const sl_memory_reservation_t *handle);

// Memory pool.
extern sl_status_t __real_sl_memory_create_pool(size_t block_size,
                                                uint32_t block_count,
                                                sl_memory_pool_t *pool_handle);
extern sl_status_t __real_sl_memory_create_pool_advanced(sl_memory_reservation_t *reservation_handle,
                                                         size_t block_size,
                                                         uint32_t block_count,
                                                         size_t align,
                                                         sl_memory_pool_t *pool_handle);
extern sl_status_t __real_sl_memory_delete_pool(sl_memory_pool_t *pool_handle);
extern sl_status_t __real_sl_memory_delete_pool_no_unreserve(sl_memory_pool_t *pool_handle);
extern sl_status_t __real_sl_memory_pool_alloc(sl_memory_pool_t *pool_handle,
                                               void **block);
extern sl_status_t __real_sl_memory_pool_free(sl_memory_pool_t *pool_handle,
                                              void *block);
extern sl_status_t __real_sl_memory_pool_handle_alloc(sl_memory_pool_t **pool_handle);
extern sl_status_t __real_sl_memory_pool_handle_free(sl_memory_pool_t *pool_handle);

// No-retention reservation.
extern sl_status_t __real_sl_memory_reserve_no_retention(size_t size,
                                                         size_t align,
                                                         void **block);
extern sl_status_t __real_sl_memory_heap_reserve_no_retention(sl_memory_heap_t *heap,
                                                              size_t size,
                                                              size_t align,
                                                              void **block);

// Heap-specific reservation.
extern sl_status_t __real_sl_memory_heap_reserve_block(sl_memory_heap_t *heap,
                                                       size_t size,
                                                       size_t align,
                                                       sl_memory_reservation_t *handle,
                                                       void **block);
extern sl_status_t __real_sl_memory_heap_reservation_handle_alloc(sl_memory_heap_t *heap,
                                                                  sl_memory_reservation_t **handle);

// Heap-specific memory pool.
extern sl_status_t __real_sl_memory_heap_create_pool(sl_memory_heap_t *heap,
                                                     size_t block_size,
                                                     uint32_t block_count,
                                                     sl_memory_pool_t *pool_handle);
extern sl_status_t __real_sl_memory_heap_create_pool_advanced(sl_memory_heap_t *heap,
                                                              size_t block_size,
                                                              uint32_t block_count,
                                                              size_t align,
                                                              sl_memory_pool_t *pool_handle);
extern sl_status_t __real_sl_memory_heap_pool_handle_alloc(sl_memory_heap_t *heap,
                                                           sl_memory_pool_t **pool_handle);

/*******************************************************************************
 **************************   ALLOCATION WRAPPERS   ****************************
 ******************************************************************************/
ATTR_EXT_VIS void *__wrap_sl_malloc(size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real_sl_malloc(size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_alloc(size_t size,
                                                uint8_t type,
                                                void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_alloc(size, type, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_alloc_advanced(size_t size,
                                                         size_t align,
                                                         uint8_t type,
                                                         void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_alloc_advanced(size, align, type, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 ***************************   FREE WRAPPERS   *********************************
 ******************************************************************************/
ATTR_EXT_VIS void __wrap_sl_free(void *ptr)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)ptr;

  __real_sl_free(ptr);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_free(void *block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)block;

  sl_status_t status = __real_sl_memory_free(block);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 **************************   CALLOC WRAPPERS   ********************************
 ******************************************************************************/
ATTR_EXT_VIS void *__wrap_sl_calloc(size_t item_count,
                                    size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real_sl_calloc(item_count, size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_calloc(size_t item_count,
                                                 size_t size,
                                                 uint8_t type,
                                                 void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_calloc(item_count, size, type, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 **************************   REALLOC WRAPPERS   *******************************
 ******************************************************************************/
ATTR_EXT_VIS void *__wrap_sl_realloc(void *ptr,
                                     size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *new_ptr = __real_sl_realloc(ptr, size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)new_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return new_ptr;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_realloc(void *ptr,
                                                  size_t size,
                                                  void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_realloc(ptr, size, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 **********************   HEAP-SPECIFIC WRAPPERS   *****************************
 ******************************************************************************/
ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_alloc(sl_memory_heap_t *heap,
                                                     size_t size,
                                                     uint8_t type,
                                                     void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_alloc(heap, size, type, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_alloc_advanced(sl_memory_heap_t *heap,
                                                              size_t size,
                                                              size_t align,
                                                              uint8_t type,
                                                              void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_alloc_advanced(heap, size, align, type, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_free(sl_memory_heap_t *heap,
                                                    void *block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)block;

  sl_status_t status = __real_sl_memory_heap_free(heap, block);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_calloc(sl_memory_heap_t *heap,
                                                      size_t item_count,
                                                      size_t size,
                                                      uint8_t type,
                                                      void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_calloc(heap, item_count, size, type, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_realloc(sl_memory_heap_t *heap,
                                                       void *ptr,
                                                       size_t size,
                                                       void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_realloc(heap, ptr, size, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 **********************   RESERVATION WRAPPERS   *******************************
 ******************************************************************************/
ATTR_EXT_VIS sl_status_t __wrap_sl_memory_reserve_block(size_t size,
                                                        size_t align,
                                                        sl_memory_reservation_t *handle,
                                                        void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_reserve_block(size, align, handle, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_release_block(sl_memory_reservation_t *handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)(handle != NULL ? handle->block_address : NULL);

  sl_status_t status = __real_sl_memory_release_block(handle);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_reservation_handle_alloc(sl_memory_reservation_t **handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_reservation_handle_alloc(handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(handle != NULL ? *handle : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_reservation_handle_free(sl_memory_reservation_t *handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)handle;

  sl_status_t status = __real_sl_memory_reservation_handle_free(handle);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_reservation_add_retention(const sl_memory_reservation_t *handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_reservation_add_retention(handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(handle != NULL ? handle->block_address : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_reservation_remove_retention(const sl_memory_reservation_t *handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_reservation_remove_retention(handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(handle != NULL ? handle->block_address : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_reserve_block(sl_memory_heap_t *heap,
                                                             size_t size,
                                                             size_t align,
                                                             sl_memory_reservation_t *handle,
                                                             void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_reserve_block(heap, size, align, handle, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_reservation_handle_alloc(sl_memory_heap_t *heap,
                                                                        sl_memory_reservation_t **handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_reservation_handle_alloc(heap, handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(handle != NULL ? *handle : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 ************************   MEMORY POOL WRAPPERS   *****************************
 ******************************************************************************/
ATTR_EXT_VIS sl_status_t __wrap_sl_memory_create_pool(size_t block_size,
                                                      uint32_t block_count,
                                                      sl_memory_pool_t *pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_create_pool(block_size, block_count, pool_handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)pool_handle,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_create_pool_advanced(sl_memory_reservation_t *reservation_handle,
                                                               size_t block_size,
                                                               uint32_t block_count,
                                                               size_t align,
                                                               sl_memory_pool_t *pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_create_pool_advanced(reservation_handle,
                                                             block_size,
                                                             block_count,
                                                             align,
                                                             pool_handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)pool_handle,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_delete_pool(sl_memory_pool_t *pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)pool_handle;

  sl_status_t status = __real_sl_memory_delete_pool(pool_handle);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_delete_pool_no_unreserve(sl_memory_pool_t *pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)pool_handle;

  sl_status_t status = __real_sl_memory_delete_pool_no_unreserve(pool_handle);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_pool_alloc(sl_memory_pool_t *pool_handle,
                                                     void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_pool_alloc(pool_handle, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_pool_free(sl_memory_pool_t *pool_handle,
                                                    void *block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)block;

  sl_status_t status = __real_sl_memory_pool_free(pool_handle, block);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_pool_handle_alloc(sl_memory_pool_t **pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_pool_handle_alloc(pool_handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(pool_handle != NULL ? *pool_handle : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_pool_handle_free(sl_memory_pool_t *pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  uint32_t saved_ptr = (uint32_t)(uintptr_t)pool_handle;

  sl_status_t status = __real_sl_memory_pool_handle_free(pool_handle);

  sli_mm_sv_record_ownership(saved_ptr,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_create_pool(sl_memory_heap_t *heap,
                                                           size_t block_size,
                                                           uint32_t block_count,
                                                           sl_memory_pool_t *pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_create_pool(heap, block_size, block_count, pool_handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)pool_handle,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_create_pool_advanced(sl_memory_heap_t *heap,
                                                                    size_t block_size,
                                                                    uint32_t block_count,
                                                                    size_t align,
                                                                    sl_memory_pool_t *pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_create_pool_advanced(heap,
                                                                  block_size,
                                                                  block_count,
                                                                  align,
                                                                  pool_handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)pool_handle,
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_pool_handle_alloc(sl_memory_heap_t *heap,
                                                                 sl_memory_pool_t **pool_handle)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_pool_handle_alloc(heap, pool_handle);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(pool_handle != NULL ? *pool_handle : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 ********************   NO-RETENTION RESERVATION WRAPPERS   ********************
 ******************************************************************************/
ATTR_EXT_VIS sl_status_t __wrap_sl_memory_reserve_no_retention(size_t size,
                                                               size_t align,
                                                               void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_reserve_no_retention(size, align, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

ATTR_EXT_VIS sl_status_t __wrap_sl_memory_heap_reserve_no_retention(sl_memory_heap_t *heap,
                                                                    size_t size,
                                                                    size_t align,
                                                                    void **block)
{
  const void * volatile return_address = sli_memory_get_return_address();
  sl_status_t status = __real_sl_memory_heap_reserve_no_retention(heap, size, align, block);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)(block != NULL ? *block : NULL),
                             (uint32_t)(uintptr_t)return_address);

  return status;
}

/*******************************************************************************
 ***********************   BLUETOOTH ALLOCATION WRAPPERS   *********************
 ******************************************************************************/

extern void *__real_bg_malloc(size_t size);

ATTR_EXT_VIS void *__wrap_bg_malloc(size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real_bg_malloc(size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

/*******************************************************************************
 ************************   MATTER ALLOCATION WRAPPERS   ***********************
 ******************************************************************************/

extern void *__real__ZN4chip8Platform11MemoryAllocEj(size_t size);

ATTR_EXT_VIS void *__wrap__ZN4chip8Platform11MemoryAllocEj(size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZN4chip8Platform11MemoryAllocEj(size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__ZN4chip8Platform12MemoryCallocEjj(size_t count, size_t size);

ATTR_EXT_VIS void *__wrap__ZN4chip8Platform12MemoryCallocEjj(size_t count, size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZN4chip8Platform12MemoryCallocEjj(count, size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

/*******************************************************************************
 *************************   C++ ALLOCATION WRAPPERS   *************************
 ******************************************************************************/

// These symbols are the overloaded global operator new variants.
// C signatures below match the ILP32 ARM Itanium ABI used by memory_manager_cpp.
extern void *__real__Znwj(size_t size);

ATTR_EXT_VIS void *__wrap__Znwj(size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__Znwj(size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__Znaj(size_t size);

ATTR_EXT_VIS void *__wrap__Znaj(size_t size)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__Znaj(size);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__ZnwjRKSt9nothrow_t(size_t size, const void *nothrow_tag);

ATTR_EXT_VIS void *__wrap__ZnwjRKSt9nothrow_t(size_t size, const void *nothrow_tag)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZnwjRKSt9nothrow_t(size, nothrow_tag);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__ZnajRKSt9nothrow_t(size_t size, const void *nothrow_tag);

ATTR_EXT_VIS void *__wrap__ZnajRKSt9nothrow_t(size_t size, const void *nothrow_tag)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZnajRKSt9nothrow_t(size, nothrow_tag);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__ZnwjSt11align_val_t(size_t size, size_t align);

ATTR_EXT_VIS void *__wrap__ZnwjSt11align_val_t(size_t size, size_t align)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZnwjSt11align_val_t(size, align);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__ZnajSt11align_val_t(size_t size, size_t align);

ATTR_EXT_VIS void *__wrap__ZnajSt11align_val_t(size_t size, size_t align)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZnajSt11align_val_t(size, align);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__ZnwjSt11align_val_tRKSt9nothrow_t(size_t size, size_t align, const void *nothrow_tag);

ATTR_EXT_VIS void *__wrap__ZnwjSt11align_val_tRKSt9nothrow_t(size_t size, size_t align, const void *nothrow_tag)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZnwjSt11align_val_tRKSt9nothrow_t(size, align, nothrow_tag);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

extern void *__real__ZnajSt11align_val_tRKSt9nothrow_t(size_t size, size_t align, const void *nothrow_tag);

ATTR_EXT_VIS void *__wrap__ZnajSt11align_val_tRKSt9nothrow_t(size_t size, size_t align, const void *nothrow_tag)
{
  const void * volatile return_address = sli_memory_get_return_address();
  void *ptr = __real__ZnajSt11align_val_tRKSt9nothrow_t(size, align, nothrow_tag);

  sli_mm_sv_record_ownership((uint32_t)(uintptr_t)ptr,
                             (uint32_t)(uintptr_t)return_address);

  return ptr;
}

#endif // SLI_MEMORY_MANAGER_ENABLE_SYSTEMVIEW
