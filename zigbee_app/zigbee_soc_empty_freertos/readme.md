# Zigbee - SoC Empty FreeRTOS

This is a board-independent Zigbee SoC empty template that runs on **FreeRTOS**. It includes only the essential Zigbee stack, application-framework, IPC, and FreeRTOS components needed to join a Zigbee 3.0 network. The project has no buttons, LEDs, display, CLI, or other WSTK-specific peripherals, so it can be created from a device OPN (not only from a development board).

**Default role: Zigbee Router** (`zigbee_pro_stack` + `zigbee_source_route`). On first boot (or after mass erase), the app starts **network steering** so the device can join an open network without user input. After a successful join, the network is stored in NVM and restored across reset.

**`SL_ZIGBEE_EMPTY_JOIN_ON_BOOT`** in `config/zigbee_soc_empty_freertos_config.h` (default **FALSE**) controls testing behavior: when enabled, the app leaves any restored NVM network on boot and steers again. To move a device to a different network, mass erase and reflash (or enable this option for lab use).

This application does **not** include a CLI. To verify join, use a second device with CLI (for example **Zigbee - SoC Z3 Light**). As a **router**, check Light **neighbor-table**; as an **end device**, check **child-table**.

As configured, only network-layer functionality is provided; no Zigbee Cluster Library (ZCL) application-layer clusters are enabled. The application's ZCL ProfileId and DeviceId are set to undefined value 0xFFFF.

The application maintains persistent storage using NVM3.

## Generate: Router (default)

```bash
slc generate \
  -p zigbee_app/zigbee_soc_empty_freertos/zigbee_soc_empty_freertos.slcp \
  --with SIMG301M104LIL,toolchain_gcc \
  -d /tmp/zigbee_soc_empty_freertos_router

cd /tmp/zigbee_soc_empty_freertos_router && make -j -f zigbee_soc_empty_freertos.Makefile
```

Use any Zigbee-capable device OPN in place of `SIMG301M104LIL`. No board is required because this template has no UART/VCOM dependency.

## Generate: End Device (override)

Changing only the device-type config is not enough. Also swap to the leaf stack and end-device support:

```bash
slc generate \
  -p zigbee_app/zigbee_soc_empty_freertos/zigbee_soc_empty_freertos.slcp \
  --with SIMG301M104LIL,toolchain_gcc,zigbee_pro_leaf_stack,zigbee_end_device_support \
  --without zigbee_pro_stack,zigbee_source_route \
  --configuration SLI_ZIGBEE_PRIMARY_NETWORK_DEVICE_TYPE:SLI_ZIGBEE_NETWORK_DEVICE_TYPE_END_DEVICE \
  -d /tmp/zigbee_soc_empty_freertos_end_device

cd /tmp/zigbee_soc_empty_freertos_end_device && make -j -f zigbee_soc_empty_freertos.Makefile
```

## Verify join (Z3 Light CLI)

On a second device with CLI (for example **Zigbee - SoC Z3 Light**):

```text
network leave
plugin network-creator start 0
plugin network-creator-security open-network
network pjoin 180
```

Then **flash or mass erase Empty** (first join) or enable **`SL_ZIGBEE_EMPTY_JOIN_ON_BOOT`** in `config/zigbee_soc_empty_freertos_config.h` and reset Empty to rejoin after a previous join.

Check the matching table on Light:

```text
plugin stack-diagnostics neighbor-table   # Empty as router
plugin stack-diagnostics child-table      # Empty as end device
```

If Empty does not appear, run `network pjoin 180` again and reset Empty.

## Bringing back CLI, debug printing, and console I/O

This empty template omits CLI and debug printing so it has no UART/VCOM board dependency. To restore them in Simplicity Studio (or SLC):

1. Add **IO Stream: Recommended Console** (`iostream_recommended_console`) so logs and CLI have a serial backend.
2. Add a console implementation for the part (for example `iostream_eusart` or `iostream_usart`).
3. Add **CLI** (`cli`, instance `example`).
4. Add **Zigbee Core CLI** (`zigbee_core_cli`) and **Zigbee ZCL CLI** (`zigbee_zcl_cli`) if you want stack/ZCL commands.
5. Add **Zigbee Debug Print** (`zigbee_debug_print`) and **Zigbee Debug Basic** (`zigbee_debug_basic`).
6. Enable VCOM on a board if required (`SL_BOARD_ENABLE_VCOM=1`) and configure UART flow control for the selected serial instance.

After that, you can join from the console with:

```text
plugin network-steering start 0
```

To print status from application code, call `sl_zigbee_app_debug_println(...)` once debug print is present.

## Adding LEDs, buttons, or other board features

Install the matching driver components (for example **Simple LED** / **Simple Button**) and assign instances in the Pin Tool. Those components are board-specific and are intentionally not part of this empty template.
