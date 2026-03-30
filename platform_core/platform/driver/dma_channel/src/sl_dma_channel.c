/***************************************************************************//**
 * @file
 * @brief DMA Channel Driver Implementation
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

#include "em_device.h"
#include "sl_hal_ldma.h"
#include "sl_dma_channel.h"
#include "sl_dma_channel_device.h"
#include "sl_device_dma.h"
#include "sl_dma_manager.h"
#include "sl_dma_descriptor_allocator.h"
#include "sl_clock_manager.h"
#include "sl_memory_manager.h"
#include "sl_assert.h"
#include "sl_core.h"
#include <string.h>

/*******************************************************************************
 *****************************   LOCAL MACROS   ********************************
 ******************************************************************************/

/// Check if a transfer buffer pointer is aligned to the specified alignment boundary
/// @param ptr Pointer to check
/// @param align Alignment boundary (must be a power of 2)
/// @return true if aligned, false otherwise
#define TRANSFER_BUFFER_IS_ALIGNED(ptr, align) (((uintptr_t)(ptr) & ((align) - 1)) == 0)

/*******************************************************************************
 *****************************   LOCAL VARIABLES   *****************************
 ******************************************************************************/

// Channel handle lookup table for IRQ processing.
static sl_dma_channel_handle_t *s_channel_handle_table[DMA_CHAN_COUNT] = { 0 };

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Find a descriptor in a linked list by its link target.
 *
 * This function walks through a linked list of DMA descriptors starting from
 * list_head and returns the descriptor whose link_addr field points to the
 * specified link parameter. When link is NULL, the function returns the tail
 * descriptor (the last descriptor in the list with no link).
 *
 * @param[in] list_head Pointer to the first descriptor in the linked list
 *                      (must not be NULL).
 * @param[in] link      Pointer to the target descriptor to search for, or NULL
 *                      to find the tail descriptor.
 *
 * @return Pointer to the descriptor that links to the specified link parameter.
 *         Returns the tail descriptor if link is NULL.
 *
 * @note The function returns a non-const pointer to allow the caller to modify
 *       the returned descriptor (e.g., via link_descriptors), even though the
 *       function itself does not modify the descriptors during traversal.
 ******************************************************************************/
static sl_dma_channel_xfer_descriptor_t* find_descriptor_by_link(const sl_dma_channel_xfer_descriptor_t* list_head,
                                                                 const sl_dma_channel_xfer_descriptor_t* link);

/***************************************************************************//**
 * @brief Link two DMA channel transfer descriptors together.
 *
 * Sets up the link field in the source descriptor to point to the destination
 * descriptor using absolute addressing mode.
 *
 * @param[in] from Pointer to the source descriptor (must not be NULL).
 * @param[in] to   Pointer to the destination descriptor, or NULL to unlink.
 ******************************************************************************/
static void link_descriptors(sl_dma_channel_xfer_descriptor_t* from,
                             sl_dma_channel_xfer_descriptor_t* to);

/***************************************************************************//**
 * @brief Validate buffer alignment for a DMA transfer based on unit size.
 *
 * This function checks that source and destination buffers are properly aligned
 * according to the transfer unit size. HALF word transfers require 2-byte
 * alignment, WORD transfers require 4-byte alignment, and BYTE transfers have
 * no alignment requirement.
 *
 * @param[in] transfer Pointer to the DMA transfer structure. Must not be NULL.
 *
 * @return true if buffers are properly aligned for the specified unit size.
 * @return false if source or destination buffers are misaligned for the specified unit size.
 *         Specifically:
 *         - SL_DMA_CTRL_SIZE_HALF: buffers must be 2-byte aligned
 *         - SL_DMA_CTRL_SIZE_WORD: buffers must be 4-byte aligned
 *         - SL_DMA_CTRL_SIZE_BYTE: no alignment requirement
 ******************************************************************************/
static bool is_transfer_buffer_aligned(const sl_dma_channel_transfer_t *transfer);

/***************************************************************************//**
 * @brief Check whether a descriptor is the currently active descriptor in
 *        a DMA channel.
 *
 * Compares the descriptor's link address against the hardware channel's
 * active LINK register value to determine if the descriptor is the one
 * currently being processed (or about to be processed) by the LDMA engine.
 *
 * @param[in] ldma Pointer to the LDMA peripheral register block.
 * @param[in] ch   Channel number to inspect.
 * @param[in] desc Pointer to the descriptor to check.
 *
 * @return true if @p desc is the active descriptor on the given channel.
 * @return false otherwise.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static bool is_active_descriptor(const LDMA_TypeDef *ldma,
                                 uint8_t ch,
                                 const sl_dma_channel_xfer_descriptor_t* desc);

/***************************************************************************//**
 * @brief Retrieve the next descriptor linked from a given descriptor.
 *
 * Reads the hardware link bit and link address from the descriptor to
 * determine the next descriptor in the chain. Returns NULL if the
 * descriptor has no link (i.e., it is the last in the chain).
 *
 * @param[in] desc Pointer to the descriptor whose successor is queried.
 *
 * @return Pointer to the next descriptor, or NULL if there is no linked
 *         successor.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static sl_dma_channel_xfer_descriptor_t* get_next_descriptor(const sl_dma_channel_xfer_descriptor_t* desc);

/***************************************************************************//**
 * @brief Populate hardware descriptor fields for a DMA transfer.
 *
 * This function configures all hardware-specific fields in a DMA descriptor
 * based on the transfer configuration. The source, destination and 
 * xfer_unit_count, parameters are provided separately to support transfer 
 * segmentation and looping transfers where these values may differ from the 
 * transfer struct.
 *
 * @param[in] descriptor Pointer to the descriptor to populate.
 * @param[in] transfer Pointer to the transfer configuration containing unit_size,
 *                     block_size, increment flags, and handshake mode.
 * @param[in] source Source address for this descriptor (may be segment-adjusted).
 * @param[in] destination Destination address for this descriptor (may be segment-adjusted).
 * @param[in] xfer_unit_count Number of transfer units for this descriptor.
 ******************************************************************************/
static void populate_descriptor_fields(sl_dma_channel_xfer_descriptor_t *descriptor,
                                       const sl_dma_channel_transfer_t *transfer,
                                       void *source,
                                       void *destination,
                                       uint32_t xfer_unit_count);

/***************************************************************************//**
 * @brief Build descriptors for a single transfer.
 *
 * This function builds and links all descriptors needed for a single transfer,
 * handling both internally allocated and user-provided descriptors. Large
 * transfers may be segmented into multiple descriptors if they exceed the
 * maximum transfer size per descriptor.
 *
 * @param[in] current_transfer Pointer to the transfer to build descriptors for.
 * @param[in] previous_transfer Pointer to the previous transfer in the list,
 *                               or NULL if this is the first transfer.
 * @param[in] xfer_unit_size Transfer unit size in bytes.
 * @param[in] xfer_unit_count Total number of transfer units for this transfer.
 * @param[in] handle Pointer to the DMA channel handle.
 *
 * @return SL_STATUS_OK if descriptors were built successfully.
 * @return SL_STATUS_INVALID_PARAMETER if transfer parameters are invalid.
 * @return SL_STATUS_ALLOCATION_FAILED if memory allocation failed.
 ******************************************************************************/
static sl_status_t build_descriptors_for_transfer(sl_dma_channel_transfer_t *current_transfer,
                                                  const sl_dma_channel_transfer_t *previous_transfer,
                                                  uint32_t xfer_unit_size,
                                                  uint32_t xfer_unit_count,
                                                  const sl_dma_channel_handle_t *handle);

/***************************************************************************//**
 * @brief Clean up internally allocated descriptors on error.
 *
 * This function frees all descriptors that were internally allocated by the
 * DMA Channel driver when an error occurs during descriptor building. The 
 * function iterates through the linked list of descriptors starting from 
 * list_head until it reaches a descriptor with no link (link bit = 0).
 *
 * @param[in] handle Pointer to the dma channel handle.
 * @param[in] list_head Pointer to the first descriptor in the linked list.
 ******************************************************************************/
static void cleanup_allocated_descriptors(const sl_dma_channel_handle_t *handle,
                                          sl_dma_channel_xfer_descriptor_t *list_head);

/***************************************************************************//**
 * @brief Process a completed descriptor by invoking callback and freeing if needed.
 *
 * This function processes a completed descriptor by invoking the user callback
 * (if present and requested) and freeing the descriptor if it was internally
 * allocated. It returns a pointer to the next descriptor in the chain.
 *
 * @param[in] handle     Pointer to the DMA channel handle.
 * @param[in] descriptor Pointer to the descriptor to process.
 * @param[in] error      Error flag to pass to callback.
 * @param[in] aborted    Aborted flag to pass to callback.
 *
 * @return Pointer to the next descriptor in the chain, or NULL if this was the last.
 ******************************************************************************/
