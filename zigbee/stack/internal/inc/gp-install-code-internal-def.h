/***************************************************************************//**
 * @file gp-install-code-internal-def.h
 * @brief internal names for 'gp-install-code' declarations
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
// automatically generated from gp-install-code.h.  Do not manually edit
#ifndef GP_INSTALL_CODE_INTERNAL_DEF_H
#define GP_INSTALL_CODE_INTERNAL_DEF_H

#include "stack/include/gp-install-code.h"

// Command Indirection

sl_status_t sli_zigbee_stack_gp_derive_key_from_install_code(const uint8_t *ic,
                                                             sl_zigbee_key_data_t *derivedKey);

sl_status_t sli_zigbee_stack_gp_encode_gpd_device_address(sl_zigbee_gp_application_id_t appId,
                                                          uint32_t srcId,
                                                          sl_802154_long_addr_t ieee,
                                                          sl_802154_long_addr_t deviceAddressOut);

sl_status_t sli_zigbee_stack_gp_lookup_ic_derived_key(const sl_zigbee_gp_address_t *gpdAddr,
                                                      sl_zigbee_key_data_t *keyOut);

sl_status_t sli_zigbee_stack_gp_tc_register_install_code(const sl_zigbee_gp_address_t *gpdAddr,
                                                         const uint8_t *ic);

sl_status_t sli_zigbee_stack_gp_tc_remove_install_code(const sl_zigbee_gp_address_t *gpdAddr);

sl_status_t sli_zigbee_stack_gp_validate_install_code_crc(const uint8_t *ic);

#endif // GP_INSTALL_CODE_INTERNAL_DEF_H
