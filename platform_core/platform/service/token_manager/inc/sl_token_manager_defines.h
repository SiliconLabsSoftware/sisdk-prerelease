/***************************************************************************//**
 * @file sl_token_manager_defines.h
 * @brief Header file for generic token manager defines
 * @version 1.0.0
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories, Inc, www.silabs.com</b>
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
#ifndef SL_TOKEN_MANAGER_DEFINES_H
#define SL_TOKEN_MANAGER_DEFINES_H

#include <stdint.h>

/***************************************************************************//**
 * @addtogroup common_token_manager Common Token Manager (CTM)
 * @{
 ******************************************************************************/
/***************************************************************************//**
 * @addtogroup defines Common Token Manager Defines
 * @brief This module provides defines for the Common Token Manager (CTM).
 * @{
 * @details
 * The defines in this module are used for creating 32-bit token identifier
 * to identify static device tokens, static secure tokens and dynamic tokens in the CTM.
 *
 * @note For more information on tokens and CTM refer to silabs application note
 * @ref 'AN1154: Using Tokens for Non-Volatile Data Storage'
 ******************************************************************************/
/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */
// MACRO's for mask values to identify token as
// NVM3 or STATIC DEVICE or STATIC SECURE
#define SL_TOKEN_DYNAMIC_MASK                     (0xF8000000)
#define SL_TOKEN_STATIC_MASK                      (0xE0000000)

/*
 * The new 32-bit token identifier used for creating the key-length-value chain.
 * To differentiate between different objects we utilize the
 * upper 5 bits of the key identifier:
 *
 * bit (31:27) Maps to:
 *
 * 0b00000 Dynamic token (Default instance)
 * 0b00001 Dynamic token (Secondary instance - Zwave usage)
 * 0b0001x Reserved for future use
 * 0b110xx Static device tokens
 * 0b111xx Static secure tokens
 * 0b100xx Reserved for future use
 * 0b101xx Upgradable tokens
 *
 * The Key-Length-Value system works as a chain of KLV objects.
 * A KLV object is identified in the following way:
 *
 * 31:29 | 28:16    | 15:0
 * 0b1xx | Reserved | Key
 *
 * */
// For backwards compatibility only. Don't use these macros for new code.
#define SL_TOKEN_TYPE_NVM3                        SL_TOKEN_TYPE_DYNAMIC
#define SL_TOKEN_TYPE_NVM3_SECONDARY              SL_TOKEN_TYPE_DYNAMIC_SECONDARY
// Use below macros
#define SL_TOKEN_TYPE_DYNAMIC                     (0x00000000U)
#define SL_TOKEN_TYPE_DYNAMIC_SECONDARY           (0x08000000U)
#define SL_TOKEN_TYPE_STATIC_DEVICE               (0xC0000000U)
#define SL_TOKEN_TYPE_STATIC_SECURE               (0xE0000000U)
#define SL_TOKEN_TYPE_UPGRADABLE                  (0xA0000000U)

/** @endcond */

/**
 * @defgroup NVM3 NVM3 key space regions
 * @brief MACRO's for NVM3 Default Instance Key Space Regions.
 * The NVM3 default instance is used for storing dynamic tokens.
 *
 * NVM3 is used for Non-Volatile storage across all our device types (zigbee, Bluetooth, MCU...).
 * Each object in the NVM3 is addressed by a 20-bit key. These keys need to be unique within an NVM3 instance
 * to avoid collisions. Such collisions can for instance occur if a user application writes to an object
 * that the RF stack is using for a different purpose.
 * For multi-protocol applications we also risk collisions when where two stacks inadvertently use the
 * same object key in a non compatible way. By default an NVM3 default instance will be shared among
 * all stacks and application code.
 * To avoid collisions it is proposed to use the 4 MSBs of the default instance NVM3 key to split
 * the NVM3 ID space into the regions in the table below. Each stack team is responsible for
 * managing the keys within their regions. A user should be able to use the keys in the User region as they wish.
 * If any configurator tools use any keys in the User region these keys should be made clearly
 * visible to the user so they can avoid using the same keys for NVM3 objects created manually in
 * the application code. Refer application note 'AN1135: Using Third Generation Non-Volatile Memory (NVM3) Data Storage' for more details.
 *
 * @note The key regions from 0xC0000 to 0xDFFFF are reserved.
 * @note Key regions 0xE0000-0xEFFFF and 0xF0000-0xFFFFF are reserved for
 *       Series 3(onwards) 16-bit static override tokens. 0xExxxx holds the full 16-bit
 *       static device key. 0xFxxxx holds the full 16-bit static secure key.
 *       These ranges do not overlap the Phase-1 override ranges 0x8E000-0x8FFFF.
 */

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for User.
 */
