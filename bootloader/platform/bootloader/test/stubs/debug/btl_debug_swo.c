#include "btl_debug.h"
#include "btl_debug_cfg.h"
#include <stdio.h>

#if defined (SL_DEBUG_PRINT) && (SL_DEBUG_PRINT == 1)
void btl_debugInit(void)
{
}
void btl_debugWriteChar(char c)
{
  printf("%c", c);
}
#else
void btl_debugInit(void)
{
  (void) 0;
}
void btl_debugWriteChar(char c)
{
  (void) c;
}
#endif
