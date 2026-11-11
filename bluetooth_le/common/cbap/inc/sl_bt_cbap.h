/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing header
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
#ifndef SL_BT_CBAP_H
#define SL_BT_CBAP_H

// -----------------------------------------------------------------------------
// Includes

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "sl_bt_cbap_config.h"

/**************************************************************************//**
 * @addtogroup sl_bt_cbap Certificate Based Authentication and Pairing
 * @brief Authenticate Bluetooth peer devices with certificates, and bond with
 * them using authenticated passkey pairing.
 *
 * Certificate Based Authentication and Pairing (CBAP) lets two devices that
 * were provisioned under the same certificate authority authenticate each
 * other, and bond with LE Secure Connections passkey entry pairing. The
 * passkey is exchanged over a session that is protected by the device
 * certificates, so the bond is authenticated by the certificates.
 *
 * What CBAP achieves beyond this depends on the peer device:
 * - Between two devices running this component, the bond is established
 *   without user interaction. The exchanged passkey takes the place of the
 *   user, who would otherwise have to type it in.
 * - Between a device running this component and a mobile device, such as an
 *   Android or iOS phone, the bond is established with user interaction on
 *   the mobile device only. The Bluetooth stack of a mobile device always
 *   reports the IO capability KeyboardDisplay, and its applications can
 *   neither change it nor install a passkey of their own choice. Therefore,
 *   the mobile device enters the passkey, and its user takes part in the
 *   pairing. The device running this component remains headless.
 *
 * In both cases, the IO capability that this component reports to the
 * Bluetooth stack describes the part that the device takes in passkey entry
 * pairing, not its user interface. A device that reports DisplayOnly displays
 * the passkey to no one, and a device that reports KeyboardOnly enters the
 * passkey without a keyboard: the passkey reaches the peer device over the
 * CBAP session. The reported IO capabilities serve only to make the Bluetooth
 * stacks select passkey entry pairing, with the device that displays and the
 * device that enters the passkey as CBAP requires.
 *
 * The central device drives the procedure through the CBAP GATT service of
 * the peripheral device. Each characteristic carries its value in fragments
 * that fit into a single ATT PDU. The first byte of every fragment is 1 if
 * another fragment follows, and 0 if it is the last one. The procedure has
 * the following steps:
 *
 * 1. Certificate exchange. A prover presents its batch and device
 *    certificates. A verifier validates them against the factory
 *    certificate it was provisioned with.
 * 2. Session key exchange. Each device sends an ephemeral P-256 public key.
 *    A prover signs its key with the private key of its device certificate,
 *    and a verifier checks that signature. This proves that the peer owns
 *    the certificates it presented, which are public and could be replayed
 *    otherwise. Both devices derive an AES-128-CCM key and nonce from the
 *    ECDH shared secret using HKDF-SHA256.
 * 3. Pairing parameters exchange. The peripheral generates a random six-digit
 *    passkey and installs it in the Bluetooth stack. It indicates the passkey
 *    to the central together with its IO capability, encrypted with the
 *    session key.
 * 4. Pairing. The central decrypts the pairing parameters and starts LE
 *    Secure Connections passkey entry pairing. The peripheral also sends a
 *    Security Request, for a central device that does not start the pairing
 *    by itself, such as a phone. If bonding is enabled, see
 *    @ref SL_BT_CBAP_BONDING, the pairing creates a bond, which is stored in
 *    the bonding database.
 *
 * In passkey entry pairing, one device displays the passkey and the other one
 * enters it. A peripheral device running this component displays the
 * passkey, and the central device enters it. A peripheral device with a
 * keyboard, such as a phone, may not be able to install a passkey of its own
 * choice in its Bluetooth stack, and enters the passkey instead. Therefore,
 * the central device adapts to the IO capability of the peripheral device: it
 * enters the passkey if the peripheral device has no keyboard, and displays
 * it otherwise. A peripheral device that can neither display nor enter a
 * passkey fails the procedure. The device that displays the passkey installs
 * it in its Bluetooth stack, the device that enters the passkey supplies it
 * when its Bluetooth stack requests it:
 * - Two devices running this component: the peripheral generates and
 *   displays the passkey, the central enters it.
 * - A central running this component and a mobile peripheral: the mobile
 *   peripheral generates the passkey and reports KeyboardDisplay. The central
 *   switches to DisplayOnly and displays the passkey, the mobile device
 *   enters it.
 * - A mobile central and a peripheral running this component: the peripheral
 *   generates and displays the passkey, the mobile device enters it.
 *
 * In every setup, a central running this component starts the pairing with
 * sl_bt_sm_increase_security(), and a peripheral running this component
 * sends a Security Request with the same function. A mobile central starts
 * the pairing in response to the Security Request.
 *
 * With bonding enabled, a peer device that already has a bond skips these
 * steps. Its stored key restores the authenticated encryption of the link
 * instead, which the peripheral asks the central for with a Security Request.
 *
 * A peer device may have lost the bond, for example because it was reset, or
 * because the user of a phone removed the device. Then it rejects the stored
 * key, or asks for a new pairing instead. The component deletes the stale
 * bond, closes the connection, and reports SL_STATUS_DELETED, so that the next
 * connection runs the CBAP procedure. A new bond requires the CBAP procedure,
 * so a peer device that only claims to have lost the bond gains nothing.
 *
 * With bonding disabled, the pairing creates no bond, and its keys are
 * discarded once the connection is closed, so every connection runs the CBAP
 * procedure. The component deletes all bonds of the bonding database at boot,
 * so that a bond stored while bonding was enabled does not skip the
 * procedure either. Both devices have to use the same setting: a device with
 * bonding enabled refuses a pairing that does not create a bond.
 *
 * A device in the Verifier or Full role only bonds with a peer device that
 * can prove itself, that is, a device in the Prover or Full role. The table
 * shows the outcome for each combination of roles, with the role of the
 * central device in the rows and the role of the peripheral device in the
 * columns. A cell names the device that verifies its peer device when the two
 * devices bond: "Mutual" means both of them, "None" means neither of them.
 *
 * | Central \\ Peripheral | Full       | Prover  | Verifier   |
 * |-----------------------|------------|---------|------------|
 * | Full                  | Mutual     | Central | Fails      |
 * | Prover                | Peripheral | None    | Peripheral |
 * | Verifier              | Fails      | Central | Fails      |
 *
 * @note Two devices in the Prover role bond without either of them checking
 * the other, and the procedure still reports success. Only use the Prover role
 * alone where the peer device is trusted by other means.
 *
 * The component owns the Security Manager configuration of the Bluetooth
 * stack: it requires Secure Connections pairing with MITM protection, and
 * bonding if bonding is enabled, and it confirms a bonding request only once
 * the CBAP procedure has authenticated the peer device. It sets the IO
 * capability of the peripheral device to DisplayOnly, and the one of the
 * central device to KeyboardOnly, which the central device changes to
 * DisplayOnly for the duration of a procedure with a peripheral device that
 * has a keyboard. The application must not change the configuration of the
 * Security Manager. It is left with advertising, scanning and opening
 * connections. The component serves a
 * single procedure at a time, and closes the connection of a peer device that
 * fails it.
 *
 * A successful procedure leaves the link in security mode 1, level 4. This
 * only protects the characteristics of the local GATT database that require an
 * authenticated and encrypted connection! Any other characteristic stays
 * accessible to every connected device!
 * @{
 *****************************************************************************/