static sl_dma_channel_xfer_descriptor_t* process_descriptor(sl_dma_channel_handle_t* handle,
                                                            sl_dma_channel_xfer_descriptor_t* descriptor,
                                                            bool error,
                                                            bool aborted);

/***************************************************************************//**
 * @brief Process completed descriptors.
 *
 * This function processes completed descriptors in the channel's queue by
 * invoking user callbacks and freeing internally allocated descriptors. It
 * walks the descriptor list and uses is_active_descriptor() to determine
 * which descriptors have completed. If all descriptors are consumed, the
 * channel is disabled and its state is set to SL_DMA_CHANNEL_STATE_DISABLED.
 *
 * @param[in] handle         Pointer to the DMA channel handle.
 ******************************************************************************/
static void process_completed_descriptors(sl_dma_channel_handle_t* handle);

/***************************************************************************//**
 * @brief Internal function to submit a list of transfers with optional looping mode.
 *
 * This internal function submits a list of transfers to the specified DMA channel
 * with the ability to transition the channel into looping mode. It is used by
 * both the public API (which never uses looping mode) and by specialized APIs
 * like ping_pong and triple_buffered transfers that require looping mode.
 *
 * When begin_looping_mode is true the channel must be in the
 * SL_DMA_CHANNEL_STATE_DISABLED state; otherwise SL_STATUS_INVALID_STATE is
 * returned. On successful submission the channel state is set to
 * SL_DMA_CHANNEL_STATE_ENABLED. If the hardware CHDONE flag indicates a
 * completed transfer, the DMA engine is restarted automatically so that
 * the newly submitted descriptors begin executing.
 *
 * @param[in] handle              Pointer to the DMA channel handle.
 * @param[in] list_head           Pointer to the head of the transfer list.
 * @param[in] begin_looping_mode  If true, transitions the channel to looping mode
 *                                after submitting the transfers. This should only
 *                                be true when the transfer list forms a loop
 *                                (circular linked list) and is submitted by
 *                                ping_pong or triple_buffered APIs.
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_INVALID_PARAMETER if any transfer validation fails.
 * @return SL_STATUS_INVALID_STATE if channel is already in looping mode, or if
 *         begin_looping_mode is true and the channel is not disabled.
 * @return SL_STATUS_ALLOCATION_FAILED if descriptor allocation fails.
 *
 * @note This function is for internal use only. External code should use the
 *       public API sl_dma_channel_submit_transfer_list() or specialized APIs
 *       like sl_dma_channel_submit_transfer_m2p_pingpong().
 ******************************************************************************/
static sl_status_t sli_dma_channel_submit_transfer_list(sl_dma_channel_handle_t *handle,
                                                        sl_dma_channel_transfer_t *list_head,
                                                        bool begin_looping_mode);

/***************************************************************************//**
 * @brief Internal per-channel interrupt processing routine.
 *
 * Responsibilities:
 *  - Detect channel error (CHERROR) and, if present, abort all queued work
 *    reporting (error=true, aborted=true) for each descriptor.
 *  - Pop and free one or more completed descriptors, invoking the user
 *    callback for each with (error=false, aborted=false).
 *  - Handle scenarios where multiple descriptors completed while interrupts
 *    were masked or delayed by walking the software queue until the currently
 *    active hardware descriptor is reached.
 *
 * The function intentionally operates without holding a long critical section
 * (hardware has already signalled completion) and relies on single context
 * invocation per hardware channel IRQ.
 *
 * @param[in] ch Channel number whose interrupt is being serviced.
 ******************************************************************************/
static void sli_dma_channel_irq_process(uint8_t ch);

/***************************************************************************//**
 * @brief DMA Channel 0 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch0(void);

/***************************************************************************//**
 * @brief DMA Channel 1 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch1(void);

/***************************************************************************//**
 * @brief DMA Channel 2 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch2(void);

/***************************************************************************//**
 * @brief DMA Channel 3 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch3(void);

/***************************************************************************//**
 * @brief DMA Channel 4 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch4(void);

/***************************************************************************//**
 * @brief DMA Channel 5 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch5(void);

/***************************************************************************//**
 * @brief DMA Channel 6 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch6(void);

/***************************************************************************//**
 * @brief DMA Channel 7 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch7(void);

#if (DMA_CHAN_COUNT > 8)
/***************************************************************************//**
 * @brief DMA Channel 8 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch8(void);
#endif

#if (DMA_CHAN_COUNT > 9)
/***************************************************************************//**
 * @brief DMA Channel 9 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch9(void);
#endif

#if (DMA_CHAN_COUNT > 10)
/***************************************************************************//**
 * @brief DMA Channel 10 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch10(void);
#endif

#if (DMA_CHAN_COUNT > 11)
/***************************************************************************//**
 * @brief DMA Channel 11 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch11(void);
#endif

#if (DMA_CHAN_COUNT > 12)
/***************************************************************************//**
 * @brief DMA Channel 12 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch12(void);
#endif

#if (DMA_CHAN_COUNT > 13)
/***************************************************************************//**
 * @brief DMA Channel 13 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch13(void);
#endif

#if (DMA_CHAN_COUNT > 14)
/***************************************************************************//**
 * @brief DMA Channel 14 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch14(void);
#endif

#if (DMA_CHAN_COUNT > 15)
/***************************************************************************//**
 * @brief DMA Channel 15 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch15(void);
#endif

#if (DMA_CHAN_COUNT > 16)
/***************************************************************************//**
 * @brief DMA Channel 16 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch16(void);
#endif

#if (DMA_CHAN_COUNT > 17)
/***************************************************************************//**
 * @brief DMA Channel 17 IRQ wrapper.
 *
 * Forwards the interrupt to the common per-channel processing routine.
 ******************************************************************************/
static void sl_dma_channel_irq_wrapper_ch17(void);
#endif

/*******************************************************************************
 ***************************   GLOBAL FUNCTIONS  *******************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *   Allocate a DMA channel handle structure from the heap.
 ******************************************************************************/
