# Wi-SUN - NCP

The Wi-SUN NCP application is a network coprocessor implementation that allows usage of Wi-SUN Stack API from an external host.

## Direct Connect

Direct Connect requires a PMK imported into PSA before authentication. The NCP does not provision a PMK by default.

Add the **wisun_direct_connect_pmk** component to import the configured PMK at boot. Configure the settings in **sl_wisun_direct_connect_pmk_config.h** or via Simplicity Studio:

| Setting | Description |
|---------|-------------|
| `SL_WISUN_DIRECT_CONNECT_PMK` | PMK as colon-separated hex bytes (32 bytes). Leave empty to skip boot-time import. |
| `SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID` | PSA key slot for the imported PMK (default `0x00080001`). |

The PMK must match on both the Direct Connect server and client.

### Using Direct Connect

- **DC client**: pass `SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID` to `sl_wisun_connect_to_direct_connect_server()`.
- **DC server**: call `sl_wisun_set_direct_connect_pmk(SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID)` before enabling Direct Connect.

## Getting Started

To get started with Wi-SUN and Simplicity Studio, see [Developing with Wi-SUN](https://docs.silabs.com/wisun/latest/wisun-start/).

## Troubleshooting

Before programming the radio board mounted on the WSTK, ensure the power supply switch is in the AEM position (right side), as shown.

![Radio Board Power Supply Switch](doc/readme_img0.png)

## Report Bugs & Get Support

You are always encouraged and welcome to ask any questions or report any issues you found to us via [Silicon Labs Community](https://community.silabs.com/s/topic/0TO1M000000qHc6WAE/wisun).
