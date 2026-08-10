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

# Metadata
__author__ = 'Silicon Laboratories, Inc'
__copyright__ = 'Copyright 2026, Silicon Laboratories, Inc.'

import os
import re
import socket
import struct
import subprocess
import time
from abc import ABC, abstractmethod

import pylink
import serial
from serial.tools import list_ports


class _Connection(ABC):
    # Maximum size of a single message chunk sent to the device.
    MSG_MAX_SIZE = 1200

    @property
    @abstractmethod
    def is_connected(self):
        """Indicate device connection is established.

        :return: True if connected, False otherwise
        """
        raise NotImplementedError

    @abstractmethod
    def connect(self):
        """Connect to the device."""
        raise NotImplementedError

    @abstractmethod
    def close(self):
        """Close connection."""
        raise NotImplementedError

    def start(self):
        """Prepare to communication start."""
        pass

    def stop(self):
        """Prepare to communication stop."""
        pass

    @abstractmethod
    def send(self):
        """Send data."""
        raise NotImplementedError

    @abstractmethod
    def receive(self):
        """Receive data."""
        raise NotImplementedError


class RttConnection(_Connection):
    # Reset strategy applied to the target for the whole provisioning session.
    #
    # The reset pin has to be involved. The default strategy resets the core and
    # the peripherals via SYSRESETREQ, which leaves the Virtual Secure Element
    # (VSE) untouched on devices without Secure Vault. Their VSE only re-runs its
    # boot sequence on a pin reset, and only that boot sequence republishes the
    # Storage Root Key in the VSE output mailbox. Any image that ran since the
    # last VSE boot may already have consumed that key, in which case the
    # provisioning application would encrypt the provisioned PSA ITS objects with
    # a key that the target firmware is unable to reproduce.
    #
    # The target is held in reset while the debug connection is established,
    # because a pin reset also resets the debug logic and therefore clears the
    # vector catch that would otherwise stop the core at its reset vector. The
    # application present in the flash must not execute at all: a TrustZone
    # application switches the core to Non-Secure state and restricts memory
    # access within its first milliseconds, which is enough to make the injected
    # provisioning application fault instead of starting up.
    RESET_STRATEGY = pylink.enums.JLinkResetStrategyCortexM3.CONNECT_UNDER_RESET

    def __init__(self,
                 chip_name: str,
                 serial_no: int = None,
                 hostname: str = None,
                 msg_max_size: int = _Connection.MSG_MAX_SIZE,
                 xml_path=None):
        """Communication interface to device.

        :param chip_name: Chip name
        :param serial_no: J-Link serial number to use USB interface
        :param hostname: Hostname to use Ethernet interface
        :param msg_max_size: Message chunk size limit
        :param xml_path: JLinkDevices.xml file to extend/overwrite the J-Link
                         devices database with new devices
        """
        self.jlink = pylink.JLink()
        self.chip_name = self.get_device_jlink_name(chip_name)
        self.serial_no = serial_no
        self.hostname = hostname
        self.msg_max_size = msg_max_size

        if not xml_path:
            try:
                p = subprocess.run(["slt", "where", "commander", "--ignore-slconf"],
                                    text=True,
                                    capture_output=True)
                if p.returncode == 0 and p.stdout:
                    xml_path = os.path.join(p.stdout.strip(), 'resources/jlink/JLinkDevices.xml')
            except (OSError, subprocess.SubprocessError):
                # SLT and/or Simplicity Commander is not installed.
                # J-Link devices database will not be extended.
                pass
        if xml_path:
            if os.path.exists(xml_path):
                self.jlink.exec_command(f"JLinkDevicesXMLPath = \"{xml_path}\"")
                print("J-Link Device XML extension added: " + xml_path)
            else:
                print(xml_path + " does not exist!")

    @property
    def is_connected(self) -> bool:
        """Indicate device connection is established.

        :return: True if connected, False otherwise
        """
        return self.jlink.connected()

    def connect(self):
        """Connect to device using serial_no or hostname."""
        if self.serial_no:
            self.jlink.open(serial_no=self.serial_no)
        elif self.hostname:
            ip_addr = socket.gethostbyname(self.hostname)
            self.jlink.open(ip_addr=f"{ip_addr}:19020")
        else:
            self.jlink.open()
        self.jlink.set_tif(interface=pylink.JLinkInterfaces.SWD)
        # Select the reset strategy before connecting, so that it also applies to
        # the reset that establishing the connection performs.
        self.jlink.set_reset_strategy(self.RESET_STRATEGY)
        self.jlink.connect(chip_name=self.chip_name, speed="auto", verbose=True)
        self.reset_and_halt()
        print(f"RTT connection opened to JLink chip: {self.chip_name}\t"
              f"Serial: {self.jlink.serial_number}")

    def close(self):
        """Close the connection."""
        self.jlink.close()

    def start(self, block_address: int = None, timeout: float = 10):
        """Start RTT processing.

        :param block_address: Optional RTT block address
        :param timeout: Time to wait for RTT processing started in seconds
        """
        self.jlink.rtt_start(block_address)
        start = now = time.time()
        while now < start + timeout:
            try:
                self.jlink.rtt_get_buf_descriptor(0, False)
            except pylink.errors.JLinkRTTException:
                pass
            else:
                return
            now = time.time()
        raise TimeoutError("The RTT control block of the application was not "
                           "found, the application did not start. Power cycle "
                           "the device and try again.")

    def stop(self):
        """Stops RTT and resets device."""
        self.jlink.rtt_stop()
        self.reset()

    def send(self, data: bytes, timeout: float = 10):
        """Send data to RTT buffer.

        :param data: Bytes to write to RTT buffer.
        :param timeout: Maximum time to wait for data to be written in seconds
        """
        remaining = data

        while remaining:
            if len(remaining) <= self.msg_max_size:
                chunk = remaining
            else:
                chunk = remaining[:self.msg_max_size]

            nb_sent = 0
            start = now = time.time()

            while nb_sent == 0 and now < start + timeout:
                nb_sent = self.jlink.rtt_write(0, chunk)
                now = time.time()

            if nb_sent == 0:
                raise TimeoutError

            remaining = remaining[nb_sent:]

    def receive(self, timeout: float = 10) -> bytes:
        """Read data from RTT buffer.

        :param timeout: Maximum time to wait for data to be received in seconds
        :return: Data received
        """
        data = bytes()
        start = now = time.time()
        while len(data) == 0 and now < start + timeout:
            data = self.jlink.rtt_read(0, self.msg_max_size)
            now = time.time()
        if len(data) == 0:
            raise TimeoutError
        return bytes(data)

    def reset(self):
        """Reset the device."""
        self.jlink.reset(halt=False)

    def reset_and_halt(self):
        """Reset the device and halt the core.

        Halting the core prevents the application present in the flash from
        running and consuming resources published by the boot sequence before
        the provisioning application is started.
        """
        try:
            self.jlink.set_reset_strategy(self.RESET_STRATEGY)
            self.jlink.reset(halt=True)
        except pylink.errors.JLinkException:
            # The reset pin is not necessarily routed to the debug connector on
            # custom hardware. Provisioning can still succeed on such boards,
            # as long as the device was power cycled recently enough.
            print("Reset over the reset pin failed. Falling back to the "
                  "default reset strategy.")
            self.jlink.set_reset_strategy(
                pylink.enums.JLinkResetStrategyCortexM3.NORMAL)
            self.jlink.reset(halt=True)

        # Injecting an application into a running core would silently do nothing,
        # therefore make sure that the core is really stopped.
        if not self.jlink.halted() and not self.jlink.halt():
            raise RuntimeError("The core could not be halted after reset. "
                               "Power cycle the device and try again.")

    def run_application(self, ram_addr: int, img: bytes):
        """Flash and run the provided firmware in device's RAM.

        :param ram_addr: Address on RAM memory to flash the firmware
        :param img: Firmware image
        """
        self.jlink.memory_write8(addr=ram_addr, data=list(img))
        sp, pc = self.jlink.memory_read32(addr=ram_addr, num_words=2)
        self.jlink.register_write(reg_index="R13 (SP)", value=sp)
        self.jlink.register_write(reg_index="R15 (PC)", value=pc)
        self.jlink.restart(num_instructions=0, skip_breakpoints=False)

    def get_mac_address(self) -> str:
        """Get the device's builtin EUI-64.

        :return: EUI-64 in String format without ':'
        """
        low, high = self.jlink.memory_read32(0x0FE08000 + 0x48, 2)
        return f"{high * 0x100000000 + low:016x}"

    @staticmethod
    def get_device_jlink_name(opn: str) -> str:
        """Map a device OPN to the closest J-Link device database name.

        :param opn: Device ordering part number
        :return: The J-Link device name matching the OPN
        """
        re_exx32_s2 = re.compile(
            r"^(EF[RM]32[MFBZSXP]G2[1-9][ABCEXL])[X\d]{3}(F\d{2,4}).*")
        re_sixx3 = re.compile(
            r"^SI[MFBZXP][AGNTFPREU](3\d{2})\w[X\d]{3}([XWG-M]).*")
        re_xgm2xx = re.compile(
            r"^([BMZF]GM2\d{2}[LPS][ABCD0]\d{2}).*")

        opn = opn.upper()
        if re_exx32_s2.search(opn):
            # EFR32MG21A020F1024IM32 -> EFR32MG21AxxxF1024 style
            opn = re_exx32_s2.sub(r"\1xxx\2", opn)
        elif re_sixx3.search(opn):
            # SIMG301M114KIHA0 -> Sixx301xxxxK
            opn = re_sixx3.sub(r"Sixx\1xxxx\2", opn)
        elif re_xgm2xx.search(opn):
            # BGM210LA22JIF3 -> BGM210LA22
            opn = re_xgm2xx.sub(r"\1", opn)

        return opn


