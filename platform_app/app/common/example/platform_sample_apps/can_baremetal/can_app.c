/***************************************************************************//**
 * @file can_app.c
 * @brief CAN baremetal examples functions
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

 #include <string.h>
#include <stdio.h>
 #include "sl_clock_manager.h"
 #include "sl_common.h"
 #include "sl_device_clock.h"
 #include "sl_device_gpio.h"
 #include "sl_hal_gpio.h"
 #include "sl_interrupt_manager.h"
 #include "sl_main_init.h"
 #include "sl_status.h"
 #include "sl_hal_can.h"
 #include "sl_sleeptimer.h"
 #include "sl_device_peripheral.h"
 #include "sl_code_classification.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

#define SAMPLE_TIMEOUT_MS         500

#define CAN_INSTANCE_PERIPHERAL_NO 0U

#if (CAN_INSTANCE_PERIPHERAL_NO == 0U)
#define CAN_INSTANCE              CAN0
#elif (CAN_INSTANCE_PERIPHERAL_NO == 1U)
#define CAN_INSTANCE              CAN1
#else
#error "Unsupported CAN instance number"
#endif

#define MESSAGE_BUFFER_SIZE       64
#define TIMEOUT_SPIN_LIMIT        500000U

 #define STD_TX_FIFO_ID            0x123
 #define EXT_TX_FIFO_ID            0x12345678
 #define STD_RX_BUFFER_ID          0x111
 #define EXT_RX_BUFFER_ID          0x11111111

#define STD_FILTER_ID_ONE          0x112
#define STD_FILTER_ID_TWO          0x7ff
#define EXT_FILTER_ID_ONE          0x11111112
#define EXT_FILTER_ID_TWO          0x1FFFFFFF

 #define TX_DEDICATED_BUFFER_IDX   0
 #define RX_DEDICATED_BUFFER_IDX   0

 #define STD_FILTER_IDX_FIFO0      0
 #define EXT_FILTER_IDX_FIFO1      0
 #define STD_FILTER_IDX_RXBUF      1

 #define CAN_TX_PORT               SL_GPIO_PORT_A
 #define CAN_TX_PIN                9
 #define CAN_RX_PORT               SL_GPIO_PORT_A
 #define CAN_RX_PIN                10

/* TSW and TOO are raised again on every timestamp wrap and timeout
   period. Leave them out of the IRQ mask so they do not interrupt. */
#define CAN_APP_INTERRUPT_FLAGS  (0x3FFFFFFFUL & ~(CAN_IE_TSWE | CAN_IE_TOOE))

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
static sl_sleeptimer_timer_handle_t sample_timer;
static volatile bool sample_due = false;

static volatile bool rx_fifo0_new_msg       = false;
static volatile bool rx_fifo0_watermark     = false;
static volatile bool rx_fifo0_full          = false;
static volatile bool rx_fifo0_msg_lost      = false;

static volatile bool rx_fifo1_new_msg       = false;
static volatile bool rx_fifo1_watermark     = false;
static volatile bool rx_fifo1_full          = false;
static volatile bool rx_fifo1_msg_lost      = false;

static volatile bool tx_complete            = false;
static volatile bool tx_cancellation_done   = false;
static volatile bool tx_fifo_empty          = false;

static volatile bool tx_event_new_entry     = false;
static volatile bool tx_event_watermark     = false;
static volatile bool tx_event_full          = false;
static volatile bool tx_event_lost          = false;

static volatile bool rx_buffer_new_msg      = false;
static volatile bool hp_message             = false;
static volatile bool timeout_occurred       = false;
static volatile bool timestamp_wraparound   = false;
static volatile bool message_ram_full       = false;

static volatile uint32_t transmitted_buffers = 0;
static volatile uint32_t canceled_buffers    = 0;

static const uint8_t payload_data[] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

/*******************************************************************************
 *                            BUFFERS
 ******************************************************************************/
static uint8_t tx_data[MESSAGE_BUFFER_SIZE];
static uint8_t rx_data[MESSAGE_BUFFER_SIZE];

static sl_hal_can_rx_frame_object_t  rx_frame;
static sl_hal_can_tx_event_object_t  tx_event;

/*******************************************************************************
 *                        FORWARD DECLARATIONS
 ******************************************************************************/
static void can_pin_configuration(void);
static void can_enable_nvic_interrupts(void);
static void can_disable_nvic_interrupts(void);
static void reset_isr_flags(void);
static void fill_tx_data(uint8_t length);
static void print_can_message(const char *direction,
                              uint32_t can_id,
                              uint8_t data_length,
                              const uint8_t *payload,
                              uint8_t payload_len);
static sl_status_t verify_rx_message(const char *frame_name,
                                     uint32_t can_id,
                                     uint8_t data_length,
                                     const uint8_t *payload,
                                     uint8_t payload_len);
static sl_status_t wait_for_isr_flag(volatile bool *flag, const char *flag_name);
static sl_status_t can_calculate_bit_timing(sl_hal_can_bit_timing_t *bt,
                                            uint32_t bitrate,
                                            uint16_t prescaler);

/*******************************************************************************
 *                      ISR HANDLER (shared for INT0/INT1)
 ******************************************************************************/
