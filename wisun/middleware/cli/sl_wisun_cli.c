/***************************************************************************//**
 * @file sl_wisun_cli.c
 * @brief Wi-SUN CLI handler
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>

#include "sl_wisun_cli.h"
#include "sl_component_catalog.h"
#include "sl_cli.h"
#include "sl_wisun_api.h"
#include "sl_wisun_trace_util.h"
#include "sl_wisun_cli_util.h"
#include "sl_wisun_cli_core.h"
#include "sl_wisun_cli_config.h"
#include "sl_wisun_cli_settings.h"

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
#include "sl_wisun_app_core_util.h"
#include "sl_wisun_app_core_config.h"
  #if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
  #include "sl_wisun_app_br_core.h"
  #endif
#endif

#if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
#include "border_router/sl_wisun_br_api.h"
#include "sl_wisun_app_setting_br.h"
#else
#include "sl_wisun_app_setting.h"
#endif

#if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
extern app_setting_br_t _wisun_br_settings;
#define APP_CLI_WISUN_SETTINGS_STORAGE  _wisun_br_settings
#else
extern app_setting_wisun_t _wisun_app_settings;
#define APP_CLI_WISUN_SETTINGS_STORAGE  _wisun_app_settings
#endif

#if defined(SL_CATALOG_WISUN_BR_DHCPV6_SERVER_PRESENT)
#include "sl_wisun_br_dhcpv6_server.h"
#endif

#if defined(SL_CATALOG_WISUN_BR_WIFI_PRESENT)
#include "sl_wisun_br_wifi.h"
#endif

#if SL_WISUN_CLI_ENABLED

#if defined(SL_CATALOG_WISUN_STACK_DEBUG_PRESENT) \
  && SL_WISUN_TRACE_LEVEL_CONTROL_CLI_ENABLED
#include "sl_wisun_trace_api.h"
#include "sl_memory_manager.h"
#endif

#endif // SL_WISUN_CLI_ENABLED
// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

/// Internal Config: Enable FAN 1.1 configuration
#define APP_CLI_ENABLE_FAN11_CONFIG         (1U)

/// Internal Config: Enable Explicit configuration
#define APP_CLI_ENABLE_EXPLICIT_CONFIG      (1U)

/// Wisun CLI domain ID
#define APP_CLI_WISUN_DOMAIN_ID             (0U)

/// Wisun MAC CLI domain ID
#define APP_CLI_WISUN_MAC_DOMAIN_ID         (1U)

/// Wisun Statistics CLI domain ID
#define APP_CLI_WISUN_STATISTICS_DOMAIN_ID  (2U)

/// Wisun Wifi CLI domain ID
#define APP_CLI_WISUN_WIFI_DOMAIN_ID        (3U)

/// App CLI input flag: None
#define APP_CLI_INPUT_FLAG_NONE             APP_SETTINGS_INPUT_FLAG_NONE

/// App CLI input flag: Signed
#define APP_CLI_INPUT_FLAG_SIGNED           APP_SETTINGS_INPUT_FLAG_SIGNED

/// App CLI input flag: Default
#define APP_CLI_INPUT_FLAG_DEFAULT          APP_SETTINGS_INPUT_FLAG_DEFAULT

/// App CLI output flag: None
#define APP_CLI_OUTPUT_FLAG_NONE            APP_SETTINGS_OUTPUT_FLAG_NONE

/// App CLI output flag: Signed
#define APP_CLI_OUTPUT_FLAG_SIGNED          APP_SETTINGS_OUTPUT_FLAG_SIGNED

/// App CLI output flag: Hex
#define APP_CLI_OUTPUT_FLAG_HEX             APP_SETTINGS_OUTPUT_FLAG_HEX

/// App CLI output flag: Fixed size
#define APP_CLI_OUTPUT_FLAG_FIXEDSIZE       APP_SETTINGS_OUTPUT_FLAG_FIXEDSIZE

/// App CLI output flag: Default
#define APP_CLI_OUTPUT_FLAG_DEFAULT         APP_SETTINGS_OUTPUT_FLAG_DEFAULT

/// App CLI value: None
#define APP_CLI_VALUE_SIZE_NONE             APP_SETTINGS_VALUE_SIZE_NONE

/// App CLI output flag: uint8
#define APP_CLI_VALUE_SIZE_UINT8            APP_SETTINGS_VALUE_SIZE_UINT8

/// App CLI output flag: uint16
#define APP_CLI_VALUE_SIZE_UINT16           APP_SETTINGS_VALUE_SIZE_UINT16

/// App CLI output flag: uint32
#define APP_CLI_VALUE_SIZE_UINT32           APP_SETTINGS_VALUE_SIZE_UINT32

/// App CLI string value length
#define APP_CLI_STR_VALUE_LENGTH            APP_SETTINGS_STR_VALUE_LENGTH

/// App CLI paramater string name: regulatory domain
#define APP_CLI_PHY_PARAM_REG_DOMAIN_STR    "regulatory_domain"

/// App CLI paramater string name: operating mode
#define APP_CLI_PHY_PARAM_OP_MODE_STR       "operating_mode"

/// App CLI paramater string name: operating class
#define APP_CLI_PHY_PARAM_OP_CLASS_STR      "operating_class"

#if APP_CLI_ENABLE_EXPLICIT_CONFIG
/// App CLI paramater string name: number of channels
#define APP_CLI_PHY_PARAM_NUM_OF_CH_STR     "number_of_channels"

/// App CLI paramater string name: CH0 frequency
#define APP_CLI_PHY_PARAM_CH0_FREQ_STR      "ch0_frequency"

/// App CLI paramater string name: channel spacing
#define APP_CLI_PHY_PARAM_CH_SPACING_STR    "channel_spacing"
#endif

#if APP_CLI_ENABLE_EXPLICIT_CONFIG || APP_CLI_ENABLE_FAN11_CONFIG
/// App CLI paramater string name: PHY mode ID
#define APP_CLI_PHY_PARAM_PHY_MODE_ID_STR   "phy_mode_id"
#endif

#if APP_CLI_ENABLE_FAN11_CONFIG
/// App CLI paramater string name: channel plan ID
#define APP_CLI_PHY_PARAM_CH_PLAN_ID_STR    "chan_plan_id"
#endif

/// App CLI paramater string name: PHY config type
#define APP_CLI_PHY_PARAM_PHY_CFG_TYPE_STR  "phy_config_type"

/// App CLI entry
typedef app_settings_entry_t app_cli_entry_t;

/// Structure typedef to store common PHY parameters, like reg. domain and PHY mode ID
typedef struct app_cli_phy_common_params {
  /// Regulatory domain
  uint8_t regulatory_domain;
  /// Operating class
  uint8_t operating_class;
  /// Operating mode
  uint16_t operating_mode;
#if APP_CLI_ENABLE_EXPLICIT_CONFIG
  /// Number of channels
  uint16_t number_of_channels;
  /// CH0 frequency
  uint32_t ch0_frequency;
  /// Channel spacing
  uint8_t channel_spacing;
#endif
#if APP_CLI_ENABLE_FAN11_CONFIG
  /// Channel plan ID
  uint8_t chan_plan_id;
#endif
#if APP_CLI_ENABLE_EXPLICIT_CONFIG || APP_CLI_ENABLE_FAN11_CONFIG
  /// PHY mode ID
  uint8_t phy_mode_id;
#endif
  /// PHY config type
  uint8_t phy_config_type;
} app_cli_phy_common_params_t;

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

/**************************************************************************//**
 * @brief Set CLI common PHY parameters from PHY config
 * @details Synchronization is required because of common parameters,
 *          like reg. domain and PHY mode ID
 * @return sl_status_t SL_STATUS_OK on success, SL_STATUS_FAIL otherwise
 *****************************************************************************/
static sl_status_t _load_common_params_from_phy_cfg(void);

/**************************************************************************//**
 * @brief Store CLI common PHY parameters to PHY config
 * @details Synchronization is required because of common parameters,
 *          like reg domain and phy mode id
 * @return sl_status_t SL_STATUS_OK on success, SL_STATUS_FAIL otherwise
 *****************************************************************************/
static sl_status_t _store_common_params_to_phy_cfg(void);

#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/**************************************************************************//**
 * @brief Helper to get connection status for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_connection(char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry);
#endif

/**************************************************************************//**
 * @brief Helper to set network name for setter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_set_network_name(const char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to set network size for setter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_set_network_size(const char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to set TX power for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_set_tx_power(const char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get network name for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_network_name(char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get network size for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_network_size(char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get TX power for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_tx_power(char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry);

#if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/**************************************************************************//**
 * @brief Helper to get state for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_state(char *value_str,
                                      const char *key_str,
                                      const app_cli_entry_t *entry);
#endif

#if defined(SL_CATALOG_WISUN_BR_WIFI_PRESENT)
/**************************************************************************//**
 * @brief Helper to get MAC address string from byte array
 * @param[out] *value_str is the desired value string
 * @param[in] *value is the byte array of MAC address
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _get_mac_address_string(char *value_str,
                                           const uint8_t *value);

/**************************************************************************//**
 * @brief Helper to get WiFi info for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_wifi_info(char *value_str,
                                          const char *key_str,
                                          const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to set Wifi SSID
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_set_ssid(const char *value_str,
                                     const char *key_str,
                                     const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get Wifi SSID
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_ssid(char *value_str,
                                     const char *key_str,
                                     const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to set Wifi Security
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_set_security(const char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get Wifi Security
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_security(char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to set Wifi Passphrase
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_set_passphrase(const char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get Wifi Passphrase
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_passphrase(char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry);
#endif

/**************************************************************************//**
 * @brief Helper to get statistics for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_statistics(char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Is CLI setting parameter is used.
 * @details It indicates the parameters used in the current setup.
 * @param param_str Parameter string.
 * @return true Parameter is used.
 * @return false Parameter is not used.
 *****************************************************************************/
