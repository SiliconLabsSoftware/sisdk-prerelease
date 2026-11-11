/***************************************************************************//**
 * @file sl_mac_mux_aux.h
 * @brief Public mac mux aux helpers for the mac mux auxiliary context.
 *
 * Provides helper to access mac mux aux stack API
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_ZIGBEE_MAC_MUX_AUX_RAW_H
#define SL_ZIGBEE_MAC_MUX_AUX_RAW_H

#include "stack/include/sl_zigbee_types.h"

/** @brief Send the given message over the air without higher layer
 * network headers.
 *
 * @param message        The message to transmit.
 * @param mesage_length  The message length to transmit.
 * @param priority       Transmit priority
 * @param useCca         Flag should the CCA stay on or not
 *
 * @return ::SL_STATUS_OK if the message was successfully submitted to
 * the transmit queue, and ::SL_STATUS_FAIL otherwise.
 * @internal SL_ZIGBEE_IPC_ARGS
 * {# message | length: message_length | max: MAX_IPC_VEC_ARG_CAPACITY #}
 */
sl_status_t sl_zigbee_mac_mux_aux_send_raw_message(const uint8_t *message, uint8_t message_length, sl_zigbee_transmit_priority_t priority, bool useCca);

/** @brief Configure Inter-PAN transmission and reception on AUX or primary channel
 *
 * When @p enable is true, Inter-PAN messages are sent and received on the AUX
 * channel, and Inter-PAN messages on the primary channel are dropped. When @p
 * enable is false, Inter-PAN messages are sent and received on the primary
 * channel, and AUX-channel Inter-PAN traffic is disabled.
 *
 * @param enable        Configure Inter-PAN traffic to use the AUX channel when true,
 *                      or the primary channel when false.
 */
void sl_zigbee_mac_mux_aux_set_interpan_enabled(bool enable);

/** @brief Check whether Inter-PAN traffic is configured to use the AUX channel
 *
 * @return ::true if Inter-PAN messages are configured to be sent and received
 * on the AUX channel, and ::false otherwise.
 */
bool sl_zigbee_mac_mux_aux_is_interpan_enabled(void);

/** @brief Get AUX channel
 *
 * @return aux channel
 */
uint8_t sl_zigbee_mac_mux_aux_get_channel(void);
#endif // SL_ZIGBEE_MAC_MUX_AUX_RAW_H

