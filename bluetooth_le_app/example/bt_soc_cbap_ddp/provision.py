#!/usr/bin/env python3

# Copyright 2026 Silicon Laboratories Inc. www.silabs.com
#
# SPDX-License-Identifier: Zlib
#
# The licensor of this software is Silicon Laboratories Inc.
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.

'''Dynamic Data Provisioning (DDP) Host Tool for Certificate Based Authentication and Pairing (CBAP)

First, this script establishes RTT connection to the device.
Then uploads and runs the DDP RAM application.
And finally, runs the DDP commands in order to:
- Generate a device key pair.
- Generate static authentication data.
- Generate a common name, based on the device's UUID.
- Build the device certificate and sign it with the issuer.
- Inject the certificate into the device.
- Inject the issuer certificate into the device as well.
'''

# Metadata
__author__ = 'Silicon Laboratories, Inc'
__copyright__ = 'Copyright 2026, Silicon Laboratories, Inc.'

import sys
import os
import argparse
import datetime
import cryptography
from cryptography import x509
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
sys.path.append(os.path.join(os.path.dirname(__file__), "autogen"))
sys.path.append(os.path.join(os.path.dirname(__file__), "script"))
from ddp_rtt import RTT
from sl_ddp_rtt_config import *
from cbap_key_id import *
from ddp_cmd_nvm import nvm_set
from ddp_cmd_psa import *
from ddp_cmd_cert import common_name_gen

DEFAULT_ISSUER_CERT                 = os.path.join(os.path.dirname(__file__), "script/ca_demo/certificate.pem")
DEFAULT_ISSUER_KEY                  = os.path.join(os.path.dirname(__file__), "script/ca_demo/private_key.pem")

DEFAULT_VALIDITY                    = 365
DEFAULT_POLICY_OID                  = None
DEFAULT_COUNTRY                     = 'US'
DEFAULT_STATE                       = 'Texas'
DEFAULT_LOCALITY                    = 'Austin'
DEFAULT_ORGANIZATION                = 'Silicon Laboratories'
DEFAULT_ORGANIZATIONAL_UNIT         = 'Wireless'
DEFAULT_EMAIL_ADDRESS               = 'support@silabs.com'

# Key arguments
PSA_KEY_USAGE_EXPORT                = 0x00000001
PSA_KEY_USAGE_SIGN_MESSAGE          = 0x00000400
PSA_KEY_USAGE_VERIFY_MESSAGE        = 0x00000800

PSA_KEY_BITS                        = (32 * 8)

PSA_ALG_NONE                        = 0x00000000
PSA_ALG_ECDSA_BASE                  = 0x06000600
PSA_ALG_HASH_MASK                   = 0x000000ff
PSA_ALG_SHA_256                     = 0x02000009

PSA_KEY_TYPE_RAW_DATA               = 0x1001
PSA_KEY_TYPE_ECC_KEY_PAIR_BASE      = 0x7100
PSA_ECC_FAMILY_SECP_R1              = 0x12

SIGNATURE_HASH_ALGORITHM            = cryptography.hazmat.primitives.hashes.SHA256()

key_att = KeyAtt(
    usage_flags = PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE,
    bits        = PSA_KEY_BITS,
    algo        = PSA_ALG_ECDSA_BASE | (PSA_ALG_SHA_256 & PSA_ALG_HASH_MASK),
    key_type    = PSA_KEY_TYPE_ECC_KEY_PAIR_BASE | PSA_ECC_FAMILY_SECP_R1,
    key_id      = CBAP_PSA_DEVICE_KEY
)

auth_data_att = KeyAtt(
    usage_flags = PSA_KEY_USAGE_EXPORT,
    bits        = PSA_KEY_BITS,
    algo        = PSA_ALG_NONE,
    key_type    = PSA_KEY_TYPE_RAW_DATA,
    key_id      = CBAP_PSA_AUTH_DATA
)