static bool _is_param_used(const char *param_str);

/**************************************************************************//**
 * @brief Get param used string flag
 * @details Helper function to get string representation of parameter usage
 * @return const char*
 *****************************************************************************/
__STATIC_INLINE const char *_param_used_flag_str(const char *param_str);

/**************************************************************************//**
 * @brief Helper to get wisun phy for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_get_phy(char *value_str,
                                    const char *key_str,
                                    const app_cli_entry_t *entry);

#if defined(SL_CATALOG_WISUN_MODE_SWITCH_PRESENT)
/**************************************************************************//**
 * @brief Helper to get wisun mode switch counters for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_ms_get_counters(char *value_str,
                                        const char *key_str,
                                        const app_cli_entry_t *entry);
#endif

/**************************************************************************//**
 * @brief Helper to set TX power for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_cli_set_phy(const char *value_str,
                                    const char *key_str,
                                    const app_settings_entry_t *entry);

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/**************************************************************************//**
 * @brief Helper to set regulation for setter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_set_regulation(const char *value_str,
                                       const char *key_str,
                                       const app_settings_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to set regulation warning threshold for setter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_set_regulation_warning_threshold(const char *value_str,
                                                         const char *key_str,
                                                         const app_settings_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to set regulation alert threshold for setter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_set_regulation_alert_threshold(const char *value_str,
                                                       const char *key_str,
                                                       const app_settings_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get regulation for getter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_get_regulation(char *value_str,
                                       const char *key_str,
                                       const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get regulation warning threshold for setter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_get_regulation_warning_threshold(char *value_str,
                                                         const char *key_str,
                                                         const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get regulation alert threshold for setter
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_get_regulation_alert_threshold(char *value_str,
                                                       const char *key_str,
                                                       const app_cli_entry_t *entry);
#endif

/**************************************************************************//**
 * @brief Helper to set MAC address
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_settings_set_mac_address(const char *value_str,
                                                 const char *key_str,
                                                 const app_settings_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get MAC address
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_settings_get_mac_address(char *value_str,
                                                 const char *key_str,
                                                 const app_settings_entry_t *entry);
#endif

#if defined(SL_CATALOG_WISUN_STACK_DEBUG_PRESENT) \
  && SL_WISUN_CLI_ENABLED                         \
  && SL_WISUN_TRACE_LEVEL_CONTROL_CLI_ENABLED
/**************************************************************************//**
 * @brief Helper to set trace level for setter
 * @param[out] *trace_config is the trace group configuration
 * @param[out] *group_count is the number of groups
 * @param[in] group_id is the group ID
 * @param[in] trace_level is the trace level
 * @return true if found existing group.
 * @return false if group is not found.
 * *****************************************************************************/
static bool _trace_level_add_entry(sl_wisun_trace_group_config_t *trace_config,
                                   uint8_t *group_count,
                                   uint8_t group_id,
                                   uint8_t trace_level);
#endif

#if defined(SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT)
/**************************************************************************//**
 * @brief Helper to get device type
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_get_device_type(char *value_str,
                                        const char *key_str,
                                        const app_cli_entry_t *entry);

/**************************************************************************//**
 * @brief Helper to get LFN profile
 * @param[out] *value_str is the desired value string
 * @param[in] *key_str is the key string of the set command
 * @param[in] *entry is the settings entry
 * @return SL_STATUS_OK if the setting is success.
 * @return SL_STATUS_FAIL if setting failed.
 *****************************************************************************/
static sl_status_t _app_get_lfn_profile(char *value_str,
                                        const char *key_str,
                                        const app_cli_entry_t *entry);
#endif

#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/**************************************************************************//**
 * @brief Connect to Wi-SUN network by PHY configuration type
 * @details CLI callback helper function
 * @param config_type PHY Configuration type
 *****************************************************************************/
static void _app_connect(const sl_wisun_phy_config_type_t config_type);
#endif

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT) \
  && defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/**************************************************************************//**
 * @brief Start Wi-SUN border router
 * @details CLI callback helper function
 *****************************************************************************/
static void _app_start(void);
#endif

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/// Wi-SUN application regulation
static sl_wisun_regulation_t _app_regulation = (sl_wisun_regulation_t)SL_WISUN_APP_CORE_REGULATION;
#endif
#endif

/// Common PHY parameters for CLI setter/getter
static app_cli_phy_common_params_t _phy_common_params = { 0U };

/// Mapping PHY config parameters by PHY config type
static const app_enum_t _phy_cfg_map[] = {
  // FAN 1.0 parameters
  { APP_CLI_PHY_PARAM_REG_DOMAIN_STR, SL_WISUN_PHY_CONFIG_FAN10 },
  { APP_CLI_PHY_PARAM_OP_CLASS_STR, SL_WISUN_PHY_CONFIG_FAN10 },
  { APP_CLI_PHY_PARAM_OP_MODE_STR, SL_WISUN_PHY_CONFIG_FAN10 },

  // FAN 1.1 parameters
#if APP_CLI_ENABLE_FAN11_CONFIG
  { APP_CLI_PHY_PARAM_REG_DOMAIN_STR, SL_WISUN_PHY_CONFIG_FAN11 },
  { APP_CLI_PHY_PARAM_CH_PLAN_ID_STR, SL_WISUN_PHY_CONFIG_FAN11 },
  { APP_CLI_PHY_PARAM_PHY_MODE_ID_STR, SL_WISUN_PHY_CONFIG_FAN11 },
#endif

  // Explicit parameters
#if APP_CLI_ENABLE_EXPLICIT_CONFIG
  { APP_CLI_PHY_PARAM_CH0_FREQ_STR, SL_WISUN_PHY_CONFIG_EXPLICIT },
  { APP_CLI_PHY_PARAM_NUM_OF_CH_STR, SL_WISUN_PHY_CONFIG_EXPLICIT },
  { APP_CLI_PHY_PARAM_CH_SPACING_STR, SL_WISUN_PHY_CONFIG_EXPLICIT },
  { APP_CLI_PHY_PARAM_PHY_MODE_ID_STR, SL_WISUN_PHY_CONFIG_EXPLICIT },
#endif
  // End list
  { NULL, 0 }
};

#if defined(SL_CATALOG_WISUN_STACK_DEBUG_PRESENT) \
  && SL_WISUN_CLI_ENABLED                         \
  && SL_WISUN_TRACE_LEVEL_CONTROL_CLI_ENABLED
static const app_enum_t _app_trace_level_type_enum[] =
{
  { "all", 0xFF },
  { NULL, 0 }
};

static const app_enum_t _app_trace_level_enum[] =
{
  { "none", 0 },
  { "error", 1 },
  { "err", 1 },
  { "warning", 2 },
  { "warn", 2 },
  { "info", 3 },
  { "debug", 4 },
  { "dbg", 4 },
  { NULL, 0 }
};
#endif

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

/// Wi-SUN application settings domain string
const char *app_settings_domain_str[] = {
  "wisun",
  "mac",
  "statistics",
#if defined(SL_CATALOG_WISUN_BR_WIFI_PRESENT)
  "wifi",
#endif
  NULL,
};

const char *app_statistics_domain_str[] =
{
  "heap",
  NULL,
};


typedef enum
{
  app_statistics_domain_heap = 0x00,
} app_statistics_domain_t;

/// Wi-SUN application statistics
static sl_wisun_statistics_t app_statistics;