// -----------------------------------------------------------------------------
// Defines

// The UUIDs are listed in little-endian byte order, as the GATT API of the
// Bluetooth stack expects them.

/// UUID of the CBAP service (d8818cc6-8147-414d-918e-dd40d0285610).
#define SL_BT_CBAP_SERVICE_UUID                   \
  0x10, 0x56, 0x28, 0xd0, 0x40, 0xdd, 0x8e, 0x91, \
  0x4d, 0x41, 0x47, 0x81, 0xc6, 0x8c, 0x81, 0xd8
/// UUID of the characteristic that indicates the batch certificate of the
/// peripheral device to the central device.
#define SL_BT_CBAP_BATCH_CERT_SERVER_CHAR_UUID    \
  0xe9, 0x4c, 0x3d, 0x5b, 0x8e, 0x9e, 0xb5, 0x81, \
  0x2e, 0x40, 0x1a, 0x19, 0x37, 0x69, 0x18, 0x31
/// UUID of the characteristic that the central device writes its batch
/// certificate to.
#define SL_BT_CBAP_BATCH_CERT_CLIENT_CHAR_UUID    \
  0xca, 0x25, 0xf3, 0xa2, 0xf6, 0xda, 0xbd, 0xb6, \
  0x69, 0x4a, 0xaa, 0x08, 0xf9, 0xd0, 0x5f, 0x31
