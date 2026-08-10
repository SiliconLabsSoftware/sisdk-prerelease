/***************************************************************************//**
 * @file
 * @brief Communication interface for DDP
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

#include <stdlib.h>
#include <string.h>
#include "sl_component_catalog.h"
#include "sl_board_control.h"
#include "sl_iostream.h"
#include "sl_ddp_common.h"
#include "sl_ddp_com.h"

// -----------------------------------------------------------------------------
// Definitions

#if defined(SL_CATALOG_SEGGER_RTT_PRESENT)
#include "SEGGER_RTT_Conf.h"
#define SL_DDP_COM_INPUT_BUF_SIZE   (BUFFER_SIZE_DOWN)
#define SL_DDP_COM_OUTPUT_BUF_SIZE  (BUFFER_SIZE_UP)
#else // SL_CATALOG_SEGGER_RTT_PRESENT
#define SL_DDP_COM_INPUT_BUF_SIZE   (1200)
#define SL_DDP_COM_OUTPUT_BUF_SIZE  (1200)
#endif // SL_CATALOG_SEGGER_RTT_PRESENT

// -----------------------------------------------------------------------------
// Private variables

// IO Stream handle
static sl_iostream_t *iostream_handle = NULL;
// Output buffer
static uint8_t output_buf[SL_DDP_COM_OUTPUT_BUF_SIZE];
// Input buffer
static uint8_t input_buf[SL_DDP_COM_INPUT_BUF_SIZE];
// Input buffer index
static uint16_t input_buf_idx;

// -----------------------------------------------------------------------------
// Public functions

/******************************************************************************
 * Initialize DPP COM interface.
 *****************************************************************************/
void sl_ddp_com_init(void)
{
  sl_board_enable_vcom();
  iostream_handle = sl_iostream_get_default();
  input_buf_idx = 0u;
}

/******************************************************************************
 * Process DPP COM interface.
 *****************************************************************************/
void sl_ddp_com_process_action(void)
{
  size_t input_len = 0;
  sl_status_t status;
  sl_ddp_com_req_t *req;
  sl_ddp_com_rsp_t *rsp;
  uint16_t output_len = 0;

  status = sl_iostream_read(iostream_handle,
                            &input_buf[input_buf_idx],
                            SL_DDP_COM_INPUT_BUF_SIZE - input_buf_idx,
                            &input_len);
  if (status == SL_STATUS_OK) {
    // Process the received input data, if any
    input_buf_idx += input_len;
    if (input_buf_idx >= sizeof(sl_ddp_com_req_t)) {
      // There is enough data for the request header
      req = (sl_ddp_com_req_t *)input_buf;
      input_len = sizeof(*req) + req->data_len;
      if (input_buf_idx >= input_len) {
        // There is enough data for the request body
        rsp = (sl_ddp_com_rsp_t *)output_buf;
        // Handle the request
        rsp->status = (uint32_t)sl_ddp_handle_command(req->id,
                                                      req->data,
                                                      req->data_len,
                                                      rsp->data,
                                                      sizeof(output_buf) - sizeof(sl_ddp_com_rsp_t),
                                                      &output_len);
        // Update the response length
        rsp->data_len = output_len;
        // Write the response header
        sl_iostream_write(iostream_handle,
                          rsp,
                          sizeof(*rsp));
        // Write the response body if any
        if ((rsp->status == 0) && output_len) {
          sl_iostream_write(iostream_handle,
                            rsp->data,
                            output_len);
        }
        // Move the remaining input data to the start of the buffer if any
        memmove(input_buf, &input_buf[input_len], input_buf_idx - input_len);
        // Amount of remaining input data
        input_buf_idx -= input_len;
      }
    }
  }
}