///  Wi-SUN application settings entry/entries
const app_cli_entry_t app_settings_entries[] =
{
  {
    .key = "network_name",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = SL_WISUN_NETWORK_NAME_SIZE + 1,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_network_name,
    .get_handler = _app_cli_get_network_name,
    .description = "Wi-SUN network [string] max 32"
  },
  {
    .key = APP_CLI_PHY_PARAM_PHY_CFG_TYPE_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = app_wisun_phy_config_type_enum,
    .set_handler = NULL,
    .get_handler = _app_cli_get_phy,
    .description = "PHY configuration type (FAN 1.0|FAN 1.1|explicit)"
  },
  {
    .key = "network_size",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_wisun_nw_size_enum,
    .output_enum_list = app_wisun_nw_size_enum,
    .set_handler = _app_cli_set_network_size,
    .get_handler = _app_cli_get_network_size,
    .description = "Wi-SUN network size [uint8]"
  },
  {
    .key = "tx_power",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_DEFAULT | APP_CLI_INPUT_FLAG_SIGNED,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT | APP_CLI_OUTPUT_FLAG_SIGNED,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_tx_power,
    .get_handler = _app_cli_get_tx_power,
    .description = "TX power in dBm [int8]"
  },
  {
    .key = "tx_power_ddbm",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT16,
    .input = APP_CLI_INPUT_FLAG_DEFAULT | APP_CLI_INPUT_FLAG_SIGNED,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT | APP_CLI_OUTPUT_FLAG_SIGNED,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_tx_power,
    .get_handler = _app_cli_get_tx_power,
    .description = "TX power in ddBm [int16]"
  },
  {
    .key = APP_CLI_PHY_PARAM_REG_DOMAIN_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_wisun_phy_reg_domain_enum,
    .output_enum_list = app_wisun_phy_reg_domain_enum,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Wi-SUN regulatory domain [uint8]"
  },
  {
    .key = APP_CLI_PHY_PARAM_OP_CLASS_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Wi-SUN operation class [uint8]"
  },
  {
    .key = APP_CLI_PHY_PARAM_OP_MODE_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Wi-SUN operating mode [uint16]"
  },
#if APP_CLI_ENABLE_FAN11_CONFIG
  {
    .key = APP_CLI_PHY_PARAM_CH_PLAN_ID_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Wi-SUN channel plan ID [uint8]"
  },
#endif
#if APP_CLI_ENABLE_FAN11_CONFIG || APP_CLI_ENABLE_EXPLICIT_CONFIG
  {
    .key = APP_CLI_PHY_PARAM_PHY_MODE_ID_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Wi-SUN PHY mode ID [uint8]"
  },
#endif
#if APP_CLI_ENABLE_EXPLICIT_CONFIG
  {
    .key = APP_CLI_PHY_PARAM_CH0_FREQ_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT32,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Ch0 frequency in kHz [uint32]"
  },
  {
    .key = APP_CLI_PHY_PARAM_NUM_OF_CH_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT16,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Number of channels [uint16]"
  },
  {
    .key = APP_CLI_PHY_PARAM_CH_SPACING_STR,
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_wisun_phy_channel_spacing_enum,
    .output_enum_list = app_wisun_phy_channel_spacing_enum,
    .set_handler = _app_cli_set_phy,
    .get_handler = _app_cli_get_phy,
    .description = "Channel spacing [string] (100kHz|200kHz|400kHz|600kHz)"
  },
#endif
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
  {
    .key = "join_state",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_wisun_conn_status_enum,
    .output_enum_list = app_wisun_conn_status_enum,
    .set_handler = NULL,
    .get_handler = _app_cli_get_connection,
    .description = "Wi-SUN join state"
  },
  {
    .key = "ip_address_global",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_ip_address,
    .description = "Global IPv6 address"
  },
  {
    .key = "ip_address_link_local",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_ip_address,
    .description = "Link local IPv6"
  },
  {
    .key = "ip_address_border_router",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_ip_address,
    .description = "Border Router IPv6 address"
  },
  {
    .key = "ip_address_primary_parent",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_ip_address,
    .description = "Primary Parent IPv6 address"
  },
#endif
#if defined(SL_CATALOG_WISUN_MODE_SWITCH_PRESENT)
  {
    .key = "mode_switch_tx_counter",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT32,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = _app_ms_get_counters,
    .description = "Mode Switch Tx count"
  },
  {
    .key = "mode_switch_tx_failed_counter",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT32,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = _app_ms_get_counters,
    .description = "Mode Switch Tx failed count"
  },
  {
    .key = "mode_switch_rx_counter",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT32,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = _app_ms_get_counters,
    .description = "Mode Switch Rx count"
  },
  {
    .key = "mode_switch_rx_failed_counter",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT32,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = _app_ms_get_counters,
    .description = "Mode Switch Rx failed count"
  },
#endif
#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
  {
    .key = "regulation",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_regulation_enum,
    .output_enum_list = app_regulation_enum,
    .set_handler = _app_set_regulation,
    .get_handler = _app_get_regulation,
    .description = "Regional regulation [uint8]"
  },
  {
    .key = "regulation_warning_threshold",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_SIGNED,
    .output = APP_CLI_OUTPUT_FLAG_SIGNED,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_set_regulation_warning_threshold,
    .get_handler = _app_get_regulation_warning_threshold,
    .description = "Transmission warning threshold in percent (-1 to disable) [int8]"
  },
  {
    .key = "regulation_alert_threshold",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_CLI_INPUT_FLAG_SIGNED,
    .output = APP_CLI_OUTPUT_FLAG_SIGNED,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_set_regulation_alert_threshold,
    .get_handler = _app_get_regulation_alert_threshold,
    .description = "Transmission alert threshold in percent (-1 to disable) [int8]"
  },
#endif
  {
    .key = "mac_address",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_CLI_INPUT_FLAG_SIGNED,
    .output = APP_CLI_OUTPUT_FLAG_SIGNED,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_settings_set_mac_address,
    .get_handler = _app_settings_get_mac_address,
    .description = "MAC address"
  },
#endif
#if defined(SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT)
  {
    .key = "device_type",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_wisun_device_type_enum,
    .output_enum_list = app_wisun_device_type_enum,
    .set_handler = NULL,
    .get_handler = _app_get_device_type,
    .description = "Device type"
  },
  {
    .key = "lfn_profile",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_wisun_lfn_profile_enum,
    .output_enum_list = app_wisun_lfn_profile_enum,
    .set_handler = NULL,
    .get_handler = _app_get_lfn_profile,
    .description = "Wi-SUN LFN profile"
  },
#endif
#if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
  {
    .key = "state",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = app_wisun_state_enum,
    .set_handler = NULL,
    .get_handler = _app_cli_get_state,
    .description = "Border router state"
  },
  {
    .key = "ip_addresses",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_ip_address,
    .description = "IPv6 addresses"
  },
#endif
#if defined(SL_CATALOG_WISUN_BR_WIFI_PRESENT)
  {
    .key = "ssid",
    .domain = APP_CLI_WISUN_WIFI_DOMAIN_ID,
    .value_size = SL_WISUN_WIFI_SSID_SIZE + 1,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_ssid,
    .get_handler = _app_cli_get_ssid,
    .description = "Wi-Fi SSID [string] max 32"
  },
  {
    .key = "security",
    .domain = APP_CLI_WISUN_WIFI_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8 + 1,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = app_wisun_wifi_security_type_enum,
    .output_enum_list = app_wisun_wifi_security_type_enum,
    .set_handler = _app_cli_set_security,
    .get_handler = _app_cli_get_security,
    .description = "Wi-Fi security type [uint8] (WPA2_TKIP|WPA2_CCMP)"
  },
  {
    .key = "passphrase",
    .domain = APP_CLI_WISUN_WIFI_DOMAIN_ID,
    .value_size = SL_WISUN_WIFI_PASSPHRASE_SIZE + 1,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = _app_cli_set_passphrase,
    .get_handler = _app_cli_get_passphrase,
    .description = "Wi-Fi passphrase [string] max 32"
  },
  {
    .key = "ipv6_address",
    .domain = APP_CLI_WISUN_WIFI_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_NONE,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = _app_cli_get_wifi_info,
    .description = "IPv6 address"
  },
#endif
#if defined(SL_CATALOG_WISUN_APP_SETTING_PRESENT)
  {
    .key = "min_be",
    .domain = APP_CLI_WISUN_MAC_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.mac.min_be,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "Minimum CSMA-CA backoff exponent [uint8]"
  },
  {
    .key = "max_be",
    .domain = APP_CLI_WISUN_MAC_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.mac.max_be,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "Maximum CSMA-CA backoff exponent [uint8]"
  },
  {
    .key = "backoff_period_us",
    .domain = APP_CLI_WISUN_MAC_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT16,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.mac.backoff_period_us,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "Length of one backoff period in microseconds [uint16]"
  },
  {
    .key = "max_cca_retries",
    .domain = APP_CLI_WISUN_MAC_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.mac.max_cca_retries,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "Maximum number of CCA retries [uint8]"
  },
  {
    .key = "max_frame_retries",
    .domain = APP_CLI_WISUN_MAC_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.mac.max_frame_retries,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "Maximum number of transmission retries [uint8]"
  },
  {
    .key = "lowpan_mtu",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT16,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.traffic.lowpan_mtu,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "6Lowpan MTU in bytes [uint16]"
  },
  {
    .key = "ipv6_mru",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT16,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.traffic.ipv6_mru,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "IPv6 MRU in bytes [uint16]"
  },
  {
    .key = "max_edfe_fragment_count",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.traffic.max_edfe_fragment_count,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "Maximum EDFE fragment count [uint8]"
  },
  {
    .key = "allow_edfe_all_packets",
    .domain = APP_CLI_WISUN_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT8,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &APP_CLI_WISUN_SETTINGS_STORAGE.traffic.allow_edfe_all_packets,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = app_settings_set_integer,
    .get_handler = app_settings_get_integer,
    .description = "Allow EDFE for non fragmented packets [uint8]"
  },
#endif
  {
    .key = "heap",
    .domain = APP_CLI_WISUN_STATISTICS_DOMAIN_ID,
    .value_size = APP_SETTINGS_VALUE_SIZE_NONE,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = (void *)SL_WISUN_STATISTICS_TYPE_HEAP,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = _app_cli_get_statistics,
    .description = "Heap usage statistics"
  },
  {
    .key = NULL,
    .domain = 0,
    .value_size = APP_CLI_VALUE_SIZE_NONE,
    .input = APP_CLI_INPUT_FLAG_DEFAULT,
    .output = APP_CLI_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = NULL,
    .description = NULL
  }
};

