#!/usr/bin/env python3

import argparse
import logging
import datetime
from datetime import timezone
import os
import glob
from cryptography import x509
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID, ObjectIdentifier
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes, serialization
from pyasn1.type import univ, namedtype
from pyasn1.codec.der.encoder import encode
from typing import Optional, Tuple, List, Dict
from enum import Enum

logger = logging.getLogger('wisunpki')
logger.setLevel(logging.DEBUG)

pki_root_cert = "wisun_root_cert.pem"
pki_root_key = "wisun_root_key.pem"
pki_mca_cert = "wisun_mca_cert.pem"
pki_mca_key = "wisun_mca_key.pem"
pki_mica_cert = "wisun_mica_cert.pem"
pki_mica_key = "wisun_mica_key.pem"
pki_device_cert = "wisun_device_{0}_cert.pem"
pki_device_key = "wisun_device_{0}_key.pem"
_PKI_CA_FILES = (
    pki_root_cert, pki_root_key,
    pki_mca_cert, pki_mca_key,
    pki_mica_cert, pki_mica_key,
)
pki_csr_cn = u"Wi-SUN CSR"
pki_root_cn = u"Wi-SUN Root"
pki_mca_cn = u"Wi-SUN MCA"
pki_mica_cn = u"Wi-SUN MICA"
pki_device_cn = u"Wi-SUN Device"

# Subject Name field for CSR
CSR_SUBJECT = x509.Name([
    x509.NameAttribute(NameOID.COMMON_NAME, pki_csr_cn),
])

# 99991231235959Z (Generalized Time)
NO_EXPIRATION = datetime.datetime(
    year=9999, month=12, day=31, hour=23, minute=59, second=59)


class WisunOID:
    """Wi-SUN specific OIDs
    """
    # id-kp-wisun-fan-device
    WISUN_FAN_DEVICE = ObjectIdentifier("1.3.6.1.4.1.45605.1")
    # id-on-hardwareModuleName
    HARDWARE_MODULE_NAME = ObjectIdentifier("1.3.6.1.5.5.7.8.4")


class HardwareModule(univ.Sequence):
    """Hardware Module Name (RFC4108)

    HardwareModuleName ::= SEQUENCE {
        hwType OBJECT IDENTIFIER,
        hwSerialNum OCTET STRING }
    """
    componentType = namedtype.NamedTypes(
        namedtype.NamedType('hwType', univ.ObjectIdentifier()),
        namedtype.NamedType('hwSerialNum', univ.OctetString())
    )


class WisunDeviceType(Enum):
    ROUTER = 1
    BORDER_ROUTER = 2


def _pki_dir(pki_path: Optional[str] = None) -> str:
    """Directory that holds PKI files. Defaults to process CWD.
    Args:
        pki_path: Absolute path to the PKI directory. If omitted, CWD is used.
    Returns:
        str: Absolute path to the PKI directory.
    """
    if pki_path is None or pki_path == "":
        return os.getcwd()
    return os.path.abspath(pki_path)


def _pki_file(name: str, pki_path: str) -> str:
    """Resolve a PKI filename under the PKI directory.
    Args:
        name: Basename of the PKI file.
        pki_path: Absolute path to the PKI directory.
    Returns:
        str: Absolute path to the PKI file.
    """
    return os.path.join(_pki_dir(pki_path), name)


def _safe_remove(path: str) -> None:
    """Remove a file if it exists, logging failures.
    Args:
        path: Path to the file to remove.
    """
    try:
        os.remove(path)
        logger.debug(f'Deleted: {path}')
    except FileNotFoundError:
        pass
    except OSError as e:
        logger.error(f'Error deleting {path}: {e}')


def _delete_pki(pki_path: str) -> None:
    """Delete PKI files under the given directory.
    Args:
        pki_path: Absolute path to the PKI directory.
    """
    for name in _PKI_CA_FILES:
        _safe_remove(_pki_file(name, pki_path))
    for pattern in ("wisun_device_*_cert.pem", "wisun_device_*_key.pem"):
        for path in glob.glob(_pki_file(pattern, pki_path)):
            _safe_remove(path)


