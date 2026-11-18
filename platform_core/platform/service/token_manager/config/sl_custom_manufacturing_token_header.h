/***************************************************************************//**
 * @file sl_custom_manufacturing_token_header.h
 * @brief Custom manufacturing token header
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

/***************************************************************************//**
 * Custom Manufacturing Token Usage Examples
 *
 * Refer sl_token_manager_manufacturing.h file on how to create a token
 *
 * On Series 2, and for a Phase-1 Series 3 key, TOKEN_MFG_* is ORed with either
 * SL_TOKEN_STATIC_DEVICE_TOKENS or SL_TOKEN_STATIC_SECURE_DATA_TOKENS.
 * A new Series 3 key is the raw 16-bit id from the 16-bit map in
 * sl_token_manager_defines.h. Do not OR either type-nibble macro into that key.
 * Static device versus static secure is selected by SL_TOKEN_GET_STATIC_DEVICE_TOKEN()
 * or SL_TOKEN_GET_STATIC_SECURE_TOKEN().
 *
 * REMEMBER: For series 2 devices, by definition, manufacturing tokens exist at fixed addresses.
 *           Tokens should not overlap.
 *           However for series 3 devices manufacturing tokens are stored as KLV object in a KLV chain
 *           and are not located at a fixed location, the TOKEN_MFG_EXAMPLE (created below as an example)
 *           is merely treated as a token key value.
 *
 * Here is a basic example of a manufacturing token header file:
 *
 * Series 2 Example:
 * Note that the address used here is just an example.
 * To review addresses of existing tokens when choosing a new address, refer
 * to the file
 * platform/service/token_manager/inc/sl_token_manager_manufacturing.h
 *
 * @code
 * #define TOKEN_MFG_EXAMPLE            (SL_TOKEN_STATIC_DEVICE_TOKENS | 0x1f0)
 * #define TOKEN_MFG_EXAMPLE_SIZE       (8u) //8 bytes
 *
 * #define TOKEN_MFG_EXAMPLE_1          (SL_TOKEN_STATIC_SECURE_TOKENS | 0x100)
 * #define TOKEN_MFG_EXAMPLE_1_SIZE     (4u) //4 bytes
 * @endcode
 *
 *
 * Series 3 Example:
 * Note that the token key value used here is just an example.
 * To review the key value of an existing tokens when choosing a new token key, refer
 * to the file
 * platform/service/token_manager/inc/sl_token_manager_defines.h, for token id
 * range information. The keys use a free 16-bit id.
 *
 *
 * @code
 *
 * // New token id range from Series 3 onwards. User window starts at 0x6000.
 * // To create a SST
 * #define SST_MFG_EXAMPLE_1          (0x6000)
 * #define SST_MFG_EXAMPLE_1_SIZE     (4u) //4 bytes
 * #define TOKEN_SST_MFG_EXAMPLE_1    (SL_TOKEN_GET_STATIC_SECURE_TOKEN(SST_MFG_EXAMPLE_1))
 * // TO create a SDT
 * #define SDT_MFG_EXAMPLE_1          (0x6000)
 * #define SDT_MFG_EXAMPLE_1_SIZE     (4u) //4 bytes
 * #define TOKEN_SDT_MFG_EXAMPLE_1    (SL_TOKEN_GET_STATIC_DEVICE_TOKEN(SDT_MFG_EXAMPLE_1))
 * @endcode
 * To use this example:
 *   Ensure the Common Token Manager's configuration in Studio
 *   has enabled Custom Manufacturing Tokens.
 *
 *   Ensure your application has:
 *   #include "sl_token_manager_api.h"
 *
 *   This code will read the token data:
 *   Refer sl_token_manager_api.h file, for sl_token_manager_get_data() API usage
 *     Static device token read API example:
 *     uint8_t data[TOKEN_MFG_EXAMPLE_SIZE];
 *     uint32_t size;
 *     sl_token_manager_get_data(SL_TOKEN_GET_STATIC_DEVICE_TOKEN(TOKEN_MFG_EXAMPLE),
 *                               &data, sizeof(data));
 *
 *     Static secure token read API example:
 *     uint32_t data1;
 *     sl_token_manager_get_data(SL_TOKEN_GET_STATIC_SECURE_TOKEN(TOKEN_MFG_EXAMPLE_1),
 *                               &data1, sizeof(data1));
 * *****************************************************************************
 * For backward compatibility, the legacy token header file is still available, for
 * silicon labs series 2 devices.
 *
 * Below is custom Manufacturing Token Usage Examples
 *
 * The macro DEFINE_MFG_TOKEN() should be used when instantiating a
 * manufacturing token.  Refer to the list of *_LOCATION defines to
 * see what memory is allocated and what memory is unused/available.
 *
 * The _LOCATION is or'ed with either USERDATA_TOKENS or LOCKBITSDATA_TOKENS
 * to control which segment of memory the token is placed in.
 *
 * REMEMBER: By definition, manufacturing tokens exist at fixed addresses.
 *           Tokens should not overlap.
 *
 * Here is a basic example of a manufacturing token header file:
 *
 * Note that the address used here is just an example.  It places the 8 bytes
 * in the middle of the USERDATA space on an EFR32MG12P433F1024GM68.
 * To review addresses of existing tokens when choosing a new address, refer
 * to an961-custom-nodes-efr32.pdf and the files
 * platform/service/token_manager/legacy/inc/sl_token_manufacturing_series_2.h
 *
 * @code
 * #define CREATOR_MFG_EXAMPLE 0x4242
 * #ifdef DEFINETYPES
 * typedef uint8_t tokTypeMfgExample[8];
 * #endif
 * #ifdef DEFINETOKENS
 * #define MFG_EXAMPLE_LOCATION (USERDATA_TOKENS | 0x2000)
 * DEFINE_MFG_TOKEN(MFG_EXAMPLE,
 *                  tokTypeMfgExample,
 *                  MFG_EXAMPLE_LOCATION,
 *                  {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF})
 * #endif
 * @endcode
 *
 * Since this file contains both the typedefs and the token defs, there are
 * two \#defines used to select which one is needed when this file is included.
 * \#define DEFINETYPES is used to select the type definitions and
 * \#define DEFINETOKENS is used to select the token definitions.
 *
 * To use this example:
 *   Ensure the Token Manager's configuration in Studio
 *   has enabled Custom Manufacturing Tokens.
 *
 *   Ensure your application has:
 *     #include "sl_token_api.h"
 *     #include "sl_token_manager.h"
 *
 *   This code will read the token data:
 *     uint8_t data[8];
 *     sl_token_get_data(TOKEN_MFG_EXAMPLE, 0, &data, sizeof(data));
 *
 ******************************************************************************/

/*
 #define TOKEN_MFG_EXAMPLE            (SL_TOKEN_STATIC_DEVICE_TOKENS | 0x1f0)
 #define TOKEN_MFG_EXAMPLE_SIZE       (8u) //8 bytes
 */

/*
   The old format for creating a token for silicon labs series 2 devices.
   This format is deprecated and will be removed in the future.

 #define CREATOR_MFG_EXAMPLE 0x4242
 #ifdef DEFINETYPES
   typedef uint8_t tokTypeMfgExample[8];
 #endif
 #ifdef DEFINETOKENS
 #define MFG_EXAMPLE_LOCATION (USERDATA_TOKENS | 0x2000)
   DEFINE_MFG_TOKEN(MFG_EXAMPLE,
                tokTypeMfgExample,
                MFG_EXAMPLE_LOCATION,
                {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF})
 #endif
 */