static const app_cli_entry_t app_statistics_entries[] =
{
  {
    .key = "arena",
    .domain = app_statistics_domain_heap,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT32,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &app_statistics.heap.arena,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_integer,
    .description = "Maximum heap usage"
  },
  {
    .key = "uordblks",
    .domain = app_statistics_domain_heap,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT32,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &app_statistics.heap.uordblks,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_integer,
    .description = "Current heap usage"
  },
  {
    .key = "free",
    .domain = app_statistics_domain_heap,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT32,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &app_statistics.heap.free,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_integer,
    .description = "Current free heap"
  },
  {
    .key = "total",
    .domain = app_statistics_domain_heap,
    .value_size = APP_SETTINGS_VALUE_SIZE_UINT32,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = &app_statistics.heap.total,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = app_settings_get_integer,
    .description = "Total available heap"
  },
  {
    .key = NULL,
    .domain = 0,
    .value_size = APP_SETTINGS_VALUE_SIZE_NONE,
    .input = APP_SETTINGS_INPUT_FLAG_DEFAULT,
    .output = APP_SETTINGS_OUTPUT_FLAG_DEFAULT,
    .value = NULL,
    .input_enum_list = NULL,
    .output_enum_list = NULL,
    .set_handler = NULL,
    .get_handler = NULL,
    .description = NULL
  }
};

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

#if defined(SL_CATALOG_APP_PROJECT_INFO_PRESENT)
/* CLI app about handler */
void app_about(const sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  sl_wisun_app_core_util_project_info_print(false);
}
#endif

#if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/* CLI app reboot handler */
void app_reboot(const sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  NVIC_SystemReset();
}
#endif

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/* CLI app connect to FAN 1.0 handler */
void app_join_fan10(const sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  _app_connect(SL_WISUN_PHY_CONFIG_FAN10);
}

/* CLI app connect to FAN 1.1 handler */
void app_join_fan11(const sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if APP_CLI_ENABLE_FAN11_CONFIG
  _app_connect(SL_WISUN_PHY_CONFIG_FAN11);
#else
  printf("[FAN 1.1 configuration is not supported]\n");
#endif
}

/* CLI app connect explicit handler */
void app_join_explicit(const sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if APP_CLI_ENABLE_EXPLICIT_CONFIG
  _app_connect(SL_WISUN_PHY_CONFIG_EXPLICIT);
#else
  printf("[Explicit PHY configuration is not supported]\n");
#endif
}

/* CLI app disconnect handler */
void app_disconnect(const sl_cli_command_arg_t *arguments)
{
  sl_status_t ret;
  sl_wisun_join_state_t join_state = SL_WISUN_JOIN_STATE_DISCONNECTED;
  (void) arguments;

  ret = sl_wisun_get_join_state(&join_state);
  if (ret != SL_STATUS_OK) {
    printf("[Failed: getting join state: %"PRIu32"]\n", ret);
    return;
  }
  if (join_state == SL_WISUN_JOIN_STATE_DISCONNECTED) {
    printf("[Failed: already disconnected]\n");
    return;
  }

  ret = sl_wisun_disconnect();
  if (ret != SL_STATUS_OK) {
    printf("[Failed: disconnecting: %"PRIu32"]\n", ret);
  }
}
#endif

void app_mac_allow(const sl_cli_command_arg_t *arguments)
{
  sl_status_t ret;
  const char *address_str = NULL;
  sl_wisun_mac_address_t address;
  const app_enum_t *value_enum;

  address_str = sl_cli_get_argument_string(arguments, 0);

  value_enum = app_util_get_enum_by_string(app_mac_enum, address_str);
  if (value_enum) {
    // Assume enumeration means a broadcast address
    address = APP_BROADCAST_MAC;
  } else {
    // Attempt to convert the MAC address string
    ret = app_util_get_mac_address(&address, address_str);
    if (ret != SL_STATUS_OK) {
      printf("[Failed: unable to parse the MAC address: %"PRIu32"]\n", ret);
      return;
    }
  }

  ret = sl_wisun_allow_mac_address(&address);
  if (ret != SL_STATUS_OK) {
    printf("[Failed: unable to add the MAC address to the access list: %"PRIu32"]\n", ret);
    return;
  }

  printf("[MAC address added to the access list]\n");
}

void app_mac_deny(const sl_cli_command_arg_t *arguments)
{
  sl_status_t ret;
  const char *address_str = NULL;
  sl_wisun_mac_address_t address;
  const app_enum_t *value_enum;

  address_str = sl_cli_get_argument_string(arguments, 0);

  value_enum = app_util_get_enum_by_string(app_mac_enum, address_str);
  if (value_enum) {
    // Assume enumeration means a broadcast address
    address = APP_BROADCAST_MAC;
  } else {
    // Attempt to convert the MAC address string
    ret = app_util_get_mac_address(&address, address_str);
    if (ret != SL_STATUS_OK) {
      printf("[Failed: unable to parse the MAC address: %"PRIu32"]\n", ret);
      return;
    }
  }

  ret = sl_wisun_deny_mac_address(&address);
  if (ret != SL_STATUS_OK) {
    printf("[Failed: unable to add the MAC address to the deny list: %"PRIu32"]\n", ret);
    return;
  }

  printf("[MAC address added to the deny list]\n");
}
#endif

#if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
/* CLI app start FAN 1.1 handler */
void app_start_fan11(const sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  _app_start();
}
#endif

/* CLI app stop handler */
void app_stop(const sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  sl_status_t ret = SL_STATUS_OK;

  ret = sl_wisun_br_stop();
  if (ret != SL_STATUS_OK) {
    printf("[Failed: unable to stop Border Router: %"PRIu32"]\n", ret);
    return;
  }

#if defined(SL_CATALOG_WISUN_BR_DHCPV6_SERVER_PRESENT)
  (void)sl_wisun_br_dhcpv6_server_stop();
#endif
}
#endif

#if defined(SL_CATALOG_WISUN_BR_WIFI_PRESENT)
void app_wifi_connect(const sl_cli_command_arg_t *arguments)
{
  (void)arguments;

  sl_status_t status = SL_STATUS_OK;
  static uint8_t ssid_buffer[SL_WISUN_WIFI_SSID_SIZE + 1] = { 0U };
  static uint8_t passphrase_buffer[SL_WISUN_WIFI_PASSPHRASE_SIZE + 1] = { 0U };
  uint8_t security_type = 0U;

  status = app_wisun_setting_get_ssid(ssid_buffer, sizeof(ssid_buffer));
  if (status != SL_STATUS_OK) {
    printf("[wifi: SSID failure %"PRIu32"]\n", status);
    return;
  }

  printf("[wifi: connecting to %s]\n", ssid_buffer);

  status = app_wisun_setting_get_security(&security_type);
  if (status != SL_STATUS_OK) {
    printf("[wifi: Security failure %"PRIu32"]\n", status);
  }

  status = app_wisun_setting_get_passphrase(passphrase_buffer, sizeof(passphrase_buffer));
  if (status != SL_STATUS_OK) {
    printf("[wifi: Passphrase failure %"PRIu32"]\n", status);
    return;
  }

  status = sl_wisun_br_wifi_connect(ssid_buffer,
                                    sizeof(ssid_buffer),
                                    (wifi_security_type_t)security_type,
                                    passphrase_buffer);
  if (status != SL_STATUS_OK) {
    printf("[wifi: connection failure %"PRIu32"]\n", status);
  }
}

void app_wifi_disconnect(const sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  (void)sl_wisun_br_wifi_disconnect();
}
#endif

#if defined(SL_CATALOG_WISUN_STACK_DEBUG_PRESENT) \
  && SL_WISUN_CLI_ENABLED                         \
  && SL_WISUN_TRACE_LEVEL_CONTROL_CLI_ENABLED
