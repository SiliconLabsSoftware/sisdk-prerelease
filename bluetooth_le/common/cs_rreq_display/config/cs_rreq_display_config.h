/***************************************************************************//**
 * @file
 * @brief CS RREQ display - configuration header
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
#ifndef CS_RREQ_DISPLAY_CONFIG_H
#define CS_RREQ_DISPLAY_CONFIG_H

/***************************************************************************//**
 * @addtogroup cs_rreq_display
 * @{
 ******************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <q CS_RREQ_DISPLAY_LOG> Enable RREQ display log
// <i> Default: 1
// <i> Enable RREQ display logging
#define CS_RREQ_DISPLAY_LOG              (1)

// <o CS_RREQ_DISPLAY_GLOBAL_ALIGNMENT> Global display alignment
// <CS_RREQ_DISPLAY_ALIGNMENT_LEFT=> Left
// <CS_RREQ_DISPLAY_ALIGNMENT_CENTER=> Centered
// <CS_RREQ_DISPLAY_ALIGNMENT_RIGHT=> Right
// <i> Default: CS_RREQ_DISPLAY_ALIGNMENT_LEFT
#define CS_RREQ_DISPLAY_GLOBAL_ALIGNMENT CS_RREQ_DISPLAY_ALIGNMENT_LEFT

// <<< end of configuration section >>>

/** @} (end addtogroup cs_rreq_display) */
#endif // CS_RREQ_DISPLAY_CONFIG_H
