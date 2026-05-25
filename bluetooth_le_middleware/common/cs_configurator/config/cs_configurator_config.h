/***************************************************************************//**
 * @file
 * @brief CS Configurator defaults
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

 #ifndef CS_CONFIGURATOR_CONFIG_H
 #define CS_CONFIGURATOR_CONFIG_H
 
 // <<< Use Configuration Wizard in Context Menu >>>
 
 // <h> Default configuration

// <o CS_CONFIGURATOR_CONFIG_DEFAULT_PROCEDURE_SCHEDULING> Procedure scheduling
// <CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY=> Optimized for frequency update
// <CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_ENERGY=> Optimized for energy consumption
// <CS_PROCEDURE_SCHEDULING_CUSTOM=> Use custom procedure scheduling
// <i> Set your custom connection and procedure intervals in the 'Custom Scheduling' section when selecting 'Use custom procedure scheduling' (CS_PROCEDURE_SCHEDULING_CUSTOM).
// <i> Default: CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY
#define CS_CONFIGURATOR_CONFIG_DEFAULT_PROCEDURE_SCHEDULING   CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY


// <o CS_CONFIGURATOR_CONFIG_DEFAULT_CHANNEL_MAP_PRESET> Channel map preset
// <CS_CHANNEL_MAP_PRESET_MEDIUM=> Medium
// <CS_CHANNEL_MAP_PRESET_HIGH=> High
// <CS_CHANNEL_MAP_PRESET_CUSTOM=> Custom
// <i> Default: CS_CHANNEL_MAP_PRESET_HIGH
#define CS_CONFIGURATOR_CONFIG_DEFAULT_CHANNEL_MAP_PRESET     CS_CHANNEL_MAP_PRESET_HIGH
 // </h>
 
 // <<< end of configuration section >>>
 
 #endif // CS_CONFIGURATOR_CONFIG_H
 