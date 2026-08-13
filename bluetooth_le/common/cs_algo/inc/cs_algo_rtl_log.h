/***************************************************************************//**
 * @file
 * @brief CS Algo - RTL library logging
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

#ifndef CS_ALGO_RTL_LOG_H
#define CS_ALGO_RTL_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Initialize the RTL library structured logging globally.
 *
 * Sets up the RTL library log session and routes the log output to the
 * BGAPI trace channel. The output path is serialized by the rtl_log_mutex
 * component, so this is safe to use in both bare-metal and RTOS builds.
 *
 * Must be called before any estimator that has RTL logging enabled is
 * created (see @ref cs_algo_config_t::rtl_config). Has no effect when
 * RTL logging is disabled at compile time (@ref CS_ALGO_CONFIG_RTL_LOG).
 *****************************************************************************/
void cs_algo_rtl_log_init(void);

/******************************************************************************
 * Deinitialize the RTL library structured logging globally.
 *
 * Tears down the RTL library log session. Has no effect when RTL logging
 * is disabled at compile time (@ref CS_ALGO_CONFIG_RTL_LOG).
 *****************************************************************************/
void cs_algo_rtl_log_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // CS_ALGO_RTL_LOG_H
