/***************************************************************************//**
 * @file fuzz_parseimageinfo.c
 * @brief Fuzz Test parseImageInfo
 * @author Silicon Labs
 * @version 1.7.0
 *******************************************************************************
 * @section License
 * <b>Copyright 2016 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/
#include <stdint.h>
#include <stdlib.h>

#include "core/btl_parse.h"

#include <string.h>

int LLVMFuzzerTestOneInput(uint8_t *Data, size_t Size)
{
  if ((Size < 16) || (Size > 4096)) {
    return -1;
  }

  BootloaderParserContext_t  context;
  ApplicationData_t          appinfo;
  uint32_t                   bootloaderVersion;

  memset(&context, 0, sizeof(BootloaderParserContext_t));

  parser_init(&context.parserContext,
              &context.decryptContext,
              &context.authContext,
              0U);

  core_parseImageInfo(&context,
                      Data,
                      Size,
                      &appinfo,
                      &bootloaderVersion);
  return 0;
}
