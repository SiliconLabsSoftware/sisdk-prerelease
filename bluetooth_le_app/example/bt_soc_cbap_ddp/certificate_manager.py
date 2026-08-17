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

'''Certificate Manager

Create an EC (NIST P-256) key pair and an X.509 certificate for a Certificate
Authority (CA) of a given level, forming a certificate chain:

    Level 0 (ROOT)    - self-signed root CA
    Level 1 (FACTORY) - intermediate CA, signed by the root CA
    Level 2 (BATCH)   - intermediate CA, signed by the factory CA

Each CA is stored in its own sub-directory of the working directory:

    <working_dir>/ca0_root/
    <working_dir>/ca1_factory/
    <working_dir>/ca2_batch/

and is made up of three files: the private key (private_key.pem), the
certificate (certificate.pem) and a YAML database of the certificates it has
issued (issued_certificates.yaml).

An intermediate CA can only be created once its issuer (the level n-1 CA)
already exists and is valid. The root CA (level 0) is self-signed.

Prerequisites:
    - Python 3.9 or higher.
    - The Python packages listed in `requirements.txt`. Install them with:
          python -m pip install -r requirements.txt
'''
# Metadata
__author__ = 'Silicon Laboratories, Inc'
__copyright__ = 'Copyright 2026, Silicon Laboratories, Inc.'

import os
import stat
import datetime
import argparse
import yaml
from enum import IntEnum
from pathlib import Path
from typing import Optional

import cryptography
from cryptography import x509
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

DEFAULT_WORKING_DIR = Path.home() / ".silabs" / "certificates"
DEFAULT_LEVEL = 0
DEFAULT_VALIDITY = 365
DEFAULT_POLICY_OID = None
DEFAULT_COUNTRY = 'US'
DEFAULT_STATE = 'Texas'
DEFAULT_LOCALITY = 'Austin'
DEFAULT_ORGANIZATION = 'Silicon Laboratories'
DEFAULT_ORGANIZATIONAL_UNIT = 'Wireless'
DEFAULT_COMMON_NAME = 'Silabs'
DEFAULT_EMAIL_ADDRESS = 'support@silabs.com'

EPILOG = '''\
Examples:
    Create a root CA:
        python %(prog)s --level 0

    Create a factory CA, overriding the default working directory:
        python %(prog)s --level 1 --working_dir /path/to/issuers

    Create a batch CA, with the validity period and the common_name
    and country certificate subjects specified:
        python %(prog)s --level 2 --common_name "John Doe" --validity 3653 --country HU
'''


