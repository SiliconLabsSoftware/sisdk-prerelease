/***************************************************************************//**
 * @file
 * @brief SL Diagnostic reason IDs
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

#ifndef SL_DIAG_REASON_IDS_H
#define SL_DIAG_REASON_IDS_H

/***************************************************************************//**
 * @addtogroup sl_diag
 * @{
 *
 * @note Reason IDs identify a Summary Ring breadcrumb and the reason_id of a
 *       capture request. Each range has one owner:
 *       - 0x0100 to 0x01FF: boot and reset.
 *       - 0x0200 to 0x02FF: IPC.
 *       - 0x0300 to 0x03FF: NWP.
 *       - 0x0400 to 0x04FF: host interface.
 *       - 0x0500 to 0x05FF: clock, sleep and wake.
 *       - 0x0600 to 0x06FF: heap.
 *       - 0x0700 to 0x07FF: socket, TLS and crypto.
 *       - 0x0800 to 0x08FF: watchdog.
 *       - 0x0900 to 0x0FFF: reserved for SL Diagnostic.
 *       - 0x1000 to 0xFFFF: application, from SL_DIAG_REASON_APP_BASE.
 *       0x0000 and values above 0xFFFF are not assigned. An assigned value
 *       and the meaning of its detail are never reused or changed, so the
 *       host decoder can name entries of any firmware version. The registry
 *       governs assignment only: reason IDs are 32-bit and are not checked
 *       at runtime, so an unassigned value is recorded as given.
 ******************************************************************************/

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/// Reset that started this boot. detail is the primary reset reason.
#define SL_DIAG_REASON_BOOT_RESET_REASON      0x0101u

/// Reset that started this boot. detail is the extended reset reason.
#define SL_DIAG_REASON_BOOT_RESET_REASON_EXT  0x0102u

/// IPC command sent. detail is the command ID.
#define SL_DIAG_REASON_IPC_TX                 0x0201u

/// IPC response received. detail is the response ID.
#define SL_DIAG_REASON_IPC_RX                 0x0202u

/// NWP heartbeat missed. detail is the last heartbeat sequence number.
#define SL_DIAG_REASON_NWP_HEARTBEAT_MISS     0x0310u

/// Heap low watermark. detail is the free heap size in bytes.
#define SL_DIAG_REASON_HEAP_WATERMARK         0x0601u

/// Watchdog warning. detail is the watchdog instance.
#define SL_DIAG_REASON_WDOG_WARNING           0x0801u

/// First application-defined reason ID. The application owns the values up
/// to 0xFFFF and the meaning of their detail.
#define SL_DIAG_REASON_APP_BASE               0x1000u

/** @} (end addtogroup sl_diag) */

#endif // SL_DIAG_REASON_IDS_H