/// UUID of the characteristic that indicates the device certificate of the
/// peripheral device to the central device.
#define SL_BT_CBAP_DEVICE_CERT_SERVER_CHAR_UUID   \
  0x16, 0xef, 0xa6, 0xce, 0x9b, 0xdc, 0x64, 0x9b, \
  0xc3, 0x48, 0xe7, 0x8c, 0x11, 0x08, 0x98, 0xbb
/// UUID of the characteristic that the central device writes its device
/// certificate to.
#define SL_BT_CBAP_DEVICE_CERT_CLIENT_CHAR_UUID   \
  0x66, 0x50, 0xfd, 0x84, 0x4d, 0xad, 0xa2, 0x99, \
  0xc9, 0x4f, 0xf5, 0x16, 0x9e, 0xda, 0xf6, 0x0c
/// UUID of the characteristic that indicates the session key of the
/// peripheral device to the central device.
#define SL_BT_CBAP_SESSION_KEY_SERVER_CHAR_UUID   \
  0x88, 0x72, 0x4c, 0x4d, 0x6f, 0x9e, 0x5b, 0xab, \
  0xcc, 0x5f, 0x9d, 0x3e, 0x2f, 0x4b, 0x8d, 0x1e
/// UUID of the characteristic that the central device writes its session key
/// to.
#define SL_BT_CBAP_SESSION_KEY_CLIENT_CHAR_UUID   \
  0x77, 0x61, 0x2b, 0x3c, 0x5e, 0x8f, 0xa4, 0x9a, \
  0xbb, 0x4e, 0x8c, 0x2d, 0x1f, 0x3a, 0x7c, 0x0d
/// UUID of the characteristic that indicates the encrypted pairing parameters
/// of the peripheral device to the central device: the passkey and the IO
/// capability of the peripheral device.
#define SL_BT_CBAP_PAIRING_PARAMETERS_SERVER_CHAR_UUID \
  0x6e, 0x94, 0x6f, 0x90, 0x91, 0xb0, 0xd7, 0xcd,      \
  0xee, 0x61, 0xbf, 0x50, 0x4f, 0x51, 0xaf, 0x30

// -----------------------------------------------------------------------------
// Data types

/// Characteristics of the CBAP service, in the order the central device
/// discovers them. A server characteristic carries a value of the peripheral
/// device in indications, a client characteristic carries a value of the
/// central device in write requests.
typedef enum {
  SL_BT_CBAP_CHAR_BATCH_CERT_SERVER,  ///< Batch certificate of the peripheral.
  SL_BT_CBAP_CHAR_BATCH_CERT_CLIENT,  ///< Batch certificate of the central.
  SL_BT_CBAP_CHAR_DEVICE_CERT_SERVER, ///< Device certificate of the peripheral.
  SL_BT_CBAP_CHAR_DEVICE_CERT_CLIENT, ///< Device certificate of the central.
  SL_BT_CBAP_CHAR_SESSION_KEY_SERVER, ///< Session key of the peripheral.
  SL_BT_CBAP_CHAR_SESSION_KEY_CLIENT, ///< Session key of the central.
  SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER, ///< Encrypted pairing parameters
                                             ///< of the peripheral.
  SL_BT_CBAP_CHAR_COUNT               ///< Number of CBAP characteristics.
} sl_bt_cbap_characteristics_t;

/// Connection that a CBAP procedure was run on.
typedef struct {
  uint8_t handle;  ///< Connection handle.
  bd_addr address; ///< Bluetooth address of the peer device, as reported by
                   ///< the connection opened event.
} sl_bt_cbap_conn_t;