class CertificateAuthority:
    '''An X.509 Certificate Authority (CA) backed by three files on disk.

    A certificate authority is fully described by the paths to its private key,
    its certificate and the YAML database listing the certificates it has
    issued. An authority can be a self-signed root or an intermediate authority
    signed by a higher-level (parent) authority.
    '''

    class Level(IntEnum):
        '''Certificate authority level within the certificate chain.'''
        ROOT = 0
        FACTORY = 1
        BATCH = 2

    def __init__(self,
                 private_key: Path,
                 certificate: Path,
                 database: Path):
        '''Create a certificate authority handle from its file paths.

        The paths are resolved to absolute, normalized form so that two
        authorities backed by the same files compare equal.

        :param private_key: Path to the private key file (PEM).
        :param certificate: Path to the certificate file (PEM).
        :param database: Path to the issued certificate database file (YAML).
        '''
        self.private_key = Path(private_key).resolve()
        self.certificate = Path(certificate).resolve()
        self.database = Path(database).resolve()

    def __str__(self):
        '''Return the authority's three file paths, one per indented line.'''
        return f"\t{self.private_key}\n\t{self.certificate}\n\t{self.database}"

    def __eq__(self, other) -> bool:
        '''Two authorities are equal if they are backed by the same files.'''
        if not isinstance(other, CertificateAuthority):
            return False
        return (self.private_key == other.private_key
                and self.certificate == other.certificate
                and self.database == other.database)

    def __hash__(self) -> int:
        '''Hash consistent with __eq__, keeping instances hashable.

        Defining __eq__ otherwise sets __hash__ to None, which would make the
        type unhashable.
        '''
        return hash((self.private_key, self.certificate, self.database))

    @classmethod
    def from_level(cls,
                   level: "CertificateAuthority.Level",
                   working_dir: Path):
        '''Build a certificate authority for the given level.

        The authority's files are placed in a level-specific sub-directory of
        the working directory (e.g. ``ca0_root`` for the root authority).

        :param level: Certificate authority level (see :class:`Level`).
        :param working_dir: Directory under which the authority's sub-directory
            is located. A string path is accepted as well.
        :returns: A :class:`CertificateAuthority` for the requested level.
        :raises ValueError: If the level is not a valid :class:`Level`.
        '''
        working_dir = Path(working_dir).resolve()
        if level == cls.Level.ROOT:
            path_dir = working_dir / 'ca0_root'
        elif level == cls.Level.FACTORY:
            path_dir = working_dir / 'ca1_factory'
        elif level == cls.Level.BATCH:
            path_dir = working_dir / 'ca2_batch'
        else:
            raise ValueError(f'Invalid certificate level: {level}')

        return cls(path_dir / "private_key.pem",
                   path_dir / "certificate.pem",
                   path_dir / "issued_certificates.yaml")

    @property
    def exist(self) -> bool:
        '''True if all three of the authority's files are present on disk.'''
        for path in (self.private_key, self.certificate, self.database):
            if not path.exists():
                return False
        return True

    @property
    def is_clear(self) -> bool:
        '''True if none of the authority's files exist on disk yet.

        Used before creation to ensure an existing authority is not
        accidentally overwritten.
        '''
        for path in (self.private_key, self.certificate, self.database):
            if path.exists():
                return False  # Dirty
        return True

    def is_valid(self,
                 issuer: Optional["CertificateAuthority"] = None) -> bool:
        '''True if the authority exists and its certificate is usable now.

        The authority is checked on its own for the following:
            - every necessary files are present on disk,
            - its certificate is within its validity period,
            - its certificate belongs to its stored private key,
            - its certificate is allowed to issue other certificates.

        :param issuer: If given, the link between the two authorities is
            also verified as well as the issuer.
        :returns: True if every check passes.
        :raises ValueError: If the issuer is not a CertificateAuthority.
        '''
        # Existence
        if not self.exist:
            print("CA validation failed: Does not exist!")
            return False

        # Validity period
        certificate = self.get_certificate()
        now = datetime.datetime.now(datetime.timezone.utc)
        not_before = certificate.not_valid_before_utc
        not_after = certificate.not_valid_after_utc

        if now < not_before or not_after < now:
            print("CA validation failed: Not in validity period!")
            return False

        # Check if the certificate and the key matches
        if certificate.public_key() != self.get_private_key().public_key():
            print("CA validation failed: Public key does not match!")
            return False

        if not self._can_issue_certificates(certificate):
            print("CA validation failed: Not allowed to issue certificates!")
            return False

        if issuer is None:
            return True

        if not self.is_issued_by(issuer):
            print("CA validation failed: Not issued by issuer!")
            return False

        # Note: If there is a revocation list (CRL) in use, then it should be
        # also checked here whether the certificate is revoked or not.

        return True

    def get_private_key(self) -> ec.EllipticCurvePrivateKey:
        '''Load and return the authority's private key from its PEM file.

        :raises FileNotFoundError: If the private key file does not exist.
        '''
        if not self.private_key.exists():
            raise FileNotFoundError(
                f'Private key does not exist: {self.private_key}')
        with open(self.private_key, 'rb') as stream:
            return serialization.load_pem_private_key(
                stream.read(), password=None)

    def get_certificate(self) -> x509.Certificate:
        '''Load and return the authority's certificate from its PEM file.

        :raises FileNotFoundError: If the certificate file does not exist.
        '''
        if not self.certificate.exists():
            raise FileNotFoundError(
                f'Certificate does not exist: {self.certificate}')
        with open(self.certificate, 'rb') as stream:
            return x509.load_pem_x509_certificate(stream.read())

    def get_database(self) -> dict:
        '''Load and return the issued certificate database.

        :returns: The database mapping, or an empty dict if the file is missing
            or empty.
        '''
        if not self.database.exists():
            return {}  # Return empty dict if the file does not exist.
        with open(self.database, mode="r", encoding="utf8") as stream:
            # Return empty dict if the file exists but is empty.
            return yaml.safe_load(stream) or {}

    def set_private_key(self, private_key: ec.EllipticCurvePrivateKey):
        '''Write the private key to its PEM file and restrict its permissions.

        Creates the parent directory if needed.

        :param private_key: The private key to store.
        '''
        self.private_key.parent.mkdir(parents=True, exist_ok=True)
        with open(self.private_key, 'wb') as stream:
            stream.write(private_key.private_bytes(
                serialization.Encoding.PEM,
                serialization.PrivateFormat.TraditionalOpenSSL,
                encryption_algorithm=serialization.NoEncryption()
            ))

        # Change file permissions.
        os.chmod(self.private_key, stat.S_IREAD + stat.S_IWRITE)

    def set_certificate(self, certificate: x509.Certificate):
        '''Write the certificate to its PEM file.

        Creates the parent directory if needed.

        :param certificate: The certificate to store.
        '''
        self.certificate.parent.mkdir(parents=True, exist_ok=True)
        with open(self.certificate, 'wb') as stream:
            stream.write(certificate.public_bytes(serialization.Encoding.PEM))

    def set_database(self, database: dict):
        '''Write the issued certificate database to its YAML file.

        Creates the parent directory if needed.

        :param database: The database mapping to store.
        '''
        self.database.parent.mkdir(parents=True, exist_ok=True)
        with open(self.database, mode="w", encoding="utf8") as stream:
            yaml.dump(database, stream, default_flow_style=False,
                      allow_unicode=True)

    def create(self,
               issuer: "CertificateAuthority",
               subject: x509.Name,
               validity: int,
               policy_oid: Optional[str] = None):
        '''Generate the authority's key pair and certificate, and store them.

        A new EC key pair is generated and a CA certificate is built and signed
        by the issuer. If the issuer is this same authority the certificate is
        self-signed (root CA); otherwise the issuer must already exist and be
        valid. The signed certificate is recorded in the issuer's database.

        :param issuer: The authority signing this certificate. Pass this same
            instance (or an equal one) to create a self-signed root.
        :param subject: The subject name of the certificate.
        :param validity: Validity period in days (must be >= 1).
        :param policy_oid: Optional certificate policy OID to embed.
        :raises FileExistsError: If this authority already exists on disk.
        :raises ValueError: If the issuer is not a CertificateAuthority or the
            validity is less than one day.
        :raises Exception: If the issuer is invalid or the created certificate
            fails validation.
        '''
        if not self.is_clear:
            raise FileExistsError(
                f'Certificate authority already exists:\n{self}\n'
                'Please remove it first.')
        if not isinstance(issuer, CertificateAuthority):
            raise ValueError('Issuer must be a CertificateAuthority object.')
        if validity < 1:
            raise ValueError(
                'Validity must be greater than or equal to one day!')

        # A root certificate authority signs its own certificate.
        self_signed = issuer == self

        if not self_signed and not issuer.is_valid():
            raise Exception('Issuer Certificate Authority is invalid.')

        # Create empty database.
        self.set_database({})

        # Generate EC key pair. Use the X9.62/SECG curve over a 256 bit prime
        # field (aka prime256v1).
        key = ec.generate_private_key(ec.SECP256R1())
        self.set_private_key(key)

        # Build certificate.
        now = datetime.datetime.now(datetime.timezone.utc)
        if self_signed:
            # A self-signed root is its own issuer.
            issuer_name = subject
        else:
            # Use the issuer's certificate subject.
            issuer_name = issuer.get_certificate().subject

        cert = (
            x509.CertificateBuilder()
            .subject_name(subject)
            .issuer_name(issuer_name)
            .public_key(key.public_key())
            .serial_number(issuer.generate_serial_number())
            .not_valid_before(now)
            .not_valid_after(now + datetime.timedelta(days=validity))
        )

        # Add extensions.
        cert = cert.add_extension(
            x509.SubjectKeyIdentifier.from_public_key(key.public_key()),
            critical=False)
        cert = cert.add_extension(
            x509.BasicConstraints(ca=True, path_length=None),
            critical=True)
        if policy_oid is not None:
            policy_info = [
                x509.PolicyInformation(x509.ObjectIdentifier(policy_oid), [])
            ]
            cert = cert.add_extension(
                x509.CertificatePolicies(policies=policy_info),
                critical=True)

        # Sign certificate.
        cert = issuer.sign(cert)
        self.set_certificate(cert)

        if not self.is_valid(issuer):
            raise Exception('The created certificate is invalid.')
        print(f'Certificate Authority created:\n{self}')

    def sign(self, certificate: x509.Certificate) -> x509.Certificate:
        '''Sign a certificate with this authority's private key.

        The signed certificate is recorded in this authority's issued
        certificate database.

        :param certificate: The certificate builder result to sign.
        :returns: The signed certificate.
        '''
        hash_algorithm = cryptography.hazmat.primitives.hashes.SHA256()
        key = self.get_private_key()
        certificate = certificate.sign(key, hash_algorithm)

        # Verify certificate signature.
        key.public_key().verify(
            certificate.signature,
            certificate.tbs_certificate_bytes,
            ec.ECDSA(hash_algorithm)
        )

        # Add certificate to issuer's database.
        self._add_certificate_to_database(certificate)
        return certificate

    def generate_serial_number(self, max_iterations=100):
        '''Generate a serial number not already present in the database.

        :param max_iterations: Maximum attempts to find a free serial.
        :returns: A unique random serial number.
        :raises RuntimeError: If no unique serial is found within the allowed
            number of iterations.
        '''
        database = self.get_database()
        for _ in range(max_iterations):
            serial = x509.random_serial_number()
            if serial not in database:
                return serial
        raise RuntimeError(
            "Failed to generate serial number. Max iterations reached:",
            max_iterations)

    def is_issued_by(self, issuer: "CertificateAuthority") -> bool:
        '''True if this authority's certificate was issued by the issuer.

        This verifies a single link of a certificate chain:
            - both certificates are present on disk,
            - the subject name of the issuer matches the issuer name of this
              certificate,
            - the signature of this certificate verifies with the public key
              of the issuer,
            - the authority key identifier of this certificate matches the
              subject key identifier of the issuer, when both are present,
            - the issuer is allowed to issue certificates,
            - the issuer has this certificate in its database of issued
              certificates.

        A root authority signs its own certificate, so passing this same
        authority (or an equal one) verifies that self-signature. In that case
        the issuer and the subject name of the certificate must be identical
        as well.

        :param issuer: The authority expected to have signed this authority's
            certificate. Pass this same instance to check a root authority.
        :returns: True if the issuer issued this authority's certificate.
        :raises ValueError: If the issuer is not a CertificateAuthority.
        '''
        if not isinstance(issuer, CertificateAuthority):
            raise ValueError('Issuer must be a CertificateAuthority object.')
        if not self.certificate.exists() or not issuer.certificate.exists():
            print("CA validation failed: Certificate does not exist!")
            return False

        certificate = self.get_certificate()
        issuer_certificate = issuer.get_certificate()

        # A root authority is its own issuer, hence its issuer and subject
        # names are the same.
        if issuer == self and certificate.issuer != certificate.subject:
            print("CA validation failed: Issuer and subject names do not match!")
            return False

        # Match the issuer name against the subject name of the issuer and
        # verify the signature with the public key of the issuer. This also
        # rejects a signature algorithm that the signature does not match.
        try:
            certificate.verify_directly_issued_by(issuer_certificate)
        except (ValueError, TypeError, InvalidSignature):
            print("CA validation failed: Issuer verification failed!")
            return False

        if not self._is_key_identifier_matching(certificate,
                                                issuer_certificate):
            print("CA validation failed: Key identifier does not match!")
            return False

        if not self._can_issue_certificates(issuer_certificate):
            print("CA validation failed: Not allowed to issue certificates!")
            return False

        # An authority keeps a record of every certificate it has signed.
        return certificate.serial_number in issuer.get_database()

    @staticmethod
    def _can_issue_certificates(certificate: x509.Certificate) -> bool:
        '''Check if the certificate is allowed to sign other certificates.

        :param certificate: The certificate of the issuing authority.
        :returns: True if the certificate can issue other certificates.
        '''
        # Check basic constraints
        try:
            constraints = certificate.extensions.get_extension_for_class(
                x509.BasicConstraints).value
        except x509.ExtensionNotFound:
            print("CA validation failed: Basic constraints not found!")
            return False

        if not constraints.ca:
            print("CA validation failed: Not a CA!")
            return False

        # Key usage extension
        # If present, must allow certificate signing. According to RFC 5280
        # a missing key usage extension means no restriction.
        try:
            key_usage = certificate.extensions.get_extension_for_class(
                x509.KeyUsage).value
        except x509.ExtensionNotFound:
            return True

        return key_usage.key_cert_sign

    @staticmethod
    def _is_key_identifier_matching(
            certificate: x509.Certificate,
            issuer_certificate: x509.Certificate) -> bool:
        '''True if the certificate points to the key of the issuer.

        The authority key identifier of a certificate names the key that
        signed it. It is what tells apart issuers sharing the same subject
        name, an authority before and after a key rollover for example. Both
        extensions involved are optional, so a certificate missing either of
        them passes this check.

        :param certificate: The issued certificate.
        :param issuer_certificate: The certificate of the expected issuer.
        :returns: True if the key identifiers match or are not available.
        '''
        extensions = certificate.extensions
        issuer_extensions = issuer_certificate.extensions
        try:
            authority_key = extensions.get_extension_for_class(
                x509.AuthorityKeyIdentifier).value.key_identifier
            subject_key = issuer_extensions.get_extension_for_class(
                x509.SubjectKeyIdentifier).value.digest
        except x509.ExtensionNotFound:
            return True

        if authority_key is None:
            # The issuer is identified by its name and serial number instead.
            return True

        return authority_key == subject_key

    def _add_certificate_to_database(self, certificate: x509.Certificate):
        '''Record an issued certificate in the database and store it on disk.

        Adds an entry keyed by the certificate serial number and also writes
        the certificate as ``<serial>.crt`` next to the database.

        :param certificate: The signed certificate to add.
        :raises ValueError: If a certificate with the same serial number is
            already present in the database.
        '''
        # Check if entry with the same serial exists.
        database = self.get_database()
        if certificate.serial_number in database:
            raise ValueError(
                "Database already contains a certificate with the same "
                "serial number:", certificate.serial_number)

        # Create new entry.
        common_name = certificate.subject.get_attributes_for_oid(
            x509.oid.NameOID.COMMON_NAME)[0].value
        public_key = certificate.public_key().public_bytes(
            serialization.Encoding.DER,
            serialization.PublicFormat.SubjectPublicKeyInfo)
        is_ca = certificate.extensions.get_extension_for_class(
            x509.BasicConstraints).value.ca
        database[certificate.serial_number] = [
            {"common_name": common_name},
            {"public_key": public_key},
            {"CA": is_ca},
        ]

        # Write back the updated database to file.
        self.set_database(database)

        # Also store the signed certificate in PEM format.
        cert_path = self.database.parent / f'{certificate.serial_number}.crt'
        with open(cert_path, 'wb') as stream:
            stream.write(certificate.public_bytes(serialization.Encoding.PEM))
        print('Issued certificate is stored in:', cert_path)


