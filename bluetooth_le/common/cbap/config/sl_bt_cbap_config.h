/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing configuration header
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#ifndef SL_BT_CBAP_CONFIG_H
#define SL_BT_CBAP_CONFIG_H

/// The device presents its own certificates to prove itself.
#define SL_BT_CBAP_ROLE_PROVER    (1 << 0)
/// The device validates the certificates of the remote device.
#define SL_BT_CBAP_ROLE_VERIFIER  (1 << 1)
/// The device both proves itself and validates the remote device.
#define SL_BT_CBAP_ROLE_FULL      (SL_BT_CBAP_ROLE_VERIFIER \
                                   | SL_BT_CBAP_ROLE_PROVER)

// <<< Use Configuration Wizard in Context Menu >>>

// <o SL_BT_CBAP_ROLE> CBAP Role
//   <SL_BT_CBAP_ROLE_PROVER=> Prover
//   <SL_BT_CBAP_ROLE_VERIFIER=> Verifier
//   <SL_BT_CBAP_ROLE_FULL=> Both prover and verifier
// <i> The authentication procedure is determined by the CBAP role of the local
// <i> and remote device(s). A prover device sends its device and batch
// <i> certificates to prove itself. A verifier device validates the received
// <i> certificates against its factory and root certificates. This is a one-way
// <i> authentication. For mutual authentication, select SL_BT_CBAP_ROLE_FULL
// <i> for both devices, so they can authenticate each other.
#define SL_BT_CBAP_ROLE                     SL_BT_CBAP_ROLE_FULL

// <q SL_BT_CBAP_BONDING> Bonding
// <i> Default: 1
// <i> Enabled (1): the pairing creates a bond, which is stored in the bonding
// <i> database. On reconnection, a bonded peer device skips the CBAP procedure,
// <i> and its stored key restores the encryption of the link.
// <i> Disabled (0): the pairing creates no bond, its keys are discarded once
// <i> the connection is closed, so every connection runs the CBAP procedure.
// <i> All bonds stored in the bonding database are deleted at boot.
// <i> Both devices have to use the same setting: a device with bonding enabled
// <i> refuses a pairing that does not create a bond.
#define SL_BT_CBAP_BONDING                  (1)

// <o SL_BT_CBAP_PROCEDURE_TIMEOUT_MS> Procedure timeout (ms) <20..65535>
// <d> 30000
// <i> Allowed time in milliseconds from opening a connection until passkey
// <i> entry pairing starts. This includes the certificate, session key and
// <i> pairing parameters exchanges, or restoring the encryption of a bonded
// <i> remote device. The pairing itself may wait for a user who types the
// <i> passkey on a phone, so it is limited by the 30-second timeout of the
// <i> Security Manager Protocol instead. The connection is closed on timeout.
// <i> Every fragment carries at most ATT_MTU - 4 bytes of a value, so the
// <i> exchanges take far longer at the default ATT MTU of 23 bytes: the
// <i> certificates alone take more than a hundred round trips then. The
// <i> remote device should exchange a larger ATT MTU, which an Android
// <i> application has to request explicitly.
#define SL_BT_CBAP_PROCEDURE_TIMEOUT_MS     (30000)

// <o SL_BT_CBAP_CERTIFICATE_MAX_SIZE> Certificate maximum size <512..1024>
// <d> 700
// <i> Size limit in bytes for a single certificate in DER format. It sets the
// <i> size of the buffers that send and receive the certificates.
#define SL_BT_CBAP_CERTIFICATE_MAX_SIZE     (700)

// <<< end of configuration section >>>

// The role selects which certificates the device has to be provisioned with,
// so it has to name at least one of the roles, and nothing besides them.
#if (SL_BT_CBAP_ROLE == 0) || ((SL_BT_CBAP_ROLE & ~SL_BT_CBAP_ROLE_FULL) != 0)
#error "SL_BT_CBAP_ROLE shall be set to SL_BT_CBAP_ROLE_PROVER, " \
  "SL_BT_CBAP_ROLE_VERIFIER or SL_BT_CBAP_ROLE_FULL!"
#endif

#endif // SL_BT_CBAP_CONFIG_H