def _load_certificate_data(cert_path: str) -> x509.Certificate:
    """Load and parse the certificate file.
    Args:
        cert_path: Path to the certificate file.
    Returns:
        x509.Certificate: The parsed certificate.
    """
    with open(cert_path, "rb") as f:
        cert_data = f.read()
        try:
            return x509.load_pem_x509_certificate(data=cert_data)
        except ValueError:
            return x509.load_der_x509_certificate(data=cert_data)


def load_private_key_data(key_path: str) -> ec.EllipticCurvePrivateKey:
    """Load and parse the private key file.
    Args:
        key_path: Path to the private key file.
    Returns:
        ec.EllipticCurvePrivateKey: The parsed private key.
    """
    with open(key_path, "rb") as f:
        key_data = f.read()
        try:
            key = serialization.load_pem_private_key(
                data=key_data, password=None)
        except ValueError:
            key = serialization.load_der_private_key(
                data=key_data, password=None)
        if not isinstance(key, ec.EllipticCurvePrivateKey):
            raise TypeError("Private key is not EllipticCurvePrivateKey")
        return key


def _load_csr_data(csr_data: bytes) -> x509.CertificateSigningRequest:
    """Parse Certificate Signing Request (CSR).
    Args:
        csr_data: Certificate Signing Request (CSR) data.
    Returns:
        x509.CertificateSigningRequest: The parsed CSR.
    """
    try:
        return x509.load_pem_x509_csr(data=csr_data)
    except ValueError:
        return x509.load_der_x509_csr(data=csr_data)


def _hardware_module_name(hwtype_oid: str, hwserialnum: bytes) -> bytes:
    """Generate RFC4108 Hardware Module Name.
    Args:
        hwtype_oid: Hardware Type OID.
        hwserialnum: Hardware Serial Number.
    Returns:
        bytes: The generated Hardware Module Name.
    """
    hardware_module_value = HardwareModule()
    hardware_module_value["hwType"] = hwtype_oid
    hardware_module_value["hwSerialNum"] = hwserialnum
    return encode(hardware_module_value)


def _pki_chain_length(pki_path: str) -> int:
    """Return the number of CAs in the PKI chain.
    Args:
        pki_path: Absolute path to the PKI directory.
    Returns:
        int: The number of CAs in the PKI chain. 0 if none.
    """
    chain_length = 0
    if os.path.exists(_pki_file(pki_root_cert, pki_path)):
        chain_length += 1
    if os.path.exists(_pki_file(pki_mca_cert, pki_path)):
        chain_length += 1
    if os.path.exists(_pki_file(pki_mica_cert, pki_path)):
        chain_length += 1
    return chain_length


def _pki_issuing_ca(chain_length: int, pki_path: str) -> Tuple[str, str]:
    """Return the issuing CA certificate and private key.
    Args:
        chain_length: The number of CAs in the PKI chain.
        pki_path: Absolute path to the PKI directory.
    Returns:
        Tuple[str, str]: Paths to the issuing CA certificate and private key.
    """
    if chain_length == 1:
        return (_pki_file(pki_root_cert, pki_path), _pki_file(pki_root_key, pki_path))
    else:
        return (_pki_file(pki_mica_cert, pki_path), _pki_file(pki_mica_key, pki_path))


def _generate_private_key(output_key_path: str) -> ec.EllipticCurvePrivateKey:
    """Generate and save a random private key
    Args:
        output_key_path: Path to the generated private key.
    Returns:
        ec.EllipticCurvePrivateKey: The generated private key.
    """
    key = ec.generate_private_key(ec.SECP256R1())
    with open(output_key_path, "wb") as f:
        f.write(key.private_bytes(encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.TraditionalOpenSSL, encryption_algorithm=serialization.NoEncryption()))
    return key


