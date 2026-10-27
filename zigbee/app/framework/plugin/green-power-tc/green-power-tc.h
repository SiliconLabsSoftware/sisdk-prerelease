/***************************************************************************//**
 * @file
 * @brief GP-aware Trust Center plugin for GP v1.1.3.
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

#ifndef GREEN_POWER_TC_H
#define GREEN_POWER_TC_H

#include "green-power-v113.h"

/**
 * @brief GP-aware Trust Center for Green Power Basic v1.1.3.
 *
 * SoC limitation: GP TC Request/Response/Pairing Completion/Decommissioning
 * are handled on the SoC application processor. Host/NCP (EZSP) passthrough
 * for these commands is not provided in the initial release — use a SoC
 * combo/TC image, or keep InvolveTC commissioning on the NCP SoC side.
 * Document this constraint in product release notes when shipping host+NCP
 * GP combos.
 */

sl_status_t sli_zigbee_gp_tc_evaluate_request(const sli_zigbee_gp_tc_policy_t *policy,
                                               sli_zigbee_gp_tc_request_context_t *req,
                                               sli_zigbee_gp_tc_response_t *resp);

void sli_zigbee_gp_tc_load_policy(sli_zigbee_gp_tc_policy_t *policy);

#endif // GREEN_POWER_TC_H
