#ifndef SL_BTCTRL_HCI_CPC_RTOS_H
#define SL_BTCTRL_HCI_CPC_RTOS_H

#include "sl_common.h"
#include "sl_status.h"

/**
 * Initialize RTOS task for HCI Greybus CPC Reader
 */
sl_status_t sl_btctrl_hci_cpc_rtos_init(void);

/**
 * Get the available stack space of the HCI CPC thread as determined
 * by the stack watermark recorded during execution.
 */
uint8_t sl_btctrl_hci_cpc_get_stack_space(uint32_t *stack_space);

#endif