void app_set_trace_level(const sl_cli_command_arg_t *arguments)
{
  sl_status_t ret = SL_STATUS_OK;
  uint8_t i = 0U;
  uint8_t group_count = 0U;
  char *trace_config_string;
  uint32_t trace_level = 0UL;
  uint32_t group_id = 0UL;
  char group_id_str[20];
  char trace_level_str[20];
  sl_wisun_trace_group_config_t *trace_config = NULL;
  int res = 0;
  const app_enum_t *value_enum;

  trace_config_string = sl_cli_get_argument_string(arguments, 0);
  trace_config = sl_malloc(SL_WISUN_TRACE_GROUP_COUNT * sizeof(sl_wisun_trace_group_config_t));
  if (trace_config == NULL) {
    printf("[Failed: Memory allocation for trace config is failed]\n");
    return;
  }

  trace_config_string = strtok(trace_config_string, ";");
  while (trace_config_string != NULL) {
    res = sscanf(trace_config_string, "%[^,],%s", group_id_str, trace_level_str);
    if (res != 2) {
      // Error parsing string
      ret = SL_STATUS_INVALID_PARAMETER;
      break;
    }

    // Check group_id enum
    value_enum = app_util_get_enum_by_string(_app_trace_level_type_enum, group_id_str);
    if (value_enum) {
      // group_id is an enum (only "all" accepted)

      // Check trace_level enum
      value_enum = app_util_get_enum_by_string(_app_trace_level_enum, trace_level_str);

      if (value_enum) {
        trace_level = value_enum->value;
      } else {
        ret = app_util_get_integer(&trace_level, trace_level_str, NULL, false);
        if (ret != SL_STATUS_OK) {
          // Not a valid level value
          ret = SL_STATUS_INVALID_TYPE;
          break;
        }
      }

      for (i = 0; i < SL_WISUN_TRACE_GROUP_COUNT; ++i) {
        _trace_level_add_entry(trace_config, &group_count, i, trace_level);
      }
    } else {
      // group_id is not an enum

      ret = app_util_get_integer(&group_id, group_id_str, NULL, false);
      if (ret != SL_STATUS_OK) {
        // Not a valid group value
        ret = SL_STATUS_INVALID_TYPE;
        break;
      }

      // Check trace_level enum
      value_enum = app_util_get_enum_by_string(_app_trace_level_enum, trace_level_str);

      if (value_enum) {
        trace_level = value_enum->value;
      } else {
        ret = app_util_get_integer(&trace_level, trace_level_str, NULL, false);
        if (ret != SL_STATUS_OK) {
          // Not a valid level value
          ret = SL_STATUS_INVALID_TYPE;
          break;
        }
      }

      _trace_level_add_entry(trace_config, &group_count, group_id, trace_level);
    }
    trace_config_string = strtok(NULL, ";");
  }

  if (ret == SL_STATUS_OK) {
    ret = sl_wisun_set_trace_level(group_count, trace_config);
  }

  sl_free(trace_config);

  if (ret == SL_STATUS_OK) {
    printf("[Set %d trace groups]\n", group_count);
  } else {
    printf("[Error when setting trace level: %"PRIu32"]\n", ret);
  }
}
#endif

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
static void _app_connect(const sl_wisun_phy_config_type_t config_type)
{
  sl_status_t stat = SL_STATUS_FAIL;

  stat = _load_common_params_from_phy_cfg();
  if (stat != SL_STATUS_OK) {
    printf("[Failed: Loading PHY config]\n");
    return;
  }

  // Set expected config type
  _phy_common_params.phy_config_type = (uint8_t) config_type;

  // Store CLI cfg to PHY cfg
  stat = _store_common_params_to_phy_cfg();

  if (stat != SL_STATUS_OK) {
    printf("[Failed: Storing PHY config]\n");
    return;
  }
  // call connect API
  sl_wisun_app_core_network_connect();
}
#endif
#endif

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT) \
  && defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
static void _app_start(void)
{
  sl_wisun_br_state_t br_state = SL_WISUN_BR_STATE_INITIALIZED;
  if ((sl_wisun_br_get_state(&br_state) == SL_STATUS_OK)
      && (br_state == SL_WISUN_BR_STATE_OPERATIONAL)) {
    printf("[Failed: Border Router already started]\n");
    return;
  }
  // call BR start API
  sl_wisun_app_br_core_start();
}
#endif

static sl_status_t _load_common_params_from_phy_cfg(void)
{
  static sl_wisun_phy_config_t phy_cfg = { 0U };
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
  if (!app_wisun_setting_is_notified(APP_SETTING_NOTIFICATION_SET_PHY_CFG,
                                     APP_SETTING_DEFAULT_SUBSCRIPT_CH)) {
    return SL_STATUS_OK;
  }
#endif

  (void) app_wisun_setting_get_phy(&phy_cfg);

  // FAN 1.0 setting
  if (phy_cfg.type == SL_WISUN_PHY_CONFIG_FAN10) {
    _phy_common_params.regulatory_domain  = phy_cfg.config.fan10.reg_domain;
    _phy_common_params.operating_mode     = phy_cfg.config.fan10.op_mode;
    _phy_common_params.operating_class    = phy_cfg.config.fan10.op_class;

    // FAN 1.1 setting
  } else if (phy_cfg.type == SL_WISUN_PHY_CONFIG_FAN11) {
#if APP_CLI_ENABLE_FAN11_CONFIG
    _phy_common_params.regulatory_domain  = phy_cfg.config.fan11.reg_domain;
    _phy_common_params.chan_plan_id       = phy_cfg.config.fan11.chan_plan_id;
    _phy_common_params.phy_mode_id        = phy_cfg.config.fan11.phy_mode_id;
#else
    (void) 0U;
#endif
    // Explicit setting
  } else if (phy_cfg.type == SL_WISUN_PHY_CONFIG_EXPLICIT) {
#if APP_CLI_ENABLE_EXPLICIT_CONFIG
    _phy_common_params.ch0_frequency      = phy_cfg.config.explicit_plan.ch0_frequency_khz;
    _phy_common_params.number_of_channels = phy_cfg.config.explicit_plan.number_of_channels;
    _phy_common_params.channel_spacing    = phy_cfg.config.explicit_plan.channel_spacing;
    _phy_common_params.phy_mode_id        = phy_cfg.config.explicit_plan.phy_mode_id;
#else
    (void) 0U;
#endif
    // Invalid setting
  } else {
    return SL_STATUS_FAIL;
  }
  _phy_common_params.phy_config_type = (uint8_t)phy_cfg.type;

  app_wisun_setting_ack_notification(APP_SETTING_NOTIFICATION_SET_PHY_CFG,
                                     APP_SETTING_DEFAULT_SUBSCRIPT_CH);
  return SL_STATUS_OK;
}

static sl_status_t _store_common_params_to_phy_cfg(void)
{
  static sl_wisun_phy_config_t phy_cfg = { 0U };

  // FAN 1.0 setting
  if (_phy_common_params.phy_config_type == SL_WISUN_PHY_CONFIG_FAN10) {
    phy_cfg.config.fan10.reg_domain            = _phy_common_params.regulatory_domain;
    phy_cfg.config.fan10.op_mode               = (uint8_t)_phy_common_params.operating_mode;
    phy_cfg.config.fan10.op_class              = _phy_common_params.operating_class;

    // FAN 1.1 setting
  } else if (_phy_common_params.phy_config_type == SL_WISUN_PHY_CONFIG_FAN11) {
#if APP_CLI_ENABLE_FAN11_CONFIG
    phy_cfg.config.fan11.reg_domain            = _phy_common_params.regulatory_domain;
    phy_cfg.config.fan11.chan_plan_id          = _phy_common_params.chan_plan_id;
    phy_cfg.config.fan11.phy_mode_id           = _phy_common_params.phy_mode_id;
#else
    (void) 0U;
#endif
    // Explicit setting
  } else if (_phy_common_params.phy_config_type == SL_WISUN_PHY_CONFIG_EXPLICIT) {
#if APP_CLI_ENABLE_EXPLICIT_CONFIG
    phy_cfg.config.explicit_plan.ch0_frequency_khz  = _phy_common_params.ch0_frequency;
    phy_cfg.config.explicit_plan.number_of_channels = _phy_common_params.number_of_channels;
    phy_cfg.config.explicit_plan.channel_spacing    = _phy_common_params.channel_spacing;
    phy_cfg.config.explicit_plan.phy_mode_id        = _phy_common_params.phy_mode_id;
#else
    (void) 0U;
#endif
    // Invalid setting
  } else {
    return SL_STATUS_FAIL;
  }

  phy_cfg.type = _phy_common_params.phy_config_type;

  app_wisun_setting_set_phy(&phy_cfg);

  return SL_STATUS_OK;
}

#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
/* App CLI getting connection state (join state) */
static sl_status_t _app_cli_get_connection(char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint32_t value;
  const app_enum_t* value_enum;
  sl_wisun_join_state_t join_state = SL_WISUN_JOIN_STATE_DISCONNECTED;
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->output_enum_list == NULL)) {
    return SL_STATUS_FAIL;
  }

  // gets the connection state from the core
  res = sl_wisun_get_join_state(&join_state);
  if (res != SL_STATUS_OK) {
    return res;
  }
  value = (uint32_t)join_state;

  // finds the proper string for the value
  value_enum = entry->output_enum_list;

  if (value_enum->value_str != NULL) {
    while (value_enum) {
      if (value_enum->value_str == NULL) {
        // No matching enumeration found
        return SL_STATUS_FAIL;
      }
      if (value_enum->value == value) {
        // Matching enumeration found
        break;
      }
      value_enum++;
    }
  } else {
    return SL_STATUS_FAIL;
  }

  // creates the value string
  snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%d)", value_enum->value_str, (uint8_t)value_enum->value);

  return SL_STATUS_OK;
}
#endif

/* App CLI setting network name */
static sl_status_t _app_cli_set_network_name(const char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry)
{
  (void)key_str;
  (void)entry;

  return app_wisun_setting_set_network_name(value_str);
}

/* App CLI setting network size */
static sl_status_t _app_cli_set_network_size(const char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint32_t value = 0U;
  (void)key_str;
  (void)entry;

  res = app_util_get_integer(&value,
                             value_str,
                             entry->input_enum_list,
                             entry->input & APP_CLI_INPUT_FLAG_SIGNED);
  if (res == SL_STATUS_OK) {
    // sets the network size
    res = app_wisun_setting_set_network_size((uint8_t*)&value);
  }

  return res;
}

/* App CLI setting tx power */
static sl_status_t _app_cli_set_tx_power(const char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry)
{
  sl_status_t ret = SL_STATUS_FAIL;
  uint32_t value = 0U;
  (void)key_str;

  ret = app_util_get_integer(&value,
                             value_str,
                             entry->input_enum_list,
                             entry->input & APP_CLI_INPUT_FLAG_SIGNED);
  if (ret == SL_STATUS_OK) {
    // If entry is in dBm, multiply by 10 to have ddBm
    if (entry->value_size == APP_SETTINGS_VALUE_SIZE_UINT8) {
      value = value * 10U;
    }
    // Set the tx power
    ret = app_wisun_setting_set_tx_power((int16_t *)&value);
  }

  return ret;
}

