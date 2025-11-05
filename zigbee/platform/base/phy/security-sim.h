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
#ifndef __SIM_SECURITY_H__
#define __SIM_SECURITY_H__

#define SECURITY_M    0
#define SECURITY_NONCE_TOTAL_SIZE 0

#define pseudoEncryptOffset 0x55

extern uint8_t emSecurityKey[];

extern uint8_t standardMic[];

sli_parcel_t *encryptParcel(sli_parcel_t *clearText);
sli_parcel_t *encryptParcelNoMic(sli_parcel_t *clearText);
sli_parcel_t *standardMicParcel(void);

#endif //__SIM_SECURITY_H__
