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

'''Dynamic Data Provisioning (DDP) Host Tool for CBAP

Provision a Silicon Labs device with data required by Certificate Based
Authentication and Pairing (CBAP):

    - Device EC key pair (NIST P-256)
    - Static authentication data
    - Device X.509 certificate, signed by an issuer Certificate Authority
    - Issuer (root) certificate stored in device PSA ITS

Connection and application loading depend on the target family:

    Non-xG22 devices
        Connect over RTT (J-Link), upload the DDP RAM application, and run it.

    xG22 family (limited memory)
        The provisioning application must be flashed beforehand. Connect over
        VCOM (serial) to the running application.

Prerequisites:
    - Python 3.9 or higher.
    - The Python packages listed in `requirements.txt`. Install them with:
          python -m pip install -r requirements.txt
    - A valid issuer Certificate Authority. Create one with
      ``certificate_manager.py`` or use the bundled demo CA for evaluation
      only.
'''
# Metadata
__author__ = 'Silicon Laboratories, Inc'
__copyright__ = 'Copyright 2026, Silicon Laboratories, Inc.'

import sys
import re
import argparse
import datetime
from pathlib import Path
from typing import Optional

from cryptography import x509
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization

sys.path.append(str(Path(__file__).parent / "autogen"))
sys.path.append(str(Path(__file__).parent / "script"))

from ddp_conn import RttConnection, SerialConnection
from sl_ddp_conn_config import *
from cbap_key_id import *
from ddp_cmd_psa import *
from ddp_cmd_cert import common_name_gen

from certificate_manager import (
    CertificateAuthority,
    add_certificate_args,
    CustomFormatter,
    DEFAULT_VALIDITY,
    DEFAULT_POLICY_OID,
    DEFAULT_COUNTRY,
    DEFAULT_STATE,
    DEFAULT_LOCALITY,
    DEFAULT_ORGANIZATION,
    DEFAULT_ORGANIZATIONAL_UNIT,
    DEFAULT_EMAIL_ADDRESS,
)

DEMO_CA_DIR = (Path(__file__).parent / "script" / "ca_demo").resolve()
DEMO_CA_LEVEL = 2  # Batch certificate

DEFAULT_BAUDRATE = 115200

# PSA Crypto constants for device key generation.
PSA_KEY_USAGE_EXPORT = 0x00000001
PSA_KEY_USAGE_SIGN_MESSAGE = 0x00000400
PSA_KEY_USAGE_VERIFY_MESSAGE = 0x00000800

PSA_KEY_BITS = (32 * 8)

PSA_ALG_NONE = 0x00000000
PSA_ALG_ECDSA_BASE = 0x06000600
PSA_ALG_HASH_MASK = 0x000000ff
PSA_ALG_SHA_256 = 0x02000009

PSA_KEY_TYPE_RAW_DATA = 0x1001
PSA_KEY_TYPE_ECC_KEY_PAIR_BASE = 0x7100
PSA_ECC_FAMILY_SECP_R1 = 0x12

EPILOG = '''\
Examples:
    Try to autodetect device and provisioning app, while using the demo CA:
        python %(prog)s
    Specify issuer Certificate Authority (recommended):
        python %(prog)s --ca_dir /path/to/issuers --ca_level 0
    Specify provisioning app binary:
        python %(prog)s --app build/debug/bt_soc_cbap_ddp.bin
    Connect to device with the given J-Link serial:
        python %(prog)s --serial 440192051
    Connect to device over Ethernet at the given IP address:
        python %(prog)s --ip 192.168.0.143
    Connect to device on the given VCOM serial port:
        python %(prog)s --port COM13
    Override the VCOM baud rate:
        python %(prog)s --baudrate 115200
    Override device OPN:
        python %(prog)s --device EFR32MG21A010F1024IM32
    Override RAM starting address:
        python %(prog)s --ram 536870912
    Extend/overwrite J-Link devices database:
        python %(prog)s --j_link_devices jlink/JLinkDevices.xml
'''