def main(app=None,
         serial=None,
         ip=None,
         device_opn=DEVICE_OPN,
         ram_addr=RAM_ADDR,
         j_link_devices=None,
         issuer_cert=DEFAULT_ISSUER_CERT,
         issuer_key=DEFAULT_ISSUER_KEY,
         validity=DEFAULT_VALIDITY,
         policy_oid=DEFAULT_POLICY_OID,
         country=DEFAULT_COUNTRY,
         state=DEFAULT_STATE,
         locality=DEFAULT_LOCALITY,
         organization=DEFAULT_ORGANIZATION,
         organizational_unit=DEFAULT_ORGANIZATIONAL_UNIT,
         email_address=DEFAULT_EMAIL_ADDRESS):
    # Get provisioning application binary if not provided
    if app == None:
        root = os.path.dirname(__file__)
        print(f"Provisioning application was not defined. Searching for the binary under {root} (recursively)...")
        app = _get_default_provisioning_app(root)

    print("Configuration")
    print(f"\tProvisioning application: {app}")
    if serial:
        print(f"\tDevice serial: {serial}")
    elif ip:
        print(f"\tDevice IP: {ip}")
    else:
        print(f"\tAuto device detection")
    print(f"\tDEVICE_OPN: {device_opn}")
    print(f"\tRAM_ADDR: {hex(ram_addr)}")
    print(f"\tIssuer")
    print(f"\t\tCertificate: {issuer_cert}")
    print(f"\t\tPrivate key: {issuer_key}")
    print(f"\tCertificate validity: {validity}[days]")
    print(f"\tCertificate object identifier: {policy_oid}")
    print(f"\tCertificate subjects")
    print(f"\t\tcountry: {country}")
    print(f"\t\tstate: {state}")
    print(f"\t\tlocality: {locality}")
    print(f"\t\torganization: {organization}")
    print(f"\t\torganizational_unit: {organizational_unit}")
    print(f"\t\temail_address: {email_address}")

    # Check inputs
    if not os.path.exists(app):
        raise FileNotFoundError(f"Provisioning application binary cannot be found at: {app}")
    if not app.lower().endswith('.bin'):
        raise FileNotFoundError("Provisioning application must be a '.bin' file.")

    if j_link_devices and not os.path.exists(j_link_devices):
        raise FileNotFoundError(f"JLinkDevices.xml file was specified, but cannot be found at: {j_link_devices}")

    if validity < 1:
        raise Exception('Valid period must be greater than or equal to one day!')

    if not os.path.exists(issuer_cert):
        raise FileNotFoundError(f"Issuer (CA) certificate cannot be found at: {issuer_cert}")
    if not issuer_cert.lower().endswith('.pem'):
        raise FileNotFoundError("Issuer (CA) certificate must be in PEM format.")

    if not os.path.exists(issuer_key):
        raise FileNotFoundError(f"Issuer (CA) private key cannot be found at: {issuer_key}")
    if not issuer_key.lower().endswith('.pem'):
        raise FileNotFoundError("Issuer (CA) private key must be in PEM format.")

    if os.path.samefile(issuer_cert, DEFAULT_ISSUER_CERT) or os.path.samefile(issuer_key, DEFAULT_ISSUER_KEY):
        print("Default certificate authority is used as the issuer. This is only meant to be used for demo purposes. " \
              "In production, make sure to create and provide your own certificate authority!")

    with open(issuer_cert, 'rb') as f:
        issuer_cert = x509.load_pem_x509_certificate(f.read())
    with open(issuer_key, 'rb') as f:
        issuer_key = serialization.load_pem_private_key(f.read(), password=None)

    # Check device key attributes. Current implementation only supports the SECP256R1 curve.
    if (key_att.key_type != (PSA_KEY_TYPE_ECC_KEY_PAIR_BASE | PSA_ECC_FAMILY_SECP_R1)) \
        or (key_att.bits != PSA_KEY_BITS):
        raise NotImplementedError("Device key EC curve was changed which is not supported. " \
            "Please update the implementation.")

    # Connect to the device
    print("Connecting...")
    rtt = RTT(device_opn, serial, ip, xml_path=j_link_devices)
    rtt.connect()
    print(f"Connected to JLink chip: {rtt.chip_name}\tserial: {rtt.jlink.serial_number}")

    try:
        print(f"Running provisioning application.{os.linesep}")
        with open(app, 'rb') as file:
            rtt.run_application(ram_addr, file.read())
        rtt.rtt_start()

        # Generate key pair. Export the raw uncompressed EC point of the public key.
        print("Generating device key...")
        status, device_key = psa_key_gen(rtt, key_att)
        assert status == 0, f"PSA key generation failure: {status:#06x}"
        device_key_public = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), device_key)
        print(f"Device public key received.{os.linesep}")

        # Generate and get static authentication data.
        print("Generating static authentication data...")
        status, static_auth_data = psa_key_gen(rtt, auth_data_att)
        assert status == 0, f"PSA key generation failure: {status:#06x}"
        print(f"Static authentication data received.{os.linesep}") # Ready for further processing.

        # Generate and get common name (UUID).
        print("Generating common name (UUID)...")
        status, common_name = common_name_gen(rtt)
        assert status == 0, f"Common name generation failure: {status:#06x}"
        common_name = common_name.decode('ascii').rstrip('\x00')
        print(f"Common name (UUID) received: {common_name}{os.linesep}")

        # Create certificate. Configure fields.
        print("Creating device certificate...")
        certificate = _create_certificate(device_key_public,
                                          common_name,
                                          validity,
                                          policy_oid,
                                          country,
                                          state,
                                          locality,
                                          organization,
                                          organizational_unit,
                                          email_address,
                                          issuer_cert,
                                          issuer_key)
        certificate = certificate.public_bytes(serialization.Encoding.DER) # Export in binary (DER) format
        print(f"Device certificate was created with success ({len(certificate)}[B]).{os.linesep}")

        # Inject device certificate.
        print("Injecting device certificate...")
        status = nvm_set(rtt, CBAP_NVM_DEVICE_CERT, certificate)
        assert status == 0, f"Set NVM failure: {status:#06x}"

        # Inject root (issuer) certificate.
        issuer_cert = issuer_cert.public_bytes(serialization.Encoding.DER) # Export in binary (DER) format
        print("Injecting root (issuer) certificate...")
        status = nvm_set(rtt, CBAP_NVM_ROOT_CERT, issuer_cert)
        assert status == 0, f"Set NVM failure: {status:#06x}"

    finally:
        print("Cleaning up.")
        rtt.rtt_stop()
        rtt.reset()
        rtt.close()

