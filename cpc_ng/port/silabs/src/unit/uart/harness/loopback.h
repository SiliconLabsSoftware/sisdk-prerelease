/***************************************************************************/ /**
 * @file
 * @brief Loopback injection for CPC driver unit tests.
 ******************************************************************************/

#ifndef CPC_DRV_UART_TEST_LOOPBACK_H
#define CPC_DRV_UART_TEST_LOOPBACK_H

#include <stddef.h>
#include <stdint.h>

#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

sl_status_t loopback_bind(void);
void loopback_inject_bytes_at(unsigned int lineno, const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