#define SL_TOKEN_NVM3_REGION_USER                 (0x00000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Zigbee EmberZnet Stack.
 */
#define SL_TOKEN_NVM3_REGION_ZIGBEE               (0x10000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for OpenThread Stack.
 */
#define SL_TOKEN_NVM3_REGION_THREAD               (0x20000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Connect Stack.
 */
#define SL_TOKEN_NVM3_REGION_CONNECT              (0x30000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Bluetooth Stack.
 */
#define SL_TOKEN_NVM3_REGION_BT                   (0x40000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Z-Wave Stack.
 */
#define SL_TOKEN_NVM3_REGION_ZWAVE                (0x50000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Bluetooth Mesh Stack.
 */
#define SL_TOKEN_NVM3_REGION_BTMESH               (0x60000)

/** @ingroup NVM3 key space regions
 *  @brief Reserved.
 */
#define SL_TOKEN_NVM3_REGION_RESERVED0            (0x70000)

// NVM3 Common region
/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Apple Homekit.
 */
#define SL_TOKEN_NVM3_REGION_APPLE_HK             (0x80000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Zigbee Cluster Library (ZCL).
 */
#define SL_TOKEN_NVM3_REGION_ZCL                  (0x81000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for DotDot.
 */
#define SL_TOKEN_NVM3_REGION_DOTDOT               (0x82000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Platform CLI SM.
 */
#define SL_TOKEN_NVM3_REGION_PLT_CLISM            (0x83000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Platform Crypto.
 */
#define SL_TOKEN_NVM3_REGION_PLT_CRYPTO           (0x83100)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Bootloader.
 */
#define SL_TOKEN_NVM3_REGION_BTL                  (0x87100)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Matter.
 */
#define SL_TOKEN_NVM3_REGION_MATTER               (0x87200)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for AWS Late Provisioning.
 */
#define SL_TOKEN_NVM3_REGION_AWS_LATE_PRN         (0x88000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for KNX IoT.
 */
#define SL_TOKEN_NVM3_REGION_KNX_IOT              (0x89000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Antenna Calibration (Platform).
 */
#define SL_TOKEN_NVM3_REGION_ANT_CAL_PLT          (0x89800)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Antenna Calibration (RAIL).
 */
#define SL_TOKEN_NVM3_REGION_ANT_CAL_RAIL         (0x89880)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Find Ecosystem.
 *  @note A total of 256 key ids are allocated for Find Ecosystem.
 */
#define SL_TOKEN_NVM3_REGION_FIND_ECOSYSTEM       (0x89900)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Aliro.
 *  @note A total of 1536 key ids are allocated for Aliro.
 */
#define SL_TOKEN_NVM3_REGION_ALIRO                (0x89a00)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for CBAP.
 *  @note A total of 128 key ids are allocated for CBAP.
 */
#define SL_TOKEN_NVM3_REGION_CBAP                 (0x8a000)

