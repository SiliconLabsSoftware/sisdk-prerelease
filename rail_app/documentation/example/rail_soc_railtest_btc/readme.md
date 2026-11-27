# RAILTest usage for BLE / BT manual measurements

#Known issues and limitations
- The RAILTest application does not support Energy Management. It stays in EM0 so any power-consumption measurement will be much higher than the values expected in EM1P.


# Purpose/Scope

These instructions explain how to use the RAILTest application to transmit and receive BLE and Bluetooth Classic (BTC) test packets and measure packet error rate (PER). RAILTest supports RF testing across multiple Low Power Wireless radio (LPW) protocols.

## Prerequisites/Setup Requirements

### Hardware Requirements

The measurement setup is:

- BRD4002 A/B Wireless Pro Kit (WPK) Mainboard (using USB-C)
- BRD6360A Radio Board
  - Mounted on the BRD9092A Adapter Board for the WPK Mainboard and based on the SiWx353 IC with part number - SiWG353M001ENT.
- USB-C cable to connect the board to your computer
- A computer with USB ports to power the boards and connect to them through a serial terminal

RF Connections: BRD6360A in a shield box <-> coax cable <-> CMW RF port. The shield box helps improve RX sensitivity.
 
Make sure to know the coax cable S21 loss. For example, the SWG connector cable that we have in the lab has -0.7 dB S21 loss. If you have a different cable whose one end is also SWG type (required for BRD6360A), then it’s hard to do a 2-PORT VNA measurement: you can calculate the S21 loss from a 1-PORT measurement as S11_dB / 2 (which is -1.4 / 2 = -0.7 for the cable mentioned above).

### Software Requirements

