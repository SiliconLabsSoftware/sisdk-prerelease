#undef  BOARD_HEADER
#undef  CONFIGURATION_HEADER
#undef  HAL_CONFIG
#undef  MODULE_LIST
#undef  EMLIB_USER_CONFIG

#define STACK 1
#define PHY_THIS 1
#define PHY_INCLUDE_SOURCE 1
#ifdef SL_ZIGBEE_TEST
#define PHY_EM250   1
#include "../em250/phy.c"     // We use legacy PHY for dualphy simulation.
#else   // !SL_ZIGBEE_TEST
#define LEGACY_PHY_BUILD 1
#define PHY_DUAL 1
#define PHY_RAIL 1
#include "./phy.c"
#endif  // SL_ZIGBEE_TEST