/** @ingroup NVM3 key space regions
*   @brief NVM3 Key Region for All the platform specific tokens.
*   This region allocates keys from 0x8DC00 to 0x8FFFF.
*   Below are the keys that are used within this region:
*   - 0x8DC00: Crash log token
*   - 0x8DC01: Upgradable token (UT) KLV chain anchor
*   - 0x8DC02: Upgradable token (UT) KLV extension info
*   - 0x8DC03: Upgradable token (UT) KLV repair info
*   - 0x8DC04: Upgradable token (UT) KLV tail info
*   - 0x8DC05 - 0x8DFFF: Unused keys
*   - 0x8E000 - 0x8EFFF: Phase-1 static device override tokens
*   - 0x8F000 - 0x8FFFF: Phase-1 static secure override tokens
*/
#define SL_TOKEN_NVM3_REGION_PLATFORM            (0x8DC00)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Wi-SUN.
 */
#define SL_TOKEN_NVM3_REGION_WISUN                (0x90000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Sidewalk.
 */
#define SL_TOKEN_NVM3_REGION_SW                   (0xA0000)

/** @ingroup NVM3 key space regions
 *  @brief NVM3 Key Region for Wi-Fi.
 */
#define SL_TOKEN_NVM3_REGION_WIFI                 (0xB0000)

/**
 * @defgroup StaticToken Static Token Key Space Regions
 * @brief MACRO's for Static Token Key Space Regions. Applicable for static device, static secure and upgradable tokens from series 3 devices onwards.
 *
 * CTM has reserved dedicated token key ranges for each domain, with a total of 1024 token keys allocated per domain.
 * The token key ranges are defined as follows:
 * |----------------------------------------------------------------|
 * | Domain         | Phase-1 logical keys     | New logical keys   |
 * |                | (Don't use these ranges) | (Use these ranges) |
 * |----------------|--------------------------|--------------------|
 * | Common         | 0x000-0x07F              | 0x0000-0x03FF      |
 * | Zigbee         | 0x080-0x0FF              | 0x0400–0x07FF      |
 * | Thread         | 0x100-0x17F              | 0x0800–0x0BFF      |
 * | Connect        | 0x180-0x1FF              | 0x0C00–0x0FFF      |
 * | Phase-1 SDT    | NA                       | 0x1000-0x1FFF      |
 * | Bluetooth      | 0x200-0x27F              | 0x2000–0x23FF      |
 * | Z-Wave         | 0x280-0x2FF              | 0x2400–0x27FF      |
 * | Bluetooth Mesh | 0x300-0x37F              | 0x2800–0x2BFF      |
 * | Matter         | 0x380-0x3FF              | 0x2C00–0x2FFF      |
 * | Phase-1 SST    | NA                       | 0x3000-0x3FFF      |
 * | Wi-Sun         | 0x400-0x47F              | 0x4000–0x43FF      |
 * | Sidewalk       | 0x480-0x4FF              | 0x4400–0x47FF      |
 * | Wi-Fi          | 0x500-0x57F              | 0x4800–0x4BFF      |
 * | Reserved       | 0x580-0x5FF              | 0x4C00–0x5FFF      |
 * | User           | 0x600-0x67F              | 0x6000–0x63FF      |
 * | Reserved       | 0x680–0xFFF              | 0x6400–0xb633      |
 * | Virtual EUI-64 | NA                       | 0xb634             |
 * | Reserved       | NA                       | 0xb635–0xFFFF      |
 *
 * A new token added for series 3 devices must follow the domain-specific token Id ranges mentioned above.
 * And the new tokens must not conflict with the existing tokens. Below is an example of how to define a new token:
 *
 *       @code{.c}
 *       New 16-bit static device token for a matter token would be 0x2C00.
 *       In order to identify the token as static device, the type nibble comes from the GET macro.
 *       The illustration below provides the 32-bit token format for the new 16-bit static device token.
 *       #define MFG_MATTER_STATIC_DEVICE_TOKEN                (SL_TOKEN_STATIC_MATTER_DOMAIN | 0x1)
 *       #define TOKEN_MFG_MATTER_STATIC_DEVICE_TOKEN          (SL_TOKEN_GET_STATIC_DEVICE_TOKEN(MFG_MATTER_STATIC_DEVICE_TOKEN))
 *       #define TOKEN_MFG_MATTER_STATIC_DEVICE_TOKEN_SIZE     (2u)
 *
 *       New 16-bit static secure token for a matter token would be 0x2C01. Do not OR a type-nibble macro.
 *       In order to identify the token as static secure, the type nibble comes from the GET macro.
 *       The illustration below provides the 32-bit token format for the new 16-bit static secure token.
 *       #define MFG_MATTER_STATIC_SECURE_TOKEN                (SL_TOKEN_STATIC_MATTER_DOMAIN | 0x1)
 *       #define TOKEN_MFG_MATTER_STATIC_SECURE_TOKEN          (SL_TOKEN_GET_STATIC_SECURE_TOKEN(MFG_MATTER_STATIC_SECURE_TOKEN))
 *       #define TOKEN_MFG_MATTER_STATIC_SECURE_TOKEN_SIZE     (2u)
 *       @endcode
 *
 * @note `SL_TOKEN_STATIC_DEVICE_TOKENS` and `SL_TOKEN_STATIC_SECURE_DATA_TOKENS`
 *       stay defined so Phase-1 Series 3 keys and Series 2 offsets keep their
 *       values. Do not OR them into a new Series 3 key.
 * @note The Phase-1 logical ids below are already taken. Do not reuse them for
 *       another token. Deleted ids may be reused only as Phase-1 12-bit keys in
 *       the original domain, not as a new 16-bit key with a different high nibble.
 *       For example, @ref TOKEN_MFG_CTUNE is reserved at logical id 0x100 (stored
 *       as 0x1100) and must not be reused for a Thread static device token.
 *       @ref TOKEN_MFG_CBKE_DATA is reserved at logical id 0x204.
 * @note Reserved Phase-1 logical ids (do not use these ids for a different token):
 *    | Static device token (Domain : Token IDs)                  | Static secure token (Domain : Token IDs)     |
 *    |-----------------------------------------------------------|----------------------------------------------|
 *    | Common : 0x002, 0x00C, 0x020, 0x030, 0x034, 0x038, 0x064  | Bluetooth : 0x204, 0x260, 0x270              |
 *    | Zigbee : 0x09C                                            | Z-Wave : 0x298                               |
 *    | Thread : 0x100                                            |                                              |
 *    | Connect : 0x1f0                                           |                                              |
 *
 */

