#undef  BOARD_HEADER
#undef  CONFIGURATION_HEADER
#undef  HAL_CONFIG
#undef  MODULE_LIST
#undef  EMLIB_USER_CONFIG

#define STACK 1

#define PHY_THIS 2
#define PHY_INCLUDE_SOURCE 1
#ifdef SL_ZIGBEE_TEST
#define PHY_TRANSCEIVER_SIM 1
#include "../transceiver/phy-trx-sim.c"
#else   // !SL_ZIGBEE_TEST
#define LEGACY_PHY_BUILD 1
#define PHY_DUAL 1
#define PHY_PRO2PLUS 1
#include "./phy.c"
#endif  // SL_ZIGBEE_TEST