static void can_irq_handler(CAN_TypeDef *can)
{
  uint32_t flags = 0U;

  // Get pending interrupts.
  flags = sl_hal_can_get_pending_interrupts(can);

  // Clear exactly the flags handled below, so a flag raised meanwhile is kept.
  sl_hal_can_clear_interrupts(can, flags);

  // High priority message.
  if ((flags & CAN_IR_HPM) != 0U) {
    hp_message = true;
  }

  // Transmission cancellation finished.
  if ((flags & CAN_IR_TCF) != 0U) {
    canceled_buffers = can->TXBCF & can->TXBCIE;
    tx_cancellation_done = true;
  }

  // Tx event FIFO.
  if ((flags & CAN_IR_TEFN) != 0U) {
    tx_event_new_entry = true;
  }
  if ((flags & CAN_IR_TEFW) != 0U) {
    tx_event_watermark = true;
  }
  if ((flags & CAN_IR_TEFF) != 0U) {
    tx_event_full = true;
  }
  if ((flags & CAN_IR_TEFL) != 0U) {
    tx_event_lost = true;
  }

  // Tx FIFO empty.
  if ((flags & CAN_IR_TFE) != 0U) {
    tx_fifo_empty = true;
  }

  // Rx FIFO 0.
  if ((flags & CAN_IR_RF0N) != 0U) {
    rx_fifo0_new_msg = true;
  }
  if ((flags & CAN_IR_RF0W) != 0U) {
    rx_fifo0_watermark = true;
  }
  if ((flags & CAN_IR_RF0F) != 0U) {
    rx_fifo0_full = true;
  }
  if ((flags & CAN_IR_RF0L) != 0U) {
    rx_fifo0_msg_lost = true;
  }

  // Rx FIFO 1.
  if ((flags & CAN_IR_RF1N) != 0U) {
    rx_fifo1_new_msg = true;
  }
  if ((flags & CAN_IR_RF1W) != 0U) {
    rx_fifo1_watermark = true;
  }
  if ((flags & CAN_IR_RF1F) != 0U) {
    rx_fifo1_full = true;
  }
  if ((flags & CAN_IR_RF1L) != 0U) {
    rx_fifo1_msg_lost = true;
  }

  // Transmission complete.
  if ((flags & CAN_IR_TC) != 0U) {
    transmitted_buffers = can->TXBTO & can->TXBTIE;
    tx_complete = true;
  }

  // Dedicated Rx buffer new message.
  if ((flags & CAN_IR_DRX) != 0U) {
    rx_buffer_new_msg = true;
  }

  // Counters.
  if ((flags & CAN_IR_TSW) != 0U) {
    timestamp_wraparound = true;
  }
  if ((flags & CAN_IR_TOO) != 0U) {
    timeout_occurred = true;
  }

  // Message RAM access failure.
  if ((flags & CAN_IR_MRAF) != 0U) {
    message_ram_full = true;
  }

}

#if defined(CAN0)
void CAN0_INT0_IRQHandler(void)
{
  can_irq_handler(CAN_INSTANCE);
}

void CAN0_INT1_IRQHandler(void)
{
  can_irq_handler(CAN_INSTANCE);
}
#endif

#if defined(CAN1)
void CAN1_INT0_IRQHandler(void)
{
  can_irq_handler(CAN_INSTANCE);
}

void CAN1_INT1_IRQHandler(void)
{
  can_irq_handler(CAN_INSTANCE);
}
#endif

/*******************************************************************************
 *                        UTILITY HELPERS
 ******************************************************************************/
static void reset_isr_flags(void)
{
  rx_fifo0_new_msg     = false;
  rx_fifo0_watermark   = false;
  rx_fifo0_full        = false;
  rx_fifo0_msg_lost    = false;
  rx_fifo1_new_msg     = false;
  rx_fifo1_watermark   = false;
  rx_fifo1_full        = false;
  rx_fifo1_msg_lost    = false;
  tx_complete          = false;
  tx_cancellation_done = false;
  tx_fifo_empty        = false;
  tx_event_new_entry   = false;
  tx_event_watermark   = false;
  tx_event_full        = false;
  tx_event_lost        = false;
  rx_buffer_new_msg    = false;
  hp_message           = false;
  timeout_occurred     = false;
  timestamp_wraparound = false;
  message_ram_full     = false;
  transmitted_buffers  = 0;
  canceled_buffers     = 0;
}

static void fill_tx_data(uint8_t length)
{
  for (uint8_t i = 0; i < length; i++) {
    tx_data[i] = i + 1;
  }
}

static void print_can_message(const char *direction,
                              uint32_t can_id,
                              uint8_t data_length,
                              const uint8_t *payload,
                              uint8_t payload_len)
{
  printf("  %s can_id      : 0x%08lx \r\n", direction, (unsigned long)can_id);
  printf("  %s data_length : %u \r\n", direction, (unsigned int)payload_data[data_length]);
  printf("  %s payload     : ", direction);
  for (uint8_t i = 0; i < payload_len; i++) {
    printf(" %02x", (unsigned int)payload[i]);
  }
  printf("\r\n");
}