/** @ingroup StaticToken
 *  @brief Static Token Domain for Common tokens.
 */
 #define SL_TOKEN_STATIC_COMMON_DOMAIN       (0x0000)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Zigbee tokens.
 */
#define SL_TOKEN_STATIC_ZIGBEE_DOMAIN       (0x0400)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Thread tokens.
 */
#define SL_TOKEN_STATIC_THREAD_DOMAIN       (0x0800)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Connect tokens.
 */
#define SL_TOKEN_STATIC_CONNECT_DOMAIN      (0x0C00)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Bluetooth tokens.
 */
#define SL_TOKEN_STATIC_BT_DOMAIN           (0x2000)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Z-Wave tokens.
 */
#define SL_TOKEN_STATIC_ZWAVE_DOMAIN        (0x2400)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Bluetooth Mesh tokens.
 */
#define SL_TOKEN_STATIC_BT_MESH_DOMAIN      (0x2800)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Matter tokens.
 */
#define SL_TOKEN_STATIC_MATTER_DOMAIN       (0x2C00)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Wi-Sun tokens.
 */
#define SL_TOKEN_STATIC_WI_SUN_DOMAIN       (0x4000)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Sidewalk tokens.
 */
#define SL_TOKEN_STATIC_SIDEWALK_DOMAIN     (0x4400)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for Wi-Fi tokens.
 */
#define SL_TOKEN_STATIC_WI_FI_DOMAIN        (0x4800)

 /** @ingroup StaticToken
 *  @brief Static Token Domain for User tokens.
 */
