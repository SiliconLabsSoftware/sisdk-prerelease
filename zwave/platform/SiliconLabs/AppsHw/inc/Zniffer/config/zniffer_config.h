/**
 * @file
 * Zniffer Configuration
 * @copyright 2022 Silicon Laboratories Inc.
 */
#ifndef ZNIFFER_CONFIG_H
#define ZNIFFER_CONFIG_H

#include <em_gpio.h>

// <<< sl:start pin_tool >>>

// <usart signal=TX,RX> ZNIFFER

// $[USART_ZNIFFER]
#ifndef ZNIFFER_PERIPHERAL
#define ZNIFFER_PERIPHERAL                       USART0
#endif
#ifndef ZNIFFER_PERIPHERAL_NO
#define ZNIFFER_PERIPHERAL_NO                    0
#endif

/**
 * TX on PA08 for most boards, PD02 for BRD4204A.
 */
#ifndef ZNIFFER_TX_PORT
#if defined(ZW_BOARD_BRD4204A)
#define ZNIFFER_TX_PORT                          SL_GPIO_PORT_D
#else
#define ZNIFFER_TX_PORT                          SL_GPIO_PORT_A
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(ZNIFFER_TX_PORT) */

#ifndef ZNIFFER_TX_PIN
#if defined(ZW_BOARD_BRD4204A)
#define ZNIFFER_TX_PIN                           2
#else
#define ZNIFFER_TX_PIN                           8
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(ZNIFFER_TX_PIN) */

/**
 * RX on PA09 for most boards, PD03 for BRD4204A.
 */
#ifndef ZNIFFER_RX_PORT
#if defined(ZW_BOARD_BRD4204A)
#define ZNIFFER_RX_PORT                          SL_GPIO_PORT_D
#else
#define ZNIFFER_RX_PORT                          SL_GPIO_PORT_A
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(ZNIFFER_RX_PORT) */

#ifndef ZNIFFER_RX_PIN
#if defined(ZW_BOARD_BRD4204A)
#define ZNIFFER_RX_PIN                           3
#else
#define ZNIFFER_RX_PIN                           9
#endif /* !defined(ZW_BOARD_BRD4204A) */
#endif /* !defined(ZNIFFER_RX_PIN) */

// [USART_ZNIFFER]$

// <<< sl:end pin_tool >>>

#endif // ZNIFFER_CONFIG_H