sl_status_t sl_dma_channel_handle_alloc(sl_dma_channel_handle_t **handle)
{
  EFM_ASSERT(handle != NULL);

  sl_status_t status = sl_memory_calloc(1,
                                        sizeof(sl_dma_channel_handle_t),
                                        BLOCK_TYPE_SHORT_TERM,
                                        (void **)handle);
  if (status != SL_STATUS_OK) {
    return SL_STATUS_ALLOCATION_FAILED;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Free a DMA channel handle previously allocated by sl_dma_channel_handle_alloc().
 ******************************************************************************/
sl_status_t sl_dma_channel_handle_free(sl_dma_channel_handle_t *handle)
{
  EFM_ASSERT(handle != NULL);

  uint8_t ch = handle->channel_number;
  EFM_ASSERT(ch < DMA_CHAN_COUNT);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  // Clear handle table entry before freeing the handle.
  if (ch < DMA_CHAN_COUNT && s_channel_handle_table[ch] == handle) {
    s_channel_handle_table[ch] = NULL;
  }

  sl_free(handle);
  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Get the size in bytes of the channel handle structure.
 ******************************************************************************/
size_t sl_dma_channel_handle_get_size(void)
{
  return sizeof(sl_dma_channel_handle_t);
}

/***************************************************************************//**
 * @brief
 *   Allocate a DMA channel transfer descriptor from the heap.
 ******************************************************************************/
sl_status_t sl_dma_channel_descriptor_alloc(const sl_dma_channel_handle_t *handle,
                                            sl_dma_channel_xfer_descriptor_t **descriptor)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(descriptor != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  return sl_dma_descriptor_allocator_allocate((sl_peripheral_t)handle->dma_peripheral,
                                              handle->channel_number,
                                              true,
                                              (void **)descriptor);
}

/***************************************************************************//**
 * @brief
 *   Free a DMA channel transfer descriptor previously allocated by
 *   sl_dma_channel_descriptor_alloc().
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
sl_status_t sl_dma_channel_descriptor_free(const sl_dma_channel_handle_t *handle,
                                           sl_dma_channel_xfer_descriptor_t *descriptor)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(descriptor != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  sl_dma_descriptor_allocator_free((sl_peripheral_t)handle->dma_peripheral,
                                   handle->channel_number,
                                   descriptor);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Get the size in bytes of the channel transfer descriptor structure.
 ******************************************************************************/
size_t sl_dma_channel_descriptor_get_size(void)
{
  return sizeof(sl_dma_channel_xfer_descriptor_t);
}

/***************************************************************************//**
 * @brief
 *   Initialize an allocated channel handle with peripheral instance, channel index
 *   and user completion callback.
 ******************************************************************************/
sl_status_t sl_dma_channel_init(sl_dma_channel_handle_t *handle,
                                sl_peripheral_t peripheral,
                                uint8_t channel_number,
                                sl_dma_channel_callback_t callback,
                                void *user_data)
{
  EFM_ASSERT(channel_number < DMA_CHAN_COUNT);
  EFM_ASSERT(handle != NULL);

  sl_peripheral_dma_t dma_peripheral;
  if ( peripheral == NULL ) {
    // Use the default dma peripheral if one is not explicitly provided.
    dma_peripheral = (sl_peripheral_dma_t)SL_PERIPHERAL_LDMA0;
  } else {
    dma_peripheral = (sl_peripheral_dma_t)peripheral;
  }

  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)dma_peripheral->base) != -1);
  LDMA_TypeDef *ldma = (LDMA_TypeDef *)(dma_peripheral->base);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  // When initializing, we look at the enabled state of the hardware instead
  // of using the dma_channel_state.
  if ( sl_hal_ldma_channel_is_enabled(ldma, channel_number) ) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_BUSY;
  }

  handle->dma_peripheral = dma_peripheral;
  handle->channel_number = channel_number;
  handle->callback = callback;
  handle->user_data = user_data;
  handle->descriptor_list = NULL;
  handle->state = SL_DMA_CHANNEL_STATE_DISABLED;
  handle->mode = SL_DMA_CHANNEL_MODE_NORMAL;

  // Initialize descriptor allocator for this channel
  sl_dma_descriptor_allocator_init((sl_peripheral_t)dma_peripheral, channel_number);

  // Enable clocks (DMA + LDMAXBAR) – minimal required for submission.
  sl_bus_clock_t dma_bus_clock = sl_device_peripheral_get_bus_clock((sl_peripheral_t)handle->dma_peripheral);
  sl_clock_manager_enable_bus_clock(dma_bus_clock);
  sl_bus_clock_t dmaxbar_bus_clock = sl_device_peripheral_get_bus_clock(SL_PERIPHERAL_LDMAXBAR0);
  sl_clock_manager_enable_bus_clock(dmaxbar_bus_clock);

  // Reset peripheral signal
  sl_dma_channel_set_peripheral_signal(handle, NULL);

  // Store handle for IRQ lookup (always register wrapper so we can free internal descriptors)
  if (channel_number < DMA_CHAN_COUNT) {
    s_channel_handle_table[channel_number] = handle;
    switch (channel_number) {
      case 0: (void)sl_dma_manager_register_channel_irq_callback(NULL, 0, sl_dma_channel_irq_wrapper_ch0); break;
      case 1: (void)sl_dma_manager_register_channel_irq_callback(NULL, 1, sl_dma_channel_irq_wrapper_ch1); break;
      case 2: (void)sl_dma_manager_register_channel_irq_callback(NULL, 2, sl_dma_channel_irq_wrapper_ch2); break;
      case 3: (void)sl_dma_manager_register_channel_irq_callback(NULL, 3, sl_dma_channel_irq_wrapper_ch3); break;
      case 4: (void)sl_dma_manager_register_channel_irq_callback(NULL, 4, sl_dma_channel_irq_wrapper_ch4); break;
      case 5: (void)sl_dma_manager_register_channel_irq_callback(NULL, 5, sl_dma_channel_irq_wrapper_ch5); break;
      case 6: (void)sl_dma_manager_register_channel_irq_callback(NULL, 6, sl_dma_channel_irq_wrapper_ch6); break;
      case 7: (void)sl_dma_manager_register_channel_irq_callback(NULL, 7, sl_dma_channel_irq_wrapper_ch7); break;
#if (DMA_CHAN_COUNT > 8)
      case 8: (void)sl_dma_manager_register_channel_irq_callback(NULL, 8, sl_dma_channel_irq_wrapper_ch8); break;
#endif
#if (DMA_CHAN_COUNT > 9)
      case 9: (void)sl_dma_manager_register_channel_irq_callback(NULL, 9, sl_dma_channel_irq_wrapper_ch9); break;
#endif
#if (DMA_CHAN_COUNT > 10)
      case 10: (void)sl_dma_manager_register_channel_irq_callback(NULL, 10, sl_dma_channel_irq_wrapper_ch10); break;
#endif
#if (DMA_CHAN_COUNT > 11)
      case 11: (void)sl_dma_manager_register_channel_irq_callback(NULL, 11, sl_dma_channel_irq_wrapper_ch11); break;
#endif
#if (DMA_CHAN_COUNT > 12)
      case 12: (void)sl_dma_manager_register_channel_irq_callback(NULL, 12, sl_dma_channel_irq_wrapper_ch12); break;
#endif
#if (DMA_CHAN_COUNT > 13)
      case 13: (void)sl_dma_manager_register_channel_irq_callback(NULL, 13, sl_dma_channel_irq_wrapper_ch13); break;
#endif
#if (DMA_CHAN_COUNT > 14)
      case 14: (void)sl_dma_manager_register_channel_irq_callback(NULL, 14, sl_dma_channel_irq_wrapper_ch14); break;
#endif
#if (DMA_CHAN_COUNT > 15)
      case 15: (void)sl_dma_manager_register_channel_irq_callback(NULL, 15, sl_dma_channel_irq_wrapper_ch15); break;
#endif
#if (DMA_CHAN_COUNT > 16)
      case 16: (void)sl_dma_manager_register_channel_irq_callback(NULL, 16, sl_dma_channel_irq_wrapper_ch16); break;
#endif
#if (DMA_CHAN_COUNT > 17)
      case 17: (void)sl_dma_manager_register_channel_irq_callback(NULL, 17, sl_dma_channel_irq_wrapper_ch17); break;
#endif
      default: break; // No wrapper defined beyond 18; extend if needed.
    }

    // When initializing, we should ensure that the CHDONE flag is set since 
    // this is the state we want to be in when there is nothing left in the DMA
    // channel. We will use this assumption later when submitting transfers.
    ldma->CHDONE_SET = 1UL << channel_number;

    // Ensure DONE + error interrupts enabled.
#if defined(LDMA_IEN_ERROR)
    sl_hal_ldma_enable_interrupts(ldma, (1UL << channel_number) | LDMA_IEN_ERROR);
#elif defined(LDMA_IEN_ERROR0)
    sl_hal_ldma_enable_interrupts(ldma, (1UL << channel_number) | (LDMA_IEN_ERROR0 << channel_number));
#else
    sl_hal_ldma_enable_interrupts(ldma, 1UL << channel_number);
#endif

    sl_hal_ldma_enable_channel_request(ldma, channel_number);
  }

  CORE_EXIT_ATOMIC();
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Deinitialize a DMA channel handle.
 *
 * @details Returns SL_STATUS_BUSY if the channel is not in the
 *          SL_DMA_CHANNEL_STATE_DISABLED state (i.e., if transfers are still
 *          active or the channel is being aborted). Call
 *          sl_dma_channel_abort() first to ensure the channel is idle.
 ******************************************************************************/
sl_status_t sl_dma_channel_deinit(sl_dma_channel_handle_t *handle)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(handle->channel_number < DMA_CHAN_COUNT);

  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);
  uint8_t channel_number = handle->channel_number;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  if ( handle->state != SL_DMA_CHANNEL_STATE_DISABLED ) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_BUSY;
  }

#if defined(LDMA_IEN_ERROR)
  sl_hal_ldma_disable_interrupts(ldma, (1UL << channel_number));
#elif defined(LDMA_IEN_ERROR0)
  sl_hal_ldma_disable_interrupts(ldma, (1UL << channel_number) | (LDMA_IEN_ERROR0 << channel_number));
#else
  sl_hal_ldma_disable_interrupts(ldma, 1UL << channel_number);
#endif

  // Disable channel requests
  sl_hal_ldma_disable_channel_request(ldma, channel_number);

  // Reset peripheral signal
  sl_dma_channel_set_peripheral_signal(handle, NULL);

  // Unregister IRQ callback by passing NULL callback
  (void)sl_dma_manager_register_channel_irq_callback(NULL, channel_number, NULL);

  // Clear handle from internal table
  s_channel_handle_table[channel_number] = NULL;

  CORE_EXIT_ATOMIC();

  // Zero out the handle structure
  memset(handle, 0, sizeof(sl_dma_channel_handle_t));

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Set the peripheral signal for a channel handle. The peripheral signal is
 *   necessary for memory-to-peripheral or peripheral-to-memory transfers which
 *   require a handshake with the peripheral for each block transferred.
 *
 * @details The peripheral signal can only be changed while the channel is in
 *          the SL_DMA_CHANNEL_STATE_DISABLED state. Returns SL_STATUS_BUSY if
 *          the channel has active or pending transfers.
 ******************************************************************************/
sl_status_t sl_dma_channel_set_peripheral_signal(const sl_dma_channel_handle_t *handle,
                                                 sl_dma_signal_t signal)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  uint8_t ch = handle->channel_number;
  EFM_ASSERT(ch < DMA_CHAN_COUNT);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  if ( handle->state > SL_DMA_CHANNEL_STATE_DISABLED ) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_BUSY;
  }