static sl_status_t verify_rx_message(const char *frame_name,
                                     uint32_t can_id,
                                     uint8_t data_length,
                                     const uint8_t *payload,
                                     uint8_t payload_len)
{

  for (uint8_t i = 0; i < payload_len; i++) {
    if (payload[i] != tx_data[i]) {
      printf("CAN payload mismatch (%s): index %u expected 0x%02x got 0x%02x\r\n",
             frame_name,
             (unsigned int)i,
             (unsigned int)tx_data[i],
             (unsigned int)payload[i]);
      return SL_STATUS_FAIL;
    }
  }
  print_can_message("RX", can_id, data_length, payload, payload_len);
  return SL_STATUS_OK;
}

static sl_status_t wait_for_isr_flag(volatile bool *flag, const char *flag_name)
{
  uint32_t spin_count = TIMEOUT_SPIN_LIMIT;

  while ((*flag == false) && (spin_count > 0U)) {
    spin_count--;
  }

  if (*flag == true) {
    return SL_STATUS_OK;
  }

  printf("CAN timeout waiting for %s\r\n", flag_name);
  return SL_STATUS_FAIL;
}

static sl_status_t can_calculate_bit_timing(sl_hal_can_bit_timing_t *bt,
                                            uint32_t bitrate,
                                            uint16_t prescaler)
{
  uint32_t ntq, sample_point;
  uint16_t tseg1;
  uint32_t can_freq;
  sl_clock_branch_t clock_branch;
  sl_status_t status;

#if (CAN_INSTANCE_PERIPHERAL_NO == 0U)
  clock_branch = sl_device_peripheral_get_clock_branch(SL_PERIPHERAL_CAN0);
#elif (CAN_INSTANCE_PERIPHERAL_NO == 1U)
  clock_branch = sl_device_peripheral_get_clock_branch(SL_PERIPHERAL_CAN1);
#endif
  status = sl_clock_manager_get_clock_branch_frequency(clock_branch, &can_freq);
  if (status != SL_STATUS_OK) {
    memset(bt, 0, sizeof(*bt));
    printf("CAN clock frequency read failed: status %lu\r\n",
           (unsigned long)status);
    return status;
  }

  ntq = can_freq / (bitrate * ((uint32_t)prescaler + 1U));

  if ((ntq < 4U) || (ntq > 385U)) {
    memset(bt, 0, sizeof(*bt));
    printf("Invalid CAN bit timing: bitrate=%lu prescaler=%u ntq=%lu\r\n",
           (unsigned long)bitrate,
           (unsigned int)prescaler,
           (unsigned long)ntq);
    return SL_STATUS_FAIL;
  }

  if (bitrate <= 2000000U) {
    sample_point = 80;
  } else if (bitrate <= 4000000U) {
    sample_point = 70;
  } else {
    sample_point = 60;
  }
  tseg1 = (uint16_t)(((ntq - 3U) * sample_point) / 100U);
  bt->phase_seg2      = (uint16_t)(ntq - tseg1 - 3U);
  bt->phase_seg1      = tseg1;
  bt->sync_jump_width = 3;
  bt->prescaler       = prescaler;

  return SL_STATUS_OK;
}

/*******************************************************************************
 *   can_sample_app: Comprehensive CAN HAL sample application.
 *
 *   The sample runs entirely in internal loopback mode and executes three
 *   protocol variants back-to-back so the same test flow can validate classic
 *   CAN and CAN-FD operation without any external transceiver or bus traffic.
 *   If you want to communicate with an external CAN device, update the CAN
 *   operating mode and board-level TX/RX connection so frames are transmitted
 *   onto the physical bus instead of being looped back internally.
 *   Example:
 *   configure for loopback mode : cfg.mode = SL_HAL_CAN_OPERATION_MODE_INTERNAL_LOOPBACK
 *   configure for external communication: cfg.mode = SL_HAL_CAN_OPERATION_MODE_NORMAL
 *
 *   Phase A : Classic CAN         (fd_enable=0, brs_enable=0)
 *   Phase B : CAN-FD without BRS  (fd_enable=1, brs_enable=0)
 *   Phase C : CAN-FD with BRS     (fd_enable=1, brs_enable=1)
 *                                 (BRS is supported only in CAN-FD mode max 4Mbps)
 *
 *   For each phase, the sequence is:
 *
 *     1. Init CAN in internal-loopback mode
 *     2. Configure filters:
 *          - Std ID range filter      → Rx FIFO 0
 *          - Ext ID range filter      → Rx FIFO 1
 *          - Std ID exact filter      → Rx Buffer (dedicated)
 *     3. Configure ancillary features:
 *          - Global filter / extended ID mask
 *          - Rx FIFO overwrite mode
 *          - FIFO watermarks
 *          - Timestamp counter
 *          - Timeout counter
 *     4. Start CAN
 *     5. Std-ID frame   → Tx FIFO   → verify Rx FIFO 0
 *     6. Ext-ID frame   → Tx FIFO   → verify Rx FIFO 1
 *     7. Specific-ID    → Tx Buffer → verify Rx Buffer
 *     8. Tx Event FIFO readback
 *     9. Tx abort test
 *    10. High-priority message status
 *    11. Protocol status
 *    12. Power-down / wake-up
 *    13. Interrupt enable / disable cycle
 *    14. Inline queries (Rx buffer check, FIFO fill level)
 *    15. De-init
 ******************************************************************************/
