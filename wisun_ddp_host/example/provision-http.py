#!/usr/bin/env python3
"""
Example on how to utilize the DDP REST API to provision a device.
"""

import argparse
import logging
import os
import sys
from pathlib import Path

import requests

# NVM Object ID for storing device certificate
_CERT_INDEX = 0x100
# PSA Crypto Key ID for storing private key
_KEY_INDEX = 0x100


def _api_request(
    base_url: str,
    token: str | None,
    method: str,
    path: str,
    json_data: dict | None = None,
    params: dict | None = None,
) -> requests.Response:
    """Make an authenticated API request."""
    url = f"{base_url.rstrip('/')}{path}"
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"

    if method == "GET":
        resp = requests.get(url, params=params, json=json_data, headers=headers, timeout=60)
    elif method == "POST":
        resp = requests.post(url, json=json_data, headers=headers, timeout=120)
    else:
        raise ValueError(f"Unsupported method: {method}")

    return resp


def _get_token_path_from_api(base_url: str) -> Path | None:
    """Retrieve token file path from /api/info (no auth required)."""
    try:
        url = f"{base_url.rstrip('/')}/api/info"
        resp = requests.get(url, timeout=10)
        if resp.status_code == 200:
            data = resp.json()
            token_file = data.get("token_file")
            if token_file:
                return Path(token_file)
    except (requests.RequestException, ValueError, KeyError):
        pass
    return None


def _load_token(base_url: str, cwd: Path) -> str | None:
    """Load token from /api/info token_file or default token file in cwd."""
    # Retrieve token path from server
    token_path = _get_token_path_from_api(base_url)
    if token_path and token_path.is_file():
        return token_path.read_text().strip()
    return None


