/***************************************************************************//**
 * @file
 * @brief UART echo example functions
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

#ifndef APP_UART_ECHO_H
#define APP_UART_ECHO_H

/***************************************************************************//**
 * Initialize the UART echo example.
 ******************************************************************************/
void app_uart_echo_init(void);

/***************************************************************************//**
 * UART echo ticking function.
 *
 * Echo is handled by a FreeRTOS task; this function is a no-op.
 ******************************************************************************/
void app_uart_echo_process_action(void);

#endif  // APP_UART_ECHO_H
