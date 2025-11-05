/***************************************************************************//**
 * @file fuzz_define_mainstage_btl.c
 * @brief Defines MainBootloader Table for Fuzzing
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
#include <stddef.h>
#include <stdbool.h>
#include "api/btl_interface.h"

MainBootloaderTable_t dummyTable;
MainBootloaderTable_t *mainBootloaderTable = &dummyTable;

#define APP_START 0x08006000UL
#define APP_END 0x8100000UL

void setmainBootloaderTable()
{
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)APP_START;
  mainBootloaderTable->endOfAppSpace = APP_END;
  return;
}