def main() -> int:
    logger = logging.getLogger("provision-http")
    logger.setLevel(logging.DEBUG)
    ch = logging.StreamHandler()
    ch.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s"))
    logger.addHandler(ch)

    parser = argparse.ArgumentParser(
        description="Script for performing Wi-SUN provisioning via DDP REST API."
    )
    parser.add_argument(
        "--jlink_ser",
        action="store",
        default=None,
        help="Serial number of J-Link adapter",
    )
    parser.add_argument(
        "--jlink_host",
        action="store",
        default=None,
        help="Host name or IP address of J-Link adapter",
    )
    prov_group = parser.add_mutually_exclusive_group(required=True)
    prov_group.add_argument(
        "--prov_img",
        action="store",
        help="Provisioning application binary (DDP RAM app path)",
    )
    prov_group.add_argument(
        "--prov_board",
        action="store",
        help="Board name for pre-built DDP RAM app",
    )
    parser.add_argument(
        "--nvm3_start_addr",
        action="store",
        default=None,
        type=lambda x: int(x, 0) if x else None,
        help="NVM3 start address in flash",
    )
    parser.add_argument(
        "--nvm3_size",
        action="store",
        default=None,
        type=lambda x: int(x, 0) if x else None,
        help="NVM3 size in bytes",
    )
    parser.add_argument(
        "--oem_country",
        action="store",
        default=None,
        help="OEM country code",
    )
    parser.add_argument(
        "--oem_company",
        action="store",
        default=None,
        help="OEM company name",
    )
    parser.add_argument(
        "--oem_hwtype",
        action="store",
        required=True,
        help="OEM hwType OID",
    )
    parser.add_argument(
        "--cwd",
        action="store",
        default=None,
        help="Working directory to change to before running",
    )
    parser.add_argument(
        "--base_url",
        action="store",
        default=os.getenv("DDP_API_BASE_URL", "http://127.0.0.1:9080"),
        help="DDP REST API base URL (default: http://127.0.0.1:9080)",
    )
    args = parser.parse_args()

    if args.prov_img:
        ddp_app_params: dict = {"ddp_app_path": os.path.abspath(args.prov_img)}
    else:
        ddp_app_params = {"ddp_app_board": args.prov_board}

    cwd = Path(args.cwd) if args.cwd else Path.cwd()
    if args.cwd:
        os.chdir(args.cwd)

    token = _load_token(args.base_url, cwd)

    # Build J-Link parameters for API
    jlink_params: dict = {}
    if args.jlink_ser:
        try:
            jlink_params["jlink_serial"] = int(args.jlink_ser)
        except ValueError:
            logger.error("jlink_ser must be a number")
            return 1
    if args.jlink_host:
        jlink_params["jlink_host"] = args.jlink_host

    if not jlink_params:
        logger.error("Either --jlink_ser or --jlink_host is required")
        return 1

    # Setup PKI if needed
    logger.info("Checking PKI")
    resp = _api_request(args.base_url, token, "GET", "/pki/ca")
    if resp.status_code == 404 or (resp.status_code == 200 and not resp.json()):
        logger.info("Creating a local PKI")
        if not args.oem_company or not args.oem_country:
            logger.error("--oem_company and --oem_country required for PKI creation")
            return 1
        create_resp = _api_request(
            args.base_url,
            token,
            "POST",
            "/pki/ca/create",
            json_data={
                "oem_company": args.oem_company,
                "oem_country": args.oem_country,
                "chain_length": 3,
            },
        )
        if create_resp.status_code != 200:
            logger.error(f"PKI creation failed: {create_resp.status_code} {create_resp.text}")
            return 1
        logger.info("PKI created")
    else:
        if resp.status_code != 200:
            logger.error(f"PKI check failed: {resp.status_code} {resp.text}")
            return 1
        logger.info("Using an existing PKI")

    # Retrieve device information
    logger.info("Retrieving device information")
    device_payload = {**jlink_params, **ddp_app_params}
    resp = _api_request(args.base_url, token, "POST", "/device/info", json_data=device_payload)
    if resp.status_code != 200:
        logger.error(f"Failed to get device info: {resp.status_code} {resp.text}")
        return 1

    device_info = resp.json()
    mac_address = device_info["mac_address"]
    nvm3_start_addr = args.nvm3_start_addr or device_info.get("nvm3_start_addr")
    nvm3_size = args.nvm3_size or device_info.get("nvm3_size")

    if not nvm3_start_addr or not nvm3_size:
        logger.error("NVM3 parameters not available (device info or --nvm3_start_addr/--nvm3_size)")
        return 1

    logger.debug(f"Device MAC address: {mac_address}")
    logger.debug(f"NVM3 start: 0x{nvm3_start_addr:08x}, size: {nvm3_size} bytes")

    # Check if device certificate exists
    cert_resp = _api_request(
        args.base_url,
        token,
        "GET",
        "/pki/certificate",
        params={"mac_address": mac_address},
    )

    if cert_resp.status_code == 404 or (cert_resp.status_code == 200 and not cert_resp.json()):
        # Device cert doesn't exist - create on device
        logger.info("Generating device certificate on the device")
        cert_payload = {
            **jlink_params,
            **ddp_app_params,
            "nvm3_start_addr": nvm3_start_addr,
            "nvm3_size": nvm3_size,
            "device_type": "router",
            "mac_address": mac_address,
            "cert_id": _CERT_INDEX,
            "key_id": _KEY_INDEX,
            "oem_hwtype": args.oem_hwtype,
        }
        resp = _api_request(
            args.base_url,
            token,
            "POST",
            "/pki/certificate/device",
            json_data=cert_payload,
        )
        if resp.status_code != 200:
            logger.error(f"Certificate creation failed: {resp.status_code} {resp.text}")
            return 1
        logger.info("Device certificate generated and stored")
    else:
        if cert_resp.status_code != 200:
            logger.error(f"Certificate check failed: {cert_resp.status_code} {cert_resp.text}")
            return 1

    # Store CA chain
    pki_resp = _api_request(args.base_url, token, "GET", "/pki/ca")
    if pki_resp.status_code != 200:
        logger.error(f"Failed to get PKI: {pki_resp.status_code} {pki_resp.text}")
        return 1
    pki = pki_resp.json()

    ca_certs = []
    cert_id = _CERT_INDEX + 1
    for ca_key in ["root", "mca", "mica"]:
        ca = pki.get(ca_key)
        if ca and isinstance(ca, dict) and ca.get("cert_path"):
            ca_certs.append({"cert_path": ca["cert_path"], "cert_id": cert_id})
            cert_id += 1

    if ca_certs:
        ca_store_payload = {
            **jlink_params,
            **ddp_app_params,
            "nvm3_start_addr": nvm3_start_addr,
            "nvm3_size": nvm3_size,
            "certificates": ca_certs,
            "keys": [],
        }
        resp = _api_request(
            args.base_url,
            token,
            "POST",
            "/device/storage",
            json_data=ca_store_payload)
        if resp.status_code != 200:
            logger.error(f"Store CA chain failed: {resp.status_code} {resp.text}")
            return 1
        logger.info("CA certificates stored to device")

    logger.info("Finished")
    return 0

if __name__ == "__main__":
    sys.exit(main())
