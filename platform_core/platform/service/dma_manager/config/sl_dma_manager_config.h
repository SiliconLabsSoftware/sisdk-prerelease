/***************************************************************************//**
 * @file
 * @brief DMA Manager - configuration file.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_DMA_MANAGER_CONFIG_H
#define SL_DMA_MANAGER_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <q SL_DMA_ENABLE_ROBIN_CHANNELS> Enable support for round-robin channel scheduling in DMA engine.
// <i> Enables support for round-robin scheduling in DMA engine. Number of channel in round-robin will have to be specified at run time using sl_dma_manager_set_nbr_round_robin_channels().
// <d> 0
#define SL_DMA_ENABLE_ROBIN_CHANNELS 0

#endif /* SL_DMA_MANAGER_CONFIG_H */

// <<< end of configuration section >>>
