/***************************************************************************//**
 * @file
 * @brief LE hci_common_transport over Greybus CPC (HCI thread RX/TX glue).
 ******************************************************************************/
#include <stddef.h>
#include <stdint.h>

#include "sl_hci_common_transport.h"
#include "sli_hci_cpc.h"
#include "sl_btctrl_hci_packet.h"
#include "sl_status.h"

static bool initialized;

void sl_btctrl_hci_packet_step(void)
{
  uint8_t *read_buf = NULL;
  int bytes_read;

  if (!sli_hci_cpc_is_connected()) {
    sli_hci_cpc_reconnect();
    if (!sli_hci_cpc_is_connected()) {
      sli_hci_cpc_on_link_down();
      return;
    }
  }

  if (sli_hci_cpc_new_data() <= 0) {
    return;
  }

  bytes_read = sli_hci_cpc_read(&read_buf);
  if (bytes_read <= 0 || read_buf == NULL) {
    return;
  }
  hci_common_transport_receive(read_buf, (int16_t)bytes_read, true);

  sli_hci_cpc_free(read_buf);
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
