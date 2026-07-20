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

from ddp_cmd import Command, Response

def common_name_gen(rtt):
    """Request the device to generate a certificate Common Name.

    Send a certificate get-Common-Name command over the DDP RTT interface and
    return the device status together with the Common Name payload. On a
    non-zero status a failure message is printed.

    Args:
        rtt: RTT transport exposing ``rtt_send`` and ``rtt_receive`` methods
            used to communicate with the device.

    Returns:
        tuple[int, bytes]: A ``(status, body)`` pair where ``status`` is the
            device status code (``0`` on success) and ``body`` is the returned
            Common Name payload.
    """
    print("Generating a common name...")
    rtt.rtt_send(CommandCertGetCn())
    resp = ResponseCertGetCn(rtt.rtt_receive())
    if resp.status != 0:
        print(f"Get Common name failure: {resp.status:#06x}")
        return resp.status, None
    return resp.status, resp.body

class CommandList:
    """List of DDP commands."""
    SL_DDP_CMD_CERT_GET_CN = 10

class CommandCertGetCn(Command):
    def __new__(cls) -> "CommandCertGetCn":
        """Input structure of DDP command for getting the Common Name.
        """
        return super().__new__(
            cls,
            CommandList.SL_DDP_CMD_CERT_GET_CN,
            b'' # No command body
        )

class ResponseCertGetCn(Response):
    """Output structure of DDP command for getting the Common Name."""
    pass