class SerialConnection(_Connection):
    # Default VCOM baud rate. Matches the device iostream configuration.
    DEFAULT_BAUDRATE = 115200
    # Size of the DDP response header (uint32 status + uint16 data length), packed.
    RSP_HEADER_SIZE = 6
    # USB Vendor ID of the on-board SEGGER J-Link, which exposes the VCOM port.
    JLINK_VID = 0x1366
    # TCP port of the VCOM-over-Ethernet server exposed by Silicon Labs
    # WSTK/WPK boards, used for remote access when the hostname does not
    # specify one explicitly.
    DEFAULT_WSTK_TCP_PORT = 4901

    def __init__(self,
                 serial_no: int = None,
                 port: str = None,
                 hostname: str = None,
                 baudrate: int = DEFAULT_BAUDRATE,
                 msg_max_size: int = _Connection.MSG_MAX_SIZE):
        """Communication interface to the device over VCOM (serial).

        The device runs the (flashed) provisioning application and exchanges DDP
        requests and responses over its Virtual COM port. The port can be a
        local VCOM port or, when a hostname is given, a remote one reached over
        the network through the board's VCOM-over-Ethernet (TCP) server.

        :param serial_no: J-Link serial number, used to pick the matching VCOM
                          port when several boards are connected
        :param port: Serial port to use (e.g. ``COM13`` or ``/dev/ttyACM0``).
                     Autodetected from the connected board(s) when omitted
        :param hostname: Host (optionally ``host:port``) of the board's
                         VCOM-over-Ethernet server. Selects remote (Ethernet)
                         access instead of a local port
        :param baudrate: Serial baud rate
        :param msg_max_size: Message chunk size limit
        """
        if [serial_no, port, hostname].count(None) < 2:
            raise ValueError(
                "Specify at most one of 'serial_no', 'port' or 'hostname': "
                "they are mutually exclusive ways to select the connection "
                "('serial_no' auto-detects a local VCOM port, 'port' selects "
                "one explicitly, and 'hostname' selects a remote one)."
            )
        self.serial_no = serial_no
        self.port = port
        self.hostname = hostname
        self.baudrate = baudrate
        self.msg_max_size = msg_max_size
        self.serial = None

    @property
    def is_connected(self) -> bool:
        """Indicate device connection is established.

        :return: True if connected, False otherwise
        """
        return self.serial is not None and self.serial.is_open

    def connect(self):
        """Open the connection to the device.

        A local VCOM (serial) port is used by default. When a hostname was
        given, the device is reached over the network through the board's
        VCOM-over-Ethernet (raw TCP) server instead.
        """
        if self.is_connected:
            print("There is a connection open already and "
                  "parallel connections are not permitted!")
            return

        if self.hostname is not None:
            url = self._build_socket_url()
        else:
            if self.port is None:
                self.port = self._detect_port()
            url = self.port

        # serial_for_url() transparently accepts a local device name or a
        # serial-over-IP URL, so the same call covers both access methods.
        self.serial = serial.serial_for_url(url,
                                             baudrate=self.baudrate,
                                             timeout=0.1,
                                             write_timeout=10,
                                             rtscts=False,
                                             dsrdtr=False)
        # Drop any boot-time or stale bytes so the first response frame is
        # aligned. Some serial-over-IP backends do not implement buffer control.
        try:
            self.serial.reset_input_buffer()
            self.serial.reset_output_buffer()
        except (OSError, ValueError, NotImplementedError):
            pass

        location = self.hostname if self.hostname is not None else self.port
        print(f"Serial connection opened to: {location}\t"
              f"Baudrate: {self.baudrate}")

    def close(self):
        """Close the connection."""
        if self.is_connected:
            self.serial.close()

    def send(self, data: bytes, timeout: float = 10):
        """Send data to the device.

        :param data: Bytes to write to the serial port.
        :param timeout: Maximum time to wait for data to be written in seconds
        """
        self.serial.write_timeout = timeout
        remaining = data

        while remaining:
            chunk = remaining[:self.msg_max_size]
            try:
                nb_sent = self.serial.write(chunk)
            except serial.SerialTimeoutException as e:
                raise TimeoutError from e

            if nb_sent is None:
                nb_sent = len(chunk)
            if nb_sent == 0:
                raise TimeoutError("The serial port did not accept any data.")

            remaining = remaining[nb_sent:]

        self.serial.flush()

    def receive(self, timeout: float = 10) -> bytes:
        """Read a single DDP response frame from the device.

        A response consists of a fixed header (uint32 status + uint16 data
        length) optionally followed by a body. The body is only transmitted by
        the device on success and when there is data, so it is read accordingly.

        :param timeout: Maximum time to wait for the response in seconds
        :return: The raw response frame (header and body)
        """
        header = self._read_exact(self.RSP_HEADER_SIZE, timeout)
        status, data_len = struct.unpack('<iH', header)

        if status == 0 and data_len > 0:
            body = self._read_exact(data_len, timeout)
        else:
            body = b''

        return header + body

    def _read_exact(self, size: int, timeout: float) -> bytes:
        """Read exactly ``size`` bytes from the serial port within ``timeout``.

        :param size: Number of bytes to read
        :param timeout: Maximum time to wait in seconds
        :return: The bytes read
        """
        deadline = time.time() + timeout
        buffer = bytearray()

        while len(buffer) < size:
            remaining = deadline - time.time()
            if remaining <= 0:
                raise TimeoutError
            self.serial.timeout = remaining
            chunk = self.serial.read(size - len(buffer))
            if chunk:
                buffer.extend(chunk)

        return bytes(buffer)

    def _build_socket_url(self) -> str:
        """Build the raw-TCP socket URL for the configured hostname.

        The default WSTK/WPK TCP port is appended when the hostname does not
        already include one.

        :return: A pyserial ``socket://`` URL
        """
        host = self.hostname
        if ':' not in host:
            host = f"{host}:{self.DEFAULT_WSTK_TCP_PORT}"
        return f"socket://{host}"

    def _detect_port(self) -> str:
        """Find the VCOM serial port of the target board.

        When a J-Link serial number was provided, the port exposed by that
        probe is selected. Otherwise a single connected J-Link VCOM port is
        used, and an error is raised if the selection is ambiguous.

        :return: The name of the detected serial port
        """
        ports = list(list_ports.comports())
        candidates = []
        serial_matches = []

        for p in ports:
            hwid = p.hwid or ''
            description = p.description or ''
            is_jlink_vcom = (p.vid == self.JLINK_VID) or ('JLink CDC UART' in description)

            if not is_jlink_vcom:
                continue

            if self.serial_no is not None:
                serial_no = str(self.serial_no)
                if serial_no in (p.serial_number or '') or serial_no in hwid:
                    serial_matches.append(p.device)
            else:
                candidates.append(p.device)

        if self.serial_no is not None:
            if len(serial_matches) == 1:
                return serial_matches[0]
            if len(serial_matches) == 0:
                raise RuntimeError(f"No VCOM port found for J-Link serial {self.serial_no}. "
                                   "Specify the port explicitly.")
            raise RuntimeError(f"Multiple VCOM ports found for J-Link serial {self.serial_no}: "
                               f"{', '.join(serial_matches)}. Specify the port explicitly.")
        if len(candidates) == 1:
            return candidates[0]
        if len(candidates) == 0:
            raise RuntimeError("No J-Link VCOM port found. Specify the port explicitly.")
        raise RuntimeError(f"Multiple VCOM ports found: {', '.join(candidates)}. "
                           "Specify the port explicitly.")