key_att = KeyAtt(
    usage_flags=PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE,
    bits=PSA_KEY_BITS,
    algo=PSA_ALG_ECDSA_BASE | (PSA_ALG_SHA_256 & PSA_ALG_HASH_MASK),
    key_type=PSA_KEY_TYPE_ECC_KEY_PAIR_BASE | PSA_ECC_FAMILY_SECP_R1,
    key_id=CBAP_PSA_DEVICE_KEY,
)

auth_data_att = KeyAtt(
    usage_flags=PSA_KEY_USAGE_EXPORT,
    bits=PSA_KEY_BITS,
    algo=PSA_ALG_NONE,
    key_type=PSA_KEY_TYPE_RAW_DATA,
    key_id=CBAP_PSA_AUTH_DATA,
)


def main(app: Optional[Path] = None,
         ca_dir: Path = DEMO_CA_DIR,
         ca_level: int = DEMO_CA_LEVEL,
         serial: Optional[int] = None,
         ip: Optional[str] = None,
         port: Optional[str] = None,
         baudrate: int = DEFAULT_BAUDRATE,
         device_opn: str = DEVICE_OPN,
         ram_addr: int = RAM_ADDR,
         j_link_devices: Optional[Path] = None,
         validity: int = DEFAULT_VALIDITY,
         policy_oid: Optional[str] = DEFAULT_POLICY_OID,
         subj_country: str = DEFAULT_COUNTRY,
         subj_state: str = DEFAULT_STATE,
         subj_locality: str = DEFAULT_LOCALITY,
         subj_organization: str = DEFAULT_ORGANIZATION,
         subj_organizational_unit: str = DEFAULT_ORGANIZATIONAL_UNIT,
         subj_email_address: str = DEFAULT_EMAIL_ADDRESS):
    '''Provision a device with keys, certificate, and issuer cert.

    Connects to the target, runs (or attaches to) the DDP provisioning
    application, generates keys and a device certificate on the host, and
    injects the certificate data into device PSA ITS.

    :param app: Path to the provisioning application ``.bin`` file. When
        omitted on non-xG22 targets, the binary is searched under this script's
        directory.
    :param ca_dir: Working directory containing issuer CA sub-directories
        (see :class:`CertificateAuthority`).
    :param ca_level: Issuer CA level (0=root, 1=factory, 2=batch).
    :param serial: J-Link serial number for device selection.
    :param ip: Device IP address (Ethernet connection).
    :param port: VCOM serial port (e.g. ``COM13`` or ``/dev/ttyACM0``).
    :param baudrate: VCOM baud rate (xG22 serial connection only).
    :param device_opn: Device ordering part number (OPN).
    :param ram_addr: RAM load address for the provisioning application.
    :param j_link_devices: Optional ``JLinkDevices.xml`` extension file.
    :param validity: Device certificate validity period in days.
    :param policy_oid: Optional certificate policy OID.
    :param subj_country: Certificate subject country.
    :param subj_state: Certificate subject state or province.
    :param subj_locality: Certificate subject locality.
    :param subj_organization: Certificate subject organization.
    :param subj_organizational_unit: Certificate subject organizational unit.
    :param subj_email_address: Certificate subject e-mail address.
    :raises FileNotFoundError: If a required file is missing.
    :raises FileExistsError: If multiple provisioning binaries are found.
    :raises ValueError: If the validity period is less than one day.
    :raises NotImplementedError: If unsupported PSA key attributes are used.
    :raises RuntimeError: If a DDP command returns a non-zero status.
    '''
    print("Configuration")

    # Detect whether the target belongs to the xG22 family.
    # The device OPN can be a SoC (e.g. EFR32BG22, EFR32MG22) or a module
    # (e.g. BGM220, MGM220), so we match an optional module 'm' before '22'.
    is_xg22_family = re.search(
        r'[bm]gm?22', device_opn, re.IGNORECASE) is not None

    if not is_xg22_family:
        # Get provisioning application binary if not provided.
        if app is None:
            root = Path(__file__).parent
            print("Provisioning application was not defined. Searching for "
                  f"the binary under {root} (recursively)...")
            app = _get_default_provisioning_app(root)
        else:
            app = Path(app).resolve()
        print(f"\tProvisioning application: {app}")

    print("\tIssuer (CA)")
    ca_dir = ca_dir.resolve()
    print(f"\t\tDirectory: {ca_dir}")
    print(f"\t\tLevel: {ca_level}")

    if serial:
        print(f"\tDevice serial: {serial}")
    elif ip:
        print(f"\tDevice IP: {ip}")
    elif port:
        print(f"\tDevice port: {port}")
        print(f"\tDevice baudrate: {baudrate}")
    else:
        print("\tAuto device detection")

    print(f"\tDEVICE_OPN: {device_opn}. xG22 family: {is_xg22_family}")
    if not is_xg22_family:
        print(f"\tRAM_ADDR: {hex(ram_addr)}")
        print(f"\tJLinkDevices extension: {j_link_devices}")

    print(f"\tCertificate validity: {validity}[days]")
    print(f"\tCertificate object identifier: {policy_oid}")
    print("\tCertificate subjects")
    print(f"\t\tcountry: {subj_country}")
    print(f"\t\tstate: {subj_state}")
    print(f"\t\tlocality: {subj_locality}")
    print(f"\t\torganization: {subj_organization}")
    print(f"\t\torganizational_unit: {subj_organizational_unit}")
    print(f"\t\temail_address: {subj_email_address}")

    # Check inputs.
    if not is_xg22_family:
        if not app.exists():
            raise FileNotFoundError(
                f"Provisioning application binary cannot be found at: {app}")
        if app.suffix.lower() != '.bin':
            raise FileNotFoundError(
                "Provisioning application must be a '.bin' file.")
        if j_link_devices is not None and not j_link_devices.exists():
            raise FileNotFoundError(
                "JLinkDevices.xml file was specified, but cannot be found "
                f"at: {j_link_devices}")

    if validity < 1:
        raise ValueError(
            'Valid period must be greater than or equal to one day!')

    # Current implementation only supports the SECP256R1 curve.
    secp256r1_type = PSA_KEY_TYPE_ECC_KEY_PAIR_BASE | PSA_ECC_FAMILY_SECP_R1
    if (key_att.key_type != secp256r1_type) or (key_att.bits != PSA_KEY_BITS):
        raise NotImplementedError(
            "Device key EC curve was changed which is not supported. "
            "Please update the implementation.")

    # Get issuer certificate authority.
    if ca_dir == DEMO_CA_DIR:
        print("Default certificate authority is used as the issuer. This is "
              "only meant to be used for demo purposes. In production, make "
              "sure to create and provide your own certificate authority!")
    ca = CertificateAuthority.from_level(ca_level, ca_dir)

    # Connect to the device.
    if is_xg22_family:
        conn = SerialConnection(serial_no=serial,
                                port=port,
                                hostname=ip,
                                baudrate=baudrate)
    else:
        conn = RttConnection(chip_name=device_opn,
                             serial_no=serial,
                             hostname=ip,
                             xml_path=j_link_devices)
    conn.connect()

    try:
        if is_xg22_family:
            print("The provisioning application is expected to be flashed "
                  "and running on the device.\n")
        else:
            with open(app, 'rb') as file:
                print("Loading and running provisioning application...\n")
                conn.run_application(ram_addr, file.read())
        conn.start()

        # Generate key pair (raw uncompressed EC point of the public key).
        print("Generating device key...")
        status, device_key = psa_key_gen(conn, key_att)
        if status != 0:
            raise RuntimeError(
                f"PSA key generation failure: {status:#06x}")
        device_key_public = ec.EllipticCurvePublicKey.from_encoded_point(
            ec.SECP256R1(), device_key)
        print("Device public key received.\n")

        # Generate and get static authentication data.
        print("Generating static authentication data...")
        status, static_auth_data = psa_key_gen(conn, auth_data_att)
        if status != 0:
            raise RuntimeError(
                f"PSA key generation failure: {status:#06x}")
        print("Static authentication data received.")
        # static_auth_data is ready for further processing.

        # Generate and get common name (UUID).
        print("Generating common name (UUID)...")
        status, subj_common_name = common_name_gen(conn)
        if status != 0:
            raise RuntimeError(
                f"Common name generation failure: {status:#06x}")
        subj_common_name = subj_common_name.decode('ascii').rstrip('\x00')
        print(f"Common name (UUID) received: {subj_common_name}\n")

        # Pack certificate subjects.
        name_oid = x509.oid.NameOID
        subjects = x509.Name([
            x509.NameAttribute(name_oid.COMMON_NAME, subj_common_name),
            x509.NameAttribute(name_oid.COUNTRY_NAME, subj_country),
            x509.NameAttribute(name_oid.STATE_OR_PROVINCE_NAME, subj_state),
            x509.NameAttribute(name_oid.LOCALITY_NAME, subj_locality),
            x509.NameAttribute(name_oid.ORGANIZATION_NAME, subj_organization),
            x509.NameAttribute(
                name_oid.ORGANIZATIONAL_UNIT_NAME, subj_organizational_unit),
            x509.NameAttribute(name_oid.EMAIL_ADDRESS, subj_email_address),
        ])
        print("Creating device certificate...")
        certificate = _create_certificate(
            ca,
            device_key_public,
            subjects,
            validity,
            policy_oid)
        certificate = certificate.public_bytes(serialization.Encoding.DER)
        print("Device certificate was created with success "
              f"({len(certificate)}[B]).\n")

        # Inject device certificate.
        print("Injecting device certificate...")
        status = psa_its_set(conn, CBAP_PSA_DEVICE_CERT, certificate)
        if status != 0:
            raise RuntimeError(f"Set PSA ITS failure: {status:#06x}")

        # Inject issuer certificate.
        issuer_cert = ca.get_certificate().public_bytes(
            serialization.Encoding.DER)
        print("Injecting root (issuer) certificate...")
        status = psa_its_set(conn, CBAP_PSA_ROOT_CERT, issuer_cert)
        if status != 0:
            raise RuntimeError(f"Set PSA ITS failure: {status:#06x}")

    finally:
        print("Cleaning up.")
        conn.stop()
        conn.close()