static void can_sample_app(void)
{
  sl_status_t status;
  sl_hal_can_init_params_t     cfg;
  sl_hal_can_filter_t          std_fifo0_filter;
  sl_hal_can_filter_t          ext_fifo1_filter;
  sl_hal_can_filter_t          std_rxbuf_filter;
  sl_hal_can_tx_frame_object_t tx_hdr;
  sl_hal_can_hpm_status_t      hpm_status;
  sl_hal_can_protocol_status_t protocol_status;
  uint8_t  dlc_classic;
  uint8_t  dlc_fd;
  uint8_t  payload_len;

  /* --- Three phases: Classic, FD no BRS, FD with BRS --- */
  struct {
    const char *label;
    uint8_t     fd_enable;
    uint8_t     brs_enable;
  } phases[] = {
    { "Classic CAN (no FD, no BRS)", SL_HAL_CAN_CLASSIC, SL_HAL_CAN_BRS_OFF },
    { "CAN-FD without BRS", SL_HAL_CAN_FD, SL_HAL_CAN_BRS_OFF },
    { "CAN-FD with BRS", SL_HAL_CAN_FD, SL_HAL_CAN_BRS_ON  },
  };
while (1) {
  for (uint8_t phase = 0; phase < 3; phase++) {
    /* Choose DLC per phase: 8 bytes for classic, 64 bytes for FD. */
    if (phases[phase].fd_enable == SL_HAL_CAN_FD) {
      dlc_fd      = 0xF;
      payload_len = 64;
    } else {
      dlc_classic = 0x8;
      payload_len = 8;
    }

    printf("\nStarting CAN communication with %s mode\r\n\n", phases[phase].label);

    /*--------------------------------------------------------------
    * 1. INIT
    *-------------------------------------------------------------*/
    cfg = (sl_hal_can_init_params_t)SL_HAL_CAN_INIT_DEFAULT;
    cfg.fd_enable    = phases[phase].fd_enable;
    cfg.brs_enable   = phases[phase].brs_enable;
    cfg.mode         = SL_HAL_CAN_OPERATION_MODE_INTERNAL_LOOPBACK;

    status = can_calculate_bit_timing(&cfg.nominal_bitrate,
                                      SL_HAL_CAN_BAUDRATE_500KBPS, 1);
    EFM_ASSERT(SL_STATUS_OK == status);
    if (phases[phase].brs_enable == SL_HAL_CAN_BRS_ON) {
      status = can_calculate_bit_timing(&cfg.data_bitrate,
                                        SL_HAL_CAN_BAUDRATE_4MBPS, 0);
      EFM_ASSERT(SL_STATUS_OK == status);
    }

    reset_isr_flags();
    status = sl_hal_can_init(CAN_INSTANCE, &cfg);
    EFM_ASSERT(SL_STATUS_OK == status);
    can_enable_nvic_interrupts();

    /*--------------------------------------------------------------
    * 2. INTERRUPTS
    *-------------------------------------------------------------*/
    sl_hal_can_enable_interrupts(CAN_INSTANCE, 0x3FFFFFFF, 0x3AAAAAAA);

    /*--------------------------------------------------------------
    * 3. FILTERS
    *     - Std IDs 0x001..0x7FF  → Rx FIFO 0  (range filter)
    *     - Ext IDs full range    → Rx FIFO 1  (range filter)
    *     - Std ID STD_RX_BUFFER_ID → Rx Buffer (exact match)
    *-------------------------------------------------------------*/
    std_fifo0_filter = (sl_hal_can_filter_t)SL_HAL_CAN_STD_ID_FILTER_CONFIG_DEFAULT;
    std_fifo0_filter.filter_type   = SL_HAL_CAN_FILTER_RANGE;
    std_fifo0_filter.filter_config = SL_HAL_CAN_FILTER_TO_RXFIFO0;
    std_fifo0_filter.filter_id_one = STD_FILTER_ID_ONE;
    std_fifo0_filter.filter_id_two = STD_FILTER_ID_TWO;
    std_fifo0_filter.filter_index  = STD_FILTER_IDX_FIFO0;
    status = sl_hal_can_configure_filter(CAN_INSTANCE, &std_fifo0_filter);
    EFM_ASSERT(SL_STATUS_OK == status);

    ext_fifo1_filter = (sl_hal_can_filter_t)SL_HAL_CAN_EXD_ID_FILTER_CONFIG_DEFAULT;
    ext_fifo1_filter.filter_type   = SL_HAL_CAN_FILTER_RANGE;
    ext_fifo1_filter.filter_config = SL_HAL_CAN_FILTER_TO_RXFIFO1;
    ext_fifo1_filter.filter_id_one = EXT_FILTER_ID_ONE;
    ext_fifo1_filter.filter_id_two = EXT_FILTER_ID_TWO;
    ext_fifo1_filter.filter_index  = EXT_FILTER_IDX_FIFO1;
    status = sl_hal_can_configure_filter(CAN_INSTANCE, &ext_fifo1_filter);
    EFM_ASSERT(SL_STATUS_OK == status);

    std_rxbuf_filter = (sl_hal_can_filter_t)SL_HAL_CAN_STD_ID_FILTER_CONFIG_DEFAULT;
    std_rxbuf_filter.filter_type    = SL_HAL_CAN_FILTER_DUAL;
    std_rxbuf_filter.filter_config  = SL_HAL_CAN_FILTER_TO_RXBUFFER;
    std_rxbuf_filter.filter_id_one  = STD_RX_BUFFER_ID;
    std_rxbuf_filter.filter_id_two  = 0;
    std_rxbuf_filter.filter_index   = STD_FILTER_IDX_RXBUF;
    std_rxbuf_filter.rx_buffer_index = RX_DEDICATED_BUFFER_IDX;
    status = sl_hal_can_configure_filter(CAN_INSTANCE, &std_rxbuf_filter);
    EFM_ASSERT(SL_STATUS_OK == status);

    /*--------------------------------------------------------------
    * 4. GLOBAL FILTER & EXTENDED ID MASK
    *-------------------------------------------------------------*/
    sl_hal_can_config_global_filter(CAN_INSTANCE,
                                    SL_HAL_CAN_NON_MATCHING_FRAMES_REJECT,
                                    SL_HAL_CAN_NON_MATCHING_FRAMES_REJECT,
                                    false, false);
    sl_hal_can_config_extended_id_mask(CAN_INSTANCE, 0x1FFFFFFF);

    /*--------------------------------------------------------------
    * 5. RX FIFO OVERWRITE MODE & WATERMARKS
    *-------------------------------------------------------------*/
    sl_hal_can_config_rx_fifo_over_write(CAN_INSTANCE, SL_HAL_CAN_RX_FIFO0,
                                         SL_HAL_CAN_RX_FIFO_BLOCKING);
    sl_hal_can_config_rx_fifo_over_write(CAN_INSTANCE, SL_HAL_CAN_RX_FIFO1,
                                         SL_HAL_CAN_RX_FIFO_BLOCKING);

    sl_hal_can_config_fifo_water_mark(CAN_INSTANCE, SL_HAL_CAN_CFG_RX_FIFO0, 4);
    sl_hal_can_config_fifo_water_mark(CAN_INSTANCE, SL_HAL_CAN_CFG_RX_FIFO1, 4);
    sl_hal_can_config_fifo_water_mark(CAN_INSTANCE, SL_HAL_CAN_CFG_TX_EVENT_FIFO, 8);

    /*--------------------------------------------------------------
    * 6. TIMESTAMP & TIMEOUT COUNTERS
    *-------------------------------------------------------------*/
    sl_hal_can_enable_timestamp_counter(CAN_INSTANCE,
                                        SL_HAL_CAN_TIMESTAMP_INTERNAL, 0);

    sl_hal_can_enable_timeout_counter(CAN_INSTANCE,
                                      SL_HAL_CAN_CONTINUOUS_OPERATION,
                                      0xFFFF, 0);

    /*--------------------------------------------------------------
    * 7. START CAN
    *-------------------------------------------------------------*/
    sl_hal_can_start(CAN_INSTANCE);

    /*==============================================================
    * STEP A : Std-ID frame  →  Tx FIFO  →  Rx FIFO 0
    *=============================================================*/
    reset_isr_flags();

    if (phases[phase].fd_enable == SL_HAL_CAN_FD) {
      tx_hdr = (sl_hal_can_tx_frame_object_t)SL_HAL_CANFD_MESSAGE_HEADER_DEFAULT;
      tx_hdr.data_length     = dlc_fd;
      tx_hdr.fd_format       = SL_HAL_CAN_FD;
      tx_hdr.bit_rate_switch = phases[phase].brs_enable;
    } else {
      tx_hdr = (sl_hal_can_tx_frame_object_t)SL_HAL_CAN_MESSAGE_HEADER_DEFAULT;
      tx_hdr.data_length = dlc_classic;
      tx_hdr.fd_format   = SL_HAL_CAN_CLASSIC;
    }
    tx_hdr.identifier            = STD_TX_FIFO_ID;
    tx_hdr.id_type               = SL_HAL_CAN_ID_STANDARD;
    tx_hdr.frame_type            = SL_HAL_CAN_FRAME_DATA;
    tx_hdr.tx_event_fifo_control = SL_HAL_CAN_TX_EVENTS_STORE;
    tx_hdr.message_marker        = 0xA1;

    fill_tx_data(payload_len);
    status = sl_hal_can_add_message_to_tx_fifo(CAN_INSTANCE, &tx_hdr, tx_data);
    EFM_ASSERT(SL_STATUS_OK == status);
    print_can_message("TX", tx_hdr.identifier, tx_hdr.data_length,
                      tx_data, payload_len);

    status = wait_for_isr_flag(&tx_complete, "Tx complete");
    EFM_ASSERT(SL_STATUS_OK == status);
    status = wait_for_isr_flag(&rx_fifo0_new_msg, "Rx FIFO0 new message");
    EFM_ASSERT(SL_STATUS_OK == status);

    memset(rx_data, 0, sizeof(rx_data));
    status = sl_hal_can_get_rx_message(CAN_INSTANCE, SL_HAL_CAN_RX_FIFO0,
                                       0, &rx_frame, rx_data);
    EFM_ASSERT(SL_STATUS_OK == status);
    EFM_ASSERT(STD_TX_FIFO_ID == rx_frame.identifier);
    EFM_ASSERT(SL_HAL_CAN_ID_STANDARD == rx_frame.id_type);
    EFM_ASSERT(tx_hdr.frame_type == rx_frame.frame_type);
    EFM_ASSERT(tx_hdr.data_length == rx_frame.data_length);
    status = verify_rx_message("Std-ID FIFO0",
                               rx_frame.identifier,
                               rx_frame.data_length,
                               rx_data,
                               payload_len);
    EFM_ASSERT(SL_STATUS_OK == status);
    if (phases[phase].fd_enable == SL_HAL_CAN_FD) {
      EFM_ASSERT(SL_HAL_CAN_FD == rx_frame.fd_format);
      EFM_ASSERT(phases[phase].brs_enable == rx_frame.bit_rate_switch);
    }

    /* Tx Event FIFO check for the std-ID message. */
    if (tx_event_new_entry) {
      status = sl_hal_can_get_tx_event(CAN_INSTANCE, &tx_event);
      EFM_ASSERT(SL_STATUS_OK == status);
      EFM_ASSERT(STD_TX_FIFO_ID == tx_event.identifier);
      EFM_ASSERT(0xA1 == tx_event.message_marker);
    }

    printf("\n  Standard-ID frame transfer and receive completed via Tx FIFO and Rx FIFO 0\r\n\n");

    /*==============================================================
    * STEP B : Ext-ID frame  →  Tx FIFO  →  Rx FIFO 1
    *=============================================================*/
    reset_isr_flags();

    if (phases[phase].fd_enable == SL_HAL_CAN_FD) {
      tx_hdr = (sl_hal_can_tx_frame_object_t)SL_HAL_CANFD_MESSAGE_HEADER_DEFAULT;
      tx_hdr.data_length     = dlc_fd;
      tx_hdr.fd_format       = SL_HAL_CAN_FD;
      tx_hdr.bit_rate_switch = phases[phase].brs_enable;
    } else {
      tx_hdr = (sl_hal_can_tx_frame_object_t)SL_HAL_CAN_MESSAGE_HEADER_DEFAULT;
      tx_hdr.data_length = dlc_classic;
      tx_hdr.fd_format   = SL_HAL_CAN_CLASSIC;
    }
    tx_hdr.identifier            = EXT_TX_FIFO_ID;
    tx_hdr.id_type               = SL_HAL_CAN_ID_EXTENDED;
    tx_hdr.frame_type            = SL_HAL_CAN_FRAME_DATA;
    tx_hdr.tx_event_fifo_control = SL_HAL_CAN_TX_EVENTS_STORE;
    tx_hdr.message_marker        = 0xB2;

    fill_tx_data(payload_len);
    status = sl_hal_can_add_message_to_tx_fifo(CAN_INSTANCE, &tx_hdr, tx_data);
    EFM_ASSERT(SL_STATUS_OK == status);
    print_can_message("TX", tx_hdr.identifier, tx_hdr.data_length,
                      tx_data, payload_len);

    status = wait_for_isr_flag(&tx_complete, "Tx complete");
    EFM_ASSERT(SL_STATUS_OK == status);
    status = wait_for_isr_flag(&rx_fifo1_new_msg, "Rx FIFO1 new message");
    EFM_ASSERT(SL_STATUS_OK == status);

    memset(rx_data, 0, sizeof(rx_data));
    status = sl_hal_can_get_rx_message(CAN_INSTANCE, SL_HAL_CAN_RX_FIFO1,
                                       0, &rx_frame, rx_data);
    EFM_ASSERT(SL_STATUS_OK == status);
    EFM_ASSERT(EXT_TX_FIFO_ID == rx_frame.identifier);
    EFM_ASSERT(SL_HAL_CAN_ID_EXTENDED == rx_frame.id_type);
    EFM_ASSERT(tx_hdr.frame_type == rx_frame.frame_type);
    EFM_ASSERT(tx_hdr.data_length == rx_frame.data_length);
    status = verify_rx_message("Ext-ID FIFO1",
                               rx_frame.identifier,
                               rx_frame.data_length,
                               rx_data,
                               payload_len);
    EFM_ASSERT(SL_STATUS_OK == status);
    if (phases[phase].fd_enable == SL_HAL_CAN_FD) {
      EFM_ASSERT(SL_HAL_CAN_FD == rx_frame.fd_format);
      EFM_ASSERT(phases[phase].brs_enable == rx_frame.bit_rate_switch);
    }

    /* Tx Event FIFO check for the ext-ID message. */
    if (tx_event_new_entry) {
      status = sl_hal_can_get_tx_event(CAN_INSTANCE, &tx_event);
      EFM_ASSERT(SL_STATUS_OK == status);
      EFM_ASSERT(EXT_TX_FIFO_ID == tx_event.identifier);
      EFM_ASSERT(0xB2 == tx_event.message_marker);
    }

    printf("\n  Extended-ID frame transfer and receive completed via Tx FIFO and Rx FIFO 1\r\n\n");

    /*==============================================================
    * STEP C : Specific-ID  →  Tx Buffer  →  Rx Buffer (dedicated)
    *=============================================================*/
    reset_isr_flags();

    if (phases[phase].fd_enable == SL_HAL_CAN_FD) {
      tx_hdr = (sl_hal_can_tx_frame_object_t)SL_HAL_CANFD_MESSAGE_HEADER_DEFAULT;
      tx_hdr.data_length     = dlc_fd;
      tx_hdr.fd_format       = SL_HAL_CAN_FD;
      tx_hdr.bit_rate_switch = phases[phase].brs_enable;
    } else {
      tx_hdr = (sl_hal_can_tx_frame_object_t)SL_HAL_CAN_MESSAGE_HEADER_DEFAULT;
      tx_hdr.data_length = dlc_classic;
      tx_hdr.fd_format   = SL_HAL_CAN_CLASSIC;
    }
    tx_hdr.identifier            = STD_RX_BUFFER_ID;
    tx_hdr.id_type               = SL_HAL_CAN_ID_STANDARD;
    tx_hdr.frame_type            = SL_HAL_CAN_FRAME_DATA;
    tx_hdr.tx_event_fifo_control = SL_HAL_CAN_TX_EVENTS_NO_STORE;
    tx_hdr.message_marker        = 0xC3;

    fill_tx_data(payload_len);
    status = sl_hal_can_add_message_to_tx_buffer(CAN_INSTANCE, &tx_hdr,
                                                 tx_data, TX_DEDICATED_BUFFER_IDX);
    EFM_ASSERT(SL_STATUS_OK == status);
    print_can_message("TX", tx_hdr.identifier, tx_hdr.data_length,
                      tx_data, payload_len);

    status = wait_for_isr_flag(&tx_complete, "Tx complete");
    EFM_ASSERT(SL_STATUS_OK == status);
    status = wait_for_isr_flag(&rx_buffer_new_msg, "Rx buffer new message");
    EFM_ASSERT(SL_STATUS_OK == status);

    memset(rx_data, 0, sizeof(rx_data));
    status = sl_hal_can_get_rx_message(CAN_INSTANCE, SL_HAL_CAN_RX_BUFFER,
                                       RX_DEDICATED_BUFFER_IDX,
                                       &rx_frame, rx_data);
    EFM_ASSERT(SL_STATUS_OK == status);
    EFM_ASSERT(STD_RX_BUFFER_ID == rx_frame.identifier);
    EFM_ASSERT(SL_HAL_CAN_ID_STANDARD == rx_frame.id_type);
    EFM_ASSERT(tx_hdr.frame_type == rx_frame.frame_type);
    EFM_ASSERT(tx_hdr.data_length == rx_frame.data_length);
    status = verify_rx_message("Std-ID Rx buffer",
                               rx_frame.identifier,
                               rx_frame.data_length,
                               rx_data,
                               payload_len);
    EFM_ASSERT(SL_STATUS_OK == status);
    if (phases[phase].fd_enable == SL_HAL_CAN_FD) {
      EFM_ASSERT(SL_HAL_CAN_FD == rx_frame.fd_format);
      EFM_ASSERT(phases[phase].brs_enable == rx_frame.bit_rate_switch);
    }

    printf("\n  Standard-ID frame transfer and receive completed via Tx buffer and Rx buffer\r\n");

    /*==============================================================
    * STEP D : Tx Abort test
    *=============================================================*/
    reset_isr_flags();
    tx_hdr.identifier = STD_TX_FIFO_ID;
    tx_hdr.id_type    = SL_HAL_CAN_ID_STANDARD;
    tx_hdr.tx_event_fifo_control = SL_HAL_CAN_TX_EVENTS_NO_STORE;

    status = sl_hal_can_add_message_to_tx_buffer(CAN_INSTANCE, &tx_hdr,
                                                 tx_data, TX_DEDICATED_BUFFER_IDX);
    if (status == SL_STATUS_OK) {
      status = sl_hal_can_request_abort_tx(CAN_INSTANCE,
                                           (1U << TX_DEDICATED_BUFFER_IDX));
      EFM_ASSERT(SL_STATUS_OK == status);
    }

    /*==============================================================
    * STEP E : High-priority message status & Protocol status
    *=============================================================*/
    sl_hal_can_get_hpm_status(CAN_INSTANCE, &hpm_status);

    sl_hal_can_get_protocol_status(CAN_INSTANCE, &protocol_status);
    EFM_ASSERT(0 == protocol_status.bus_off);

    /*==============================================================
    * STEP F : FIFO fill level queries
    *=============================================================*/
    (void)sl_hal_can_get_rx_fifo_fill_level(CAN_INSTANCE, SL_HAL_CAN_RX_FIFO0);
    (void)sl_hal_can_get_rx_fifo_fill_level(CAN_INSTANCE, SL_HAL_CAN_RX_FIFO1);

    /*==============================================================
    * STEP G : Power-down / wake-up cycle
    *=============================================================*/
    sl_hal_can_enter_power_down_mode(CAN_INSTANCE);
    sl_hal_can_exit_power_down_mode(CAN_INSTANCE);

    /*==============================================================
    * STEP H : Stop CAN
    *=============================================================*/
    sl_hal_can_stop(CAN_INSTANCE);

    /*==============================================================
    * STEP I : Interrupt enable/disable cycle
    *=============================================================*/
    sl_hal_can_disable_interrupts(CAN_INSTANCE, 0x3FFFFFFF);
    sl_hal_can_enable_interrupts(CAN_INSTANCE, 0x3FFFFFFF, 0x00000000);
    sl_hal_can_disable_interrupts(CAN_INSTANCE, 0x3FFFFFFF);

    /*==============================================================
    * STEP J : Disable ancillary features & cleanup
    *=============================================================*/
    sl_hal_can_disable_timestamp_counter(CAN_INSTANCE);
    sl_hal_can_disable_timeout_counter(CAN_INSTANCE);

    /* Filter removal. */
    status = sl_hal_can_remove_rx_filter(CAN_INSTANCE, SL_HAL_CAN_ID_STANDARD,
                                         STD_FILTER_IDX_FIFO0);
    EFM_ASSERT(SL_STATUS_OK == status);
    status = sl_hal_can_remove_rx_filter(CAN_INSTANCE, SL_HAL_CAN_ID_STANDARD,
                                         STD_FILTER_IDX_RXBUF);
    EFM_ASSERT(SL_STATUS_OK == status);
    status = sl_hal_can_remove_rx_filter(CAN_INSTANCE, SL_HAL_CAN_ID_EXTENDED,
                                         EXT_FILTER_IDX_FIFO1);
    EFM_ASSERT(SL_STATUS_OK == status);

    /*==============================================================
    * STEP K : De-init
    *=============================================================*/
    can_disable_nvic_interrupts();
    status = sl_hal_can_deinit(CAN_INSTANCE);
    EFM_ASSERT(SL_STATUS_OK == status);
    printf("\nCAN mode running: %s completed \r\n\n", phases[phase].label);
  }
  }
}