def main(working_dir: Path = DEFAULT_WORKING_DIR,
         level: int = DEFAULT_LEVEL,
         validity: int = DEFAULT_VALIDITY,
         policy_oid: Optional[str] = DEFAULT_POLICY_OID,
         subj_country: str = DEFAULT_COUNTRY,
         subj_state: str = DEFAULT_STATE,
         subj_locality: str = DEFAULT_LOCALITY,
         subj_organization: str = DEFAULT_ORGANIZATION,
         subj_organizational_unit: str = DEFAULT_ORGANIZATIONAL_UNIT,
         subj_common_name: str = DEFAULT_COMMON_NAME,
         subj_email_address: str = DEFAULT_EMAIL_ADDRESS):
    '''Create the certificate authority for the requested level.

    Resolves the issuer authority (the level n-1 authority, or the root itself
    for level 0), then creates the target authority in the working directory.

    :param working_dir: Directory in which the authority sub-directories live.
    :param level: Authority level to create (0=root, 1=factory, 2=batch).
    :param validity: Certificate validity period in days.
    :param policy_oid: Optional certificate policy OID.
    :param subj_country: Certificate subject country.
    :param subj_state: Certificate subject state or province.
    :param subj_locality: Certificate subject locality.
    :param subj_organization: Certificate subject organization.
    :param subj_organizational_unit: Certificate subject organizational unit.
    :param subj_common_name: Certificate subject common name.
    :param subj_email_address: Certificate subject e-mail address.
    '''
    # Pack certificate subjects.
    name_oid = x509.oid.NameOID
    subjects = x509.Name([
        x509.NameAttribute(name_oid.COUNTRY_NAME, subj_country),
        x509.NameAttribute(name_oid.STATE_OR_PROVINCE_NAME, subj_state),
        x509.NameAttribute(name_oid.LOCALITY_NAME, subj_locality),
        x509.NameAttribute(name_oid.ORGANIZATION_NAME, subj_organization),
        x509.NameAttribute(
            name_oid.ORGANIZATIONAL_UNIT_NAME, subj_organizational_unit),
        x509.NameAttribute(name_oid.COMMON_NAME, subj_common_name),
        x509.NameAttribute(name_oid.EMAIL_ADDRESS, subj_email_address),
    ])
    issuer_level = max(level - 1, 0)  # Clamp to 0 if negative.

    print("Configuration")
    print(f"\tWorking directory: {working_dir}")
    print(f"\tAuthority level: {level}")
    print(f"\tIssuer level: {issuer_level}")
    print(f"\tCertificate validity: {validity}[days]")
    print(f"\tCertificate object identifier: {policy_oid}")
    print("\tCertificate subjects")
    print(f"\t\tcountry: {subj_country}")
    print(f"\t\tstate: {subj_state}")
    print(f"\t\tlocality: {subj_locality}")
    print(f"\t\torganization: {subj_organization}")
    print(f"\t\torganizational_unit: {subj_organizational_unit}")
    print(f"\t\tcommon_name: {subj_common_name}")
    print(f"\t\temail_address: {subj_email_address}")

    # Get issuer certificate authority. Root CA is self-signed.
    issuer = CertificateAuthority.from_level(issuer_level, working_dir)

    # Create certificate authority.
    ca = CertificateAuthority.from_level(level, working_dir)
    ca.create(issuer,
              subjects,
              validity,
              policy_oid)


