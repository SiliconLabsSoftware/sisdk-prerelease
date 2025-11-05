# Bluetooth Mesh Host Decoder

This application provides a reference implementation for decoding Bluetooth Mesh messages
on a host system (Linux/Windows/Mac). It enables the logging of recived messages after decryption and decoding, allowing users to inspect actual mesh traffic within the selected Bluetooth Mesh network.

The decoder application can decrypt and decode both segmented and unsegmented messages. All Bluetooth Mesh protocol layers are available for logging, with Sensor model messages being the primary supported model type.

## Getting started
### Requirements
* One mainboard with a supported board installed for the NCP target application.
* Two additional Silicon Labs EFR devices with capability for mesh network participation
* [Simplicity Studio 5](https://www.silabs.com/software-and-tools/simplicity-studio)
* Simplicity SDK 2025.12.0 or later, distributed through Simplicity Studio 5. The prebuilt demos and examples are included in the SDK.
* A host system with MbedCrypto package
* Make tool

### Bluetooth Mesh - NCP Empty Application
The **Bluetooth Mesh – NCP Empty** example application is the target application running on a Silicon Labs device. The application is provided as a prebuilt demo binary image, ready to download and use, and a corresponding example project that you can modify and then build for the target part. If you want to build your own projects based on the example project, see the [Getting Started Guide](https://docs.silabs.com/btmesh/latest/btmesh-getting-started-overview/). This section describes how to install the prebuilt demo binary to the device.

The precompiled demos are only available for a limited set of parts, the examples can be built for any part supported by the Bluetooth Mesh SDK.

1. Open Simplicity Studio 5 with a compatible SoC wireless kit connected to the computer.
2. Select the part in **Debug Adapters** view to open the **Launcher** perspective.
3. Click the **Example Projects & Demos** tab.
4. To see only the demos, turn off the **Example Projects**.
5. Under **Technology Type**, filter on **Bluetooth Mesh**. Next to **Bluetooth Mesh – NCP Empty**, click **RUN**.
![Flash precompiled demo](images/readme_img0.png)

> **Note**: EFR32xG22 parts can run the Bluetooth Mesh – NCP Empty example but do not support provisioner functionality.

### Create a Bluetooth Mesh Network
The decoder can decode the complete set of sensor messages, making a Sensor Server - Sensor Client setup the ideal choice for testing this feature set. Use the same method as described in the [NCP application](#bluetooth-mesh---ncp-empty-application)  section. For all required examples, demos are available.

**Minimum Network Requirements:**
* **1 Sensor Client device** - Flash with the **Bluetooth Mesh - SoC Sensor Client** example
* **1 Sensor Server device** - Choose from the following options:
  * **Bluetooth Mesh - SoC Sensor Server**
  * **Bluetooth Mesh - NLC Occupancy Sensor**
  * **Bluetooth Mesh - NLC Ambient Light Sensor** (mock version available)
  * **Bluetooth Mesh - SoC Sensor Thermometer**

**Recommended Network Setup:**
For comprehensive testing, consider using multiple sensor types to generate diverse message traffic that showcases the decoder's full capabilities.

**Setting up the test network**

Flash the devices with their respective applications using Simplicity Studio 5.
Make sure that all devices used (except NCP Empty) have the status of unprovisioned on the device LCD screen before starting with the application.
Follow the procedure below to set up the network and save the desired keys with **Bluetooth NCP Commander**.

1. To open the  **Bluetooth NCP Commander**, select the target board in the **Debug Adapters view**. Select the **Compatible Tools** tab, and click **Launch** next to **Bluetooth NCP Commander**.
![Launch Bluetooth NCP Commander](images/readme_img1.png)
2. Select the NCP target device and click **Connect**.
![Connect to NCP target](images/readme_img2.png)
3. Select **Provision** on the left menu and click **Initialize as Provisioner**
![Initialize as Provisioner](images/readme_img3.png)
4. Create a new network with **Create New Network**. Enter the name of the new network and click **Confirm**. Copy the **Network Key** and save it to use it later for the decoder app.
![Create a network](images/readme_img4.png)
5. To provision devices, click **Start Scan** in the right panel. The devices that are transmitting unprovisioned beacons are shown in the **Discovered Devices** section. Click **Provision** next to the device you want to provision.
![Scanning and provisioning](images/readme_img5.png)
6. Create application key and group, select **Networks & Nodes** on the left menu. To create an application key, click **Create App Key**, name the key, and click **Confirm**. Copy the **Application Key** and save it to use it later for the decoder app. To create a group, click **Add Group**, name the group, and click **Confirm**. Copy the **App Key** and save it to use it later for the decoder app.
![Create a keys](images/readme_img6.png)
7. The devices that provisioned are shown in the **Nodes (Provisioned Devices)** section of the **Settings** tab. Click **Configure** and a **Mesh Node** tab opens in which you can configure the device.
![Device configuration](images/readme_img7.png)
8. In the **Application Keys** section of the **Mesh Node** tab, select an application key from the drop-down list and then click **Add**. Do this repetitevly for all the provisioned devices. Copy the **Address** and **Device Key** and save it to use it later for the decoder app
![Add appkey](images/readme_img8.png)
9. Click **Get DCD** to configure **Sensor Server** and **Sensor Client** depending on the flashed device, bind to app key, set publishing or subscription to group.
![Get DCD](images/readme_img9.png)
10. Disconnect from Bluetooth NCP Commander by clicking on the COM port connection indicator in the bottom right corner and selecting **Disconnect**.
![Disconnect](images/readme_img10.png)

> **Note**: Test communication between client and server devices before running the decoder

### Decoder sample application
To build the project into an executable, a make-tool is needed. On Windows MinGW is recommended. Steps to compile the decoder sample application:
* Open a terminal and change the directory to the path `app/btmesh/example_host/btmesh_host_provisioner` of Simplicity SDK.
* Run `make` command
* The build output is created in a new folder

## Usage
### Command-Line Options
| Option | Parameter | Description |
|--------|-----------|-------------|
| `-t` | `<tcp_address>` | TCP/IP address of the dev board. |
| `-u` | `<serial_port>` | Serial port assigned to the dev board by the host system. (COM# on Windows, /dev/tty# on POSIX). |
| `-b` | `<baud_rate>` | Baud rate, default: 115200. |
| `-f` | - |  Disable flow control (RTS/CTS), default: enabled. |
| `-l` | `<level>` | Set logging level (0-4, default: 3). See [Log Levels](#log-levels) section below.|
| `-N` | `<net_key>` | One 128-bit network key defined as a hex string. Will be used to decrypt and authenticate network PDUs and Mesh beacons. The option can be given multiple times to define multiple keys. |
| `-A` | `<app_key>` | One 128-bit application key defined as a hex string. Will be used to decrypt and authenticate access messages. The option can be given multiple times to define multiple keys. |
| `-D` | `<dev_key_addr>` | One 128-bit device key, immediately followed by 16-bit device address, defined as a hex string. Will be used to decrypt and authenticate access messages. The option can be given multiple times to define multiple keys. |
| `-L` | `<label_uuid>` | One 128-bit virtual address label UUID. Will be used to decrypt and authenticate access messages when virtual addresses are used. The option can be given multiple times to define multiple labels. |
| `-I` | `<iv_index>` | The 32-bit IV index of the network as an integer. Default: 0. |
| `-h` | - | Display help message |

### Log Levels
| Level | Description | Output |
|-------|-------------|---------|
| `0` | Critical | Critical messages only |
| `1` | Error | Critical + Error messages |
| `2` | Warning | Critical + Error + Warning messages |
| `3` | Info | Critical + Error + Warning + Info messages |
| `4` | Debug | All messages including Debug output |

### Example
Use the same NCP target application for the decoder app which was used to create the network.

    ./exe/btmesh_host_decoder.exe -u COM7 -N 32CCBBB5DB021E8E3F4E174DAB9E43BC -A BBE7C546EF247E89B08B6B812A015D09 -D A500B594B36FED22C11E71FF3E5C3CA12005 -D 2ECAF06D28BD50243F388EC2DDAD7BCA2006

| Parameter | Value | Explanation |
|-----------|-------|-------------|
| `-u` | `COM7` | NCP target device serial port |
| `-N` | `32CCBBB5DB021E8E3F4E174DAB9E43BC` | Network key value |
| `-A` | `BBE7C546EF247E89B08B6B812A015D09` | Application key value |
| `-D` | `A500B594B36FED22C11E71FF3E5C3CA12005` | Device key + address combination for device:<br/>• Device key: `A500B594B36FED22C11E71FF3E5C3CA1`<br/>• Address: `0x2005`<br/> |
| `-D` | `2ECAF06D28BD50243F388EC2DDAD7BCA2006` | Device key + address combination for device:<br/>• Device key: `2ECAF06D28BD50243F388EC2DDAD7BCA`<br/>• Address: `0x2006`<br/> |

> All cryptographic keys and device addresses can be found in the respective Bluetooth NCP Commander screenshots shown in the network setup section above.

**Output**

An example log for a Sensor Status message.

    [NET] IVI: 0x00 NID: 0x30 CTL: 0x00 TTL: 0x0a SEQ: 0x00151f SRC: 0x2006 DST: 0xc000
    [NET] Transport PDU: 0x6d63d3fe98bbae367aca
    [NET] Net MIC: 0x79d29e6a                                                        
    ---------------------------
    [TRANS] SEG: 0x0, AKF: 0x01, AID: 0x2d
    ---------------------------
    [SENSOR] Opcode: 0x0052 (Sensor Status)
    [SENSOR] Marshalled Proprety ID_0:
    [SENSOR]  Format: 0x00
    [SENSOR]  Length: 0x01
    [SENSOR]  Property ID: 0x004c (People Count)
    [SENSOR]  Raw Value 0x0000
    ---------------------------

## Configuration options
### Logging
Logging behavior can be customized to suit for the desired requirements. The logging system is organized by Bluetooth Mesh protocol layers, allowing granular control over output verbosity.

**Default configurations:**
- Advertisement header data: disabled
- Bluetooth Mesh Layers: enabled

It can be configured in the `config/decoder_app_config.h` file.

> **Note**: The application must be rebuilt after changing any configuration options in `config/decoder_app_config.h`.

**Retargeted logging**
Logging output can be redirected to a selected file with the following commands. 
* To **overwrite** the file with new log data, use the `>` operator:
    ```bash
    <decoder_executable> [options] > log.txt
    ```
* To **append** new log data to the end of an existing file, use the `>>` operator:
    ```bash
    <decoder_executable> [options] >> log.txt
    ```

### Replay Protection
The replay protection mechanism prevents processing of duplicate or replayed messages by maintaining a list of recently seen sequence numbers. The relevant macro can be configured in the `config/decoder_app_config.h` file.

> **Important**: The replay protection list size should equal the maximum number of nodes in your Bluetooth Mesh network to ensure proper functionality.

> **Note**: The application must be rebuilt after changing any configuration options in `config/decoder_app_config.h`.

## Troubleshooting
Note that Software Example-based projects do not include a bootloader. However, they are configured to expect a bootloader to be present on the device. To install a bootloader, from the Launcher perspective's EXAMPLE PROJECTS & DEMOS tab either build and flash one of the bootloader examples or run one of the precompiled demos. Precompiled demos flash a bootloader as well as the application image.

- To flash an OTA DFU-capable bootloader to the device, flash the **Bluetooth Mesh - SoC Switch** demo.
- To flash a UART DFU-capable bootloader to the device, flash the **Bluetooth Mesh - NCP Empty** demo.
- For other bootloader types, create your own bootloader project and flash it to the device before flashing your application.
- When you flash your application image to the device, use the *.hex* or *.s37* output file. Flashing *.bin* files may overwrite (erase) the bootloader.
- For more information, see [UG103.6: Bootloader Fundamentals](https://www.silabs.com/documents/public/user-guides/ug103-06-fundamentals-bootloading.pdf) and [UG489: Silicon Labs Gecko Bootloader User's Guide for GSDK 4.0 and Higher](https://cn.silabs.com/documents/public/user-guides/ug489-gecko-bootloader-user-guide-gsdk-4.pdf).

Before programming the radio board mounted on the mainboard, make sure the power supply switch the AEM position (right side) as shown below.

![Radio board power supply switch](images/readme_img11.png)

## Documentation & Resources

[Bluetooth Documentation](https://docs.silabs.com/bluetooth/latest/)

[Bluetooth Mesh Network - An Introduction for Developers](https://www.bluetooth.com/wp-content/uploads/2019/03/Mesh-Technology-Overview.pdf)

[QSG176: Bluetooth Mesh SDK v2.x Quick Start Guide](https://www.silabs.com/documents/public/quick-start-guides/qsg176-bluetooth-mesh-sdk-v2x-quick-start-guide.pdf)

[AN1259: Using the v3.x Silicon Labs Bluetooth Stack in Network Co-Processor Mode](https://www.silabs.com/documents/public/application-notes/an1259-bt-ncp-mode-sdk-v3x.pdf)

[AN1300: Understanding the Silicon Labs Bluetooth Mesh SDK v2.x Sensor Model Demonstration](https://www.silabs.com/documents/public/application-notes/an1300-understanding-bluetooth-mesh-sensor-model-demo-sdk-2x.pdf)

[UG295: Silicon Labs Bluetooth Mesh C Application Developer's Guide for SDK v2.x](https://www.silabs.com/documents/public/user-guides/ug295-bluetooth-mesh-dev-guide.pdf)

[UG472: Silicon Labs Bluetooth ® C Application Developer's Guide for SDK v3.x](https://www.silabs.com/documents/public/user-guides/ug434-bluetooth-c-soc-dev-guide-sdk-v3x.pdf)

[Bluetooth Training](https://www.silabs.com/support/training/bluetooth)

## Report Bugs & Get Support
You are always encouraged and welcome to report any issues you found to us via [Silicon Labs Community](https://www.silabs.com/community).