def _generate_root(oem_name: str, oem_country: str, output_cert_path: str, output_key_path: str) -> None:
    """Generate and save a root CA certificate-key pair.
    Args:
        oem_name: OEM name.
        oem_country: OEM country.
        output_cert_path: Path to the generated root CA certificate.
        output_key_path: Path to the generated root CA private key.
    """
    # Root CA private key
    key = _generate_private_key(output_key_path)
    # Root CA certificate
    subject = issuer = x509.Name([
        x509.NameAttribute(NameOID.COUNTRY_NAME, oem_country),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, oem_name),
        x509.NameAttribute(NameOID.COMMON_NAME, pki_root_cn),
    ])
    cert = x509.CertificateBuilder().subject_name(
        subject
    ).issuer_name(
        issuer
    ).public_key(
        key.public_key()
    ).serial_number(
        x509.random_serial_number()
    ).not_valid_before(
        datetime.datetime.now(timezone.utc)
    ).not_valid_after(
        NO_EXPIRATION
    ).add_extension(
        x509.BasicConstraints(ca=True, path_length=2), critical=True
    ).add_extension(
        x509.KeyUsage(digital_signature=False, content_commitment=False, key_encipherment=False, data_encipherment=False, key_agreement=False, key_cert_sign=True, crl_sign=True, encipher_only=False, decipher_only=False), critical=False
    ).add_extension(
        x509.SubjectKeyIdentifier.from_public_key(key.public_key()), critical=False
    ).add_extension(
        x509.AuthorityKeyIdentifier.from_issuer_public_key(key.public_key()), critical=False
    ).sign(key, hashes.SHA256())
    with open(output_cert_path, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))


def _generate_mca(oem_name: str, oem_country: str, sign_cert_path: str, sign_key_path: str, output_cert_path: str, output_key_path: str) -> None:
    """Generate and save an MCA certificate-key pair.
    Args:
        oem_name: OEM name.
        oem_country: OEM country.
        sign_cert_path: Path to the signing CA certificate.
        sign_key_path: Path to the signing CA private key.
        output_cert_path: Path to the generated MCA certificate.
        output_key_path: Path to the generated MCA private key.
    """
    # Signing certificate
    sign_cert = _load_certificate_data(sign_cert_path)
    # Signing key
    sign_key = load_private_key_data(sign_key_path)
    # Signing Subject Key Identifier
    sign_ski = sign_cert.extensions.get_extension_for_class(
        x509.SubjectKeyIdentifier)
    # MCA private key
    key = _generate_private_key(output_key_path)
    # MCA certificate
    subject = issuer = x509.Name([
        x509.NameAttribute(NameOID.COUNTRY_NAME, oem_country),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, oem_name),
        x509.NameAttribute(NameOID.COMMON_NAME, pki_mca_cn),
    ])
    cert = x509.CertificateBuilder().subject_name(
        subject
    ).issuer_name(
        sign_cert.subject
    ).public_key(
        key.public_key()
    ).serial_number(
        x509.random_serial_number()
    ).not_valid_before(
        datetime.datetime.now(timezone.utc)
    ).not_valid_after(
        NO_EXPIRATION
    ).add_extension(
        x509.BasicConstraints(ca=True, path_length=1), critical=True
    ).add_extension(
        x509.KeyUsage(digital_signature=False, content_commitment=False, key_encipherment=False, data_encipherment=False, key_agreement=False, key_cert_sign=True, crl_sign=True, encipher_only=False, decipher_only=False), critical=False
    ).add_extension(
        x509.SubjectKeyIdentifier.from_public_key(key.public_key()), critical=False
    ).add_extension(
        x509.AuthorityKeyIdentifier.from_issuer_subject_key_identifier(sign_ski.value), critical=False
    ).sign(sign_key, hashes.SHA256())
    with open(output_cert_path, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))


def _generate_mica(oem_name: str, oem_country: str, sign_cert_path: str, sign_key_path: str, output_cert_path: str, output_key_path: str) -> None:
    """Generate and save an MICA certificate-key pair.
    Args:
        oem_name: OEM name.
        oem_country: OEM country.
        sign_cert_path: Path to the signing CA certificate.
        sign_key_path: Path to the signing CA private key.
        output_cert_path: Path to the generated MICA certificate.
        output_key_path: Path to the generated MICA private key.
    """
    # Signing certificate
    sign_cert = _load_certificate_data(sign_cert_path)
    # Signing Subject Key Identifier
    sign_ski = sign_cert.extensions.get_extension_for_class(
        x509.SubjectKeyIdentifier)
    # Signing key
    sign_key = load_private_key_data(sign_key_path)
    # MICA private key
    key = _generate_private_key(output_key_path)
    # MICA certificate
    subject = issuer = x509.Name([
        x509.NameAttribute(NameOID.COUNTRY_NAME, oem_country),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, oem_name),
        x509.NameAttribute(NameOID.COMMON_NAME, pki_mica_cn),
    ])
    cert = x509.CertificateBuilder().subject_name(
        subject
    ).issuer_name(
        sign_cert.subject
    ).public_key(
        key.public_key()
    ).serial_number(
        x509.random_serial_number()
    ).not_valid_before(
        datetime.datetime.now(timezone.utc)
    ).not_valid_after(
        NO_EXPIRATION
    ).add_extension(
        x509.BasicConstraints(ca=True, path_length=0), critical=True
    ).add_extension(
        x509.KeyUsage(digital_signature=False, content_commitment=False, key_encipherment=False, data_encipherment=False, key_agreement=False, key_cert_sign=True, crl_sign=True, encipher_only=False, decipher_only=False), critical=False
    ).add_extension(
        x509.SubjectKeyIdentifier.from_public_key(key.public_key()), critical=False
    ).add_extension(
        x509.AuthorityKeyIdentifier.from_issuer_subject_key_identifier(sign_ski.value), critical=False
    ).sign(sign_key, hashes.SHA256())
    with open(output_cert_path, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))