class CustomFormatter(argparse.ArgumentDefaultsHelpFormatter,
                      argparse.RawDescriptionHelpFormatter):
    pass


def add_certificate_args(parser: argparse.ArgumentParser,
                         common_name: bool = True):
    '''Add the certificate subject and validity arguments to a parser.

    :param parser: The argument parser to extend.
    :param common_name: Whether to include the common name certificate subject.
    :returns: The same parser, for convenience.
    '''
    parser.add_argument('--validity',
                        default=DEFAULT_VALIDITY,
                        type=int,
                        help='The valid period of the certificate in days '
                             'starting from the moment of generation.')
    parser.add_argument('--policy_oid',
                        default=DEFAULT_POLICY_OID,
                        help='The optional Certificate Policy Information '
                             'extension. Only a policy OID is supported.')

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
                        help='The organization subject of the x509 '
                             'certificate.')
    parser.add_argument('--organizational_unit',
                        default=DEFAULT_ORGANIZATIONAL_UNIT,
                        help='The organizational unit subject of the x509 '
                             'certificate.')
    if common_name:
        parser.add_argument('--common_name',
                            default=DEFAULT_COMMON_NAME,
                            help='The common name unit subject of the x509 '
                                 'certificate.')
    parser.add_argument('--email_address',
                        default=DEFAULT_EMAIL_ADDRESS,
                        help='The e-mail address unit subject of the x509 '
                             'certificate.')

    return parser


def load_args():
    '''Parse command line arguments.

    :returns: The parsed arguments namespace.
    '''
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog=EPILOG,
        formatter_class=CustomFormatter)
    parser.add_argument('--working_dir',
                        default=DEFAULT_WORKING_DIR,
                        type=Path,
                        help='Specify the working directory where the '
                             'certificates and the related files shall be '
                             'created and/or loaded from. '
                             '(default: %(default)s)')
    parser.add_argument('-l', '--level',
                        default=DEFAULT_LEVEL,
                        type=int,
                        choices=[
                            level.value
                            for level in CertificateAuthority.Level],
                        help='Authority level. If zero passed, root '
                             'certificate will be created. With greater '
                             'values than zero, intermediate certificate '
                             'will be created with the given level.')
    add_certificate_args(parser)
    return parser.parse_args()


if __name__ == '__main__':
    args = load_args()

    main(args.working_dir,
         args.level,
         args.validity,
         args.policy_oid,
         args.country,
         args.state,
         args.locality,
         args.organization,
         args.organizational_unit,
         args.common_name,
         args.email_address)
