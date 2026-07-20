/***************************************************************************//**
 * @file
 * @brief Serial Port Profile IO Stream component
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

#include <string.h>

#include "sl_common.h"
#include "spp_iostream.h"
#include "spp_iostream_config.h"
#include "spp.h"
#include "sl_core.h"
#include "sli_iostream.h"
#ifdef SL_CATALOG_APP_ASSERT_PRESENT
#include "app_assert.h"
#endif // SL_CATALOG_APP_ASSERT_PRESENT
typedef struct {
  uint8_t rx_buffer[SPP_IOSTREAM_RX_BUFFER_SIZE];
  size_t rx_head;
  size_t rx_count;
} spp_iostream_context_t;

static sl_status_t spp_iostream_write(void *context,
                                      const void *buffer,
                                      size_t buffer_length);
static sl_status_t spp_iostream_read(void *context,
                                     void *buffer,
                                     size_t buffer_length,
                                     size_t *bytes_read);
static size_t spp_iostream_on_data_received(const uint8_t *data, size_t data_size);
static void spp_iostream_reset_rx_buffer(spp_iostream_context_t *context);

static spp_iostream_context_t spp_iostream_context;

static sl_iostream_t sl_iostream_spp = {
  .context = &spp_iostream_context,
  .write = spp_iostream_write,
  .write_async = NULL,
  .read = spp_iostream_read,
};

sl_iostream_t *sl_iostream_spp_handle = &sl_iostream_spp;

sl_iostream_instance_info_t sl_iostream_instance_spp_info = {
  .handle = &sl_iostream_spp,
  .name = SPP_IOSTREAM_INSTANCE_NAME,
  .type = SL_IOSTREAM_TYPE_UNDEFINED,
  .periph_id = 0,
  .init = spp_iostream_init,
};

sl_status_t spp_iostream_init(void)
{
  spp_iostream_reset_rx_buffer(&spp_iostream_context);
  spp_set_data_receive_callback(spp_iostream_on_data_received);
  return SL_STATUS_OK;
}

sl_iostream_t *spp_iostream_get_stream_instance(void)
{
  return sl_iostream_spp_handle;
}

SL_WEAK void spp_iostream_on_error(spp_iostream_error_t error, sl_status_t result)
{
  #ifdef SL_CATALOG_APP_ASSERT_PRESENT
  app_assert_status_f(result,
                      "spp_iostream: Error %d occurred.",
                      error);
  #endif // SL_CATALOG_APP_ASSERT_PRESENT
}

static sl_status_t spp_iostream_write(void *context,
                                      const void *buffer,
                                      size_t buffer_length)
{
  (void)context;

  if (buffer == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (buffer_length == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  return spp_transmit((const uint8_t *)buffer, buffer_length);
}

static sl_status_t spp_iostream_read(void *context,
                                     void *buffer,
                                     size_t buffer_length,
                                     size_t *bytes_read)
{
  spp_iostream_context_t *spp_stream = (spp_iostream_context_t *)context;
  uint8_t *output = (uint8_t *)buffer;
  size_t read_count;
  CORE_DECLARE_IRQ_STATE;

  if (bytes_read != NULL) {
    *bytes_read = 0U;
  }

  if (spp_stream == NULL || buffer == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (buffer_length == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  CORE_ENTER_ATOMIC();
  if (spp_stream->rx_count == 0U) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_EMPTY;
  }

  read_count = (buffer_length < spp_stream->rx_count) ? buffer_length : spp_stream->rx_count;
  for (size_t i = 0; i < read_count; i++) {
    output[i] = spp_stream->rx_buffer[spp_stream->rx_head];
    spp_stream->rx_head = (spp_stream->rx_head + 1U) % SPP_IOSTREAM_RX_BUFFER_SIZE;
  }
  spp_stream->rx_count -= read_count;
  CORE_EXIT_ATOMIC();

  if (bytes_read != NULL) {
    *bytes_read = read_count;
  }

  return SL_STATUS_OK;
}

static size_t spp_iostream_on_data_received(const uint8_t *data, size_t data_size)
{
  size_t bytes_consumed;
  size_t write_index;
  CORE_DECLARE_IRQ_STATE;

  if (data == NULL || data_size == 0U) {
    return 0U;
  }

  CORE_ENTER_ATOMIC();
  bytes_consumed = SPP_IOSTREAM_RX_BUFFER_SIZE - spp_iostream_context.rx_count;
  if (bytes_consumed > data_size) {
    spp_iostream_on_error(SPP_IOSTREAM_ERROR_RX_OVERFLOW, SL_STATUS_WOULD_OVERFLOW);
    CORE_EXIT_ATOMIC();
    return 0U;
  }

  write_index = (spp_iostream_context.rx_head + spp_iostream_context.rx_count)
                % SPP_IOSTREAM_RX_BUFFER_SIZE;
  for (size_t i = 0; i < bytes_consumed; i++) {
    spp_iostream_context.rx_buffer[write_index] = data[i];
    write_index = (write_index + 1U) % SPP_IOSTREAM_RX_BUFFER_SIZE;
  }
  spp_iostream_context.rx_count += bytes_consumed;
  CORE_EXIT_ATOMIC();

  return bytes_consumed;
}

static void spp_iostream_reset_rx_buffer(spp_iostream_context_t *context)
{
  CORE_DECLARE_IRQ_STATE;

  if (context == NULL) {
    return;
  }

  CORE_ENTER_ATOMIC();
  context->rx_head = 0U;
  context->rx_count = 0U;
  CORE_EXIT_ATOMIC();
}