def _generate_csr(output_key_path: str) -> bytes:
    """Generate and save a CSR-key pair.
    Args:
        output_key_path: Path to the generated private key.
    Returns:
        bytes: The generated CSR.
    """
    # Private key
    key = _generate_private_key(output_key_path)
    # CSR
    csr = x509.CertificateSigningRequestBuilder().subject_name(
        CSR_SUBJECT
    ).sign(key, hashes.SHA256())
    return csr.public_bytes(serialization.Encoding.PEM)


def _generate_device(hwtype_oid: str, hwserialnum: bytes, device_type: WisunDeviceType, csr_data: bytes, sign_cert_path: str, sign_key_path: str, output_cert_path: str) -> None:
    """Generate and save a device certificate from a CSR.
    Args:
        hwtype_oid: Hardware Type OID.
        hwserialnum: Hardware Serial Number.
        device_type: Device type.
        csr_data: CSR data.
        sign_cert_path: Path to the signing CA certificate.
        sign_key_path: Path to the signing CA private key.
        output_cert_path: Path to the generated device certificate.
    """
    # Signing certificate
    sign_cert = _load_certificate_data(sign_cert_path)
    # Signing Subject Key Identifier
    sign_ski = sign_cert.extensions.get_extension_for_class(
        x509.SubjectKeyIdentifier)
    # Signing key
    sign_key = load_private_key_data(sign_key_path)
    # CSR
    csr = _load_csr_data(csr_data)
    # Certificate
    subject = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, pki_device_cn),
    ])
    hardware_module = x509.OtherName(
        type_id=WisunOID.HARDWARE_MODULE_NAME,
        value=_hardware_module_name(
            hwtype_oid=hwtype_oid, hwserialnum=hwserialnum)
    )
    if device_type == WisunDeviceType.ROUTER:
        key_usage = ExtendedKeyUsageOID.CLIENT_AUTH
    else:
        key_usage = ExtendedKeyUsageOID.SERVER_AUTH
    cert = x509.CertificateBuilder().subject_name(
        subject
    ).issuer_name(
        sign_cert.subject
    ).public_key(
        csr.public_key()
    ).serial_number(
        x509.random_serial_number()
    ).not_valid_before(
        datetime.datetime.now(timezone.utc)
    ).not_valid_after(
        NO_EXPIRATION
    ).add_extension(
        x509.KeyUsage(
            digital_signature=True, content_commitment=False, key_encipherment=False, data_encipherment=False,
            key_agreement=True, key_cert_sign=False, crl_sign=False, encipher_only=False, decipher_only=False
        ), critical=True
    ).add_extension(
        x509.ExtendedKeyUsage([WisunOID.WISUN_FAN_DEVICE, key_usage]), critical=True
    ).add_extension(
        x509.SubjectAlternativeName([hardware_module]), critical=True
    ).add_extension(
        x509.AuthorityKeyIdentifier.from_issuer_subject_key_identifier(sign_ski.value), critical=False
    ).sign(sign_key, hashes.SHA256())
    with open(output_cert_path, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))


