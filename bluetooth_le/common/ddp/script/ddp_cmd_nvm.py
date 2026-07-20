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
import struct
from ddp_cmd import Command, Response

def nvm_set(rtt, key, data, check=True):
    """Write a value to an NVM key over the DDP RTT interface.

    Send an NVM set command for the given key. When ``check`` is enabled,
    read the key back and verify that the stored value matches the data that
    was written. Any failure is reported via a printed message and a non-zero
    status code.

    Args:
        rtt: RTT transport exposing ``rtt_send`` and ``rtt_receive`` methods
            used to communicate with the device.
        key (int): NVM key identifier to write.
        data (bytes): Payload to store under ``key``.
        check (bool): If True, read the key back and verify the stored value
            matches ``data``. Defaults to True.

    Returns:
        int: ``0`` on success. Otherwise the non-zero device status from the
            failing set or get command, or ``1`` if the read-back value does
            not match ``data``.
    """
    print(f"Set NVM. Key: {hex(key)}")
    rtt.rtt_send(CommandNvmSet(key, data))
    resp = ResponseNvmSet(rtt.rtt_receive())
    if resp.status != 0:
        print(f"Set NVM failure: {resp.status:#06x}")
        return resp.status

    if check:
        rtt.rtt_send(CommandNvmGet(key))
        resp = ResponseNvmGet(rtt.rtt_receive())
        if resp.status != 0:
            print(f"Get NVM failure: {resp.status:#06x}")
            return resp.status
        if resp.body != data:
            print(f"NVM key mismatch!{os.linesep}Original key:{os.linesep}{data}{os.linesep}Received:{resp.body}")
            return 1 # Set status to generic error
    return resp.status

class CommandList:
    """List of DDP commands."""
    SL_DDP_CMD_NVM_SET = 0
    SL_DDP_CMD_NVM_GET = 1

class CommandNvmSet(Command):
    def __new__(cls, id: int, data: bytes) -> "CommandNvmSet":
        """Input structure of DDP command for NVM set.
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_NVM_SET,
            struct.pack('<LH', id, len(data)) + data
        )

class ResponseNvmSet(Response):
    """Output structure of DDP command for NVM set."""
    pass

class CommandNvmGet(Command):
    def __new__(cls, id: int) -> "CommandNvmGet":
        """Input structure of DDP command for NVM get.
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_NVM_GET,
            struct.pack('<L', id)
        )

class ResponseNvmGet(Response):
    """Output structure of DDP command for NVM get."""
    pass
