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

import struct
from dataclasses import dataclass
from ddp_cmd import Command, Response

def psa_its_set(conn, id, data, check=True):
    """Store a value in PSA ITS over the DDP Connection interface.

    Send a PSA ITS set command for the given ID. When ``check`` is enabled,
    read the entry back and verify that the stored value matches the data that
    was written. Any failure is reported via a printed message.

    Args:
        conn: Transport layer exposing ``send`` and ``receive`` methods
            used to communicate with the device.
        id (int): PSA ITS UID to write.
        data (bytes): Payload to store under ``id``.
        check (bool): If True, read the entry back and verify the stored value
            matches ``data``. Defaults to True.

    Returns:
        int: ``0`` on success, or ``1`` if the read-back value does not match
            ``data``.
    """
    print(f"Set PSA ITS. ID: {hex(id)}")
    conn.send(CommandPsaItsSet(id, data))
    resp = ResponsePsaItsSet(conn.receive())
    if resp.status != 0:
        print(f"Set PSA ITS failure: {resp.status}")
        return resp.status

    if check:
        conn.send(CommandPsaItsGet(id))
        resp = ResponsePsaItsGet(conn.receive())
        if resp.status != 0:
            print(f"Get PSA ITS failure: {resp.status}")
        elif resp.body != data:
            print(f"PSA ITS key mismatch!\nOriginal key:\n{data}\nReceived:\n{resp.body}")
            return 1 # Set status to generic error
    return resp.status

def psa_key_gen(conn, att, check=True):
    """Generate a PSA Crypto key on the device over the DDP Connection interface.

    Send a PSA key generation command described by ``att``. When ``check`` is
    enabled, read the key attributes back and compare them against ``att``.
    Any failure is reported via a printed message.

    Args:
        conn: Transport layer exposing ``send`` and ``receive`` methods
            used to communicate with the device.
        att (KeyAtt): Attributes describing the key to generate.
        check (bool): If True, read the key attributes back and compare them
            against ``att``. Defaults to True.

    Returns:
        tuple[int, bytes | None]: A ``(status, key)`` pair. ``status`` is the
            device status code (``0`` on success) and ``key`` is the generated
            key material, or ``None`` if generation or the attribute read-back
            failed.
    """
    print(f"Generate PSA key. ID: {hex(att.key_id)}")
    conn.send(CommandPsaKeyGen(att))
    resp = ResponsePsaKeyGen(conn.receive())
    if resp.status != 0:
        print(f"Generation failure: {resp.status}")
        return resp.status, None
    print(f"Generated Key: ({resp.key})")
    key = resp.key

    if check:
        conn.send(CommandPsaGetAtt(att.key_id))
        resp = ResponsePsaGetAtt(conn.receive())
        if resp.status != 0:
            print(f"Get attributes failure: {resp.status}")
            return resp.status, None
        if resp.key_att != att:
            print(f"Mismatching key attributes!\nOriginal:\n{att}\nReceived:\n{resp.key_att}")
            return 1, None # Set status to generic error
    return resp.status, key

def psa_key_inj(conn, att, key, check=True):
    """Inject an existing PSA Crypto key into the device over DDP Connection.

    Send a PSA key injection command carrying ``key`` and its attributes
    ``att``. When ``check`` is enabled, read the key attributes back and
    compare them against ``att``. Any failure is reported via a printed
    message.

    Args:
        conn: Transport layer exposing ``send`` and ``receive`` methods
            used to communicate with the device.
        att (KeyAtt): Attributes describing the key being injected.
        key (bytes): Key material to inject.
        check (bool): If True, read the key attributes back and compare them
            against ``att``. Defaults to True.

    Returns:
        int: The device status code (``0`` on success), or ``1`` if the read-
            back attributes do not match ``att``.
    """
    print(f"Inject PSA key. ID: {hex(att.key_id)}")
    conn.send(CommandPsaKeyInj(att, key))
    resp = ResponsePsaKeyInj(conn.receive())
    if resp.status != 0:
        print(f"Injection failure: {resp.status}")
        return resp.status

    if check:
        conn.send(CommandPsaGetAtt(att.key_id))
        resp = ResponsePsaGetAtt(conn.receive())
        if resp.status != 0:
            print(f"Get attributes failure: {resp.status}")
            return resp.status
        if resp.key_att != att:
            print(f"Mismatching key attributes!\nOriginal:\n{att}\nReceived:\n{resp.key_att}")
            return 1 # Set status to generic error
    return resp.status