static void on_sample_tick(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sample_due = true;
}

/*******************************************************************************
 *                       GPIO PIN CONFIGURATION
 ******************************************************************************/
static void can_pin_configuration(void)
{
  sl_gpio_t gpio_pins;
#if defined(GPIO_CAN_ROUTEEN_CANTXPEN)
  GPIO->CANROUTE[0].ROUTEEN    = GPIO_CAN_ROUTEEN_CANTXPEN;
  GPIO->CANROUTE[0].CANRXROUTE = ((CAN_RX_PORT << _GPIO_CAN_CANRXROUTE_PORT_SHIFT)
                                  | (CAN_RX_PIN << _GPIO_CAN_CANRXROUTE_PIN_SHIFT));
  GPIO->CANROUTE[0].CANTXROUTE = ((CAN_TX_PORT << _GPIO_CAN_CANTXROUTE_PORT_SHIFT)
                                  | (CAN_TX_PIN << _GPIO_CAN_CANTXROUTE_PIN_SHIFT));
#endif
#if defined(GPIO_DBUSCAN_ROUTEEN_CANTXPEN)
  GPIO->DBUSCANROUTE[0].ROUTEEN    = GPIO_DBUSCAN_ROUTEEN_CANTXPEN;
  GPIO->DBUSCANROUTE[0].CANRXROUTE = ((CAN_RX_PORT << _GPIO_DBUSCAN_CANRXROUTE_PORT_SHIFT)
                                      | (CAN_RX_PIN << _GPIO_DBUSCAN_CANRXROUTE_PIN_SHIFT));
  GPIO->DBUSCANROUTE[0].CANTXROUTE = ((CAN_TX_PORT << _GPIO_DBUSCAN_CANTXROUTE_PORT_SHIFT)
                                      | (CAN_TX_PIN << _GPIO_DBUSCAN_CANTXROUTE_PIN_SHIFT));
#endif
  gpio_pins.port = SL_GPIO_PORT_A;
  gpio_pins.pin  = CAN_RX_PIN;
  sl_hal_gpio_set_pin_mode(&gpio_pins, SL_GPIO_MODE_INPUT, 1);
  gpio_pins.pin  = CAN_TX_PIN;
  sl_hal_gpio_set_pin_mode(&gpio_pins, SL_GPIO_MODE_PUSH_PULL, 1);
}

