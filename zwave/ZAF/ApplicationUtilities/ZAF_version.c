#include "ZAF_version.h"

uint8_t ZAF_GetVersionMajor(void)
{
  return (uint8_t)(ZAF_VERSION_MAJOR);
}

uint8_t ZAF_GetVersionMinor(void)
{
  return (uint8_t)(ZAF_VERSION_MINOR);
}

uint8_t ZAF_GetVersionPatch(void)
{
  return (uint8_t)(ZAF_VERSION_PATCH);
}

uint8_t SDK_GetVersionMajor(void)
{
  return (uint8_t)(SDK_VERSION_MAJOR);
}

uint8_t SDK_GetVersionMinor(void)
{
  return (uint8_t)(SDK_VERSION_MINOR);
}

uint8_t SDK_GetVersionPatch(void)
{
  return (uint8_t)(SDK_VERSION_PATCH);
}