/* App CLI setting network name */
static sl_status_t _app_cli_get_network_name(char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  char name_buffer[APP_SETTING_NETWORK_NAME_MAX_SIZE] = { 0 };
  (void)key_str;
  (void)entry;

  // gets the network name
  res = app_wisun_setting_get_network_name(name_buffer, sizeof(name_buffer));

  if (res == SL_STATUS_OK) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s", name_buffer);
  }

  return res;
}

/* App CLI setting network size */
static sl_status_t _app_cli_get_network_size(char *value_str,
                                             const char *key_str,
                                             const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  const app_enum_t* value_enum;
  uint8_t value = 0U;
  (void)key_str;

  res = app_wisun_setting_get_network_size(&value);
  // finds the proper string for the value
  value_enum = entry->output_enum_list;

  if (res != SL_STATUS_OK || value_enum == NULL || value_enum->value_str == NULL) {
    return SL_STATUS_FAIL;
  }

  while (value_enum) {
    if (value_enum->value_str == NULL) {
      // No matching enumeration found
      return SL_STATUS_FAIL;
    }
    if (value_enum->value == value) {
      // Matching enumeration found
      break;
    }
    value_enum++;
  }

  // creates the value string
  snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%d)", value_enum->value_str, (uint8_t)value_enum->value);

  return res;
}

/* App CLI setting tx power */
static sl_status_t _app_cli_get_tx_power(char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry)
{
  sl_status_t ret = SL_STATUS_FAIL;
  int16_t tx_power_ddbm = 0;
  (void)key_str;

  ret = app_wisun_setting_get_tx_power(&tx_power_ddbm);
  if (ret == SL_STATUS_OK) {
    if (entry->value_size == APP_SETTINGS_VALUE_SIZE_UINT8) {
      // If entry is in dBm, print in float format
      snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%d.%d", tx_power_ddbm / 10, abs(tx_power_ddbm % 10));
    } else if (entry->value_size == APP_SETTINGS_VALUE_SIZE_UINT16) {
      snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%d", tx_power_ddbm);
    } else {
      ret = SL_STATUS_FAIL;
      snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "N/A");
    }
  } else {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "N/A");
  }

  return ret;
}

static bool _is_param_used(const char *param_str)
{
  const app_enum_t *iter = NULL;

  iter = (const app_enum_t *) _phy_cfg_map;

  while (iter != NULL && iter->value_str != NULL) {
    if (iter->value == _phy_common_params.phy_config_type
        && !strncmp(param_str, iter->value_str, APP_CLI_STR_VALUE_LENGTH)) {
      return true;
    }
    ++iter;
  }

  return false;
}

__STATIC_INLINE const char *_param_used_flag_str(const char *param_str)
{
  return _is_param_used(param_str) ? "" : "(unused)";
}

/* App CLI getting PHY parameters */
static sl_status_t _app_cli_get_phy(char *value_str,
                                    const char *key_str,
                                    const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return res;
  }

  res = _load_common_params_from_phy_cfg();
  if (res != SL_STATUS_OK) {
    return res;
  }

  // Regulatory domain
  if (strstr(entry->key, APP_CLI_PHY_PARAM_REG_DOMAIN_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%u) %s",
             app_wisun_trace_util_reg_domain_to_str(_phy_common_params.regulatory_domain),
             _phy_common_params.regulatory_domain,
             _param_used_flag_str(APP_CLI_PHY_PARAM_REG_DOMAIN_STR));

    // Operating class
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_OP_CLASS_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%u %s",
             _phy_common_params.operating_class,
             _param_used_flag_str(APP_CLI_PHY_PARAM_OP_CLASS_STR));

    // Operating mode
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_OP_MODE_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "0x%x %s",
             _phy_common_params.operating_mode,
             _param_used_flag_str(APP_CLI_PHY_PARAM_OP_MODE_STR));

#if APP_CLI_ENABLE_EXPLICIT_CONFIG
    // Number of channels
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_NUM_OF_CH_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%u %s",
             _phy_common_params.number_of_channels,
             _param_used_flag_str(APP_CLI_PHY_PARAM_NUM_OF_CH_STR));

    // CH0 Frequency
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_CH0_FREQ_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%"PRIu32" %s",
             _phy_common_params.ch0_frequency,
             _param_used_flag_str(APP_CLI_PHY_PARAM_CH0_FREQ_STR));

    // Channel spacing
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_CH_SPACING_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%u) %s",
             app_wisun_trace_util_ch_spacing_to_str(_phy_common_params.channel_spacing),
             _phy_common_params.channel_spacing,
             _param_used_flag_str(APP_CLI_PHY_PARAM_CH_SPACING_STR));
#endif

#if APP_CLI_ENABLE_FAN11_CONFIG
    // Chan plan ID
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_CH_PLAN_ID_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%u %s",
             _phy_common_params.chan_plan_id,
             _param_used_flag_str(APP_CLI_PHY_PARAM_CH_PLAN_ID_STR));
#endif

#if APP_CLI_ENABLE_EXPLICIT_CONFIG || APP_CLI_ENABLE_FAN11_CONFIG
    // PHY mode ID
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_PHY_MODE_ID_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%u %s",
             _phy_common_params.phy_mode_id,
             _param_used_flag_str(APP_CLI_PHY_PARAM_PHY_MODE_ID_STR));
#endif

    // PHY config type
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_PHY_CFG_TYPE_STR)) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%d)",
             app_wisun_trace_util_phy_cfg_type_to_str(_phy_common_params.phy_config_type),
             _phy_common_params.phy_config_type);

    // Not found
  } else {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/* App CLI getting PHY parameters */
static sl_status_t _app_cli_set_phy(const char *value_str,
                                    const char *key_str,
                                    const app_settings_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint32_t value = 0UL;
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return res;
  }
  res = _load_common_params_from_phy_cfg();
  if (res != SL_STATUS_OK) {
    return res;
  }

  res = app_util_get_integer(&value,
                             value_str,
                             entry->input_enum_list,
                             entry->input & APP_SETTINGS_INPUT_FLAG_SIGNED);
  if (res != SL_STATUS_OK) {
    return res;
  }

  // Regulatory domain
  if (strstr(entry->key, APP_CLI_PHY_PARAM_REG_DOMAIN_STR)) {
    _phy_common_params.regulatory_domain = (uint8_t)value;

    // Operating class
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_OP_CLASS_STR)) {
    _phy_common_params.operating_class = (uint8_t)value;

    // Operating mode
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_OP_MODE_STR)) {
    _phy_common_params.operating_mode = (uint16_t)value;

#if APP_CLI_ENABLE_EXPLICIT_CONFIG
    // Number of channels
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_NUM_OF_CH_STR)) {
    _phy_common_params.number_of_channels = (uint16_t)value;

    // CH0 Frequency
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_CH0_FREQ_STR)) {
    _phy_common_params.ch0_frequency = value;

    // Channel spacing
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_CH_SPACING_STR)) {
    _phy_common_params.channel_spacing = (uint8_t)value;
#endif

#if APP_CLI_ENABLE_FAN11_CONFIG
    // Chan plan ID
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_CH_PLAN_ID_STR)) {
    _phy_common_params.chan_plan_id = (uint8_t)value;
#endif

#if APP_CLI_ENABLE_EXPLICIT_CONFIG || APP_CLI_ENABLE_FAN11_CONFIG
    // PHY mode ID
  } else if (strstr(entry->key, APP_CLI_PHY_PARAM_PHY_MODE_ID_STR)) {
    _phy_common_params.phy_mode_id = (uint8_t)value;
#endif
    // Not found
  } else {
    return SL_STATUS_FAIL;
  }

  _store_common_params_to_phy_cfg();

  return SL_STATUS_OK;
}

#if defined(SL_CATALOG_WISUN_MODE_SWITCH_PRESENT)
/* App CLI getting mode switch counters */
static sl_status_t _app_ms_get_counters(char *value_str,
                                        const char *key_str,
                                        const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  sl_wisun_statistics_t stat;
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return res;
  }

  // gets the statistics that contains the mode switch information
  res = sl_wisun_get_statistics(SL_WISUN_STATISTICS_TYPE_MAC, &stat);

  if (res != SL_STATUS_OK) {
    return res;
  }
  // mode switch TX-RX counters (inc. failed)
  if (strstr(entry->key, "mode_switch_tx_counter")) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%"PRIu32, stat.mac.tx_ms_count);
  } else if (strstr(entry->key, "mode_switch_tx_failed_counter")) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%"PRIu32, stat.mac.tx_ms_failed_count);
  } else if (strstr(entry->key, "mode_switch_rx_counter")) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%"PRIu32, stat.mac.rx_ms_count);
  } else if (strstr(entry->key, "mode_switch_rx_failed_counter")) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%"PRIu32, stat.mac.rx_ms_failed_count);
  } else {
    // nothing to do
  }

  return SL_STATUS_OK;
}
#endif

