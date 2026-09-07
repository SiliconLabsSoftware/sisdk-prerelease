# EFR32 Sleepy Demo

The EFR32 Sleepy applications demonstrate Sleepy End Device behavior using the EFR32's low power EM2 and EM4 modes.

Note that a Sleepy End Device can be demonstrated in three ways.
**_sleepy-demo-mtd_**: Demonstrates Sleepy End Device (SED) behaviour with polling.
**_sleepy-demo-mtd-em4_**: Demonstrates SED behaviour with EM4 deep sleep and BURTC wake.
**_sleepy-demo-ssed_**: Demonstrates Synchronous Sleepy End Device (SSED) behaviour with CSL.

The following are the steps to run the demo.

## 1. Starting Nodes

For demonstration purposes the network settings are hardcoded within the source files. The devices start Thread and form a network within a few seconds of powering on. In a real-life application the devices should implement and go through a commissioning process to create a network and add devices.

When the **_sleepy-demo-ftd_** device is started, the CLI should show:

```
sleepy-demo-ftd started
sleepy-demo-ftd changed to leader
```

When the **_sleepy-demo-mtd_** device is started, the CLI should show:

```
sleepy-demo-mtd started
[poll period: 2000 ms.]
```

The application is configured to join the pre-configured Thread network, disabling Rx-on-when-idle mode to become a Sleepy End Device. The default poll period is set in `sleepy-mtd.c`.

Issue the command `child table` in the FTD console and observe that the R (Rx-on-when-idle) flag of the child is 0.

```
> child table
| ID  | RLOC16 | Timeout    | Age        | LQ In | C_VN |R|D|N|Ver|CSL|QMsgCnt|Suprvsn| Extended MAC     |
+-----+--------+------------+------------+-------+------+-+-+-+---+---+-------+-------+------------------+
|   1 | 0x8401 |        240 |          3 |     3 |    3 |0|0|0|  4| 0 |     0 |   129 | 667bf54fcc2aed8a |

Done
```

When the **_sleepy-demo-ssed_** device is started, the CLI should show:

```
sleepy-demo-ssed started
[csl period: 500000 us.] [csl timeout: 30 sec.]
```

The application is configured to join the pre-configured Thread network, disabling Rx-on-when-idle mode to become a Synchronous Sleepy End Device. The default CSL parameters are set in `sleepy-ssed.c`

Issue the command `child table` in the FTD console and observe that the R (Rx-on-when-idle) flag of the child is 0, and the CSL flag is 1.

```
> child table
| ID  | RLOC16 | Timeout    | Age        | LQ In | C_VN |R|D|N|Ver|CSL|QMsgCnt|Suprvsn| Extended MAC     |
+-----+--------+------------+------------+-------+------+-+-+-+---+---+-------+-------+------------------+
|   1 | 0x8402 |        240 |          3 |     3 |    3 |0|0|0|  4| 1 |     0 |   129 | 8e8582dbd78c243c |

Done
```

When the **_sleepy-demo-mtd-em4_** device is started, the CLI should show:

```
starting (child_timeout=28800 s, wake_ms=27187000)
```

After the device attaches as a child it sends a multicast UDP sample, saves network state, and enters EM4. Example CLI output:

```
sent sample: em4 join sample
network store ok
store done; enter EM4 when stack idle
entering EM4
```

On the FTD CLI you should see:

```
Message Received: em4 join sample
```

Use **_sleepy-demo-ftd_** as the parent so the sample string is displayed. After BURTC wake and reattach, the MTD sends `em4 wake sample` and then returns to EM4. The default poll period and BURTC wake interval are set in `sleepy-mtd-em4.c`. Child timeout is set with `OPENTHREAD_CONFIG_MLE_CHILD_TIMEOUT_DEFAULT` in `sleepy-demo-mtd-em4.slcp`.

Issue the command `child table` in the FTD console and observe that the R (Rx-on-when-idle) flag of the child is 0, as with the classic MTD.

## 2. Buttons on the MTD/SSED

Pressing button 0 on the MTD/SSED toggles between EM2 (sleep) and EM1 (idle) modes.

Pressing button 1 on the MTD/SSED sends a multicast UDP message containing a pre-defined string. The FTD listens on the multicast address and displays `Message Received: <string>` in the CLI.

**_sleepy-demo-mtd-em4_** does not use buttons. It sends its UDP sample automatically when it becomes a child (after join or after EM4 wake), then enters EM4. Use **_sleepy-demo-mtd_** if you need button-driven sleep toggle and interactive UDP.

## 3. Buttons on the FTD

Pressing either button 0 or 1 on the FTD sends a UDP message to the FTD containing the string "ftd button". First, press the MTD's/SSED's button 1 to send a multicast message so that the FTD knows the address of the sleepy device to send messages to.

## 4. Monitoring power consumption of the MTD/SSED

Open the Energy Profiler in Simplicity Studio 5 (SSv5). In the Quick Access menu select **Start Energy Capture...** and select the MTD/SSED device. When operating during EM2 (sleep) mode, the current should be under 20 microamps with occasional spikes during waking, polling the parent or turning its receiver on during a CSL window.

When the device goes back to EM1 (idle) mode, observe that the current is in the order of 10 mA.

With further configuration of GPIOs and peripherals it is possible to reduce the sleepy current consumption further.

For **_sleepy-demo-mtd-em4_**, use Energy Profiler to observe EM4 current after `entering EM4`. BURTC wake is computed at runtime in `sleepy-mtd-em4.c` as `otThreadGetChildTimeout() * SLEEPY_EM4_ULFRCO_FMIN_HZ / SLEEPY_EM4_ULFRCO_FTYP_HZ` (EFR32xG24 datasheet Table 4.30 ULFRCO: 0.944 / 1.0 kHz). Child timeout comes from project configuration (for example `OPENTHREAD_CONFIG_MLE_CHILD_TIMEOUT_DEFAULT` in `sleepy-demo-mtd-em4.slcp`, CMSIS, or `otThreadSetChildTimeout()`).

## 5. Notes on sleeping, sleepy callback and interrupts

To allow the EFR32 to enter sleepy mode, the application must register a callback with `efr32SetSleepCallback`. The return value of the callback is used to indicate that the application has no further work to do and that it is safe to go into a low power mode. The callback is called with interrupts disabled so should do the minimum required to check if it can sleep.

**_sleepy-demo-mtd-em4_** enters EM4 from the power manager EM2 transition path once the network state has been saved and the stack is idle. After EM4 wake, network configuration is restored from NVM, so `setNetworkConfiguration()` is skipped on that boot.