#if defined(_SILICON_LABS_32B_SERIES_3)
  uint32_t ldma_instance_number = LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base);

  LDMAXBAR_TypeDef* ldma_xbar_instance = LDMAXBAR(ldma_instance_number);
  EFM_ASSERT((uint32_t)ldma_xbar_instance != 0x0UL);
#else
  LDMAXBAR_TypeDef* ldma_xbar_instance = LDMAXBAR;
#endif

  if ( signal != NULL ) {
    // Set peripheral signal of the dma channel in the LDMAXBAR
    EFM_ASSERT(!(*signal & ~_LDMAXBAR_CH_REQSEL_MASK));
    ldma_xbar_instance->CH[ch].REQSEL = *signal;
  } else {
    ldma_xbar_instance->CH[ch].REQSEL = 0;
  }

  CORE_EXIT_ATOMIC();
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Aborts all pending transfers on a given DMA channel.
 *
 * @details This function may be called from thread context or from within a
 *          DMA channel callback (IRQ context). When invoked from a callback,
 *          the function detects that it is executing inside
 *          process_completed_descriptors() and fast-forwards past already-
 *          processed descriptors, aborting only those that remain. Re-entrant
 *          calls (e.g. from an abort callback that itself calls abort) are
 *          safe: the function returns SL_STATUS_OK immediately when the
 *          channel state is already SL_DMA_CHANNEL_STATE_ABORTING or
 *          SL_DMA_CHANNEL_STATE_DISABLED.
 ******************************************************************************/
sl_status_t sl_dma_channel_abort(sl_dma_channel_handle_t *handle)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);
  uint8_t ch = handle->channel_number;

  // Return an OK status if we are already in the middle of aborting, or the 
  // channel is already disabled.
  if ( handle->state == SL_DMA_CHANNEL_STATE_ABORTING 
      || handle->state == SL_DMA_CHANNEL_STATE_DISABLED ) {
    return SL_STATUS_OK;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  sl_hal_ldma_disable_channel(ldma, ch);
  handle->state = SL_DMA_CHANNEL_STATE_ABORTING;

  // Let's determine which descriptors need to be aborted and process any
  // completed descriptors if we are not already in the middle of doing so.
  sl_dma_channel_xfer_descriptor_t *aborted_head = handle->descriptor_list;
  sl_dma_channel_xfer_descriptor_t *completed_tail = NULL;
  if (handle->mode == SL_DMA_CHANNEL_MODE_LOOPING) {
    // Find the tail of the looping descriptor list and set its link to NULL so
    // that we do not iterate over the descriptor_list indefinitely.
    link_descriptors(find_descriptor_by_link(handle->descriptor_list, handle->descriptor_list), NULL);
  } else {
    // Find the tail of the completed descriptor list, all other descriptors
    // will be aborted so they will be removed from the descriptor list.
    while ( aborted_head != NULL && !is_active_descriptor(ldma, ch, aborted_head) ) {
      completed_tail = aborted_head;
      aborted_head = get_next_descriptor(aborted_head);
    }
    if ( completed_tail != NULL ) {
      link_descriptors(completed_tail, NULL);
    }

    // Lets process the completed descriptors now if there are any 
    // remaining to be processed.
    if ( completed_tail != NULL && !CORE_IN_IRQ_CONTEXT() ) {
        // If we were in an IRQ context, then we would be aborting from
        // within `process_completed_descriptors` called by the IRQ 
        // handler.
        //
        // We currently aren't in an IRQ context, so the abort was not 
        // initiated within `process_completed_descriptors`, so we need 
        // to manually process the completed descriptors.
        process_completed_descriptors(handle);
    }
  }

  // Abort the active descriptor and all descriptors linked to it.
  while (aborted_head != NULL) {
    aborted_head = process_descriptor(handle, aborted_head, false, true);
  }

  // Reset DMA channel. 
  handle->mode = SL_DMA_CHANNEL_MODE_NORMAL;
  handle->state = SL_DMA_CHANNEL_STATE_DISABLED;
  handle->descriptor_list = NULL;
  ldma->CHDONE_SET = 1UL << ch;

  ldma->REQCLEAR = 1UL << ch;
  ldma->CH[ch].CTRL = 0;
  ldma->CH[ch].SRC = 0;
  ldma->CH[ch].DST = 0;
  ldma->CH[ch].LINK = 0UL;

  sl_hal_ldma_clear_interrupts(ldma, 1UL << ch);
  sl_hal_ldma_disable_interrupts(ldma, 1UL << ch);
  __DMB();

  sl_hal_ldma_enable_channel(ldma, ch);
  while(sl_hal_ldma_channel_is_active(ldma, ch));
  sl_hal_ldma_disable_channel(ldma, ch);

  sl_hal_ldma_enable_interrupts(ldma, 1UL << ch);
  __DMB();

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Suspends peripheral requests for a given DMA channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_suspend(const sl_dma_channel_handle_t *handle)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);

  sl_hal_ldma_disable_channel_request(ldma, handle->channel_number);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Resumes peripheral requests for a previously suspended DMA channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_resume(const sl_dma_channel_handle_t *handle)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);

  sl_hal_ldma_enable_channel_request(ldma, handle->channel_number);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Gets the status of the given DMA channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_get_status(sl_dma_channel_handle_t *handle,
                                      sl_dma_channel_status_t *status)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);
  EFM_ASSERT(status != NULL);

  LDMA_TypeDef *ldma_hw_instance = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);
  status->bytes_completed = 0U;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  status->enabled = sl_hal_ldma_channel_is_enabled(ldma_hw_instance, handle->channel_number);
  status->active = sl_hal_ldma_channel_is_active(ldma_hw_instance, handle->channel_number);

  // Compute bytes completed for the current (head) descriptor only.
  // Definition: bytes_completed = (original_xfer_count -
  // remaining_xfer_count) * unit_size_bytes, where original_xfer_count is
  // (descriptor->xfer.xfer_count + 1).
  // Notes:
  //  - We only look at the head queue entry (first not-yet-processed
  //    descriptor). Completed descriptors are removed by the IRQ handler.
  //  - Remaining item count only reflects the active descriptor; chained
  //    descriptors beyond the head are ignored here by design.
  if (handle->descriptor_list == NULL || handle->mode == SL_DMA_CHANNEL_MODE_LOOPING ) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_OK;
  }

  sl_dma_channel_xfer_descriptor_t *entry = handle->descriptor_list;

  // The descriptors to get status are the current active descriptor.
  while ( !is_active_descriptor(ldma_hw_instance, handle->channel_number, entry) ) {
    entry = get_next_descriptor(entry);
    if ( entry == NULL) {
        CORE_EXIT_ATOMIC();
        return SL_STATUS_OK;
    }
  }
  
  uint32_t orig_items = ((sl_hal_ldma_descriptor_t*)entry)->xfer.xfer_count + 1U;

#if defined(_LDMA_CH_XCTRL_XFERCNT_MASK) && !defined(_LDMA_CH_XCTRL_BUFFERABLE_MASK)
  orig_items += ((sl_hal_ldma_descriptor_extend_t*)entry)->xfer_count_high;
#endif

  uint32_t remaining = sl_hal_ldma_transfer_remaining_count(ldma_hw_instance, handle->channel_number);
  if (remaining > orig_items) {
    remaining = 0U;
  }
  uint32_t completed_items = orig_items - remaining;

  uint32_t unit_bytes = 1U;
  switch (((sl_hal_ldma_descriptor_t*)entry)->xfer.size) {
    case SL_HAL_LDMA_CTRL_SIZE_WORD: unit_bytes = 4U; break;
    case SL_HAL_LDMA_CTRL_SIZE_HALF: unit_bytes = 2U; break;
    case SL_HAL_LDMA_CTRL_SIZE_BYTE:
    default: unit_bytes = 1U; break;
  }
  status->bytes_completed = completed_items * unit_bytes;
  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Submits a list of transfers to the specified DMA channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_transfer_list(sl_dma_channel_handle_t *handle,
                                                sl_dma_channel_transfer_t *list_head)
{
  return sli_dma_channel_submit_transfer_list(handle, list_head, false);
}