def _create_certificate(public_key,
                        common_name,
                        validity,
                        policy_oid,
                        country,
                        state,
                        locality,
                        organization,
                        organizational_unit,
                        email_address,
                        issuer_cert,
                        issuer_key):
    # Pack certificate subjects
    subjects = x509.Name([
        x509.NameAttribute(x509.oid.NameOID.COMMON_NAME, common_name),
        x509.NameAttribute(x509.oid.NameOID.COUNTRY_NAME, country),
        x509.NameAttribute(x509.oid.NameOID.STATE_OR_PROVINCE_NAME, state),
        x509.NameAttribute(x509.oid.NameOID.LOCALITY_NAME, locality),
        x509.NameAttribute(x509.oid.NameOID.ORGANIZATION_NAME, organization),
        x509.NameAttribute(x509.oid.NameOID.ORGANIZATIONAL_UNIT_NAME, organizational_unit),
        x509.NameAttribute(x509.oid.NameOID.EMAIL_ADDRESS, email_address),
    ])

    # Create certificate
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (x509.CertificateBuilder()
        .public_key(public_key)
        .subject_name(subjects)
        .issuer_name(issuer_cert.subject)
        .serial_number(x509.random_serial_number()) # TODO: Generate a unique serial number which is not present in the database.
        .not_valid_before(now)
        .not_valid_after(now + datetime.timedelta(days=validity)))

    # Add extensions
    cert = cert.add_extension(x509.SubjectKeyIdentifier.from_public_key(public_key), critical=False)
    cert = cert.add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)

    # Allow the certificate's public key to be used for key agreement (needed for secure pairing).
    cert = cert.add_extension(x509.KeyUsage(digital_signature=False,
                                            content_commitment=False,
                                            key_encipherment=False,
                                            data_encipherment=False,
                                            key_agreement=True,
                                            key_cert_sign=False,
                                            crl_sign=False,
                                            encipher_only=False,
                                            decipher_only=False),
                              critical=True)

    # Add certificate policy if present
    if policy_oid is not None:
        obj_id = x509.ObjectIdentifier(policy_oid)
        policy_info = [x509.PolicyInformation(obj_id, [])]
        cert = cert.add_extension(x509.CertificatePolicies(policies=policy_info), critical=True)

    # Verify issuer
    # Check the validity of the higher authority.
    # Note: if there is a revocation list, it should be also checked if the certificate is revoked or not.
    if now < issuer_cert.not_valid_before_utc or issuer_cert.not_valid_after_utc < now:
        raise Exception('The validity period of the issuer (CA) has expired.')

    # Sign certificate
    cert = cert.sign(issuer_key, SIGNATURE_HASH_ALGORITHM)

    # Verify signature.
    issuer_cert.public_key().verify(cert.signature,
                                    cert.tbs_certificate_bytes,
                                    ec.ECDSA(SIGNATURE_HASH_ALGORITHM))

    # Check if the public key can be used for key agreement (needed for secure pairing).
    if cert.extensions.get_extension_for_class(x509.KeyUsage).value.key_agreement != True:
        raise Exception("KeyUsage key_agreement should be set to True!")

    # TODO
    #add_certificate_to_database(cert, path_database)

    return cert