/**************************************************************************//**
 * Report the result of a CBAP procedure.
 *
 * The component calls it once for every connection it serves: when the
 * procedure succeeds, when it fails, and when the connection closes before the
 * procedure completes.
 *
 * On success, the link is encrypted in security mode 1, level 4, and a bond
 * with the peer device is stored if bonding is enabled. A new peer device got
 * there through the CBAP procedure, a bonded peer device by restoring the
 * encryption with its stored key. On failure, the component has already
 * closed the connection, or the connection was already closed.
 *
 * The component no longer serves the connection when it calls the callback,
 * so the application can start looking for another peer device from it.
 *
 * @param[in] conn Connection handle and address of the peer device.
 * @param[in] sc SL_STATUS_OK on success. Otherwise, the reason of the failure:
 *   - SL_STATUS_INVALID_SIGNATURE: the peer device failed to authenticate
 *     itself. Its certificate chain, the signature of its session key, or
 *     the tag of its encrypted pairing parameters is invalid.
 *   - SL_STATUS_INVALID_CREDENTIALS: the pairing did not use the passkey that
 *     CBAP exchanged, did not reach security mode 1, level 4, or did not
 *     create a bond although bonding is enabled.
 *   - SL_STATUS_INVALID_STATE: the peer device broke the order of the
 *     procedure, its CBAP role does not work together with the local one, or
 *     it tried to pair before the procedure completed.
 *   - SL_STATUS_NOT_FOUND: the peer device lacks the CBAP service or one of
 *     its characteristics.
 *   - SL_STATUS_NOT_SUPPORTED: the peer device can neither display nor enter
 *     a passkey.
 *   - SL_STATUS_DELETED: the bonded peer device has lost the bond, and the
 *     component deleted the stale bond. The next connection runs the CBAP
 *     procedure. Only reported if bonding is enabled.
 *   - SL_STATUS_TIMEOUT: the procedure did not reach the pairing within
 *     @ref SL_BT_CBAP_PROCEDURE_TIMEOUT_MS.
 *   - Any other code: for example the reason the pairing failed for, such as
 *     the user of a mobile device cancelling it or entering a wrong passkey,
 *     the reason the connection was closed for, a malformed value from the
 *     peer device, or a local error.
 *
 *   The first five codes are deterministic: another attempt with the same
 *   peer device would end the same way. A new attempt after any other failure
 *   starts with a new passkey.
 *
 * @note Success only means that the peer device was authenticated if the local
 * device is a verifier. A device in the Prover role alone does not check the
 * certificates of its peer device.
 *****************************************************************************/
typedef void (*sl_bt_cbap_cb_t)(sl_bt_cbap_conn_t conn, sl_status_t sc);

// -----------------------------------------------------------------------------
// Public function declarations

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * Initialize the CBAP component.
 *
 * Call it once, before the Bluetooth stack boots, for example from app_init().
 * The component configures the Security Manager of the stack when it boots,
 * and ignores every Bluetooth event until it is initialized.
 *
 * From then on, the component starts a CBAP procedure on every connection that
 * is opened, as long as no other procedure is in progress. A connection opened
 * while another procedure is in progress is closed.
 *
 * @param[in] cb Function that the result of every procedure is reported to.
 * @param[in] central True if the device runs in the central (GATT client)
 *                    role, false if it runs in the peripheral (GATT server)
 *                    role.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_NULL_POINTER if @p cb is NULL,
 *         SL_STATUS_ALREADY_INITIALIZED if the component was already
 *         initialized.
 *****************************************************************************/
sl_status_t sl_bt_cbap_init(sl_bt_cbap_cb_t cb, bool central);

/**************************************************************************//**
 * Check whether the component is serving a connection.
 *
 * A procedure is in progress from the moment a connection is opened until its
 * result is reported to the callback. This includes restoring the encryption
 * of a bonded peer device. Use it to hold back advertising or scanning, as the
 * component closes any connection opened in the meantime.
 *
 * @return true if a CBAP procedure is in progress, false otherwise.
 *****************************************************************************/
bool sl_bt_cbap_is_procedure_in_progress(void);

#ifdef __cplusplus
}
#endif

/** @} (end addtogroup sl_bt_cbap) */
#endif // SL_BT_CBAP_H