def create_pki(oem_name: str, oem_country: str, chain_length: int,
               pki_path: Optional[str] = None) -> None:
    """Create a local Public Key Infrastructure (PKI).
    Args:
        oem_name: OEM name.
        oem_country: OEM country.
        chain_length: The number of CAs in the PKI chain.
        pki_path: Absolute path to the PKI directory. If omitted, CWD is used.
    """
    pki_path = _pki_dir(pki_path)
    os.makedirs(pki_path, exist_ok=True)
    _delete_pki(pki_path)
    root_cert = _pki_file(pki_root_cert, pki_path)
    root_key = _pki_file(pki_root_key, pki_path)
    mca_cert = _pki_file(pki_mca_cert, pki_path)
    mca_key = _pki_file(pki_mca_key, pki_path)
    mica_cert = _pki_file(pki_mica_cert, pki_path)
    mica_key = _pki_file(pki_mica_key, pki_path)
    logger.debug("Generating a new root CA certificate and private key")
    _generate_root(oem_name=oem_name, oem_country=oem_country,
                   output_cert_path=root_cert, output_key_path=root_key)
    if chain_length == 2:
        logger.debug("Generating a new MICA certificate and private key")
        _generate_mica(oem_name=oem_name, oem_country=oem_country, sign_cert_path=root_cert,
                       sign_key_path=root_key, output_cert_path=mica_cert, output_key_path=mica_key)
    if chain_length == 3:
        logger.debug("Generating a new MCA certificate and private key")
        _generate_mca(oem_name=oem_name, oem_country=oem_country, sign_cert_path=root_cert,
                      sign_key_path=root_key, output_cert_path=mca_cert, output_key_path=mca_key)
        logger.debug("Generating a new MICA certificate and private key")
        _generate_mica(oem_name=oem_name, oem_country=oem_country, sign_cert_path=mca_cert,
                       sign_key_path=mca_key, output_cert_path=mica_cert, output_key_path=mica_key)
    logger.debug("PKI successfully generated")


def setup_pki(root_cert: str, root_key: Optional[str] = None, mca_cert: Optional[str] = None,
              mca_key: Optional[str] = None, mica_cert: Optional[str] = None,
              mica_key: Optional[str] = None, pki_path: Optional[str] = None) -> None:
    """Setup an existing Public Key Infrastructure (PKI).
    Args:
        root_cert: Path to the root CA certificate.
        root_key: Path to the root CA private key.
        mca_cert: Path to the MCA certificate.
        mca_key: Path to the MCA private key.
        mica_cert: Path to the MICA certificate.
        mica_key: Path to the MICA private key.
        pki_path: Absolute path to the destination PKI directory. If omitted, CWD is used.
    """
    pki_path = _pki_dir(pki_path)
    os.makedirs(pki_path, exist_ok=True)
    # Load the source files into memory before removing the current PKI to
    # prevent data loss if source and destination directories overlap.
    source_pki = (
        (root_cert, pki_root_cert, "root CA certificate"),
        (root_key, pki_root_key, "root CA private key"),
        (mca_cert, pki_mca_cert, "MCA certificate"),
        (mca_key, pki_mca_key, "MCA private key"),
        (mica_cert, pki_mica_cert, "MICA certificate"),
        (mica_key, pki_mica_key, "MICA private key"),
    )
    dest_pki = []
    for src, name, label in source_pki:
        if src is None:
            continue
        if not os.path.isfile(src):
            raise FileNotFoundError(src)
        logger.debug("Importing %s", label)
        with open(src, "rb") as f:
            dest_pki.append((name, f.read()))
    _delete_pki(pki_path)
    for name, data in dest_pki:
        with open(_pki_file(name, pki_path), "wb") as f:
            f.write(data)
    logger.debug("PKI successfully setup")