static void can_enable_nvic_interrupts(void)
{
  sl_interrupt_manager_clear_irq_pending(CAN0_INT0_IRQn);
  sl_interrupt_manager_enable_irq(CAN0_INT0_IRQn);
  sl_interrupt_manager_clear_irq_pending(CAN0_INT1_IRQn);
  sl_interrupt_manager_enable_irq(CAN0_INT1_IRQn);
}

static void can_disable_nvic_interrupts(void)
{
  NVIC_DisableIRQ(CAN0_INT0_IRQn);
  sl_interrupt_manager_clear_irq_pending(CAN0_INT0_IRQn);
  NVIC_DisableIRQ(CAN0_INT1_IRQn);
  sl_interrupt_manager_clear_irq_pending(CAN0_INT1_IRQn);
}

/*******************************************************************************
 *                                GLOBAL FUNCTIONS
 ******************************************************************************/
void can_init(void)
{
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_CAN0);
  can_pin_configuration();
  sl_sleeptimer_init();

  sl_sleeptimer_start_periodic_timer_ms(&sample_timer,
                                        SAMPLE_TIMEOUT_MS,
                                        on_sample_tick, NULL,
                                        0,
                                        SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
  printf("CAN communication started\r\n");
}

void can_process_action(void)
{
  if (sample_due) {
    can_sample_app();
    sample_due = false;
  }
}
