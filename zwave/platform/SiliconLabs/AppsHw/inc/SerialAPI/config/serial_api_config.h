/**
 * @file
 * Serial API Configuration
 * @copyright 2022 Silicon Laboratories Inc.
 */
#ifndef SERIAL_API_CONFIG_H
#define SERIAL_API_CONFIG_H

#include <em_gpio.h>

// <<< sl:start pin_tool >>>

// <usart signal=TX,RX> SERIAL_API

// $[USART_SERIAL_API]
#ifndef SERIAL_API_PERIPHERAL
#define SERIAL_API_PERIPHERAL                    USART0
#endif
#ifndef SERIAL_API_PERIPHERAL_NO
#define SERIAL_API_PERIPHERAL_NO                 0
#endif

/**
 * TX on PA08 for most boards, PD02 for BRD4204A.
 */
#ifndef SERIAL_API_TX_PORT
#if defined(ZW_BOARD_BRD4204A)
#define SERIAL_API_TX_PORT                       SL_GPIO_PORT_D
#else
#define SERIAL_API_TX_PORT                       SL_GPIO_PORT_A
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(SERIAL_API_TX_PORT) */

#ifndef SERIAL_API_TX_PIN
#if defined(ZW_BOARD_BRD4204A)
#define SERIAL_API_TX_PIN                        2
#else
#define SERIAL_API_TX_PIN                        8
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(SERIAL_API_TX_PIN) */

/**
 * RX on PA09 for most boards, PD03 for BRD4204A.
 */
#ifndef SERIAL_API_RX_PORT
#if defined(ZW_BOARD_BRD4204A)
#define SERIAL_API_RX_PORT                       SL_GPIO_PORT_D
#else
#define SERIAL_API_RX_PORT                       SL_GPIO_PORT_A
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(SERIAL_API_RX_PORT) */

#ifndef SERIAL_API_RX_PIN
#if defined(ZW_BOARD_BRD4204A)
#define SERIAL_API_RX_PIN                        3
#else
#define SERIAL_API_RX_PIN                        9
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(SERIAL_API_RX_PIN) */

// [USART_SERIAL_API]$

// <<< sl:end pin_tool >>>

#endif // SERIAL_API_CONFIG_H