#define SL_TOKEN_STATIC_USER_DOMAIN         (0x6000)

 /** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */
 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Common tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_COMMON       (0x000)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Zigbee tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_ZIGBEE       (0x080)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Thread tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_THREAD       (0x100)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Connect tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_CONNECT      (0x180)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Bluetooth tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_BT           (0x200)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Z-Wave tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_ZWAVE        (0x280)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Bluetooth Mesh tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_BT_MESH      (0x300)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Matter tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_MATTER       (0x380)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Wi-Sun tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_WI_SUN       (0x400)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Sidewalk tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_SIDEWALK     (0x480)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for Wi-Fi tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_WI_FI        (0x500)

 /** @ingroup StaticToken
 *  @brief Phase-1 Static Token Domain for User tokens.
 */
#define SL_TOKEN_DOMAIN_STATIC_USER         (0x600)
/** @endcond */

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */
#ifndef SL_TOKEN_NVM3_OVERRIDE
//To store a static token in NVM3 region as an override token we shall follow NVM3 key format.
//Each object in the NVM3 is addressed by a 20-bit key.
//Phase-1 override keys keep a 12-bit key. Bits 19:16 of the NVM3 key are used to identify
//the static tokens that are stored in NVM3 memory space. Bits 15:12 of the static token keys
//are used to identify the token type.
//NVM3 key select the range: 0x8E000 for static device, 0x8F000 for static secure.
//New 16-bit override keys use one NVM3 domain each (bits 19:16), with the
//NVM3 domain 0xE0000 for static device, 0xF0000 for static secure.
#define SL_TOKEN_STATIC_TOKEN_KEY_MASK                      (0x0FFFU)
#define SL_TOKEN_STATIC_TOKEN_16BIT_KEY_MASK                (0xFFFFU)
#define SL_TOKEN_STATIC_TOKEN_RANGE_MASK                    (0xFF000U)
// Bits 15:12 of the static key. Phase-1 device is 0x1, Phase-1 secure is 0x3.
#define SL_TOKEN_STATIC_TOKEN_BASE_TYPE_KEY_MASK            (0xF000U)
#define SL_TOKEN_NVM3_REGION_STATIC_TOKEN_RANGE_OFFSET      (0xC)
#define SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_RANGE_ID   (0x8E)
#define SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_RANGE_ID   (0x8F)
#define SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_RANGE      (SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_RANGE_ID \
                                                             << SL_TOKEN_NVM3_REGION_STATIC_TOKEN_RANGE_OFFSET)

#define SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_RANGE      (SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_RANGE_ID \
                                                             << SL_TOKEN_NVM3_REGION_STATIC_TOKEN_RANGE_OFFSET)

// 16-bit override domains. Bits 19:16 are the domain. Bits 15:0 are the key.
#define SL_TOKEN_NVM3_REGION_STATIC_TOKEN_16BIT_RANGE_OFFSET    (0x10)
#define SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_16BIT_RANGE_ID (0xE)
#define SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_16BIT_RANGE_ID (0xF)
#define SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_16BIT_RANGE    (SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_16BIT_RANGE_ID \
                                                                 << SL_TOKEN_NVM3_REGION_STATIC_TOKEN_16BIT_RANGE_OFFSET)
#define SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_16BIT_RANGE    (SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_16BIT_RANGE_ID \
                                                                 << SL_TOKEN_NVM3_REGION_STATIC_TOKEN_16BIT_RANGE_OFFSET)

#endif

// Returned when @ref SL_TOKEN_GET_DYNAMIC_OVER_RIDE_TOKEN cannot identify token type.
#define SL_TOKEN_NVM3_DYNAMIC_OVERRIDE_TOKEN_INVALID              (0xFFFFFFFFU)

