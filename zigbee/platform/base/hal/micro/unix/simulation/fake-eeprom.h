/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#define FAKE_EEPROM_DEFAULT_PAGE_SIZE     256
#define FAKE_EEPROM_DEFAULT_PAGE_ERASE_MS 50
#define FAKE_EEPROM_DEFAULT_WORD_SIZE     1

// Called by the application to setup the characteristics of the EEPROM.
void setupFakeEeprom(uint32_t partSize,
                     uint32_t addressBase,
                     uint16_t pageSize,
                     uint32_t pageEraseMs,
                     bool pageEraseRequired,
                     uint16_t wordSizeBytes);

uint32_t getFakeEepromSize(void);
uint8_t* getFakeEepromPointer(void);

// Called by the simulator after a reboot.
void reinitializeFakeEeprom(uint32_t size);
