# Copyright: 2023 Silicon Laboratories Inc. www.silabs.com
# License: MSLA
#
# The licensor of this software is Silicon Laboratories Inc. Your use of this
# software is governed by the terms of the Silicon Labs Master Software License
# Agreement (MSLA) available at
# https://www.silabs.com/about-us/legal/master-software-license-agreement
# By installing, copying or otherwise using this software, you agree to the
# terms of the MSLA.
#
--------------------- ZW 700 Bootloaders ------------------------

Author: Anders Lynge Esbensen anders.esbensen@silabs.com
Date: 15/6-2018

Folder contains the source code for the bootloader
for the ZGM13 and EFR32ZG14 targets.

------------------- The 3 bootloaders ----------------------------

There are two types of bootlaoders, a OTA bootloader 
and a OTW bootloader.

The OTA bootloader is an internal flash bootloader as described in
UG266. OTA bootloader is only built for the ZGM13 target, because
it loads its images from the internal flash, hence it needs a big flash.

The OTW bootloader is and XMODEM bootloader and is getting its image
from the UART using the xmodem protocol. See UG266.

The bootloaders has been configured to require images to be signed,
and they support lz4 compression as well as encryption. The compression
and encryption is optional.

The three variants of the bootlaoder is build with the rest of the 
protocol. The built bootloader image are called:
  OTA-bootloader-fg13-combined.s37  
  OTW-bootloader-fg13-combined.s37  
  OTW-bootloader-fg14-combined.s37

They can be flashed to a ZW700 module by the command
 commander flash OTW-bootloader-fg13-combined.s37 -d ZGM13
 commander flash --tokengroup znet --tokenfile sample_encrypt.key --tokenfile sample_sign.key-tokens.txt -d ZGM13

For the gateway devices FG14
commander flash OTW-bootloader-fg14-combined.s37 -d EFR32ZG14
commander flash --tokengroup znet --tokenfile sample_encrypt.key --tokenfile sample_sign.key-tokens.txt -d EFR32ZG14

gbl create appname.gbl  --app appname.hex  --sign sample_sign.key --encrypt sample_encrypt.key  --compress lz4

commander gbl create railtest_efr32.gbl --app railtest_efr32.hex  --sign sample_sign.key --encrypt sample_encrypt.key  --compress lz4
------------------------ Firmware files -------------------------

The CMAKE system has been setup to auto generate application images
which can be read by the bootloaders. The image files a called GBL
files. All images are automatically signed, compressed and encrypted, 
using the keys stored in the keys folder. It is important to note 
the key files supplied with the SDK is sample key files. Each
product vendor MUST create his own keys and keep them safe. Leaking
the keys make the product more vulnerable to hacking, and illegal 
copying.

Creating new signing keys:
# commander gbl keygen --type ecc-p256 -o sample_sign.key

Creating new encryption keys:
# commander gbl keygen --type aes-ccm -o sample_encrypt.key

Add keys to device:
# commander flash --tokengroup znet --tokenfile sample_encrypt.key --tokenfile sample_sign.key-tokens.txt -d ZGM13



------------------------ Source code --------------------

The source for the bootloaders is all taken from the FGSDKs. The 
CMakeLists.txt in this directory only references SDK sources, together
with some header files in the OTW and OTA directories. Each of the 
directories also contain a Simplicity Studio project which can be 
used for changing the configurations. Please note that if the 
bootloader plugin configuration is changed, it may be necessary to 
change the CMakeLists.txt to adopt the changes. Also note that 
Simplicity Studio MUST not be used for compiling the bootloaders, 
they MUST be generated with the CMake build system.

References:
 UG266: Silicon Labs Gecko Bootloader User's Guide
 
------------------------ Example for app using OTA --------------------

1. Flash bootloader: commander flash OTA-bootloader-fg13-combined.s37 -d ZGM13
2. Flash application.
3. Flash token files: commander flash --tokengroup znet --tokenfile sample_encrypt.key --tokenfile sample_sign.key-tokens.txt -d ZGM13