//The bits 28:16 has the size of the static token embedded
#define SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET                (0x10)
#define SL_TOKEN_STATIC_TOKEN_SIZE_MASK                  (0x1FFF)
//The bit 20 defines if the NVM3 token is a counter object or a data object
#define SL_TOKEN_NVM3_OBJ_TYPE_COUNTER_TOKEN             (1)
#define SL_TOKEN_NVM3_OBJ_TYPE_OFFSET                    (0x14)
#define SL_TOKEN_NVM3_OBJ_TYPE_BIT_MASK                  (SL_TOKEN_NVM3_OBJ_TYPE_COUNTER_TOKEN << SL_TOKEN_NVM3_OBJ_TYPE_OFFSET)
/** @endcond */

/**
 * @brief Helper MACRO to get the size of a predefined token.
 *
 * @param[in] token_name  Name of the static token.
 *
 * @note The size MACRO's should be predefined to use this helper macro.
 */

#define SL_TOKEN_GET_STATIC_TOKEN_SIZE(token_name)       token_name##_SIZE

/**
 * @brief Helper MACRO to create a new static device token in 32-bit identifier format,
 *        the tokens created are stored in secure memory region and are mass erase protected,
 *        this MACRO can be used with `sl_token_get_data()` or `sl_token_set_data()` functions,
 *        for more information @ref `sl_token_manager_defines.h`.
 *
 * @param[in] token  16-bit key for the static device token.
 *                   Refer @ref StaticToken for more information on 16-bit token id ranges.
 * Key format  :
 *    | 31:30 | 29                 | 28:16       | 15:0 |
 *    |-------|--------------------|-------------|------|
 *    | type  | static token type  | Data length | key  |
 *
 *    - type              : Static Token.
 *    - static token type : 0 - Static device token region (secure memory region)
 *    - key               : 16-bit key. Phase-1: type nibble plus 12-bit id.
 *                          New token id ranges for Series 3 onwards: 16-bit id.
 *
 */
