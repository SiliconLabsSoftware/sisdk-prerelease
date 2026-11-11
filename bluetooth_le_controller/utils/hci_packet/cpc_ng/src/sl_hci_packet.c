/***************************************************************************//**
 * @file
 * @brief LE hci_common_transport over CPC-NG (HCI thread RX/TX glue).
 ******************************************************************************/
#include <stdint.h>

#include "sl_hci_common_transport.h"
#include "sli_hci_cpc.h"
#include "sl_status.h"

static bool initialized;

void sl_btctrl_hci_packet_step(void)
{
  const uint8_t *payload;
  sl_cpc_buf_t *rx_buf;
  int bytes_read;

  if (!sli_hci_cpc_is_connected()) {
    sli_hci_cpc_reconnect();
    if (!sli_hci_cpc_is_connected()) {
      sli_hci_cpc_on_link_down();
    }
    return;
  }

  if (sli_hci_cpc_new_data() <= 0) {
    return;
  }

  bytes_read = sli_hci_cpc_read(&payload, &rx_buf);
  if (bytes_read <= 0 || payload == NULL || rx_buf == NULL) {
    return;
  }

  if ((uint16_t)bytes_read > SLI_HCI_CPC_NG_MAX_H4_PACKET_SIZE) {
    sli_hci_cpc_free_rx(rx_buf);
    return;
  }

  {
    uint16_t expected = sli_hci_cpc_h4_packet_size(payload, (uint16_t)bytes_read);

    if (expected == 0U || expected != (uint16_t)bytes_read) {
      sli_hci_cpc_free_rx(rx_buf);
      return;
    }
  }

  (void)hci_common_transport_receive((uint8_t *)payload, (int16_t)bytes_read, true);
  sli_hci_cpc_free_rx(rx_buf);
}

uint32_t hci_common_transport_transmit(uint8_t *data, int16_t len)
{
  return (uint32_t)sli_hci_cpc_write(data, (uint16_t)len);
}

void hci_common_transport_init(void)
{
  sl_status_t status;

  if (initialized) {
    return;
  }

  status = sli_hci_cpc_init();
  if (status != SL_STATUS_OK) {
    return;
  }

  initialized = true;
}
