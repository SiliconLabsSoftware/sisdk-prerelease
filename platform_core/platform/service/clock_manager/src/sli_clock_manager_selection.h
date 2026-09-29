/***************************************************************************//**
 * @file
 * @brief Clock Manager Runtime Selection.
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

#ifndef SLI_CLOCK_MANAGER_SELECTION_H
#define SLI_CLOCK_MANAGER_SELECTION_H

#include "em_device.h"

#ifdef __cplusplus
extern "C" {
#endif


#define SLI_CLOCK_MANAGER_RUNTIME_UNRESTRICTED

/*******************************************************************************
 *************************   OSCILLATORS   *************************************
 ******************************************************************************/

#define SLI_CLOCK_MANAGER_RUNTIME_HFXO0
#define SLI_CLOCK_MANAGER_RUNTIME_LFXO
#define SLI_CLOCK_MANAGER_RUNTIME_HFRCO0
#define SLI_CLOCK_MANAGER_RUNTIME_HFRCOEM23
#define SLI_CLOCK_MANAGER_RUNTIME_LFRCO

/*******************************************************************************
 *************************   CMU FUNCTIONS   ***********************************
 ******************************************************************************/

#define SLI_CLOCK_MANAGER_RUNTIME_SYSCLK
#define SLI_CLOCK_MANAGER_RUNTIME_CAL

/*******************************************************************************
 *************************   CLOCK CONTROL   ***********************************
 ******************************************************************************/

#define SLI_CLOCK_MANAGER_RUNTIME_EXPORTCLK

#ifdef __cplusplus
}
#endif

#endif /* SLI_CLOCK_MANAGER_SELECTION_H */