def _create_certificate(issuer: CertificateAuthority,
                        public_key: ec.EllipticCurvePublicKey,
                        subject: x509.Name,
                        validity: int,
                        policy_oid: Optional[str] = None) -> x509.Certificate:
    '''Build and sign a device certificate with the issuer CA.

    :param issuer: Certificate Authority that signs the device certificate.
    :param public_key: Device public key (NIST P-256).
    :param subject: X.509 subject name (including device common name).
    :param validity: Validity period in days.
    :param policy_oid: Optional certificate policy OID.
    :returns: The signed device certificate.
    :raises Exception: If the issuer is invalid.
    '''
    if not issuer.is_valid:
        raise Exception('Issuer Certificate Authority is invalid.')

    # Build certificate.
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (
        x509.CertificateBuilder()
        .public_key(public_key)
        .subject_name(subject)
        .issuer_name(issuer.get_certificate().subject)
        .serial_number(issuer.generate_serial_number())
        .not_valid_before(now)
        .not_valid_after(now + datetime.timedelta(days=validity))
    )

    # Add extensions.
    cert = cert.add_extension(
        x509.SubjectKeyIdentifier.from_public_key(public_key),
        critical=False)
    cert = cert.add_extension(
        x509.BasicConstraints(ca=False, path_length=None),
        critical=True)
    if policy_oid is not None:
        policy_info = [
            x509.PolicyInformation(x509.ObjectIdentifier(policy_oid), [])
        ]
        cert = cert.add_extension(
            x509.CertificatePolicies(policies=policy_info),
            critical=True)
    cert = cert.add_extension(
        x509.KeyUsage(digital_signature=False,
                      content_commitment=False,
                      key_encipherment=False,
                      data_encipherment=False,
                      key_agreement=True,  # Required for secure pairing
                      key_cert_sign=False,
                      crl_sign=False,
                      encipher_only=False,
                      decipher_only=False),
        critical=True)

    # Sign certificate.
    return issuer.sign(cert)