#if defined(SL_CATALOG_WISUN_APP_CORE_PRESENT)
#if !defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
static sl_status_t _app_set_regulation(const char *value_str,
                                       const char *key_str,
                                       const app_settings_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint32_t value = 0U;
  sl_wisun_app_core_reg_thresholds_t thresholds = { 0U };
  sl_wisun_join_state_t join_state = SL_WISUN_JOIN_STATE_DISCONNECTED;
  const sl_wisun_regulation_params_t *regulation_params = NULL;
  (void)key_str;
  (void)entry;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return SL_STATUS_FAIL;
  }

  // checking if the device connected or not
  res = sl_wisun_get_join_state(&join_state);
  if (res != SL_STATUS_OK) {
    return res;
  } else {
    if (join_state != SL_WISUN_JOIN_STATE_DISCONNECTED) {
      printf("[Regulation related settings unavailable, disconnection is needed]\n");
      return SL_STATUS_FAIL;
    }
  }

  res = app_util_get_integer(&value,
                             value_str,
                             entry->input_enum_list,
                             entry->input & APP_CLI_INPUT_FLAG_SIGNED);

  if (res != SL_STATUS_OK) {
    printf("[Failed: Get value error: %"PRIu32"]\n", res);
    return res;
  }

  if (strstr(entry->key, "regulation")) {
    // sets the thresholds
    (void)sl_wisun_app_core_get_regulation_thresholds(&thresholds);
    res = sl_wisun_set_regulation_tx_thresholds(thresholds.warning_threshold,
                                                thresholds.alert_threshold);
    if (res != SL_STATUS_OK) {
      printf("[Failed: unable to set regulation TX thresholds: %"PRIu32"]\n", res);
      return res;
    }

    // sets regulation
    switch ((sl_wisun_regulation_t)value) {
      case SL_WISUN_APP_CORE_REGULATION_NONE:
        regulation_params = &SL_WISUN_REGULATION_PARAMS_NONE;
        break;

      case SL_WISUN_APP_CORE_REGULATION_ARIB:
        regulation_params = &SL_WISUN_REGULATION_PARAMS_ARIB;
        break;

      case SL_WISUN_APP_CORE_REGULATION_WPC:
        regulation_params = &SL_WISUN_REGULATION_PARAMS_WPC;
        break;

      default:
        printf("[Failed: unsupported regulation]\n");
        return SL_STATUS_FAIL;
    }

    res = sl_wisun_set_regulation_parameters(regulation_params);
    if (res != SL_STATUS_OK) {
      printf("[Regulation not valid]\n");
      return res;
    } else {
      _app_regulation = (sl_wisun_regulation_t)value;
    }

    // sets status of regulation
    if ((sl_wisun_regulation_t)value == SL_WISUN_APP_CORE_REGULATION_NONE) {
      sl_wisun_app_core_set_regulation_active(false);
    } else {
      sl_wisun_app_core_set_regulation_active(true);
    }
  }

  return SL_STATUS_OK;
}

static sl_status_t _app_set_regulation_warning_threshold(const char *value_str,
                                                         const char *key_str,
                                                         const app_settings_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint32_t value = 0U;
  sl_wisun_app_core_reg_thresholds_t thresholds = { 0U };
  sl_wisun_join_state_t join_state = SL_WISUN_JOIN_STATE_DISCONNECTED;
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return SL_STATUS_FAIL;
  }

  // checking if the device connected or not
  res = sl_wisun_get_join_state(&join_state);
  if (res != SL_STATUS_OK) {
    return res;
  } else {
    if (join_state != SL_WISUN_JOIN_STATE_DISCONNECTED) {
      printf("[Regulation related settings unavailable, disconnection is needed]\n");
      return SL_STATUS_FAIL;
    }
  }

  res = app_util_get_integer(&value,
                             value_str,
                             entry->input_enum_list,
                             entry->input & APP_CLI_INPUT_FLAG_SIGNED);

  if (res != SL_STATUS_OK) {
    printf("[Failed: Get value error: %"PRIu32"]\n", res);
    return res;
  }

  if (strstr(entry->key, "regulation_warning_threshold")) {
    (void)sl_wisun_app_core_get_regulation_thresholds(&thresholds);
    res = sl_wisun_set_regulation_tx_thresholds((int8_t)value, thresholds.alert_threshold);
    if (res != SL_STATUS_OK) {
      printf("[Failed: unable to set regulation TX warning threshold: %"PRIu32"]\n", res);
      return res;
    } else {
      sl_wisun_app_core_set_regulation_thresholds((int8_t)value, thresholds.alert_threshold);
    }
  }

  return SL_STATUS_OK;
}

static sl_status_t _app_set_regulation_alert_threshold(const char *value_str,
                                                       const char *key_str,
                                                       const app_settings_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint32_t value = 0U;
  sl_wisun_app_core_reg_thresholds_t thresholds = { 0U };
  sl_wisun_join_state_t join_state = SL_WISUN_JOIN_STATE_DISCONNECTED;
  (void)key_str;
  (void)entry;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return SL_STATUS_FAIL;
  }

  // checking if the device connected or not
  res = sl_wisun_get_join_state(&join_state);
  if (res != SL_STATUS_OK) {
    return res;
  } else {
    if (join_state != SL_WISUN_JOIN_STATE_DISCONNECTED) {
      printf("[Regulation related settings unavailable, disconnection is needed]\n");
      return SL_STATUS_FAIL;
    }
  }

  res = app_util_get_integer(&value,
                             value_str,
                             entry->input_enum_list,
                             entry->input & APP_CLI_INPUT_FLAG_SIGNED);

  if (res != SL_STATUS_OK) {
    printf("[Failed: Get value error: %"PRIu32"]\n", res);
    return res;
  }

  if (strstr(entry->key, "regulation_alert_threshold")) {
    (void)sl_wisun_app_core_get_regulation_thresholds(&thresholds);
    res = sl_wisun_set_regulation_tx_thresholds(thresholds.warning_threshold, (int8_t)value);
    if (res != SL_STATUS_OK) {
      printf("[Failed: unable to set regulation TX alert threshold: %"PRIu32"]\n", res);
      return res;
    } else {
      sl_wisun_app_core_set_regulation_thresholds(thresholds.warning_threshold, (int8_t)value);
    }
  }

  return SL_STATUS_OK;
}

static sl_status_t _app_get_regulation(char *value_str,
                                       const char *key_str,
                                       const app_cli_entry_t *entry)
{
  const app_enum_t *value_enum = NULL;
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return SL_STATUS_FAIL;
  }

  if (!strstr(entry->key, "regulation")) {
    return SL_STATUS_FAIL;
  }

  // finds the proper string for the value for regulation
  value_enum = entry->output_enum_list;
  if (value_enum == NULL || value_enum->value_str == NULL) {
    return SL_STATUS_FAIL;
  }

  while (value_enum) {
    if (value_enum->value_str == NULL) {
      // No matching enumeration found
      return SL_STATUS_FAIL;
    }
    if (value_enum->value == _app_regulation) {
      // Matching enumeration found
      break;
    }
    value_enum++;
  }

  snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%d)",
           value_enum->value_str, (uint8_t)value_enum->value);

  return SL_STATUS_OK;
}

static sl_status_t _app_get_regulation_warning_threshold(char *value_str,
                                                         const char *key_str,
                                                         const app_cli_entry_t *entry)
{
  sl_wisun_app_core_reg_thresholds_t thresholds = { 0U };
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return SL_STATUS_FAIL;
  }

  if (strstr(entry->key, "regulation_warning_threshold")) {
    (void)sl_wisun_app_core_get_regulation_thresholds(&thresholds);
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%d",
             thresholds.warning_threshold);
  }

  return SL_STATUS_OK;
}

static sl_status_t _app_get_regulation_alert_threshold(char *value_str,
                                                       const char *key_str,
                                                       const app_cli_entry_t *entry)
{
  sl_wisun_app_core_reg_thresholds_t thresholds = { 0U };
  (void)key_str;

  if ((value_str == NULL) || (entry == NULL) || (entry->key == NULL)) {
    return SL_STATUS_FAIL;
  }

  if (strstr(entry->key, "regulation_alert_threshold")) {
    (void)sl_wisun_app_core_get_regulation_thresholds(&thresholds);
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%d",
             thresholds.alert_threshold);
  }

  return SL_STATUS_OK;
}
#endif

static sl_status_t _app_settings_set_mac_address(const char *value_str,
                                                 const char *key_str,
                                                 const app_settings_entry_t *entry)
{
  sl_status_t ret = SL_STATUS_FAIL;
  sl_wisun_mac_address_t address;
  (void)key_str;
  (void)entry;

  ret = app_util_get_mac_address(&address, value_str);
  if (ret == SL_STATUS_OK) {
    ret = sl_wisun_set_mac_address(&address);
  }

  return ret;
}

static sl_status_t _app_settings_get_mac_address(char *value_str,
                                                 const char *key_str,
                                                 const app_settings_entry_t *entry)
{
  sl_status_t ret = SL_STATUS_FAIL;
  sl_wisun_mac_address_t address;
  (void)key_str;
  (void)entry;

  ret = sl_wisun_get_mac_address(&address);
  if (ret == SL_STATUS_OK) {
    ret = app_util_get_mac_address_string(value_str, &address);
  }

  return ret;
}
#endif

#if defined(SL_CATALOG_WISUN_STACK_DEBUG_PRESENT) \
  && SL_WISUN_CLI_ENABLED                         \
  && SL_WISUN_TRACE_LEVEL_CONTROL_CLI_ENABLED
