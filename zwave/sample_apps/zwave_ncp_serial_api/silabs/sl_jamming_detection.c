/*******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * https://www.silabs.com/about-us/legal/master-software-license-agreement
 * By installing, copying or otherwise using this software, you agree to the
 * terms of the MSLA.
 *
 ******************************************************************************/

/**
 * @file sl_jamming_detection.c
 * @brief Sample application implementation of RSSI-based jamming detection.
 *
 */
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "zpal_log.h"
#include "zpal_radio.h"
#include "zpal_radio_utils.h"
#include "sl_jamming_detection.h"

/** Task stack size in bytes. */
#define SL_JAMMING_DETECTION_TASK_SIZE_BYTES               (256)

/** Task period in milliseconds. */
#define SL_JAMMING_DETECTION_TASK_PERIOD_MS                (100)

/** Time period to wait before evaluating the buffers in milliseconds. */
#define SL_JAMMING_DETECTION_EVALUATION_DELAY_MS           (1000)

/** Number of sample periods to wait before evaluating the buffers. */
#define SL_JAMMING_DETECTION_EVALUATION_INTERVAL_PERIODS \
  (SL_JAMMING_DETECTION_EVALUATION_DELAY_MS / SL_JAMMING_DETECTION_TASK_PERIOD_MS)

/** Buffer size: 15 s at 100 ms = 150 samples. */
#define SL_JAMMING_DETECTION_BUFFER_SIZE                   (150)

/** User Config Default Value: Trigger count. */
#define SL_JAMMING_DETECTION_DEFAULT_TRIGGER               (95)

/** User Config Default Value: RSSI threshold in dBm. */
#define SL_JAMMING_DETECTION_DEFAULT_RSSI_THRESHOLD_DBM    (-50)

_Static_assert(SL_JAMMING_DETECTION_TASK_SIZE_BYTES >= 256, "SL_JAMMING_DETECTION_TASK_SIZE_BYTES should be greater than or equal to 256");

_Static_assert(SL_JAMMING_DETECTION_TASK_PERIOD_MS == 100, "SL_JAMMING_DETECTION_TASK_PERIOD_MS must be 100 ms");

_Static_assert(SL_JAMMING_DETECTION_EVALUATION_DELAY_MS == 1000, "SL_JAMMING_DETECTION_EVALUATION_DELAY_MS must be 1000 ms");

_Static_assert(SL_JAMMING_DETECTION_BUFFER_SIZE == 150, "SL_JAMMING_DETECTION_BUFFER_SIZE must be 150 bytes");

_Static_assert((SL_JAMMING_DETECTION_DEFAULT_TRIGGER > 0) && (SL_JAMMING_DETECTION_DEFAULT_TRIGGER < SL_JAMMING_DETECTION_BUFFER_SIZE), "SL_JAMMING_DETECTION_DEFAULT_TRIGGER must be in (0, SL_JAMMING_DETECTION_BUFFER_SIZE)");

_Static_assert((SL_JAMMING_DETECTION_DEFAULT_RSSI_THRESHOLD_DBM > -127) && (SL_JAMMING_DETECTION_DEFAULT_RSSI_THRESHOLD_DBM <= 0), "SL_JAMMING_DETECTION_DEFAULT_RSSI_THRESHOLD_DBM must be in (-127, 0]");

/****************************************************************************/
/* Private data                                                             */
/****************************************************************************/

/* Structure that will hold the TCB of the task being created. */
static StaticTask_t JammingTaskBuffer;

/* Buffer that the task being created will use as its stack. */
static StackType_t JammingStackBuffer[SL_JAMMING_DETECTION_TASK_SIZE_BYTES];

/** Jamming detection configuration */
static sl_jamming_detection_config_t jamming_detection_config = { 0 };

/** Circular buffer of RSSI samples per channel (last 15 s at 100 ms). */
static int8_t rssi_buffer[SL_JAMMING_DETECTION_NUM_CHANNELS][SL_JAMMING_DETECTION_BUFFER_SIZE];

/** Index of the next sample to write in the circular buffer. */
static uint8_t indexes[SL_JAMMING_DETECTION_NUM_CHANNELS];

