/***************************************************************************//**
 * @brief RAIL mux aux application configuration.
 *
 * This header is copied into the application config directory by SLC. Values
 * here are compiled with the rail_mux_aux sources, not with the prebuilt
 * Zigbee stack library.
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

#ifndef SL_MAC_MUX_AUX_CONFIG_H
#define SL_MAC_MUX_AUX_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h>RAIL mux aux configuration

// <q SL_MAC_MUX_AUX_DEBUG> Enable mux-aux debug prints
// <i> Default: 0
// <i> When enabled, the application-side mux-aux debug hooks print dual-channel
// <i> join/TX/RX breadcrumbs. The prebuilt stack always calls the hooks; with
// <i> this option disabled the hook bodies do nothing.
#ifndef SL_MAC_MUX_AUX_DEBUG
#define SL_MAC_MUX_AUX_DEBUG  0
#endif

// </h>

// <<< end of configuration section >>>

#endif // SL_MAC_MUX_AUX_CONFIG_H
