/***************************************************************************//**
 * @file
 * @brief APIs and defines for the Critical Message Queue plugin.
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

#ifndef SILABS_CRITICAL_MESSAGE_QUEUE_H
#include "app/framework/include/af-types.h"

/**
 * @defgroup critical-message-queue Critical Message Queue
 * @ingroup component
 * @brief API and Callbacks for the Critical Message Queue Component
 *
 * Silicon Labs implementation of a critical message queue.
 *
 */

/**
 * @addtogroup critical-message-queue
 * @{
 */

/**
 * @name API
 * @{
 */

/** @brief Print the critical message queue.
 *
 */
void sl_zigbee_af_critical_message_queue_print_queue(void);

/** @brief Print critical message queue configuration.
 *
 */
void sl_zigbee_af_critical_message_queue_print_config(void);

/** @brief Initialize critical message queue.
 *
 */
void sl_zigbee_af_critical_message_queue_init(void);

/** @} */ // end of name APIs

/**
 * @name Callbacks
 * @{
 */

/**
 * @defgroup critical_msg_queue_cb Critical Message Queue
 * @ingroup af_callback
 * @brief Callbacks for Critical Message Queue Component
 *
 */

/**
 * @addtogroup critical_msg_queue_cb
 * @{
 */

/** @brief To send a message using the critical message queue functionality,
 *  pass this callback to any of the sl_zigbee_af_send_unicast_with_cb functions.
 *
 */
extern const sl_zigbee_af_message_sent_function_t sl_zigbee_af_critical_message_queue_enqueue_cb;
/** @} */ // end of critical_msg_queue_cb
/** @} */ // end of name Callbacks
/** @} */ // end of critical-message-queue

/** @brief The retry callback is exposed for af-main-common.c; it is not to be used by
 *  a typical application
 */
extern const sl_zigbee_af_message_sent_function_t sli_zigbee_af_critical_message_queue_retry_callback;

#endif