def _get_default_provisioning_app(root):
    candidates = []
    for dirpath, _, filenames in os.walk(root):
        for fname in filenames:
            if fname.lower() == 'bt_soc_cbap_ddp.bin':
                candidates.append(os.path.join(dirpath, fname))

    if not candidates:
        raise FileNotFoundError("No provisioning application found. (Was the project built with success?)")
    elif len(candidates) > 1:
        print("More binaries were found:")
        for c in candidates:
            print(c)
        print("Please specify the provisioning application.")
        raise FileExistsError()

    return candidates[0]

class CustomFormatter(argparse.ArgumentDefaultsHelpFormatter, argparse.RawDescriptionHelpFormatter):
    pass

if __name__ == '__main__':
    # Parse command line arguments.
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=CustomFormatter)
    parser.epilog = (
        f'examples:{os.linesep}'
        f'\t%(prog)s                                           Try to autodetect device and provisioning app.{os.linesep}'
        f'\t%(prog)s --issuer_cert <PATH> --issuer_key <PATH>  Specify custom certificate authority as the issuer (recommended).{os.linesep}'
        f'\t%(prog)s --app build/debug/bt_soc_cbap_ddp.bin     Specify provisioning app binary.{os.linesep}'
        f'\t%(prog)s --serial 440192051                        Connect to device with the given J-Link serial.{os.linesep}'
        f'\t%(prog)s --ip 192.168.0.143                        Connect to device with the given IP address.{os.linesep}'
        f'\t%(prog)s --device EFR32MG21A010F1024IM32           Override device OPN.{os.linesep}'
        f'\t%(prog)s --ram 536870912                           Override RAM starting address.{os.linesep}'
        f'\t%(prog)s --j_link_devices jlink/JLinkDevices.xml   Extend/overwrite J-Link devices database.{os.linesep}'
    )

    parser.add_argument('-a', '--app', help='Path to the provisioning application binary, in ".bin" format.')

    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument('-s', '--serial', help='J-Link serial number.', type=int)
    group.add_argument('-i', '--ip', help='IP Address.')

    parser.add_argument('-d', '--device', help='Override device OPN.', default=DEVICE_OPN)
    parser.add_argument('-r', '--ram', help='Override RAM start address.', default=RAM_ADDR, type=int)
    parser.add_argument('--j_link_devices', help='JLinkDevices.xml file to extend (or overwrite) J-Link devices database with new devices.')

    parser.add_argument('--issuer_cert',
                        default=DEFAULT_ISSUER_CERT,
                        help='Specifies the location for issuer (CA) certificate in PEM format. (default: %(default)s)')
    parser.add_argument('--issuer_key',
                        default=DEFAULT_ISSUER_KEY,
                        help='Specifies the location for issuer (CA) EC private key in PEM format. (default: %(default)s)')

    parser.add_argument('--validity',
                        default=DEFAULT_VALIDITY,
                        type=int,
                        help='The valid period of the certificate in days starting '\
                             'from the moment of generation.')
    parser.add_argument('--policy_oid',
                        default=DEFAULT_POLICY_OID,
                        help='The optional Certificate Policy Information extension. '\
                             'Only a policy OID is supported.')

    parser.add_argument('--country',
                        default=DEFAULT_COUNTRY,
                        type=str.upper,
                        help='The country subject of the x509 certificate.')
    parser.add_argument('--state',
                        default=DEFAULT_STATE,
                        help='The state subject of the x509 certificate.')
    parser.add_argument('--locality',
                        default=DEFAULT_LOCALITY,
                        help='The locality subject of the x509 certificate.')
    parser.add_argument('--organization',
                        default=DEFAULT_ORGANIZATION,
                        help='The organization subject of the x509 certificate.')
    parser.add_argument('--organizational_unit',
                        default=DEFAULT_ORGANIZATIONAL_UNIT,
                        help='The organizational unit subject of the x509 certificate.')
    parser.add_argument('--email_address',
                        default=DEFAULT_EMAIL_ADDRESS,
                        help='The e-mail address unit subject of the x509 certificate.')

    args = parser.parse_args()

    if bool(args.issuer_cert) != bool(args.issuer_key):
        parser.error("--issuer_cert and --issuer_key must be provided together.")

    main(args.app,
         args.serial,
         args.ip,
         args.device,
         args.ram,
         args.j_link_devices,
         args.issuer_cert,
         args.issuer_key,
         args.validity,
         args.policy_oid,
         args.country,
         args.state,
         args.locality,
         args.organization,
         args.organizational_unit,
         args.email_address)
