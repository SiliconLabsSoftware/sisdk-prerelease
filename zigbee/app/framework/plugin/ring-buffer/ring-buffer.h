/***************************************************************************//**
 * @file
 * @brief Definitions for the Ring Buffer plugin.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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

/**
 * @defgroup ring-buffer Ring Buffer
 * @ingroup component
 * @brief API and Callbacks for the Ring Buffer Component
 *
 * This component manages a ring buffer of binary entries. Each entry has a
 * 1- or 2-byte length field that is recorded to manage the buffer. This ring buffer
 * can be used by multiple entities as the buffer space for the ring buffer is
 * allocated by the caller.
 *
 */

/**
 * @addtogroup ring-buffer
 * @{
 */

#define SL_ZIGBEE_AF_PLUGIN_RING_BUFFER_WIDE_LENGTH_ENTRY_OVERHEAD 2
#define SL_ZIGBEE_AF_PLUGIN_RING_BUFFER_NARROW_LENGTH_ENTRY_OVERHEAD 1

#define SL_ZIGBEE_AF_PLUGIN_RING_BUFFER_MAX_LENGTH_ENTRY_OVERHEAD \
  SL_ZIGBEE_AF_PLUGIN_RING_BUFFER_WIDE_LENGTH_ENTRY_OVERHEAD

typedef void (sl_zigbee_af_plugin_ring_buffer_delete_function_t)(uint16_t entryNumber,
                                                                 uint16_t entrySize);

typedef struct {
  const uint16_t maxSize;

  // Private variables, don't modify outside ring-buffer.c
  // (I wish we had C++)
  uint8_t* data;
  uint16_t firstItemIndex;
  uint16_t tailIndex;
  uint16_t entryCount;
  sl_zigbee_af_plugin_ring_buffer_delete_function_t* deleteCallback;
  uint32_t addedItems;
  uint32_t deletedItems;
  uint16_t lastItemLengthIndex; // used for append
  uint16_t iteratorIndex;
  bool iteratorValid;
  uint8_t lengthFieldWidthBytes;
} sl_zigbee_ring_buffer_t;

/**
 * @name API
 * @{
 */

/** @brief Initialize the ring buffer struct based on data passed.
 * widLengthField determines whether the ring buffer uses 1 or 2 byte length fields
 * for entry.  wideLengthField = false uses 1 byte length, wideLengthField = true
 * uses 2 byte lengths.
 * All entries in the same ring buffer utilze the same size length field.
 * Different ring buffers may use differnt sizes.
 *
 * @param ringBufferStruct Ver.: always
 * @param wideLengthField Ver.: always
 * @param deleteCallback Ver.: always
 * @param dataPtr Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_init_struct(sl_zigbee_ring_buffer_t* ringBufferStruct,
                                                 bool wideLengthField,
                                                 sl_zigbee_af_plugin_ring_buffer_delete_function_t* deleteCallback,
                                                 uint8_t* dataPtr);

/** @brief Add an entry in the ring buffer.
 *
 * @param ring Ver.: always
 * @param dataToAdd Ver.: always
 * @param dataToAddSize Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_add_entry(sl_zigbee_ring_buffer_t* ring,
                                               uint8_t* dataToAdd,
                                               uint16_t dataToAddSize);

/** @brief Retrieve data from the specified entry.  Returns the entry's total
 * size in value pointed to by returnEntryTotalSize.  The amount of data
 * returned is written to the pointer returnDataSize.  This allows
 * for the caller to get the entry's total size at the same time they
 * are able to grab a chunk of the entry up to some limit.
 * @param entryNumber numbering starts from 0
 * @param returnData may be set to NULL, in which case no return data is written.
 * @param returnEntryTotalSize does not include the entry's length overhead, which the caller shouldn't care about anyway.
 * @param maxReturnSize
 * @param returnDataSize
 * @param returnData
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_get_entry_by_entry_number(sl_zigbee_ring_buffer_t* ring,
                                                               uint16_t  entryNumber,
                                                               uint16_t  dataIndexInEntry,
                                                               uint16_t* returnEntryTotalSize,
                                                               uint16_t  maxReturnSize,
                                                               uint16_t* returnDataSize,
                                                               uint8_t*  returnData);

/** @brief Get the last entry of the ring buffer.
 *
 * @param ring Ver.: always
 * @param dataIndexInEntry Ver.: always
 * @param returnEntryTotalSize Ver.: always
 * @param maxReturnSize Ver.: always
 * @param returnDataSize Ver.: always
 * @param returnData Ver.: always
 *
 * @return sl_status_t status code
 */
sl_status_t sl_zigbee_af_ring_buffer_get_last_entry(sl_zigbee_ring_buffer_t* ring,
                                                    uint16_t dataIndexInEntry,
                                                    uint16_t* returnEntryTotalSize,
                                                    uint16_t maxReturnSize,
                                                    uint16_t* returnDataSize,
                                                    uint8_t* returnData);

/** @brief Initialize the iterator to the first entry in the ring buffer.
 * You may immediately call sl_zigbee_af_ring_buffer_get_entry_by_iterator()
 * after this function is called.
 *
 * @param ring Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_init_iterator(sl_zigbee_ring_buffer_t* ring);

/** @brief Iterate to next entry in ring buffer.
 *
 * @param ring Ver.: always
 *
 * @return sl_status_t status code
 *
 * @note Return SL_STATUS_IN_PROGRESS if there are more items left to
 * iterate through.  Returns SL_STATUS_OK on reaching the last item,
 * and the iterator is marked invalid (needs to be initialized again).
 * Returns SL_STATUS_INVALID_STATE if the iterator has not been initialized.
 * An iterator can also be marked invalid if the
 * ring buffer has deleted the entry that the iterator was currently on.
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_iterator_next_entry(sl_zigbee_ring_buffer_t* ring);

/** @brief Similar to sl_zigbee_af_ring_buffer_get_entry_by_entry_number().
 *
 * @param ring Ver.: always
 * @param dataIndexInEntry Ver.: always
 * @param returnEntryTotalSize Ver.: always
 * @param maxReturnSize Ver.: always
 * @param returnDataSize Ver.: always
 * @param returnData Ver.: always
 *
 * @return sl_status_t status code
 *
 * @note The iterator must have already been initialized
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_get_entry_by_iterator(sl_zigbee_ring_buffer_t* ring,
                                                           uint16_t dataIndexInEntry,
                                                           uint16_t* returnEntryTotalSize,
                                                           uint16_t maxReturnSize,
                                                           uint16_t* returnDataSize,
                                                           uint8_t* returnData);

/** @brief Update a ring buffer entry by passing in the iterator.
 *
 * @param ring Ver.: always
 * @param dataIndexEntry Ver.: always
 * @param updateData Ver.: always
 * @param updateDataLength Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_update_entry_by_iterator(sl_zigbee_ring_buffer_t* ring,
                                                              uint16_t dataIndexInEntry,
                                                              uint8_t* updatedData,
                                                              uint16_t updatedDataLength);

/** @brief Append an entry to end of the ring buffer.
 *
 * @param ring Ver.: always
 * @param dataToAdd Ver.: always
 * @param dataToAddSize Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_ring_buffer_append_last_entry(sl_zigbee_ring_buffer_t* ring,
                                                       uint8_t* dataToAdd,
                                                       uint16_t dataToAddSize);

/** @} */ // end of name API
/** @} */ // end of ring-buffer