def get_pki(pki_path: Optional[str] = None) -> Optional[List[Dict[str, str]]]:
    """Return paths to existing PKI files.
    Args:
        pki_path: Absolute path to the PKI directory. If omitted, CWD is used.
    Returns:
        List[Dict[str, str]]: Paths to the existing PKI files. None if not found.
    """
    pki_path = _pki_dir(pki_path)
    pki = []
    root_cert = _pki_file(pki_root_cert, pki_path)
    root_key = _pki_file(pki_root_key, pki_path)
    root = {}
    if os.path.exists(root_cert):
        root['cert_path'] = root_cert
    if os.path.exists(root_key):
        root['key_path'] = root_key
    if len(root):
        pki.append(root)
    mca_cert = _pki_file(pki_mca_cert, pki_path)
    mca_key = _pki_file(pki_mca_key, pki_path)
    mca = {}
    if os.path.exists(mca_cert):
        mca['cert_path'] = mca_cert
    if os.path.exists(mca_key):
        mca['key_path'] = mca_key
    if len(mca):
        pki.append(mca)
    mica_cert = _pki_file(pki_mica_cert, pki_path)
    mica_key = _pki_file(pki_mica_key, pki_path)
    mica = {}
    if os.path.exists(mica_cert):
        mica['cert_path'] = mica_cert
    if os.path.exists(mica_key):
        mica['key_path'] = mica_key
    if len(mica):
        pki.append(mica)
    if len(pki) == 0:
        return None
    return pki


def generate_on_host(hwtype_oid: str, hwserialnum: bytes, device_type: WisunDeviceType,
                     pki_path: Optional[str] = None) -> Dict[str, str]:
    """Generate device certificate-key pair on the host.
    Args:
        hwtype_oid: Hardware Type OID.
        hwserialnum: Hardware Serial Number.
        device_type: Device type.
        pki_path: Absolute path to the PKI directory. If omitted, CWD is used.
    Returns:
        Dict[str, str]: Paths to the device certificate and private key.
    """
    pki_path = _pki_dir(pki_path)
    sign_cert_path, sign_key_path = _pki_issuing_ca(
        _pki_chain_length(pki_path), pki_path)
    output_cert_path = _pki_file(pki_device_cert.format(hwserialnum.hex()), pki_path)
    output_key_path = _pki_file(pki_device_key.format(hwserialnum.hex()), pki_path)
    logger.debug("Generating CSR and private key")
    csr_data = _generate_csr(output_key_path)
    logger.debug(f"Generating device certificate for {hwserialnum.hex()}")
    _generate_device(hwtype_oid=hwtype_oid, hwserialnum=hwserialnum, device_type=device_type, csr_data=csr_data,
                     sign_cert_path=sign_cert_path, sign_key_path=sign_key_path, output_cert_path=output_cert_path)
    return {"cert_path": output_cert_path, "key_path": output_key_path}


def generate_from_csr(hwtype_oid: str, hwserialnum: bytes, device_type: WisunDeviceType,
                      csr_data: bytes, pki_path: Optional[str] = None) -> Dict[str, str]:
    """Generate device certificate from a CSR.
    Args:
        hwtype_oid: Hardware Type OID.
        hwserialnum: Hardware Serial Number.
        device_type: Device type.
        csr_data: CSR data.
        pki_path: Absolute path to the PKI directory. If omitted, CWD is used.
    Returns:
        Dict[str, str]: Paths to the device certificate.
    """
    pki_path = _pki_dir(pki_path)
    sign_cert_path, sign_key_path = _pki_issuing_ca(
        _pki_chain_length(pki_path), pki_path)
    output_cert_path = _pki_file(pki_device_cert.format(hwserialnum.hex()), pki_path)
    logger.debug(f"Generating device certificate for {hwserialnum.hex()}")
    _generate_device(hwtype_oid=hwtype_oid, hwserialnum=hwserialnum, device_type=device_type, csr_data=csr_data,
                     sign_cert_path=sign_cert_path, sign_key_path=sign_key_path, output_cert_path=output_cert_path)
    return {"cert_path": output_cert_path}


def get_credential(hwserialnum: bytes, pki_path: Optional[str] = None) -> Optional[Dict[str, str]]:
    """Return paths to device certificate and private key.
    Args:
        hwserialnum: Hardware Serial Number.
        pki_path: Absolute path to the PKI directory. If omitted, CWD is used.
    Returns:
        Dict[str, str]: Paths to the device certificate and private key. None if not found.
    """
    pki_path = _pki_dir(pki_path)
    pki = {}
    cert_path = _pki_file(pki_device_cert.format(hwserialnum.hex()), pki_path)
    key_path = _pki_file(pki_device_key.format(hwserialnum.hex()), pki_path)
    if os.path.exists(cert_path):
        pki["cert_path"] = cert_path
    if os.path.exists(key_path):
        pki["key_path"] = key_path
    if len(pki) == 0:
        return None
    return pki