static bool _trace_level_add_entry(sl_wisun_trace_group_config_t *trace_config,
                                   uint8_t *group_count,
                                   uint8_t group_id,
                                   uint8_t trace_level)
{
  for (uint8_t i = 0; i < *group_count; i++) {
    if (trace_config[i].group_id == group_id) {
      // Found existing group. Update it.
      trace_config[i].trace_level = trace_level;
      return true;
    }
  }

  // Group not found. Add new one at the current count index.
  trace_config[*group_count].group_id = group_id;
  trace_config[*group_count].trace_level = trace_level;
  (*group_count)++;
  return false;
}
#endif

#if defined(SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT)
static sl_status_t _app_get_device_type(char *value_str,
                                        const char *key_str,
                                        const app_cli_entry_t *entry)
{
  const char *dev_type_str = NULL;
  sl_wisun_device_type_t dev_type = SL_WISUN_ROUTER;
  (void) entry;
  (void) key_str;

  if (value_str == NULL) {
    return SL_STATUS_FAIL;
  }

  dev_type =  sl_wisun_app_core_get_device_type();
  dev_type_str = app_wisun_trace_util_device_type_to_str((uint32_t) dev_type);

  if (dev_type_str == NULL) {
    return SL_STATUS_FAIL;
  }

  (void) snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%"PRIu32")", dev_type_str, (uint32_t) dev_type);
  return SL_STATUS_OK;
}

static sl_status_t _app_get_lfn_profile(char *value_str,
                                        const char *key_str,
                                        const app_cli_entry_t *entry)
{
  const char *lfn_profile_str = NULL;
  sl_wisun_lfn_profile_t lfn_profile = SL_WISUN_LFN_PROFILE_TEST;
  (void) entry;
  (void) key_str;

  if (value_str == NULL) {
    return SL_STATUS_FAIL;
  }

  lfn_profile =  sl_wisun_app_core_get_lfn_profile();
  lfn_profile_str = app_wisun_trace_util_lfn_profile_to_str((uint32_t) lfn_profile);

  if (lfn_profile_str == NULL) {
    return SL_STATUS_FAIL;
  }

  (void) snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%"PRIu32")", lfn_profile_str, (uint32_t) lfn_profile);
  return SL_STATUS_OK;
}
#endif

#if defined(SL_CATALOG_WISUN_BR_STACK_PRESENT)
static sl_status_t _app_cli_get_state(char *value_str,
                                      const char *key_str,
                                      const app_cli_entry_t *entry)
{
  (void)key_str;

  sl_wisun_br_state_t state = SL_WISUN_BR_STATE_INITIALIZED;
  uint8_t value_length = sizeof(sl_wisun_br_state_t);

  if (sl_wisun_br_get_state(&state) != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }

  return app_util_get_string(value_str,
                             state,
                             entry->output_enum_list,
                             false,
                             false,
                             value_length);
}
#endif

#if defined(SL_CATALOG_WISUN_BR_WIFI_PRESENT)
/* Convert MAC address to string */
static sl_status_t _get_mac_address_string(char *value_str,
                                           const uint8_t *value)
{
  for (uint8_t i = 0U; i < 6U; ++i) {
    sprintf(value_str, "%02x:", value[i]);
    value_str += 3;
  }

  // Remove the last colon
  *(value_str - 1) = '\0';

  return SL_STATUS_OK;
}
/* App CLI getting Wi-Fi information */
static sl_status_t _app_cli_get_wifi_info(char *value_str,
                                          const char *key_str,
                                          const app_cli_entry_t *entry)
{
  sl_status_t status = SL_STATUS_OK;
  bool connected = false;
  uint16_t channel_number = 0U;
  uint8_t mac_address[6] = { 0U };
  uint8_t ipv6_address[16] = { 0U };
  char str[40] = { 0 };

  (void)value_str;
  (void)key_str;

  status = sl_wisun_br_wifi_get_info(&connected, &channel_number, mac_address, ipv6_address);
  if (status != SL_STATUS_OK) {
    printf("[Failed to retrieve Wi-Fi information: %"PRIu32"]\n", status);
    return SL_STATUS_FAIL;
  }

  if (strcmp(entry->key, "wlan_state") == 0) {
    printf("%s.%s = %s\n", app_settings_domain_str[entry->domain], entry->key, connected ? "connected" : "disconnected");
  } else if (strcmp(entry->key, "channel_number") == 0) {
    printf("%s.%s = %d\n", app_settings_domain_str[entry->domain], entry->key, channel_number);
  } else if (strcmp(entry->key, "mac_address") == 0) {
    _get_mac_address_string(str, mac_address);
    printf("%s.%s = %s\n", app_settings_domain_str[entry->domain], entry->key, str);
  } else if (strcmp(entry->key, "ipv6_address") == 0) {
    ip6tos(ipv6_address, str);
    printf("%s.%s = %s\n", app_settings_domain_str[entry->domain], entry->key, str);
  }

  // Prevent parent from printing anything
  return SL_STATUS_FAIL;
}

static sl_status_t _app_cli_set_ssid(const char *value_str,
                                     const char *key_str,
                                     const app_cli_entry_t *entry)
{
  (void)key_str;
  (void)entry;

  return app_wisun_setting_set_ssid((const uint8_t *)value_str);
}

static sl_status_t _app_cli_get_ssid(char *value_str,
                                     const char *key_str,
                                     const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint8_t ssid_buffer[SL_WISUN_WIFI_SSID_SIZE + 1] = { 0U };
  (void)key_str;
  (void)entry;

  // gets the wifi ssid
  res = app_wisun_setting_get_ssid(ssid_buffer, sizeof(ssid_buffer));

  if (res == SL_STATUS_OK) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s", ssid_buffer);
  }

  return res;
}

static sl_status_t _app_cli_set_security(const char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint32_t value = 0U;
  (void)key_str;
  (void)entry;

  res = app_util_get_integer(&value,
                             value_str,
                             entry->input_enum_list,
                             entry->input & APP_CLI_INPUT_FLAG_SIGNED);
  if (res == SL_STATUS_OK) {
    // sets the security
    res = app_wisun_setting_set_security((uint8_t*)&value);
  }

  return res;
}

static sl_status_t _app_cli_get_security(char *value_str,
                                         const char *key_str,
                                         const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  const app_enum_t *value_enum;
  uint8_t value = 0U;
  (void)key_str;

  res = app_wisun_setting_get_security(&value);
  // finds the proper string for the value
  value_enum = entry->output_enum_list;

  if (res != SL_STATUS_OK || value_enum == NULL || value_enum->value_str == NULL) {
    return SL_STATUS_FAIL;
  }

  while (value_enum) {
    if (value_enum->value_str == NULL) {
      // No matching enumeration found
      return SL_STATUS_FAIL;
    }
    if (value_enum->value == value) {
      // Matching enumeration found
      break;
    }
    value_enum++;
  }

  // creates the value string
  snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s (%d)", value_enum->value_str, (uint8_t)value_enum->value);

  return res;
}

static sl_status_t _app_cli_set_passphrase(const char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry)
{
  (void)key_str;
  (void)entry;

  return app_wisun_setting_set_passphrase((const uint8_t *)value_str);
}

static sl_status_t _app_cli_get_passphrase(char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry)
{
  sl_status_t res = SL_STATUS_FAIL;
  uint8_t passphrase_buffer[SL_WISUN_WIFI_PASSPHRASE_SIZE + 1] = { 0U };
  (void)key_str;
  (void)entry;

  // gets the wifi passphrase
  res = app_wisun_setting_get_passphrase(passphrase_buffer, sizeof(passphrase_buffer));

  if (res == SL_STATUS_OK) {
    snprintf(value_str, APP_CLI_STR_VALUE_LENGTH, "%s", passphrase_buffer);
  }

  return res;
}
#endif

static sl_status_t _app_cli_get_statistics(char *value_str,
                                           const char *key_str,
                                           const app_cli_entry_t *entry)
{
  sl_status_t ret;
  const app_cli_entry_t *iter;
  sl_wisun_statistics_type_t statistics_type;

  // API statistics type is stored in the settings entry value.
  #ifdef __GNUC__
  statistics_type = (sl_wisun_statistics_type_t) entry->value;
  #elif defined __ICCARM__
  statistics_type = (sl_wisun_statistics_type_t)((uint32_t) entry->value & 0xFF);
  #endif

  // Update statistics
  ret = sl_wisun_get_statistics(statistics_type, &app_statistics);
  if (ret != SL_STATUS_OK) {
    printf("[Failed to retrieve statistics: %"PRIu32"]\r\n", ret);
    return SL_STATUS_FAIL;
  }

  iter = app_statistics_entries;
  while (iter->key) {
    if (!strcmp(entry->key, app_statistics_domain_str[iter->domain])) {
      if (!key_str || !strcmp(iter->key, key_str)) {
        if (iter->get_handler) {
          ret = iter->get_handler(value_str, NULL, iter);
          if (ret == SL_STATUS_OK) {
            printf("%s.%s.%s = %s\r\n", app_settings_domain_str[entry->domain], app_statistics_domain_str[iter->domain], iter->key, value_str);
          }
        }
      }
    }
    iter++;
  }

  // Prevent parent from printing anything
  return SL_STATUS_FAIL;
}