class CommandList:
    """List of DDP commands."""
    SL_DDP_CMD_PSA_ITS_SET      = 2
    SL_DDP_CMD_PSA_ITS_GET      = 3
    SL_DDP_CMD_PSA_KEY_GEN      = 4
    SL_DDP_CMD_PSA_KEY_INJ      = 5
    SL_DDP_CMD_PSA_KEY_GET_ATT  = 6

@dataclass
class KeyAtt:
    # Permitted usage of the key as psa_key_usage_t
    usage_flags: int
    # Length of key in bits
    bits: int
    # Permitted algorithms of the key as psa_algorithm_t
    algo: int
    # Type of the key as psa_key_type_t
    key_type: int
    # PSA Key ID
    key_id: int

class CommandPsaItsSet(Command):
    def __new__(cls, id: int, data: bytes) -> "CommandPsaItsSet":
        """Input structure of DDP command for PSA ITS set.
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_PSA_ITS_SET,
            struct.pack('<QH', id, len(data)) + data
        )

class ResponsePsaItsSet(Response):
    """Output structure of DDP command for PSA ITS set."""
    pass

class CommandPsaItsGet(Command):
    def __new__(cls, id: int) -> "CommandPsaItsGet":
        """Input structure of DDP command for PSA ITS get.
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_PSA_ITS_GET,
            struct.pack('<Q', id)
        )

class ResponsePsaItsGet(Response):
    """Output structure of DDP command for PSA ITS get."""
    pass

class CommandPsaKeyGen(Command):
    def __new__(cls, att: KeyAtt) -> "CommandPsaKeyGen":
        """Input structure of DDP command for generating a PSA Crypto key.

        :param: Key arguments
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_PSA_KEY_GEN,
            struct.pack('<LLLHL', att.usage_flags, att.bits, att.algo, att.key_type, att.key_id)
        )

class ResponsePsaKeyGen(Response):
    def __init__(self, data: bytes):
        """Output structure of DDP command for generating a PSA Crypto key.

        :param data: Response data
        """
        super().__init__(data)
        self.key = None
        if self.status == 0:
            length, = struct.unpack('<L', self.body[:4])
            self.key = self.body[4:4+length]

class CommandPsaKeyInj(Command):
  def __new__(cls, att: KeyAtt, key: bytes) -> "CommandPsaKeyInj":
        """Input structure of DDP command for injecting a PSA Crypto key.

        :param: Key arguments
        :param key: Injected key
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_PSA_KEY_INJ,
            struct.pack('<LLLHLL', att.usage_flags, att.bits, att.algo, att.key_type, att.key_id, len(key)) + key
        )

class ResponsePsaKeyInj(Response):
    """Output structure of DDP command for injecting a PSA Crypto key."""
    pass

class CommandPsaGetAtt(Command):
    def __new__(cls, id: int) -> "CommandPsaGetAtt":
        """Input structure of DDP command for getting PSA key attributes.
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_PSA_KEY_GET_ATT,
            struct.pack('<L', id)
        )

class ResponsePsaGetAtt(Response):
    def __init__(self, data: bytes):
        """Output structure of DDP command for getting PSA key attributes.

        :param data: Response data
        """
        super().__init__(data)
        self.key_att = None
        if self.status == 0:
            format = '<LLLHL'
            self.key_att = KeyAtt(*struct.unpack(format, self.body[:struct.calcsize(format)]))
