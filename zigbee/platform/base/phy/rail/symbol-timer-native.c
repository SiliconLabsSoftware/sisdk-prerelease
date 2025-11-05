#undef  BOARD_HEADER
#undef  CONFIGURATION_HEADER
#undef  HAL_CONFIG
#undef  MODULE_LIST
#undef  EMLIB_USER_CONFIG

#define STACK 1
#define LEGACY_PHY_BUILD 1

#define PHY_DUAL 1
#define PHY_RAIL 1
#define PHY_THIS 1
#define PHY_INCLUDE_SOURCE 1
#include "./symbol-timer.c"