#define SL_TOKEN_GET_STATIC_DEVICE_TOKEN(token)    (SL_TOKEN_TYPE_STATIC_DEVICE \
                                                    | token                     \
                                                    | (token##_SIZE << SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET))

/**
 * @brief Helper MACRO to create a new static secure token in 32-bit identifier format,
 *        the tokens created are stored in dedicated flash region, with additional security measures
 *        and are not protected by mass/device erase,
 *        this MACRO can be used with `sl_token_get_data()` or `sl_token_set_data()` functions,
 *        for more information @ref `sl_token_manager_defines.h`.
 *
 * @param[in] token  16-bit key for the static secure token.
 *                   Refer @ref StaticToken for more information on 16-bit token id ranges.
 * Key format  :
 *    | 31:30 | 29                 | 28:16       | 15:0 |
 *    |-------|--------------------|-------------|------|
 *    | type  | static token type  | Data length | key  |
 *
 *    - type              : Static Token.
 *    - static token type : 1 - Static secure token region (dedicated flash region)
 *    - key               : 16-bit key. Phase-1: type nibble plus 12-bit id.
 *                          New token id ranges for Series 3 onwards: 16-bit id.
 *
 */
#define SL_TOKEN_GET_STATIC_SECURE_TOKEN(token)    (SL_TOKEN_TYPE_STATIC_SECURE \
                                                    | token                     \
                                                    | (token##_SIZE << SL_TOKEN_STATIC_TOKEN_SIZE_OFFSET))

/**
 * @brief Helper MACRO to create dynamic token, user has to take care of providing the unique
 *        token for each NVM3 token created, the tokens created are stored in NVM3 region and are
 *        not protected by mass/device/nvm3 erase,
 *        this MACRO can be used with `sl_token_get_data()` or `sl_token_set_data()` functions,
 *        for more information @ref `sl_token_manager_defines.h`.
 *
 * @param[in] token 20-bit NVM3 key (it has both NVM3 domain and NVM3 key combined).
 * @param[in] IsCounterObj should be set to 1, if a token is created for counter objects.
 *
 * Key format  :
 *    | 31:27 | 26:21    | 20               |19:16        | 15:0     |
 *    |-------|----------|------------------|-------------|----------|
 *    | type  | reserved | NVM3 object type | NVM3 domain | NVM3 key |
 *
 *    - type              : Dynamic Token.
 *    - NVM3 object type : 0 - Data object
 *                         1 - Counter object
 *    - NVM3 domain      : NVM3 Default Instance Key Space Regions @ref NVM3 key space regions
 *    - NVM3 key         : 16-bit unique key.
 *
 */
#define SL_TOKEN_GET_DYNAMIC_TOKEN(token, IsCounterObj)   (SL_TOKEN_TYPE_DYNAMIC \
                                                           | token            \
                                                           | (IsCounterObj << SL_TOKEN_NVM3_OBJ_TYPE_OFFSET))

/**
 * @brief Helper MACRO to create dynamic token for NVM3 secondary instance, user has to take
 *        care of providing the unique token for each NVM3 token created, the tokens created are
 *        stored in NVM3 region and are not protected by mass/device/nvm3 erase,
 *        this MACRO can be used with `sl_token_get_data()` or `sl_token_set_data()` functions,
 *        for more information @ref `sl_token_manager_defines.h`.
 *
 * @param[in] token 20-bit NVM3 key (it has both NVM3 domain and NVM3 key combined).
 * @param[in] IsCounterObj should be set to 1, if a token is created for counter objects.
 * Key format  :
 *    | 31:27 | 26:21    | 20               |19:16        | 15:0     |
 *    |-------|----------|------------------|-------------|----------|
 *    | type  | reserved | NVM3 object type | NVM3 domain | NVM3 key |
 *
 *    - type              : Dynamic Secondary Token.
 *    - NVM3 object type : 0 - Data object
 *                         1 - Counter object
 *    - NVM3 domain      : @ref NVM3 key space regions
 *    - NVM3 key         : 16-bit unique key.
 *
 * @note This is specifically used for Z-Wave usage, where the secondary instance of NVM3 is utilized.
 *
 */
#define SL_TOKEN_GET_DYNAMIC_SECONDARY_INSTANCE_TOKEN(token, IsCounterObj)   (SL_TOKEN_TYPE_DYNAMIC_SECONDARY \
                                                                              | token                      \
                                                                              | (IsCounterObj << SL_TOKEN_NVM3_OBJ_TYPE_OFFSET))

/**
 * @brief Helper MACRO to create a NVM3 override key for a static token.
 *        This MACRO can be used with `sl_token_get_data()` or `sl_token_set_data()`.
 *
 * @param[in] token A 32-bit static token from `SL_TOKEN_GET_STATIC_DEVICE_TOKEN()` / `SL_TOKEN_GET_STATIC_SECURE_TOKEN()`.
 *
 * Range selection (first match wins):
 *
 * 1. **Phase-1 key** — bits 15:12 are `SL_TOKEN_STATIC_DEVICE_TOKENS` (`0x1000`) or
 *    `SL_TOKEN_STATIC_SECURE_DATA_TOKENS` (`0x3000`). Applies to a bare Phase-1 16-bit
 *    value or to a 32-bit token that still carries that nibble in bits 15:12.
 *    - Static device: NVM3 key `0x8E000 | (token & 0xFFF)`
 *    - Static secure: NVM3 key `0x8F000 | (token & 0xFFF)`
 *
 * 2. **New 16-bit key** — bits 15:12 are not a Phase-1 type nibble, and bits 31:29
 *    (`@ref SL_TOKEN_STATIC_MASK`) select static device (`0xC0000000`) or static secure
 *    (`0xE0000000`).
 *    - Static device: NVM3 key `0xE0000 | (token & 0xFFFF)`
 *    - Static secure: NVM3 key `0xF0000 | (token & 0xFFFF)`
 *
 * 3. **Invalid** — neither bits 15:12 nor bits 31:29 identify the token type. Returns
 *    @ref SL_TOKEN_NVM3_DYNAMIC_OVERRIDE_TOKEN_INVALID.
 *
 * @note Bits 28:27 hold the top of the embedded static-token size, so type detection
 *       uses @ref SL_TOKEN_STATIC_MASK (bits 31:29), not an equality test on all of
 *       bits 31:27.
 *
 * @note The MACRO expands to an integer constant expression, so it can be used in
 *       static initializers. The Phase-1 comparison uses `SL_TOKEN_STATIC_DEVICE_TOKENS`
 *       and `SL_TOKEN_STATIC_SECURE_DATA_TOKENS`, which are defined in
 *       `sl_token_manager_manufacturing.h`. Include that header before using this MACRO.
 *       @p token may be evaluated more than once, so do not pass an expression with side effects.
 */