/***************************************************************************//**
 * @brief
 *   Submit a memory-to-memory transfer on a channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_transfer_m2m(sl_dma_channel_handle_t *handle,
                                               void *source,
                                               void *destination,
                                               size_t size,
                                               sl_dma_channel_xfer_descriptor_t *descriptor)
{
  // Calculate the optimal transfer unit size based on buffer alignment and transfer size.
  // Select the largest unit size possible: WORD if both buffers are 4-byte aligned and
  // size is a multiple of 4, HALF if both buffers are 2-byte aligned and size is a
  // multiple of 2, otherwise BYTE.
  sl_dma_ctrl_size_t unit_size = SL_DMA_CTRL_SIZE_BYTE;
  if (TRANSFER_BUFFER_IS_ALIGNED(source, 4)
      && TRANSFER_BUFFER_IS_ALIGNED(destination, 4)
      && (size % 4U) == 0U) {
    unit_size = SL_DMA_CTRL_SIZE_WORD;
  } else if (TRANSFER_BUFFER_IS_ALIGNED(source, 2)
             && TRANSFER_BUFFER_IS_ALIGNED(destination, 2)
             && (size % 2U) == 0U) {
    unit_size = SL_DMA_CTRL_SIZE_HALF;
  }

  sl_dma_channel_transfer_t transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = true,
    .block_handshake_mode = false,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptor,
    .next = NULL,
  };

  return sl_dma_channel_submit_transfer_list(handle, &transfer);
}

/***************************************************************************//**
 * @brief
 *   Submit a memory-to-peripheral transfer on a channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_transfer_m2p(sl_dma_channel_handle_t *handle,
                                               void *source,
                                               void *reg,
                                               size_t size,
                                               sl_dma_ctrl_size_t unit_size,
                                               sl_dma_channel_xfer_descriptor_t *descriptor)
{
  sl_dma_channel_transfer_t transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = reg,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = false,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptor,
    .next = NULL,
  };

  return sl_dma_channel_submit_transfer_list(handle, &transfer);
}

/***************************************************************************//**
 * @brief
 *   Submit a peripheral-to-memory transfer on a channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_transfer_p2m(sl_dma_channel_handle_t *handle,
                                               void *reg,
                                               void *destination,
                                               size_t size,
                                               sl_dma_ctrl_size_t unit_size,
                                               sl_dma_channel_xfer_descriptor_t *descriptor)
{
  sl_dma_channel_transfer_t transfer = (sl_dma_channel_transfer_t) {
    .source = reg,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = false,
    .increment_destination = true,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptor,
    .next = NULL,
  };

  return sl_dma_channel_submit_transfer_list(handle, &transfer);
}

/***************************************************************************//**
 * @brief
 *   Updates size of the current transfer.
 ******************************************************************************/
sl_status_t sl_dma_channel_update_active_transfer(sl_dma_channel_handle_t *handle,
                                                  size_t new_size)
{
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  if (new_size == 0 || handle->descriptor_list == NULL) {
    return (handle->descriptor_list == NULL) ? SL_STATUS_INVALID_STATE : SL_STATUS_INVALID_PARAMETER;
  }

  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);
  uint8_t ch = handle->channel_number;
  sl_dma_channel_xfer_descriptor_t* active_desc = NULL;

  EFM_ASSERT(ch < DMA_CHAN_COUNT);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  active_desc = handle->descriptor_list;

  // The descriptors to update is the current active descriptor.
  while ( !is_active_descriptor(ldma, ch, active_desc) ) {
    active_desc = get_next_descriptor(active_desc);
    if (active_desc == NULL) {
        CORE_EXIT_ATOMIC();
        return SL_STATUS_INVALID_STATE;
    }
  }

#if defined(SL_DMA_CHANNEL_HAS_EXTENDED_DESCRIPTORS)
  sl_hal_ldma_descriptor_extend_t* hw_desc = (sl_hal_ldma_descriptor_extend_t*)active_desc;
  sl_hal_ldma_ctrl_size_t size = hw_desc->size;
#else
  sl_hal_ldma_descriptor_t* hw_desc = (sl_hal_ldma_descriptor_t*)active_desc;
  sl_hal_ldma_ctrl_size_t size = hw_desc->xfer.size;
#endif

  // Unit size lookup: 1<<(size*2) gives 1, 2, 4 for BYTE, HALF, WORD
  static const uint8_t unit_bytes_lut[] = { 1, 2, 4 };
  uint32_t unit_bytes = (size < 3) ? unit_bytes_lut[size] : 1U;

  if ((new_size % unit_bytes) != 0) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint32_t new_unit_count = new_size / unit_bytes;

  if (new_unit_count == 0 || new_unit_count > SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Disable channel before reading and updating the active transfer
  sl_hal_ldma_disable_channel(ldma, ch);

  uint32_t ctrl_reg_current = ldma->CH[ch].CTRL;
  uint32_t current_remaining_units;
#if defined(SL_DMA_CHANNEL_HAS_EXTENDED_DESCRIPTORS)
  uint32_t xctrl_reg_current = ldma->CH[ch].XCTRL;
  // Extended descriptor: combine low bits from CTRL and high bits from XCTRL
  uint32_t remaining_low = (ctrl_reg_current & _LDMA_CH_CTRL_XFERCNT_MASK) >> _LDMA_CH_CTRL_XFERCNT_SHIFT;
  uint32_t remaining_high = (xctrl_reg_current & _LDMA_CH_XCTRL_XFERCNT_MASK) >> _LDMA_CH_XCTRL_XFERCNT_SHIFT;
  current_remaining_units = remaining_low + (remaining_high << _LDMA_CH_XCTRL_XFERCNT_SHIFT);
#else
  // Regular descriptor: only use CTRL register
  current_remaining_units = (ctrl_reg_current & _LDMA_CH_CTRL_XFERCNT_MASK) >> _LDMA_CH_CTRL_XFERCNT_SHIFT;
#endif
  // XFERCNT is 0-based, so add 1 to get actual remaining count
  current_remaining_units = current_remaining_units + 1UL;

  // Reconstruct original count from descriptor
  // The descriptor stores (count - 1) encoded, so we need to decode it
  uint32_t original_unit_count;
#if defined(SL_DMA_CHANNEL_HAS_EXTENDED_DESCRIPTORS)
  // Extended descriptor: combine low 11 bits and high 5 bits
  uint32_t original_low = hw_desc->xfer_count;
  uint32_t original_high = hw_desc->xfer_count_high;
  // Reconstruct: (low_bits) + (high_bits << 11) + 1
  original_unit_count = original_low + (original_high << _LDMA_CH_XCTRL_XFERCNT_SHIFT) + 1UL;
#else
  // Regular descriptor: xfer_count is (count - 1), so add 1
  original_unit_count = hw_desc->xfer.xfer_count + 1UL;
#endif

  // Calculate how many units have already been transferred
  uint32_t already_transferred_units = original_unit_count - current_remaining_units;

  // Check if we've already transferred more than the new size
  if (already_transferred_units > new_unit_count) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Calculate new remaining count: new_total_size - already_transferred
  uint32_t new_remaining_units = new_unit_count - already_transferred_units;

  // Encode new remaining count (0-based) for writing to registers
#if defined(SL_DMA_CHANNEL_HAS_EXTENDED_DESCRIPTORS)
  uint32_t ctrl_val = SL_HAL_LDMA_EXTEND_XFER_COUNT(new_remaining_units);
  uint32_t xctrl_val = SL_HAL_LDMA_EXTEND_XFER_COUNT_HIGH(new_remaining_units);
  hw_desc->xfer_count = ctrl_val;
  hw_desc->xfer_count_high = xctrl_val;
#else
  uint32_t ctrl_val = new_remaining_units - 1UL;
  hw_desc->xfer.xfer_count = ctrl_val;
#endif

  // Update CTRL register (common for both descriptor types)
  uint32_t ctrl_reg = ldma->CH[ch].CTRL;
  ctrl_reg &= ~_LDMA_CH_CTRL_XFERCNT_MASK;
  ctrl_reg |= (ctrl_val << _LDMA_CH_CTRL_XFERCNT_SHIFT) & _LDMA_CH_CTRL_XFERCNT_MASK;
  ldma->CH[ch].CTRL = ctrl_reg;

#if defined(SL_DMA_CHANNEL_HAS_EXTENDED_DESCRIPTORS)
  // Update XCTRL register for extended descriptors
  uint32_t xctrl_reg = ldma->CH[ch].XCTRL;
  xctrl_reg &= ~_LDMA_CH_XCTRL_XFERCNT_MASK;
  xctrl_reg |= (xctrl_val << _LDMA_CH_XCTRL_XFERCNT_SHIFT) & _LDMA_CH_XCTRL_XFERCNT_MASK;
  ldma->CH[ch].XCTRL = xctrl_reg;
#endif

  sl_hal_ldma_enable_channel(ldma, ch);
  CORE_EXIT_ATOMIC();
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *   Submit a memory-to-peripheral ping pong transfer on a channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_ping_pong_transfer_m2p(sl_dma_channel_handle_t *handle,
                                                         void *source,
                                                         void *source_2,
                                                         void *destination,
                                                         size_t size,
                                                         sl_dma_ctrl_size_t unit_size,
                                                         sl_dma_channel_xfer_descriptor_t *descriptors)
{
  sl_dma_channel_transfer_t ping_transfer;
  sl_dma_channel_transfer_t pong_transfer;

  ping_transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = false,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[0] : NULL,
    .next = &pong_transfer,
  };
  pong_transfer = (sl_dma_channel_transfer_t) {
    .source = source_2,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = false,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[1] : NULL,
    .next = NULL, // The looping link will be set via sli_dma_channel_submit_transfer_list.
  };

  return sli_dma_channel_submit_transfer_list(handle, &ping_transfer, true);
}

