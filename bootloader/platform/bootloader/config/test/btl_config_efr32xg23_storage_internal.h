/***************************************************************************//**
 * @file
 * @brief Configuration header for EFM32/EFR32 bootloader.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef BTL_CONFIG_STORAGE_H
#define BTL_CONFIG_STORAGE_H

// --------------------------------
// Bootloader settings

#define BOOTLOADER_ENFORCE_SIGNED_UPGRADE               1
#define BOOTLOADER_ENFORCE_ENCRYPTED_UPGRADE            0
#define BOOTLOADER_ENFORCE_SECURE_BOOT                  1
#define BOOTLOADER_USE_SYMMETRIC_KEY_FROM_SE_STORAGE    0
#define APPLICATION_WRITE_DISABLE                       0
#define BOOTLOADER_ROLLBACK_PROTECTION                  0
#define BOOTLOADER_ROLLBACK_PROTECTION_MINIMUM_VERSION  0
//#define BOOTLOADER_SUPPORT_CERTIFICATES               0
#define BOOTLOADER_REJECT_DIRECT_SIGNED_IMG             0
//#define BOOTLOADER_FALLBACK_LEGACY_KEY                0
#define BOOTLOADER_WRITE_DISABLE                        0
#define APPLICATION_VERIFICATION_SKIP_EM4_RST           0

// --------------------------------
// Bootloader Memory Layout

#define STORAGE_BASE                      0x08000000UL
#define BTL_UPGRADE_LOCATION_BASE         (0x10000UL) // Page nr 8

#define BTL_APP_SPACE_SIZE                0x30000UL

// --------------------------------
// Storage component configuration

//#define BOOTLOADER_SUPPORT_STORAGE
#define BOOTLOADER_SUPPORT_RAW_STORAGE

#define BTL_STORAGE_NUM_SLOTS 2
#define BTL_STORAGE_SLOTS       \
  {                             \
    {                           \
      (STORAGE_BASE + 0x40000), \
      0x18000                   \
    },                          \
    {                           \
      (STORAGE_BASE + 0x58000), \
      0x18000                   \
    }                           \
  }

#define BTL_SLOT_BASE_ADDRESS STORAGE_BASE + 0x40000

#define BTL_STORAGE_BASE_ADDRESS STORAGE_BASE + 0x30000

// Number of slots in bootload list
#define BTL_STORAGE_BOOTLOAD_LIST_LENGTH  BTL_STORAGE_NUM_SLOTS

// Enable internal flash storage component
#define BTL_PLUGIN_STORAGE_INTERNAL_FLASH

// --------------------------------

#endif // BTL_CONFIG_STORAGE_H
