#undef  BOARD_HEADER
#undef  CONFIGURATION_HEADER
#undef  HAL_CONFIG
#undef  MODULE_LIST
#undef  EMLIB_USER_CONFIG

#define STACK 1

#ifdef SL_ZIGBEE_TEST
#define PHY_TRANSCEIVER_SIM 1
#else   // !SL_ZIGBEE_TEST
#define LEGACY_PHY_BUILD 1
#define PHY_DUAL 1
#define PHY_PRO2PLUS 1
#undef PHY_RAIL // Legacy HAL SOC defines this conditionally, but sled doesn't test the conditions.  This is a workaround until we fix the cmake platform variations.
#endif  // SL_ZIGBEE_TEST

#define PHY_THIS 2
#define PHY_INCLUDE_SOURCE 1
#include "../pro2class/radio_config_Si4468_863MHz.c"
