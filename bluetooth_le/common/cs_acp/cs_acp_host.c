/***************************************************************************//**
 * @file
 * @brief CS Application Co-Processor host - core implementation
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

#include "sl_bt_api.h"
#include "cs_acp_host.h"

// Sends the CS ACP target a command.
sl_status_t cs_acp_send_cmd(size_t data_len,
                            const uint8_t *data,
                            size_t max_response_size,
                            size_t *response_len,
                            uint8_t *response)
{
  return sl_bt_user_cs_service_message_to_target(data_len,
                                                 data,
                                                 max_response_size,
                                                 response_len,
                                                 response);
}
