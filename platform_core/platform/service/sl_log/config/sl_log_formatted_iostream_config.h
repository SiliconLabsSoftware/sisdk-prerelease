/***************************************************************************/ /**
* @file sl_log_formatted_iostream_config.h
* @brief Formatted I/O stream log backend output options
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

// <<< Use Configuration Wizard in Context Menu >>>

#ifndef SL_LOG_FORMATTED_IOSTREAM_CONFIG_H
#define SL_LOG_FORMATTED_IOSTREAM_CONFIG_H

// <q SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP> Formatted iostream: prefix timestamp
// <i> When set, each line begins with an 8-digit hexadecimal timestamp inside the leading bracket.
// <i> May be combined with SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE ([TIMESTAMP|TYPE] when both set).
// <i> Default: 0
#define SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP 0

// <q SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE> Formatted iostream: prefix log type
// <i> When set, each line prefix includes S (string log) or E (event log) inside the leading bracket.
// <i> May be combined with SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP ([TIMESTAMP|TYPE] when both set).
// <i> Default: 0
#define SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE 0

// <q SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID> Formatted iostream: append core ID
// <i> When set, each line ends with a trailing " [CC]" suffix (2 hex digits) that
// <i> identifies the captive core that produced the log. Applies to both string
// <i> and event lines. When cleared, the core ID is not emitted.
// <i> Default: 0
#define SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID 0

#endif /* SL_LOG_FORMATTED_IOSTREAM_CONFIG_H */

// <<< end of configuration section >>>