/** Number of channels to monitor. */
static uint8_t num_channels = 0;

/** Number of remaining sample collection periods (a period is 100 ms by default; 0xffff means forever). */
static uint16_t collection_duration = 0;

/****************************************************************************/
/* Private Functions                                                        */
/****************************************************************************/

/*
 * @brief Fetch the RSSI value for a given channel.
 *
 * @param[in] channel Channel index.
 * @param[out] rssi Pointer to the RSSI value.
 * @return ZPAL_STATUS_OK if successful, ZPAL_STATUS_INVALID_ARGUMENT if the arguments are invalid.
 */
static zpal_status_t jamming_fetch_rssi(uint8_t channel, int8_t *rssi)
{
  zpal_status_t status = ZPAL_STATUS_FAIL;

  if (NULL == rssi) {
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  if (channel >= SL_JAMMING_DETECTION_NUM_CHANNELS) {
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  status = zpal_radio_get_background_rssi(channel, rssi);

  return status;
}

/*
 * @brief Fill the buffers with the RSSI value for a given channel.
 *
 * @param[in] channel Channel index.
 * @param[in] rssi Pointer to the RSSI value.
 * @return ZPAL_STATUS_OK if successful, ZPAL_STATUS_INVALID_ARGUMENT if the arguments are invalid.
 */
static zpal_status_t jamming_store_sample(uint8_t channel, int8_t *rssi)
{
  zpal_status_t status = ZPAL_STATUS_FAIL;

  if (NULL == rssi) {
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  if (channel >= SL_JAMMING_DETECTION_NUM_CHANNELS) {
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  if (*rssi > jamming_detection_config.settings[channel].rssi_threshold_dbm) {
    uint8_t index = indexes[channel];
    rssi_buffer[channel][index] = *rssi;
    index = (uint8_t)((index + 1u) % SL_JAMMING_DETECTION_BUFFER_SIZE);
    indexes[channel] = index;
  } else {
    (void)memset(rssi_buffer[channel], (unsigned char)(int8_t)ZPAL_RADIO_INVALID_RSSI_DBM,
                 sizeof(rssi_buffer[channel]));
    indexes[channel] = 0;
  }

  status = ZPAL_STATUS_OK;

  return status;
}

/**
 * @brief Check if the collection is enabled.
 *   Decrement the collection duration by 1 in case of collection is enabled and duration is not set to forever.
 *   Set 0xffff to enable collection forever.
 *   Set 0x0000 to disable collection.
 * @return true if the collection is enabled, false otherwise.
 */
static bool jamming_collection_is_enabled(void)
{
  bool is_enabled = (collection_duration > 0u) || (collection_duration == 0xffffu);

  if (collection_duration > 0u && collection_duration != 0xffffu) {
    collection_duration--;
  }

  return is_enabled;
}

/**
 * @brief Check if the evaluation interval has elapsed.
 *
 * @return true if the evaluation interval has elapsed, false otherwise.
 */
static bool jamming_evaluation_interval_elapsed(void)
{
  static uint8_t period_counter = 0;
  bool is_elapsed = false;

  if (++period_counter >= SL_JAMMING_DETECTION_EVALUATION_INTERVAL_PERIODS) {
    period_counter = 0;
    is_elapsed = true;
  }

  return is_elapsed;
}

/**
 * @brief Check if the channel is jammed.
 *
 * @param[in] channel Channel index.
 * @param[in] samples Number of samples above the threshold.
 * @return true if samples >= trigger value, false otherwise.
 */
static bool jamming_is_channel_jammed(uint8_t channel, uint8_t samples)
{
  bool is_jammed = false;

  if (samples >= jamming_detection_config.settings[channel].critical_number_of_samples) {
    is_jammed = true;
  }

  return is_jammed;
}

/**
 * @brief Count the number of samples above the threshold.
 * @param[in] channel Channel to count samples for.
 * @return Number of samples above the threshold.
 */
static uint8_t jamming_count_samples_above_threshold(uint8_t channel)
{
  uint8_t samples_above_threshold = 0;

  for (uint8_t x = 0; x < SL_JAMMING_DETECTION_BUFFER_SIZE; x++) {
    if (rssi_buffer[channel][x] > jamming_detection_config.settings[channel].rssi_threshold_dbm) {
      samples_above_threshold++;
    }
  }

  return samples_above_threshold;
}

/*
 * @brief Main task for jamming detection.
 */
static void jamming_detection_task(void *pvParameters)
{
  (void)pvParameters;
  zpal_status_t status = ZPAL_STATUS_FAIL;
  sl_jamming_detection_collection_t collection = { 0 };

  num_channels = zpal_radio_get_num_phy_channels(zpal_radio_get_protocol_mode());
  ZPAL_LOG_INFO(ZPAL_LOG_APP_JAMMING, "Number of channels : %u\n", num_channels);

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(SL_JAMMING_DETECTION_TASK_PERIOD_MS));

    /* Step 1: Fetch RSSI and fill the buffers */
    for (uint8_t channel = 0; channel < num_channels; channel++) {
      int8_t rssi = ZPAL_RADIO_INVALID_RSSI_DBM;
      status = jamming_fetch_rssi(channel, &rssi);
      if (ZPAL_STATUS_OK == status) {
        collection.samples[channel].rssi = rssi;
        status = jamming_store_sample(channel, &rssi);
        if (ZPAL_STATUS_OK != status) {
          ZPAL_LOG_ERROR(ZPAL_LOG_APP_JAMMING, "1. RSSI[%d] : fill failed with status %d\n", channel, status);
        }
      } else {
        ZPAL_LOG_ERROR(ZPAL_LOG_APP_JAMMING, "1. RSSI[%d] : fetch failed with status %d\n", channel, status);
      }
    }

    ZPAL_LOG_DEBUG(ZPAL_LOG_APP_JAMMING, "1. RSSI %d, %d, %d, %d\n", collection.samples[0].rssi, collection.samples[1].rssi,
                   collection.samples[2].rssi, collection.samples[3].rssi);
    /* Step 2:
       - case 1: Collect and send the collection to the user callback if enabled.
       - case 2: Evaluate the buffers every second for jamming detection */
    if (jamming_collection_is_enabled()) {
      status = jamming_detection_config.collection_callback(&collection);
    } else if (jamming_evaluation_interval_elapsed()) {
      sl_jamming_detection_statistics_t report = { 0 };
      for (uint8_t channel = 0; channel < num_channels; channel++) {
        /* 2.1 Count the number of samples above the threshold */
        uint8_t samples_above_threshold = jamming_count_samples_above_threshold(channel);
        ZPAL_LOG_DEBUG(ZPAL_LOG_APP_JAMMING, "2.%d Ch[%d]: %u/%u above threshold\n", (channel + 1), channel, (unsigned)samples_above_threshold, (unsigned)SL_JAMMING_DETECTION_BUFFER_SIZE);

        report.statistics[channel].rssi_threshold_dbm         = jamming_detection_config.settings[channel].rssi_threshold_dbm;
        report.statistics[channel].critical_number_of_samples = jamming_detection_config.settings[channel].critical_number_of_samples;
        report.statistics[channel].samples_above_threshold    = samples_above_threshold;

        /* 2.2 Jammed if number of samples above threshold >= trigger count */
        if (jamming_is_channel_jammed(channel, samples_above_threshold)) {
          report.channel_bitmap |= (uint8_t)(1u << channel);
        }
      }

      /* Step 3: Invoke the callback with statistics */
      if (report.channel_bitmap != 0) {
        if (NULL != jamming_detection_config.report_callback) {
          status = jamming_detection_config.report_callback(&report);
          ZPAL_LOG_DEBUG(ZPAL_LOG_APP_JAMMING, "3. Callback invoked (%s)\n", (status == ZPAL_STATUS_OK) ? "success" : "failure");
        }
      }
    }
  } /* end of for(;;) */
}

/****************************************************************************/
/* public functions                                                         */
/****************************************************************************/
zpal_status_t sl_jamming_detection_init(const sl_jamming_detection_config_t *user_config)
{
  zpal_status_t status = ZPAL_STATUS_FAIL;

  if (user_config == NULL) {
    ZPAL_LOG_ERROR(ZPAL_LOG_APP_JAMMING, "User config is NULL\n");
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  if (NULL == user_config->report_callback) {
    ZPAL_LOG_ERROR(ZPAL_LOG_APP_JAMMING, "Report callback is NULL\n");
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  if (NULL == user_config->collection_callback) {
    ZPAL_LOG_ERROR(ZPAL_LOG_APP_JAMMING, "Collection callback is NULL\n");
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  for (uint8_t channel = 0; channel < SL_JAMMING_DETECTION_NUM_CHANNELS; channel++) {
    if (user_config->settings[channel].critical_number_of_samples == 0 || user_config->settings[channel].critical_number_of_samples > SL_JAMMING_DETECTION_BUFFER_SIZE) {
      ZPAL_LOG_ERROR(ZPAL_LOG_APP_JAMMING, "Channel %d critical number of samples is invalid\n", channel);
      return ZPAL_STATUS_INVALID_ARGUMENT;
    }

    if ((user_config->settings[channel].rssi_threshold_dbm <= ZPAL_RADIO_INVALID_RSSI_DBM) || (user_config->settings[channel].rssi_threshold_dbm > 0)) {
      ZPAL_LOG_ERROR(ZPAL_LOG_APP_JAMMING, "Channel %d RSSI threshold is invalid (must be -127..0 dBm)\n", channel);
      return ZPAL_STATUS_INVALID_ARGUMENT;
    }
  }

  (void)memset(indexes, 0, sizeof(indexes));
  (void)memset(rssi_buffer, (unsigned char)(int8_t)ZPAL_RADIO_INVALID_RSSI_DBM, sizeof(rssi_buffer));

  /* Copy user configuration values */
  jamming_detection_config = *user_config;

  __attribute__((unused)) TaskHandle_t xHandle = xTaskCreateStatic(
    (TaskFunction_t)&jamming_detection_task,  // pvTaskCode
    "jamming_det",                            // pcName
    (uint16_t)SL_JAMMING_DETECTION_TASK_SIZE_BYTES, // usStackDepth
    NULL,                                     // pvParameters
    10,                                       // uxPriority
    JammingStackBuffer,                       // pxStackBuffer
    &JammingTaskBuffer                        // pxTaskBuffer
    );

  if (NULL != xHandle) {
    status = ZPAL_STATUS_OK;
  }

  return status;
}

zpal_status_t sl_jamming_detection_default_config(sl_jamming_detection_config_t *user_config)
{
  zpal_status_t status = ZPAL_STATUS_FAIL;

  if (NULL == user_config) {
    status = ZPAL_STATUS_INVALID_ARGUMENT;
  } else {
    for (uint8_t ch = 0; ch < SL_JAMMING_DETECTION_NUM_CHANNELS; ch++) {
      user_config->settings[ch].rssi_threshold_dbm = SL_JAMMING_DETECTION_DEFAULT_RSSI_THRESHOLD_DBM;
      user_config->settings[ch].critical_number_of_samples = SL_JAMMING_DETECTION_DEFAULT_TRIGGER;
    }
    user_config->report_callback = NULL;
    user_config->collection_callback = NULL;
    status = ZPAL_STATUS_OK;
  }

  return status;
}

zpal_status_t sl_jamming_detection_enable_collection(uint16_t periods)
{
  zpal_status_t status = ZPAL_STATUS_FAIL;

  if (periods > 0) {
    collection_duration = periods;
    status = ZPAL_STATUS_OK;
  }

  return status;
}

zpal_status_t sl_jamming_detection_set_channel_configuration(uint8_t channel, int8_t threshold, uint8_t critical_number_of_samples)
{
  zpal_status_t status = ZPAL_STATUS_FAIL;

  if (channel >= SL_JAMMING_DETECTION_NUM_CHANNELS) {
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  if (critical_number_of_samples == 0 || critical_number_of_samples > SL_JAMMING_DETECTION_BUFFER_SIZE) {
    return ZPAL_STATUS_INVALID_ARGUMENT;
  }

  jamming_detection_config.settings[channel].rssi_threshold_dbm = threshold;
  jamming_detection_config.settings[channel].critical_number_of_samples = critical_number_of_samples;
  status = ZPAL_STATUS_OK;

  return status;
}