/***************************************************************************//**
 * @brief
 *   Submit a peripheral-to-memory ping pong transfer on a channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_ping_pong_transfer_p2m(sl_dma_channel_handle_t *handle,
                                                         void *source,
                                                         void *destination,
                                                         void *destination_2,
                                                         size_t size,
                                                         sl_dma_ctrl_size_t unit_size,
                                                         sl_dma_channel_xfer_descriptor_t *descriptors)
{
  sl_dma_channel_transfer_t ping_transfer;
  sl_dma_channel_transfer_t pong_transfer;

  ping_transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = false,
    .increment_destination = true,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[0] : NULL,
    .next = &pong_transfer,
  };
  pong_transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination_2,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = false,
    .increment_destination = true,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[1] : NULL,
    .next = NULL, // The looping link will be set via sli_dma_channel_submit_transfer_list.
  };

  return sli_dma_channel_submit_transfer_list(handle, &ping_transfer, true);
}

/***************************************************************************//**
 * @brief
 *   Submit a memory-to-peripheral triple buffered transfer on a channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_triple_buffered_transfer_m2p(sl_dma_channel_handle_t *handle,
                                                               void *source,
                                                               void *source_2,
                                                               void *source_3,
                                                               void *destination,
                                                               size_t size,
                                                               sl_dma_ctrl_size_t unit_size,
                                                               sl_dma_channel_xfer_descriptor_t *descriptors)
{
  sl_dma_channel_transfer_t buf1_transfer;
  sl_dma_channel_transfer_t buf2_transfer;
  sl_dma_channel_transfer_t buf3_transfer;

  buf1_transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = false,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[0] : NULL,
    .next = &buf2_transfer,
  };
  buf2_transfer = (sl_dma_channel_transfer_t) {
    .source = source_2,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = false,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[1] : NULL,
    .next = &buf3_transfer,
  };
  buf3_transfer = (sl_dma_channel_transfer_t) {
    .source = source_3,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = false,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[2] : NULL,
    .next = NULL, // The looping link will be set via sli_dma_channel_submit_transfer_list.
  };

  return sli_dma_channel_submit_transfer_list(handle, &buf1_transfer, true);
}

/***************************************************************************//**
 * @brief
 *   Submit a peripheral-to-memory triple buffered transfer on a channel.
 ******************************************************************************/
sl_status_t sl_dma_channel_submit_triple_buffered_transfer_p2m(sl_dma_channel_handle_t *handle,
                                                               void *source,
                                                               void *destination,
                                                               void *destination_2,
                                                               void *destination_3,
                                                               size_t size,
                                                               sl_dma_ctrl_size_t unit_size,
                                                               sl_dma_channel_xfer_descriptor_t *descriptors)
{
  sl_dma_channel_transfer_t buf1_transfer;
  sl_dma_channel_transfer_t buf2_transfer;
  sl_dma_channel_transfer_t buf3_transfer;

  buf1_transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = false,
    .increment_destination = true,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[0] : NULL,
    .next = &buf2_transfer,
  };
  buf2_transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination_2,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = false,
    .increment_destination = true,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[1] : NULL,
    .next = &buf3_transfer,
  };
  buf3_transfer = (sl_dma_channel_transfer_t) {
    .source = source,
    .destination = destination_3,
    .size = size,
    .unit_size = unit_size,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = false,
    .increment_destination = true,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .cacheable = false,             // Not implemented
    .descriptor = descriptors ? &descriptors[2] : NULL,
    .next = NULL, // The looping link will be set via sli_dma_channel_submit_transfer_list.
  };

  return sli_dma_channel_submit_transfer_list(handle, &buf1_transfer, true);
}

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/
static sl_status_t sli_dma_channel_submit_transfer_list(sl_dma_channel_handle_t *handle,
                                                        sl_dma_channel_transfer_t *list_head,
                                                        bool begin_looping_mode)
{
  sl_status_t status = SL_STATUS_OK;

  // Validate programming errors
  EFM_ASSERT(handle != NULL);
  EFM_ASSERT(list_head != NULL);
  EFM_ASSERT(handle->dma_peripheral != NULL);
  EFM_ASSERT(LDMA_NUM((LDMA_TypeDef *)handle->dma_peripheral->base) != -1);

  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);
  EFM_ASSERT(ldma != NULL);
  uint8_t ch = handle->channel_number;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  // Check if channel is in looping mode, if so we cannot submit anymore
  // transfers to the channel while it is in this mode. We also cannot submit
  // transfers while the channel is being aborted.
  if (handle->mode == SL_DMA_CHANNEL_MODE_LOOPING) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_INVALID_STATE;
  }

  // If the transfer submitted wishes to begin looping mode, the channel must
  // already be disabled (empty) before submission. This is to prevent dangling
  // descriptors in the channel that never get consumed.
  if ( begin_looping_mode && handle->state != SL_DMA_CHANNEL_STATE_DISABLED ) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_INVALID_STATE;
  }

  CORE_EXIT_ATOMIC();

  sl_dma_channel_transfer_t* current_transfer = list_head;
  const sl_dma_channel_transfer_t* previous_transfer = NULL;

  // Build and link one or more descriptors for each transfer.
  while ( current_transfer != NULL ) {
    EFM_ASSERT(current_transfer->unit_size <= SL_DMA_CTRL_SIZE_WORD);
    uint32_t xfer_unit_size = (1U << current_transfer->unit_size);

    // Validate transfer size and buffer alignment
    if ((current_transfer->size == 0)
        || ((current_transfer->size % xfer_unit_size) != 0)
        || !is_transfer_buffer_aligned(current_transfer)) {
      status = SL_STATUS_INVALID_PARAMETER;
      cleanup_allocated_descriptors(handle, list_head->descriptor);
      return status;
    }

    uint32_t xfer_unit_count = (current_transfer->size / xfer_unit_size);

    // DMA Channel Transfer segmentation is only supported for descriptors
    // whose lifetimes are managed by the DMA Channel driver. DMA Channel
    // Transfers submitted with a pre-allocated descriptor structure must be
    // encodable in that single descriptor structure.
    if (current_transfer->descriptor != NULL
        && xfer_unit_count > SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT ) {
      return SL_STATUS_INVALID_PARAMETER;
    }

    // Build descriptors for this transfer
    status = build_descriptors_for_transfer(current_transfer,
                                            previous_transfer,
                                            xfer_unit_size,
                                            xfer_unit_count,
                                            handle);
    if (status != SL_STATUS_OK) {
      cleanup_allocated_descriptors(handle, list_head->descriptor);
      return status;
    }

    previous_transfer = current_transfer;
    current_transfer = current_transfer->next;
  }

  CORE_ENTER_ATOMIC();

  // Disable the channel. This pauses active transfers.
  sl_hal_ldma_disable_channel(ldma, ch);

  if (begin_looping_mode) {
    // If we are transitioning to looping mode, then the channel must have been
    // empty before arriving here. Set the dma channel mode to looping.
    handle->mode = SL_DMA_CHANNEL_MODE_LOOPING;
  }

  // Append the submitted descriptor list
  sl_dma_channel_xfer_descriptor_t* descriptor_list_tail;
  if ( handle->descriptor_list == NULL ) {
    handle->descriptor_list = list_head->descriptor;
  } else {
    descriptor_list_tail = find_descriptor_by_link(handle->descriptor_list, NULL);
    link_descriptors(descriptor_list_tail, list_head->descriptor);
  }

  // Transitioning to looping mode will link the last descriptor of the
  // submitted transfer list to the first descriptor of the submitted list.
  if (begin_looping_mode) {
    descriptor_list_tail = find_descriptor_by_link(handle->descriptor_list, NULL);
    link_descriptors(descriptor_list_tail, list_head->descriptor);
  }

  // After processing completed descriptors, if the descriptor head in
  // the channel handle is the same as the head of the list of descriptors that
  // is being submitted, this means that the DMA channel must be refreshed.
  // We are ignoring the link mode field, only checking if the link address is 0.
  if ( (ldma->CH[ch].LINK & ~_LDMA_CH_LINK_LINKMODE_MASK) == 0UL ) {
    ldma->CH[ch].LINK = ((uint32_t)(list_head->descriptor) & _LDMA_CH_LINK_LINKADDR_MASK) | LDMA_CH_LINK_LINKMODE | LDMA_CH_LINK_LINK;
    if ( sl_hal_ldma_transfer_is_done(ldma, ch) ) {
      sl_hal_ldma_start_transfer(ldma, ch);
    }
  }

  sl_hal_ldma_enable_channel(ldma, ch);
  handle->state = SL_DMA_CHANNEL_STATE_ENABLED;

  CORE_EXIT_ATOMIC();
  return status;
}