#ifndef SL_TOKEN_GET_DYNAMIC_OVER_RIDE_TOKEN
#define SL_TOKEN_GET_DYNAMIC_OVER_RIDE_TOKEN(token)                                                                  \
  ((((uint32_t)(token) & SL_TOKEN_STATIC_TOKEN_BASE_TYPE_KEY_MASK) == (uint32_t)SL_TOKEN_STATIC_DEVICE_TOKENS)       \
   ? (SL_TOKEN_TYPE_NVM3                                                                                             \
      | SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_RANGE                                                               \
      | ((uint32_t)(token) & SL_TOKEN_STATIC_TOKEN_KEY_MASK))                                                        \
   : ((((uint32_t)(token) & SL_TOKEN_STATIC_TOKEN_BASE_TYPE_KEY_MASK) == (uint32_t)SL_TOKEN_STATIC_SECURE_DATA_TOKENS) \
      ? (SL_TOKEN_TYPE_NVM3                                                                                          \
         | SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_RANGE                                                            \
         | ((uint32_t)(token) & SL_TOKEN_STATIC_TOKEN_KEY_MASK))                                                     \
      : ((((uint32_t)(token) & SL_TOKEN_STATIC_MASK) == SL_TOKEN_TYPE_STATIC_DEVICE)                                 \
         ? (SL_TOKEN_TYPE_NVM3                                                                                       \
            | SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_16BIT_RANGE                                                   \
            | ((uint32_t)(token) & SL_TOKEN_STATIC_TOKEN_16BIT_KEY_MASK))                                            \
         : ((((uint32_t)(token) & SL_TOKEN_STATIC_MASK) == SL_TOKEN_TYPE_STATIC_SECURE)                              \
            ? (SL_TOKEN_TYPE_NVM3                                                                                    \
               | SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_16BIT_RANGE                                                \
               | ((uint32_t)(token) & SL_TOKEN_STATIC_TOKEN_16BIT_KEY_MASK))                                         \
            : SL_TOKEN_NVM3_DYNAMIC_OVERRIDE_TOKEN_INVALID))))
#endif

/**
 * @brief Helper MACRO to create a new upgradable token in 32-bit identifier format.
 *        Tokens created using this macro are stored in the upgradable NVM region and
 *        can be updated (overwritten) at runtime. This dedicated NVM region is reserved
 *        exclusively for upgradable tokens.
 *        This MACRO can be used with `sl_token_get_data()` or `sl_token_set_data()` functions.
 *
 * @param[in] token  16-bit unique key
 * Key format  :
 *    | 31:29 | 28:16       | 15:0 |
 *    |-------|-------------|------|
 *    | type  | Reserved    | key  |
 *
 *    - type : Upgradable token
 *    - key  : 16-bit unique key.
 *
 */
#define SL_TOKEN_GET_UPGRADABLE_TOKEN(token)           (SL_TOKEN_TYPE_UPGRADABLE | token)
/** @} (end addtogroup defines) */
/** @} (end addtogroup common_token_manager) */

#endif // SL_TOKEN_MANAGER_DEFINES_H
