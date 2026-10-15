# Using the BTC Commands in RAILtest (DH5 TX and RX)

## Overview

Since RAIL_LIB-16186, the BTC (Bluetooth Classic) CLI commands are part of the
public `sl_rail_test_core` component. Before that change, they were only
available in `sl_rail_test_core_internal`. They are built only on devices with
the `device_has_btcphy` capability.

## Available Commands

| Command | Arguments | Description |
|---|---|---|
| `setBtcMode` | `0`/`1` | Disable or enable BTC mode (`sl_rail_btc_init()` / `sl_rail_btc_deinit()`) |
| `BTC_Init` | `accessCode1` `accessCode2` `bdAddr5` | Initialize the BTC hardware: 64-bit sync word and byte 5 of the BD_ADDR (`sl_rail_btc_hw_init()`) |
| `BTC_SetWhitening` | `seed` | Set the whitening seed (0 = no whitening) |
| `BTC_Prepare` | `txOrRx` `pktType` `payloadLen` `encryption` | Prepare the next TX or RX operation (`sl_rail_btc_prepare_tx_rx()`). Arguments are detailed in "Configuring BTC_Prepare" below. |
| `BTC_setAESEncryption` | none | Load the hardcoded AES-CCM configuration |
| `BTC_setAESEncryptionNonce` | none | Reload only the per-packet AES parameters (IV, payload counter, direction) |
| `BTC_setE0Encryption` | none | Load the hardcoded E0 configuration |
| `BTC_setE0MasterClock` | none | Reload the E0 master clock (`0x40034be`) |
| `BTC_configureContinuousTx` | `rate` | Continuous TX: 0 = disabled, 1 = BDR, 2 = EDR2, 3 = EDR3 |

### Configuring BTC_Prepare

- `txOrRx`: 0 = TX, 1 = RX.
- `pktType`: a value of the `sl_rail_btc_packet_type_t` enum:
  - 0 = ID, 1 = NULL, 2 = POLL, 3 = FHS
  - 4 = DH1, 5 = DH3, **6 = DH5**
  - 7 = DM1, 8 = DM3, 9 = DM5
  - 10 = 2-DH1, 11 = 2-DH3, 12 = 2-DH5, 13 = 3-DH1, 14 = 3-DH3, 15 = 3-DH5
  - 16 = HV1, 17 = HV2, 18 = HV3, 19 = DV
  - 20 = EV3, 21 = EV4, 22 = EV5, 23 = 2-EV3, 24 = 2-EV5, 25 = 3-EV3, 26 = 3-EV5
  - 27 = AUX1
- `payloadLen`: user payload length in bytes. It excludes the packet header,
  the payload header and the CRC.
- `encryption`: 0 = none, 1 = E0, 2 = AES.

In TX, `BTC_Prepare` also writes the packet into the TX FIFO:

- the packet header, with `LT_ADDR = 2` and `FLOW = 1`;
- the payload header, with `LLID = 2`;
- an incrementing payload: `0x00, 0x01, 0x02, ...`.

## Prerequisites

- Both boards (TX and RX) run a RAILtest application built with
  `sl_rail_test_core` on a BTC-capable device.
- `setBtcMode 1` enables BTC mode and loads the BTC PHY. The radio must be
  idle, so run `rx 0` first if RX is enabled.
- Both boards are on the same channel. The default is channel 0, which is
  2402 MHz.
- These settings must match on both sides:
  - the access code and `bdAddr5` passed to `BTC_Init`;
  - the packet type;
  - the encryption mode;
  - the whitening seed.

## Transmitting an AES-Encrypted DH5 Packet

```
setBtcMode 1
BTC_Init 0x63a96257 0xb0d159e3 0x12
setTxLength 24
BTC_setAESEncryption
BTC_Prepare 0 6 20 2
tx 1
```

What each step does:

1. `setBtcMode 1` enables BTC mode and loads the BTC PHY.
2. `BTC_Init 0x63a96257 0xb0d159e3 0x12` sets the sync word (access code
   `0xb0d159e3_63a96257`) and `bd_addr[5] = 0x12`.
3. `setTxLength 24` sets the TX buffer length. It must equal packet header +
   payload header + payload. For a 20-byte DH5, that is 2 + 2 + 20 = **24**.
   The CRC is not included.
4. `BTC_setAESEncryption` loads the AES key, IV and payload counter. These are
   the sample values from the BT Core Specification, Vol. 2 Part G,
   Section 1.2.
5. `BTC_Prepare 0 6 20 2` prepares a DH5 TX with a 20-byte payload and AES
   encryption, then fills the TX FIFO.
6. `tx 1` transmits one packet.

For other packet types, the `setTxLength` value depends on the payload header
size:

- 0 bytes for ID, NULL, POLL, FHS, HV and EV;
- 1 byte for DH1, DM1, DV and AUX1;
- 2 bytes for all other types.

## Receiving an AES-Encrypted DH5 Packet

```
setBtcMode 1
BTC_Init 0x63a96257 0xb0d159e3 0x12
BTC_setAESEncryption
BTC_Prepare 1 6 0 2
rx 1
```

What each step does:

1. `setBtcMode 1` enables BTC mode and loads the BTC PHY. `BTC_Init` uses the
   same values as the TX side. Otherwise synchronization fails.
2. `BTC_setAESEncryption` loads the same key and nonce as the TX side.
   Otherwise decryption or MIC check fails.
3. `BTC_Prepare 1 6 0 2` prepares a DH5 RX with AES encryption. The payload
   length is set to 0 in RX.
4. `rx 1` enables the receiver.

## Running a TX-to-RX Test

1. Run the RX sequence on the receiving board.
2. Run the TX sequence on the transmitting board right after.
3. On the RX side, check the receive event: packet received, CRC OK, payload
   `00 01 02 ... 13`.

## Variants

- **No encryption**: skip `BTC_setAESEncryption` and pass `0` as the last
  `BTC_Prepare` argument. For example, `BTC_Prepare 0 6 20 0` on TX and
  `BTC_Prepare 1 6 0 0` on RX.
- **E0 encryption**: replace `BTC_setAESEncryption` with `BTC_setE0Encryption`
  and pass `1` as the last `BTC_Prepare` argument.
- **Multiple AES packets**: before each new `BTC_Prepare`, run
  `BTC_setAESEncryptionNonce` on both sides so the TX and RX nonces stay
  aligned.
- **Whitening**: run `BTC_SetWhitening <seed>` after `BTC_Init`, with the same
  seed on both sides.
- **Continuous TX (RF measurements)**: `BTC_configureContinuousTx 1` for BDR,
  then `BTC_configureContinuousTx 0` to stop.