- Simplicity Studio IDE via Simplicity Installer
- Visual Studio Code
- SiWG353 Pre-Alpha1 package
- Serial Terminal - [Tera Term](https://ttssh2.osdn.jp/index.html.en) or similar

See `Getting Started with SiWG353` for more details about the hardware setup and connections.

## Getting Started

Refer to `Getting Started with SiWG353` to install the software and prepare the devices.

Create an example project according to chapter **Create Your First Example Application Project** using `ALPHA - RAIL - SoC RAILtest` and build and flash the boards according to the instructions in the `Getting Started with SiWG353`.

Confirm that the RAILTest application responds in the serial terminal. For example, use `getAppMode` for a short response or `help` to list the commands; the help output is long.

For more information about the RAILTest usage, see:

- the public documentation [RAILtest User Guide](https://docs.silabs.com/rail/latest/railtest-users-guide/)
  
  - [RAILtest - Protocol-Specific Features - Bluetooth Low Energy](https://docs.silabs.com/rail/latest/railtest-users-guide/05-protocol-specific-features#bluetooth-low-energy )

- Bluetooth Classic BR/EDR commands are not yet public, hence documented below

## BLE / BT manual measurements

Examples: BLE LE 1M, CH19; BT DH1, CH39. The DUT is powered at 3.3 V, and the RAILTest VCOM prompt is open.

## Measurement counts

- **TX**: one CMW measurement per selected test condition (PHY/packet, channel, and power). RAILTest sends a repeated packet train; CMW uses 30 samples.
- **RX**: one packet batch per power point: the settings used are **1500 BLE packets**, or **about 1000 BT packets**. Sweep point count varies. One CMW PER start per BLE point; one CMW ARB trigger per BT point.

## TX

### BLE TX — LE 1M, CH19, 10 dBm

1. **CMW:** Open **Bluetooth → Multi Evaluation**. In **RF Settings**, choose the AMF RF port and set input path loss. In **Input Signal**, choose **LE / RF PHY Test**, **1 Mbps**, **CH19 / 2440 MHz**, **PRBS9**, **37 bytes**, access address `0x71764129`. Set expected power to **10 dBm**, margin **10 dB**, statistic count **30**. Leave the measurement ready.

2. **RAILTest:** Send:

```text
setBleMode 1
setBle1Mbps                         # Other PHYs: setBle2Mbps, setBleCoding 2 (500 kbps), setBleCoding 0 (125 kbps)
setBleChannelParams 17 0x71764129 0x555555 1   # CH19 uses logical channel 17; choose any supported BLE RF channel 0–39
setChannel 19                       # BLE RF channel 0–39
setPower 100                         # dBm mode is dBm × 10; 10 dBm = 100
setTxLength 39                       # 2-byte DTM header + 37-byte payload
setTxPayloadQuiet 0 0 37 255 193 251 232 76 144 114 139 231 179 81 137 99 171   # PRBS9 example; load the next chunks too
setTxPayloadQuiet 16 35 35 2 132 24 114 170 97 47 59 81 168 229 55 73 251
setTxPayloadQuiet 32 201 202 12 24 83 44 253
setTxDelay 1
tx 0                                # Starts repeated packets; send tx 0 once again to stop
```

3. **CMW:** Press **RUN**. Read Average Power, Peak Power, modulation, and ACP. After it finishes, send `tx 0` once to stop RAILTest.

### BT TX — DH1, CH39, 10 dBm

1. **CMW:** Open **Bluetooth → Multi Evaluation**. Choose the AMF RF port, input path loss, **BR / DH1**, **CH39 / 2441 MHz**, BD address **000012345678**, expected power **10 dBm**, margin **10 dB**, statistic count **30**. Leave the measurement ready.

2. **RAILTest:** Send:

```text
setBtcMode 1
BTC_Init 0x63a96257 0xb0d159e3 0x12   # BD address 000012345678; change address and matching waveform together
setChannel 39                          # BT channel 0–78; frequency = 2402 + channel MHz
setPower 100                           # 10 dBm; use desired dBm × 10
setTxLength 30                         # DH1 max: 27 payload bytes + 3 bytes overhead
BTC_Prepare 0 4 27 0                   # TX=0; DH1=4; other types: DH3=5, DH5=6, 2-DH*=10–12, 3-DH*=13–15
setTxDelay 1
tx 0                                   # Starts repeated packets; send tx 0 once again to stop
```

3. **CMW:** Press **RUN**. Read the TX power and BT spec results. After it finishes, send `tx 0` once to stop RAILTest.

## RX

### BLE RX — LE 1M, CH19, start at −96 dBm

1. **CMW:** Open **Bluetooth Signaling**. Choose **LE**, **CH19 / 2440 MHz**, **LE 1M**, **RF PHY Test**, **PRBS9**, **37 bytes**, access address `0x71764129`, **1500 packets**, and output path loss. Press **ON** once. For each power point, set the DUT-side level; press **INIT / Start PER** once.

2. **RAILTest:** Configure the receiver:

```text
setBleMode 1
setBle1Mbps                         # Other PHYs: setBle2Mbps, setBleCoding 2 (500 kbps), setBleCoding 0 (125 kbps)
setBleChannelParams 17 0x71764129 0x555555 1   # CH19 -> logical 17; change both channel values for another channel
setChannel 19
```

3. For each power point, start listening **before** the CMW burst:

```text
resetCounters
rx 1
```

4. **CMW:** Press **INIT / Start PER** once. Wait for all 1500 packets.

5. **RAILTest:** Send `status`, record `RxCount`, `RxCrcErrDrop`, `SyncDetect`, then send `rx 0`.

```text
PER = (1500 - RxCount) / 1500 × 100%
```

Pass: **PER ≤ 30.8%**. Example sweep: start at **−96 dBm**; if PASS, go weaker by 2 dB; if FAIL, go stronger by 2 dB; refine the PASS/FAIL edge by 1 dB, then 0.1 dB. Use the selected PHY's DS start; AMF stays within ±10 dB.

### BT RX — DH1, CH39, start at −94 dBm

1. **CMW:** Open **GPRF Generator**. Choose **ARB**, load the validated `bt_DH1_000012345678.wv` waveform (upload to CMW `USERDATA` first if missing), set RF port, **2441 MHz**, output path loss, and DUT-side level **−94 dBm**. Set repetition to **SING**, set cycles from the matching `.wv.json` file (at least 1000 packets), choose **Manual Trigger**, then press **ON** to arm.

2. **RAILTest:** Configure BT RX:

```text
setBtcMode 1
BTC_Init 0x63a96257 0xb0d159e3 0x12   # Must match the waveform BD address
setChannel 39                          # BT channel 0–78
BTC_Prepare 1 4 0 0                    # RX=1; DH1=4; other types: DH3=5, DH5=6, DM1–5=7–9, 2-DH*=10–12, 3-DH*=13–15
```

3. For each power point, start listening, then trigger exactly one CMW burst:

```text
resetCounters
rx 1
```

4. **CMW:** Press **Manual Trigger → Execute** once. Wait for the ARB burst to finish.

5. **RAILTest:** Send `status`; record `RxCount`, `RxCrcErrDrop`, `SyncDetect`; then send `rx 0`. Turn the CMW generator **OFF** before changing setup.

```text
PER = (actual packets sent - RxCount) / actual packets sent × 100%
```

AMF targets **1000 packets per power point**. Full waveform cycles can make the actual count slightly higher; use `cycles × frames_per_cycle` from the sidecar. BR pass limit comes from **0.1% BER**; EDR pass limit comes from **0.01% BER**, converted to PER for that packet length. Use the selected packet's DS start; AMF stays within ±10 dB.

## Detailed RX setup reference

### BT RX with a CMW ARB burst

**1. Pick the right waveform**

The `.wv` file must match the BT packet type being tested: `DH1`, `DH5`, `2-DH5`, etc. It must also have the same BD address as the DUT. The address/access code is already inside the waveform; there is no separate BD-address field to set in the CMW ARB screen. A mismatch usually means `RxCount` stays at zero.

**2. Set up the CMW**

- Press **TASKS** and open **GPRF 2 Generator** if the cable is on **RF2**. AMF’s current setup uses RF2C/TX2. Use GPRF 1 for an RF1 cable.
- Set **Baseband Mode = ARB**, then open **ARB** and select the matching .wv file from the CMW.
- Set **Frequency = 2402 MHz + BT channel**. For example, channel 38 is 2440 MHz.
- Set **Ext. Att. (Output)** to the measured cable/fixture loss. Then set **Level (RMS)** to the wanted level at the DUT input.
- Set **Repetition = Single**, **Cycles = N**, and **Trigger Source = Manual**. These AMF waveforms contain one packet per cycle, so 1,000 packets means 1,000 cycles.
- Turn **GPRF Generator ON**. Let any startup playback finish before starting the DUT counters; AMF does this before each measured burst.

**3. Start and measure**

- On the DUT, configure the same packet type, channel, and BD address. Reset its counters, then start RX.
- Trigger the CMW burst **once** using its manual ARB trigger control.
- Wait for the full burst duration. For 1,000 packets, that is about 1.25 s for DH1 or 3.75 s for DH5; use the waveform’s cycle time if available.
- Read DUT status. `RxCount` is the number of good packets received. Calculate:

   `PER (%) = (burst packets − RxCount) / burst packets × 100`

The CMW generator can still show **ON** after the burst; that means the generator is enabled, not that packets are still being sent. AMF uses the calculated burst time and the DUT counter settling to decide the measurement point has finished.

**BLE is different from BT Classic**: AMF does **not** use a `.wv` file or GPRF ARB for BLE RX. CMW sends BLE test packets from its **Bluetooth Signaling** task.

### BLE RX with CMW Bluetooth Signaling

**1. Set up the CMW**

- Press **TASKS** and open **Bluetooth Signaling**. Do not open **GPRF Generator**.
- Use the RF output connected to the DUT. If the cable is on RF2, use **RF2C/TX2**; if it is on RF1, use **RF1C/TX1**.
- Set the BLE RF channel to match the DUT: channels **0–39**. Frequency is `2402 + 2 × channel MHz`. For example, channel 19 is **2440 MHz**.
- Set the matching PHY:
  - `LE 1M` for 1 Mbps
  - `LE 2M` for 2 Mbps
  - `LE Coded, S=2` for 500 kbps
  - `LE Coded, S=8` for 125 kbps
- Set packet type to **RF PHY Test**. Set payload length to **37 bytes**, pattern to **PRBS9**, and Access Address / Sync Word to **0x71764129**. BLE uses this access address; it does not use a BT BD address.
- In **RX Quality / PER**, set the packet count to **N** for the selected PHY. Use the same N in your calculation.
- Set **Ext. Att. (Output)** to the measured cable/fixture loss. Set the CMW level to the wanted level **at the DUT input**.
- Turn Bluetooth Signaling **ON** and wait until the CMW is ready. Do not start PER yet.

**2. Start the DUT**

- Configure DUT RAILTest RX for the same PHY, channel, 37-byte packet, PRBS9, and Access Address.
- Reset the DUT counters.
- Start DUT RX.

**3. Send the packets**

- On the CMW, press **Start / Init PER** once in **RX Quality / PER**. This sends the configured N packets.
- Wait for the burst to finish, then read the DUT status.
- `RxCount` is the number of good packets. Calculate:

   `PER (%) = (N − RxCount) / N × 100`

- Also record `RxCrcErrDrop` and `SyncDetect`. If `RxCount` is zero, check the PHY, channel, Access Address, RF route, and level.
- With 1,000 packets, expect roughly **0.63 s** for LE 1M or 2M, **1.88 s** for 500 kbps, and **3.75 s** for 125 kbps, plus a little settling time. Scale the time with N.
- CMW may still show **ON** after the burst. That means Bluetooth Signaling is enabled; it does not mean the finite PER burst is still sending.

With the AMF default **37-byte payload**, the current BLE RX pass limit is **30.8% PER**.

---
---

## RAILTest usage and commands

For more information about the RAILTest usage, see:

- the public documentation [RAILtest User Guide](https://docs.silabs.com/rail/latest/railtest-users-guide/)
  
  - [RAILtest - Protocol-Specific Features - Bluetooth Low Energy](https://docs.silabs.com/rail/latest/railtest-users-guide/05-protocol-specific-features#bluetooth-low-energy )

- Bluetooth Classic BR/EDR commands are not yet public, hence documented below

### Useful RAILtest commands

- `help`                See the full command list

- `reset`               Perform a reboot of the chip.

- `getAppMode`          Get the current sl_rail_test AppMode_t mode.

- `getRadioState`       Get the current sl_rail_radio_state_t radio state.

- `getChannel`          Get the current radio channel.

- `setChannel <channel>` Set the radio channel. [uint16] channel

- `printDataRates`      To show the PHY data rate

- `printChipFeatures`   Display RAIL features supported at compile and runtime.

- CTUNE related

  - `setCtune <ctune>`    Set the value of HFXO CTUNE. The radio must be IDLE. [uint16] ctune
  - `getCtune`            Get the value of HFXO CTUNE
  - `setCtuneDelta <delta>` Set the value of HFXO CTUNE delta  [uint16] delta
  - `getCtuneDelta`       Get the value of HFXO CTUNE delta

### Most common BLE RAILTest commands

- `setBle1Mbps [optional enctype]`         Switch to the 1Mbps BLE PHY. [uint8opt] 0=Legacy 1=Viterbi [chip default]

- `setBle2Mbps [optional enctype]`         Switch to the 2Mbps BLE PHY. [uint8opt] 0=Legacy 1=Viterbi [chip default]

- `setBleCoding [optional enctype]`        Switch to a BLE coded PHY. [uint8] sl_rail_ble_coding_t value

- `setBleSimulscan`                        Switch to BLE Simulscan mode, which receives 1M and Coded PHY packets simultaneously.

### Most common BT RAILTest commands

- `BTC_Prepare <txOrRx> <pktType> <payloadLen> encryption>` Prepare the next TX or RX operation (sl_rail_btc_prepare_tx_rx())

- `txOrRx`: 0 = TX, 1 = RX.

- `pktType`: BTC packet type, a value of the sl_rail_btc_packet_type_t enum:
0 = ID, 1 = NULL, 2 = POLL, 3 = FHS
4 = DH1, 5 = DH3, 6 = DH5
7 = DM1, 8 = DM3, 9 = DM5
10 = 2-DH1, 11 = 2-DH3, 12 = 2-DH5, 13 = 3-DH1, 14 = 3-DH3, 15 = 3-DH5
16 = HV1, 17 = HV2, 18 = HV3, 19 = DV
20 = EV3, 21 = EV4, 22 = EV5, 23 = 2-EV3, 24 = 2-EV5, 25 = 3-EV3, 26 = 3-EV5
27 = AUX1

- `payloadLen`: user payload length in bytes. It excludes the packet header, the payload header and the CRC.

- `encryption`: 0 = none, 1 = E0, 2 = AES.

---
---

## RAILTest BT Classic BR/EDR Commands manual

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
  - 4 = DH1, 5 = DH3, 6 = DH5
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

### Prerequisites

- Both boards (TX and RX) run a RAILTest application built with
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

### Transmitting an AES-Encrypted DH5 Packet

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

### Receiving an AES-Encrypted DH5 Packet

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

### Running a TX-to-RX Test

1. Run the RX sequence on the receiving board.
2. Run the TX sequence on the transmitting board right after.
3. On the RX side, check the receive event: packet received, CRC OK, payload
   `00 01 02 ... 13`.

### Variants

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
