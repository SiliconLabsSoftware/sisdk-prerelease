/***************************************************************************//**
 * @file
 * @brief SL Diagnostic crash capture example functions
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

#ifndef DIAG_CRASH_CAPTURE_APP_H
#define DIAG_CRASH_CAPTURE_APP_H

/***************************************************************************//**
 * Initialize the SL Diagnostic crash capture example.
 ******************************************************************************/
void diag_crash_capture_app_init(void);

/***************************************************************************//**
 * SL Diagnostic crash capture example ticking function.
 ******************************************************************************/
void diag_crash_capture_app_process_action(void);

#endif // DIAG_CRASH_CAPTURE_APP_H