static sl_dma_channel_xfer_descriptor_t* find_descriptor_by_link(const sl_dma_channel_xfer_descriptor_t* list_head,
                                                                 const sl_dma_channel_xfer_descriptor_t* link)
{
  EFM_ASSERT(list_head != NULL);

  const sl_hal_ldma_descriptor_t* link_hw_desc = (const sl_hal_ldma_descriptor_t*) link;
  const sl_hal_ldma_descriptor_t* hw_desc_iter = (const sl_hal_ldma_descriptor_t*)list_head;
  while ( SL_HAL_LDMA_DESCRIPTOR_LINKABS_LINKADDR_TO_ADDR(hw_desc_iter->xfer.link_addr) != link_hw_desc ) {
    hw_desc_iter = SL_HAL_LDMA_DESCRIPTOR_LINKABS_LINKADDR_TO_ADDR(hw_desc_iter->xfer.link_addr);
  }

  // Cast away const: this function doesn't modify the input, but returns a non-const
  // pointer so the caller can modify the returned descriptor (e.g., via link_descriptors).
  // Cast through non-const intermediate type to explicitly handle const removal.
  return (sl_dma_channel_xfer_descriptor_t*)hw_desc_iter;
}

static void link_descriptors(sl_dma_channel_xfer_descriptor_t* from,
                             sl_dma_channel_xfer_descriptor_t* to)
{
  EFM_ASSERT(from != NULL);
  ((sl_hal_ldma_descriptor_t*)from)->xfer.link = to ? 1 : 0;
  ((sl_hal_ldma_descriptor_t*)from)->xfer.link_mode = SL_HAL_LDMA_LINK_MODE_ABS;
  ((sl_hal_ldma_descriptor_t*)from)->xfer.link_addr = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(to);
}

static bool is_transfer_buffer_aligned(const sl_dma_channel_transfer_t *transfer)
{
  EFM_ASSERT(transfer != NULL);

  // Validate buffer alignment based on unit size
  // HALF requires 2-byte alignment, WORD requires 4-byte alignment
  switch (transfer->unit_size) {
    case SL_DMA_CTRL_SIZE_HALF:
      if (!TRANSFER_BUFFER_IS_ALIGNED(transfer->source, 2)
          || !TRANSFER_BUFFER_IS_ALIGNED(transfer->destination, 2)) {
        return false;
      }
      break;
    case SL_DMA_CTRL_SIZE_WORD:
      if (!TRANSFER_BUFFER_IS_ALIGNED(transfer->source, 4)
          || !TRANSFER_BUFFER_IS_ALIGNED(transfer->destination, 4)) {
        return false;
      }
      break;
    case SL_DMA_CTRL_SIZE_BYTE:
      // BYTE unit size has no alignment requirement
      break;
    default:
      // Unknown unit size, consider as misaligned
      return false;
  }

  return true;
}