def _get_default_provisioning_app(root: Path) -> Path:
    '''Locate the default provisioning application binary under ``root``.

    Recursively searches for ``bt_soc_cbap_ddp.bin`` (case-insensitive).

    :param root: Directory to search.
    :returns: Path to the unique matching binary.
    :raises FileNotFoundError: If no binary is found.
    :raises FileExistsError: If more than one matching binary is found.
    '''
    candidates = [
        path for path in root.rglob('*')
        if path.is_file() and path.name.lower() == 'bt_soc_cbap_ddp.bin'
    ]

    if not candidates:
        raise FileNotFoundError(
            "No provisioning application found. "
            "(Was the project built with success?)")
    if len(candidates) > 1:
        print("More binaries were found:")
        for candidate in candidates:
            print(candidate)
        print("Please specify the provisioning application.")
        raise FileExistsError()

    return candidates[0]


def load_args():
    '''Parse command line arguments.

    :returns: The parsed arguments namespace.
    '''
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog=EPILOG,
        formatter_class=CustomFormatter)

    # Provisioning application
    parser.add_argument(
        '-a', '--app',
        type=Path,
        help='Path to the provisioning application binary, in ".bin" format.')

    # Issuer CA parameters
    parser.add_argument(
        '--ca_dir',
        default=DEMO_CA_DIR,
        type=Path,
        help='Directory containing issuer CA sub-directories. See '
             'certificate_manager.py for CA creation. The bundled demo CA is '
             'for evaluation only. (default: %(default)s)')
    parser.add_argument(
        '--ca_level',
        default=DEMO_CA_LEVEL,
        type=int,
        choices=[level.value for level in CertificateAuthority.Level],
        help='Issuer CA level under --ca_dir (0=root, 1=factory, 2=batch). '
             '(default: %(default)s)')

    # Connection parameters
    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument(
        '-s', '--serial',
        type=int,
        help='J-Link serial number, used to select the matching VCOM port.')
    group.add_argument('-i', '--ip', help='IP address.')
    group.add_argument(
        '-p', '--port',
        help='Serial (VCOM) port, e.g. COM13 or /dev/ttyACM0. Autodetected '
             'if omitted.')
    parser.add_argument(
        '-b', '--baudrate',
        default=DEFAULT_BAUDRATE,
        type=int,
        help='Serial (VCOM) baud rate. Ignored when RTT connection is used.')
    parser.add_argument(
        '-d', '--device',
        default=DEVICE_OPN,
        help='Override device OPN.')
    parser.add_argument(
        '-r', '--ram',
        default=RAM_ADDR,
        type=int,
        help='Override RAM start address.')
    parser.add_argument(
        '--j_link_devices',
        type=Path,
        help='JLinkDevices.xml file to extend (or overwrite) the J-Link '
             'devices database with new devices.')

    # Add common certificate-related arguments, but exclude the common name.
    # Instead, generate it on the device based on its UUID.
    add_certificate_args(parser, common_name=False)
    return parser.parse_args()


if __name__ == '__main__':
    args = load_args()

    main(args.app,
         args.ca_dir,
         args.ca_level,
         args.serial,
         args.ip,
         args.port,
         args.baudrate,
         args.device,
         args.ram,
         args.j_link_devices,
         args.validity,
         args.policy_oid,
         args.country,
         args.state,
         args.locality,
         args.organization,
         args.organizational_unit,
         args.email_address)
