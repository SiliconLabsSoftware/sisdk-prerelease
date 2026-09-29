#!/usr/bin/env python3
"""
Example on how to utilize the DDP python modules to provision a device.
"""

import argparse
import logging
import os
import wisun.common
import wisun.command
import wisun.response
import ddp.command
import ddp.response
from ddp.rtt import SerialWire
from service import wisunpki
from service.wisunpki import WisunDeviceType
import time

if __name__ == '__main__':
    logger = logging.getLogger('provision')
    logger.setLevel(logging.DEBUG)
    ch = logging.StreamHandler()
    ch.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
    logger.addHandler(ch)

    parser = argparse.ArgumentParser(
        description='Script for performing Wi-SUN provisioning.')
    parser.add_argument('--jlink_ser', action='store',
                        default=None, help='Serial number of J-Link adapter')
    parser.add_argument('--jlink_host', action='store', default=None,
                        help='Host name or IP address of J-Link adapter')
    parser.add_argument('--prov_img', action='store',
                        required=True, help='Provisiong application binary')
    parser.add_argument('--nvm3_start_addr', action='store',
                        default=None, help='NVM3 start address in flash')
    parser.add_argument('--nvm3_size', action='store',
                        default=None, help='NVM3 size in bytes')
    parser.add_argument('--oem_country', action='store',
                        default=None, help='OEM country code')
    parser.add_argument('--oem_company', action='store',
                        default=None, help='OEM company name')
    parser.add_argument('--oem_hwtype', action='store',
                        required=True, help='OEM hwType OID')
    parser.add_argument('--cwd', action='store', default=None,
                        help='Working directory to change to before running')
    args = parser.parse_args()

    with open(args.prov_img, 'rb') as f:
        provisioning_app = f.read()

    if args.cwd:
        os.chdir(args.cwd)

    # NVM Object ID for storing certificates
    cert_index = 0x100
    # PSA Crypto Key ID for storing private key
    key_index = 0x100

    # Setup PKI if needed
    pki = wisunpki.get_pki()
    if pki and len(pki) > 0:
        logger.info("Using an existing PKI")
    else:
        logger.info("Creating a local PKI")
        if not args.oem_company or not args.oem_country:
            raise ValueError(
                "--oem_company and --oem_country are required when creating a new PKI")
        wisunpki.create_pki(
            oem_name=args.oem_company, oem_country=args.oem_country, chain_length=3)
        pki = wisunpki.get_pki()

    sw = SerialWire('EFR32FG28AxxxF1024', args.jlink_ser, args.jlink_host)

    try:
        # Connect to the device
        logger.info("Opening SerialWire connection to the device")
        start_ts = time.perf_counter_ns()
        sw.connect()
        sw.reset_and_halt()
        end_ts = time.perf_counter_ns()
        logger.debug(f"Connection opened in {(end_ts - start_ts) / 1000000} ms")

        # Retrieve device information
        logger.info("Retrieving device flash information")
        flash_size, flashpage_size = sw.get_flash_size()
        logger.debug(
            f"Device flash size: {flash_size} bytes, flash page size: {flashpage_size} bytes")

        logger.info("Retrieving device MAC addresss")
        hwserialnum = sw.get_mac_address()
        logger.debug(f"Device MAC address: {hwserialnum.hex()}")

        # Determine NVM3 parameters if not set
        if not (args.nvm3_start_addr or args.nvm3_size):
            logger.info("Determining default NVM3 parameters")
            args.nvm3_start_addr, args.nvm3_size = wisun.common.get_default_nvm3_parameters(
                flash_size=flash_size, flashpage_size=flashpage_size, nvm3_start_addr=args.nvm3_start_addr, nvm3_size=args.nvm3_size)
            logger.debug(
                f"NVM3 start address: 0x{args.nvm3_start_addr:08x}, NVM3 size: {args.nvm3_size} bytes")

        # Inject and run provisioning application
        logger.info("Injecting provisioning application")
        start_ts = time.perf_counter_ns()
        sw.run_application(provisioning_app)
        sw.rtt_start()
        end_ts = time.perf_counter_ns()
        logger.info(f"Provisioning application running in {(end_ts - start_ts) / 1000000} ms")

        # Initialize NVM
        logger.info("Initializing NVM")
        start_ts = time.perf_counter_ns()
        tx = ddp.command.InitializeNvm(
            base_addr=args.nvm3_start_addr, nvm3_inst_size=args.nvm3_size)
        sw.rtt_send(tx)
        rx = sw.rtt_receive()
        resp = ddp.response.InitializeNvm(rx)
        end_ts = time.perf_counter_ns()
        assert resp.status == 0, f"Failure during NVM initialization ({resp.status})"
        logger.info(f"NVM initialized in {(end_ts - start_ts) / 1000000} ms")

        device = wisunpki.get_credential(hwserialnum=hwserialnum)
        if device is None or "cert_path" not in device:
            # Generate device key-pair
            logger.info("Generating device key pair on the device")
            start_ts = time.perf_counter_ns()
            tx = wisun.command.GenerateKeyPair(key_index)
            sw.rtt_send(tx)
            rx = sw.rtt_receive()
            resp = wisun.response.GenerateKeyPair(rx)
            end_ts = time.perf_counter_ns()
            assert resp.status in (
                0, 19), f"Failure during device key pair generation ({resp.status})"
            if resp.status == 19:
                logger.warning("Device key pair already exists")
            else:
                logger.info(f"Device key pair generated in {(end_ts - start_ts) / 1000000} ms")

            # Generate CSR
            logger.info("Generating CSR on the device")
            start_ts = time.perf_counter_ns()
            tx = wisun.command.GenerateCsr(key_index)
            sw.rtt_send(tx)
            rx = sw.rtt_receive()
            resp = wisun.response.GenerateCsr(rx)
            end_ts = time.perf_counter_ns()
            assert resp.status == 0, f"Failure during CSR generation ({resp.status})"
            logger.info(f"CSR generated in {(end_ts - start_ts) / 1000000} ms")

            # Generate device certificate
            logger.info("Generating device certificate")
            device = wisunpki.generate_from_csr(
                hwtype_oid=args.oem_hwtype, hwserialnum=hwserialnum,
                device_type=WisunDeviceType.ROUTER, csr_data=resp.csr)
            logger.info("Device certificate generated")

        # Write device certificate to NVM
        with open(device["cert_path"], "rb") as f:
            cert_data = f.read()
            logger.info(
                f"Storing device certificate to NVM Object ID 0x{cert_index:x}")
            start_ts = time.perf_counter_ns()
            tx = ddp.command.WriteNvm(cert_index, cert_data)
            sw.rtt_send(tx)
            rx = sw.rtt_receive()
            resp = ddp.response.WriteNvm(rx)
            end_ts = time.perf_counter_ns()
            assert resp.status == 0, f"Failure storing device certificate to NVM ({resp.status})"
            logger.info(f"Device certificate stored in {(end_ts - start_ts) / 1000000} ms")

        # Write trusted CA certificates to NVM
        for ca in pki:
            if ca.get("cert_path"):
                with open(ca["cert_path"], "rb") as f:
                    cert_data = f.read()
                    cert_index += 1
                    logger.info(
                        f"Storing CA certificate to NVM Object ID 0x{cert_index:x}")
                    start_ts = time.perf_counter_ns()
                    tx = ddp.command.WriteNvm(cert_index, cert_data)
                    sw.rtt_send(tx)
                    rx = sw.rtt_receive()
                    resp = ddp.response.WriteNvm(rx)
                    end_ts = time.perf_counter_ns()
                    assert resp.status == 0, f"Failure storing CA certificate to NVM ({resp.status})"
                    logger.info(f"CA certificate stored in {(end_ts - start_ts) / 1000000} ms")

    finally:
        sw.rtt_stop()
        sw.reset()
        sw.close()

    logger.info("Finished")