static bool is_active_descriptor( const LDMA_TypeDef *ldma,
                                  uint8_t ch,
                                  const sl_dma_channel_xfer_descriptor_t* desc)
{
  bool is_active = false;

  // If the channel is done, then no descriptor is the active descriptor
  if ( ldma->CHDONE & (1UL << ch) ) {
    return is_active;
  }

  const uint32_t active_link_addr = (ldma->CH[ch].LINK & _LDMA_CH_LINK_LINKADDR_MASK);
  const sl_hal_ldma_descriptor_t* hw = (const sl_hal_ldma_descriptor_t*)desc;
  const uint32_t desc_link_addr = (uint32_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_LINKADDR_TO_ADDR(hw->xfer.link_addr);
  if ( desc_link_addr == active_link_addr) {
    is_active = true;
  }

  return is_active;
}

static sl_dma_channel_xfer_descriptor_t* get_next_descriptor(const sl_dma_channel_xfer_descriptor_t* desc)
{
  const sl_hal_ldma_descriptor_t* hw = (const sl_hal_ldma_descriptor_t*)desc;
  sl_dma_channel_xfer_descriptor_t* next = NULL;
  if (hw->xfer.link) {
    next = (sl_dma_channel_xfer_descriptor_t *)(SL_HAL_LDMA_DESCRIPTOR_LINKABS_LINKADDR_TO_ADDR(hw->xfer.link_addr));
  }
  return next;
}

static void populate_descriptor_fields(sl_dma_channel_xfer_descriptor_t *descriptor,
                                       const sl_dma_channel_transfer_t *transfer,
                                       void *source,
                                       void *destination,
                                       uint32_t xfer_unit_count)
{
  sl_hal_ldma_descriptor_t* hw_desc = (sl_hal_ldma_descriptor_t*)descriptor;

  // Configure basic descriptor structure
  hw_desc->xfer.struct_type = SL_HAL_LDMA_CTRL_STRUCT_TYPE_XFER;
  hw_desc->xfer.struct_req = transfer->block_handshake_mode ? 0 : 1;
  hw_desc->xfer.block_size = transfer->block_size;
  hw_desc->xfer.done_ifs = 1;

  // Configure request mode and address increment behavior
  hw_desc->xfer.req_mode = transfer->block_handshake_mode ? SL_HAL_LDMA_CTRL_REQ_MODE_BLOCK : SL_HAL_LDMA_CTRL_REQ_MODE_ALL;
  hw_desc->xfer.src_inc = transfer->increment_source ? SL_HAL_LDMA_CTRL_SRC_INC_ONE : SL_HAL_LDMA_CTRL_SRC_INC_NONE;
  hw_desc->xfer.dst_inc = transfer->increment_destination ? SL_HAL_LDMA_CTRL_DST_INC_ONE : SL_HAL_LDMA_CTRL_DST_INC_NONE;

  // Set transfer unit size and address modes
  hw_desc->xfer.size = transfer->unit_size;
  hw_desc->xfer.src_addr_mode = SL_HAL_LDMA_CTRL_SRC_ADDR_MODE_ABS;
  hw_desc->xfer.dst_addr_mode = SL_HAL_LDMA_CTRL_DST_ADDR_MODE_ABS;

  // Set source and destination addresses
  hw_desc->xfer.src_addr = (uint32_t)source;
  hw_desc->xfer.dst_addr = (uint32_t)destination;

#if defined(SL_DMA_CHANNEL_HAS_EXTENDED_DESCRIPTORS)
  sl_hal_ldma_descriptor_extend_t* extend_desc = (sl_hal_ldma_descriptor_extend_t*)hw_desc;
  extend_desc->extend = 1;

  // For extended descriptors, split the transfer count between CTRL and XCTRL registers
#if defined(_LDMA_CH_XCTRL_XFERCNT_MASK)
  hw_desc->xfer.xfer_count = SL_HAL_LDMA_EXTEND_XFER_COUNT(xfer_unit_count);
  extend_desc->xfer_count_high = SL_HAL_LDMA_EXTEND_XFER_COUNT_HIGH(xfer_unit_count);
#else
  hw_desc->xfer.xfer_count = xfer_unit_count - 1UL;
#endif
#else
  hw_desc->xfer.xfer_count = xfer_unit_count - 1UL;
#endif
}

static sl_status_t build_descriptors_for_transfer(sl_dma_channel_transfer_t *current_transfer,
                                                  const sl_dma_channel_transfer_t *previous_transfer,
                                                  uint32_t xfer_unit_size,
                                                  uint32_t xfer_unit_count,
                                                  const sl_dma_channel_handle_t *handle)
{
  sl_status_t status = SL_STATUS_OK;

  // Determine the number of transfer segments, and the number of units in the
  // partial segment if there is a number of xfer units remaining.
  uint32_t xfer_segment_count = xfer_unit_count / SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT;
  uint32_t xfer_partial_segment_unit_count = xfer_unit_count % SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT;
  if (xfer_partial_segment_unit_count > 0) {
    xfer_segment_count += 1;
  }

  // Then build and link descriptors for segments, allocating memory for
  // descriptors if no pre-allocated descriptor structure is provided. For
  // multi-segment transfers, only the last descriptor will trigger an
  // interrupt if a callback on transfer complete was requested.
  bool is_user_allocated = true;
  sl_dma_channel_xfer_descriptor_t* current_descriptor = current_transfer->descriptor;
  sl_dma_channel_xfer_descriptor_t* previous_descriptor = NULL;

  if (current_descriptor == NULL) {
    // All descriptors will be internally allocated if the descriptor provided
    // for the transfer is NULL ( the descriptor is not user-allocated ).
    is_user_allocated = false;
  }

  for ( uint32_t segment = 0; segment < xfer_segment_count; segment++ ) {
    if ( !is_user_allocated ) {
      status = sl_dma_channel_descriptor_alloc(handle,
                                               &current_descriptor);

      // Save pointer to first descriptor in the transfer structure
      if ( segment == 0 ) {
        current_transfer->descriptor = current_descriptor;
      }

      if (status != SL_STATUS_OK) {
        return status;
      }

      // Indicate that the descriptor was allocated internally.
      current_descriptor->flags.driver_allocated = 1;
    } else {
      // Make sure that the user-allocated descriptor buffer is zeroed before
      // proceeding.
      memset(current_descriptor, 0, sizeof(sl_dma_channel_xfer_descriptor_t));
    }

    // Calculate source and destination addresses for this segment
    uint32_t segment_offset = segment * SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT * xfer_unit_size;
    void *segment_source = (void *)((uint32_t)(current_transfer->source) + segment_offset);
    void *segment_destination = (void *)((uint32_t)(current_transfer->destination) + segment_offset);

    // Calculate transfer unit count for this segment
    bool is_last_segment = (segment == (xfer_segment_count - 1));
    uint32_t segment_unit_count;
    if (is_last_segment && xfer_partial_segment_unit_count > 0) {
      segment_unit_count = xfer_partial_segment_unit_count;
    } else {
      segment_unit_count = SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT;
    }

    // Determine if done interrupt should be set (only on last segment if callback requested)
    current_descriptor->flags.callback_on_complete = (current_transfer->callback_on_complete && is_last_segment) ? 1 : 0;

    // Populate hardware specific descriptor fields.
    populate_descriptor_fields(current_descriptor,
                               current_transfer,
                               segment_source,
                               segment_destination,
                               segment_unit_count);

    // If there was a previous descriptor, link this current descriptor
    // to it. Otherwise, if it's the first descriptor, link it to the
    // previous DMA Channel Transfer's descriptor(s) if there was
    // a previous transfer in the list.
    if ( previous_descriptor != NULL ) {
      link_descriptors(previous_descriptor, current_descriptor);
    } else {
      if ( previous_transfer != NULL ) {
        // Find the tail of the list of descriptors of the last DMA Channel
        // Transfer.
        sl_dma_channel_xfer_descriptor_t* prev_xfer_desc_tail = find_descriptor_by_link(previous_transfer->descriptor, NULL);
        link_descriptors(prev_xfer_desc_tail, current_descriptor);
      }
    }

    previous_descriptor = current_descriptor;
  }

  return status;
}

static void cleanup_allocated_descriptors(const sl_dma_channel_handle_t *handle,
                                          sl_dma_channel_xfer_descriptor_t *list_head)
{
  sl_dma_channel_xfer_descriptor_t *descriptor_iter = list_head;
  sl_dma_channel_xfer_descriptor_t* temp = NULL;
  while (descriptor_iter != NULL) {
    sl_dma_channel_xfer_descriptor_t* next = NULL;
    const sl_hal_ldma_descriptor_t* hw = (sl_hal_ldma_descriptor_t*)descriptor_iter;
    if (hw->xfer.link) {
      sl_hal_ldma_descriptor_t *next_hw = SL_HAL_LDMA_DESCRIPTOR_LINKABS_LINKADDR_TO_ADDR(hw->xfer.link_addr);
      next = (sl_dma_channel_xfer_descriptor_t *)next_hw;
    }

    if ( descriptor_iter->flags.driver_allocated ) {
      // The descriptor memory was internally allocated by the driver.
      temp = descriptor_iter;
    }

    if ( temp != NULL ) {
      (void)sl_dma_channel_descriptor_free(handle, temp);
      temp = NULL;
    }
    descriptor_iter = next;
  }
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static sl_dma_channel_xfer_descriptor_t* process_descriptor(sl_dma_channel_handle_t * handle,
                                                            sl_dma_channel_xfer_descriptor_t * descriptor,
                                                            bool error,
                                                            bool aborted)
{
  // Call the user callback if present and if callback was requested for this descriptor.
  if (handle->callback != NULL && descriptor->flags.callback_on_complete) {
    handle->callback(handle, handle->user_data, error, aborted);
  }

  // Get the next descriptor in the chain before potentially freeing the current one.
  const sl_dma_channel_xfer_descriptor_t* next = get_next_descriptor(descriptor);

  // Free the descriptor if it was allocated internally by the DMA Channel driver.
  if (descriptor->flags.driver_allocated) {
    (void)sl_dma_channel_descriptor_free(handle, descriptor);
  }

  return (sl_dma_channel_xfer_descriptor_t*)next;
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void process_completed_descriptors(sl_dma_channel_handle_t* handle)
{
  EFM_ASSERT(handle != NULL);
  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)handle->dma_peripheral);

  uint8_t ch = handle->channel_number;
  EFM_ASSERT(ch < DMA_CHAN_COUNT);

  // Walk queue to process completed descriptors (before active one)
  sl_dma_channel_xfer_descriptor_t* iter = handle->descriptor_list;
  while (iter != NULL) {
    // If the linked transfer is not yet done, stop processing descriptors if
    // the current descriptor's linkaddr is the actively linked descriptor in
    // the DMA channel (may be NULL).
    if ( is_active_descriptor(ldma, ch, iter) && !sl_hal_ldma_transfer_is_done(ldma, ch)) {
      break;
    }

    // If we reach here, this descriptor has completed, so process it.
    iter = process_descriptor(handle, iter, false, false);
  }

  // Check if there is an error reported for the DMA channel, if so, flush
  // the remaining descriptors in the descriptor list and call their callbacks
  // with the appropriate error/abort flags. The first descriptor remaining in
  // the descriptor list is the descriptor that caused the error.
  uint32_t pending_errors = sl_dma_manager_get_pending_errors(ch);
  if (pending_errors) {
    const sl_dma_channel_xfer_descriptor_t* error_desc = iter;
    while (iter != NULL) {
      // Call the callback with the appropriate error/abort flags.
      iter = process_descriptor(handle, iter, (iter == error_desc), true);
    }

    // Clear the LINK register.
    ldma->CH[ch].LINK = 0UL;
    // Clear the pending error flag after processing all error descriptors
    sl_dma_manager_clear_pending_errors(ch);
  }

  // Update channel descriptor list head
  handle->descriptor_list = iter;
  if ( handle->descriptor_list == NULL ) {
    // Disable the DMA channel when all descriptors have been processed
    sl_hal_ldma_disable_channel(ldma, ch);
    handle->state = SL_DMA_CHANNEL_STATE_DISABLED;
  }
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sli_dma_channel_irq_process(uint8_t ch)
{
  sl_dma_channel_handle_t *handle = s_channel_handle_table[ch];
  EFM_ASSERT(ch < DMA_CHAN_COUNT);
  EFM_ASSERT(handle != NULL);

  if ( handle->mode == SL_DMA_CHANNEL_MODE_NORMAL ) {
    // In normal mode, we will process each completed descriptor by consuming
    // the descriptor from the DMA channel's descriptor list and calling the
    // user's callback for each descriptor that requests the callback.
    process_completed_descriptors(handle);
  } else if ( handle->mode == SL_DMA_CHANNEL_MODE_LOOPING ) {
    // When the channel is in looping mode, we don't want to consume descriptors
    // from the channel's descriptor list, instead we will call the user's
    // callback each time we receive an IRQ.
    if (handle->callback != NULL) {
      handle->callback(handle, handle->user_data, false, false);
    }
  }
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch0(void)
{
  sli_dma_channel_irq_process(0);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch1(void)
{
  sli_dma_channel_irq_process(1);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch2(void)
{
  sli_dma_channel_irq_process(2);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch3(void)
{
  sli_dma_channel_irq_process(3);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch4(void)
{
  sli_dma_channel_irq_process(4);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch5(void)
{
  sli_dma_channel_irq_process(5);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch6(void)
{
  sli_dma_channel_irq_process(6);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch7(void)
{
  sli_dma_channel_irq_process(7);
}

#if (DMA_CHAN_COUNT > 8)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch8(void)
{
  sli_dma_channel_irq_process(8);
}
#endif

#if (DMA_CHAN_COUNT > 9)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch9(void)
{
  sli_dma_channel_irq_process(9);
}
#endif

#if (DMA_CHAN_COUNT > 10)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch10(void)
{
  sli_dma_channel_irq_process(10);
}
#endif

#if (DMA_CHAN_COUNT > 11)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch11(void)
{
  sli_dma_channel_irq_process(11);
}
#endif

#if (DMA_CHAN_COUNT > 12)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch12(void)
{
  sli_dma_channel_irq_process(12);
}
#endif

#if (DMA_CHAN_COUNT > 13)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch13(void)
{
  sli_dma_channel_irq_process(13);
}
#endif

#if (DMA_CHAN_COUNT > 14)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch14(void)
{
  sli_dma_channel_irq_process(14);
}
#endif

#if (DMA_CHAN_COUNT > 15)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch15(void)
{
  sli_dma_channel_irq_process(15);
}
#endif

#if (DMA_CHAN_COUNT > 16)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch16(void)
{
  sli_dma_channel_irq_process(16);
}
#endif

#if (DMA_CHAN_COUNT > 17)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_DMA_CHANNEL, SL_CODE_CLASS_DMA_CHANNEL_PERFORMANCE)
static void sl_dma_channel_irq_wrapper_ch17(void)
{
  sli_dma_channel_irq_process(17);
}
#endif
