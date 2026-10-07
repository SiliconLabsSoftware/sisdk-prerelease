/*******************************************************************************
 * @file
 * @brief Sleepy MTD EM4 application logic (sleepy-demo-mtd-em4.slcp).
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

// Define module name for Power Manager debuging feature.
#define CURRENT_MODULE_NAME "OPENTHREAD_SAMPLE_APP"

#include <assert.h>
#include <string.h>

#include <common/code_utils.hpp>
#include <openthread/cli.h>
#include <openthread/dataset_ftd.h>
#include <openthread/instance.h>
#include <openthread/link.h>
#include <openthread/message.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#include <openthread/platform/toolchain.h>

#include "sl_component_catalog.h"

// Use this config file to edit the default dataset for this application
#include "sl_openthread_default_dataset_values_config.h"

#include "em4_sleep.h"
#include "openthread-system.h"
#include "sl_sleeptimer.h"

#ifdef SL_CATALOG_KERNEL_PRESENT
#include "sl_ot_rtos_adaptation.h"
#endif // SL_CATALOG_KERNEL_PRESENT

// Constants
#define MULTICAST_ADDR "ff03::1"
#define MULTICAST_PORT 123
#define RECV_PORT 234
#define SLEEPY_POLL_PERIOD_MS 2000
#define SLEEPY_EM4_REQUEST_RETRY_MS 2000U
#define EM4_JOIN_SAMPLE "em4 join sample"
#define EM4_WAKE_SAMPLE "em4 wake sample"

// Forward declarations
void        mtdReceiveCallback(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);
extern void otSysEventSignalPending(void);

// Variables
static otUdpSocket                  sMtdSocket;
static volatile bool                sRequestRetryPending = false;
static volatile bool                sRequestRetryArmed   = false;
static bool                         sReconnectLogged     = false;
static bool                         sSampleSent          = false;
static sl_sleeptimer_timer_handle_t sRequestRetryTimer;

/*
 * Weak hook for app.c so setNetworkConfiguration() is skipped after EM4 wake.
 */
bool sleepyEm4IsWakeFromEm4(void)
{
    return sl_ot_em4_is_wake_from_em4();
}

/*
 * Sleeptimer callback: set a flag so applicationTick re-requests EM4.
 */
static void requestRetryTimerCallback(sl_sleeptimer_timer_handle_t *aHandle, void *aData)
{
    OT_UNUSED_VARIABLE(aHandle);
    OT_UNUSED_VARIABLE(aData);
    sRequestRetryArmed   = false;
    sRequestRetryPending = true;
    otSysEventSignalPending();
#ifdef SL_CATALOG_KERNEL_PRESENT
    sl_ot_rtos_set_pending_event(SL_OT_RTOS_EVENT_APP);
#endif
}

/*
 * Start a one-shot retry timer when sl_ot_em4_request() returns OT_ERROR_FAILED.
 */
static void scheduleRequestRetry(void)
{
    if (sRequestRetryArmed)
    {
        return;
    }

    if (sl_sleeptimer_start_timer_ms(&sRequestRetryTimer,
                                     SLEEPY_EM4_REQUEST_RETRY_MS,
                                     requestRetryTimerCallback,
                                     NULL,
                                     0,
                                     0)
        == SL_STATUS_OK)
    {
        sRequestRetryArmed = true;
    }
}

static void cancelRequestRetry(void)
{
    if (sRequestRetryArmed)
    {
        (void)sl_sleeptimer_stop_timer(&sRequestRetryTimer);
        sRequestRetryArmed = false;
    }
    sRequestRetryPending = false;
}

/*
 * Send a multicast UDP sample (join vs wake payload).
 */
static void sendSample(void)
{
    otMessage    *message = NULL;
    otMessageInfo messageInfo;
    const char   *payload = sl_ot_em4_is_wake_from_em4() ? EM4_WAKE_SAMPLE : EM4_JOIN_SAMPLE;

    // Setup messageInfo
    memset(&messageInfo, 0, sizeof(messageInfo));

    // Get a message buffer
    VerifyOrExit((message = otUdpNewMessage(otInstanceGetSingle(), NULL)) != NULL);
    SuccessOrExit(otIp6AddressFromString(MULTICAST_ADDR, &messageInfo.mPeerAddr));
    messageInfo.mPeerPort = MULTICAST_PORT;

    // Append the sample payload and send
    SuccessOrExit(otMessageAppend(message, payload, (uint16_t)strlen(payload)));
    SuccessOrExit(otUdpSend(otInstanceGetSingle(), &sMtdSocket, message, &messageInfo));

    // OpenThread took ownership of the buffer on success
    message = NULL;
    otCliOutputFormat("sent sample: %s\r\n", payload);

exit:
    if (message != NULL)
    {
        otMessageFree(message);
    }
}

/*
 * Ask the PAL to arm EM4. Pass wake_ms=0 to use the default from child timeout.
 */
static void requestEm4(otInstance *aInstance)
{
    otError error;

    if (sl_ot_em4_is_pending())
    {
        return;
    }

    error = sl_ot_em4_request(aInstance, 0);
    if (error == OT_ERROR_NONE)
    {
        cancelRequestRetry();
        otCliOutputFormat("em4 armed; enter when stack idle\r\n");
        return;
    }

    otCliOutputFormat("em4 request %s\r\n", otThreadErrorToString(error));

    // Stack was busy; retry from applicationTick after SLEEPY_EM4_REQUEST_RETRY_MS
    if (error == OT_ERROR_FAILED)
    {
        scheduleRequestRetry();
    }
}

/*
 * Print child/parent RLOC once after waking from EM4.
 */
static void logReconnectIfNeeded(otInstance *aInstance)
{
    otRouterInfo parentInfo;

    if (!sl_ot_em4_is_wake_from_em4() || sReconnectLogged)
    {
        return;
    }

    if (otThreadGetParentInfo(aInstance, &parentInfo) != OT_ERROR_NONE)
    {
        return;
    }

    otCliOutputFormat("child rloc=0x%04x parent=0x%04x\r\n",
                      (unsigned)otThreadGetRloc16(aInstance),
                      (unsigned)parentInfo.mRloc16);
    sReconnectLogged = true;
}

/*
 * On CHILD: send one sample, then arm EM4.
 * On detach: cancel pending EM4 / retry so we do not sleep offline.
 */
static void stateChangedCallback(otChangedFlags aFlags, void *aContext)
{
    otInstance *instance = (otInstance *)aContext;

    if ((aFlags & OT_CHANGED_THREAD_ROLE) == 0U)
    {
        return;
    }

    if (otThreadGetDeviceRole(instance) == OT_DEVICE_ROLE_CHILD)
    {
        logReconnectIfNeeded(instance);
        if (!sSampleSent)
        {
            sendSample();
            sSampleSent = true;
        }
        requestEm4(instance);
    }
    else
    {
        sSampleSent = false;
        sl_ot_em4_cancel();
        cancelRequestRetry();
    }
}

void sleepyInit(void)
{
    otError     error    = OT_ERROR_NONE;
    otInstance *instance = otInstanceGetSingle();

    SuccessOrExit(error = sl_ot_em4_init());
    if (sl_ot_em4_is_wake_from_em4())
    {
        otCliOutputFormat("woke from EM4\r\n");
    }
    else
    {
        otCliOutputFormat("starting (child_timeout=%lu s, wake_s=%lu)\r\n",
                          (unsigned long)otThreadGetChildTimeout(instance),
                          (unsigned long)(sl_ot_em4_get_default_wake_ms(instance) / 1000U));
    }

    otCliOutputFormat("[poll period: %d ms.]\r\n", SLEEPY_POLL_PERIOD_MS);
    SuccessOrExit(error = otLinkSetPollPeriod(instance, SLEEPY_POLL_PERIOD_MS));

    {
        otLinkModeConfig config;

        config.mRxOnWhenIdle = 0;
        config.mDeviceType   = 0;
        config.mNetworkData  = 0;
        SuccessOrExit(error = otThreadSetLinkMode(instance, config));
    }

    SuccessOrExit(error = otSetStateChangedCallback(instance, stateChangedCallback, instance));

exit:
    if (error != OT_ERROR_NONE)
    {
        otCliOutputFormat("Initialization failed with: %d, %s\r\n", error, otThreadErrorToString(error));
    }
}

/*
 * Override default network settings, such as panid, so the devices can join a network
 */
void setNetworkConfiguration(void)
{
    static char          aNetworkName[] = "SleepyEFR32";
    otError              error;
    otOperationalDataset aDataset;

    memset(&aDataset, 0, sizeof(otOperationalDataset));

    /*
     * Fields that can be configured in otOperationalDataset to override defaults:
     *     Network Name, Mesh Local Prefix, Extended PAN ID, PAN ID, Delay Timer,
     *     Channel, Channel Mask Page 0, Network Key, PSKc, Security Policy
     */
    aDataset.mActiveTimestamp.mSeconds             = 1;
    aDataset.mComponents.mIsActiveTimestampPresent = true;

    /* Set Channel */
    aDataset.mChannel                      = SL_OPENTHREAD_DEFAULT_DATASET_CHANNEL;
    aDataset.mComponents.mIsChannelPresent = true;

    /* Set Pan ID */
    aDataset.mPanId                      = (otPanId)SL_OPENTHREAD_DEFAULT_DATASET_PANID;
    aDataset.mComponents.mIsPanIdPresent = true;

    /* Set Extended Pan ID */
    {
        uint8_t extPanId[OT_EXT_PAN_ID_SIZE] = SL_OPENTHREAD_DEFAULT_DATASET_EXTPANID;

        memcpy(aDataset.mExtendedPanId.m8, extPanId, sizeof(aDataset.mExtendedPanId));
    }
    aDataset.mComponents.mIsExtendedPanIdPresent = true;

    /* Set Network Key */
    {
        uint8_t key[OT_NETWORK_KEY_SIZE] = SL_OPENTHREAD_DEFAULT_DATASET_NETWORKKEY;

        memcpy(aDataset.mNetworkKey.m8, key, sizeof(aDataset.mNetworkKey));
    }
    aDataset.mComponents.mIsNetworkKeyPresent = true;

    /* Set Network Name to SleepyEFR32 */
    {
        size_t length = strlen(aNetworkName);

        assert(length <= OT_NETWORK_NAME_MAX_SIZE);
        memcpy(aDataset.mNetworkName.m8, aNetworkName, length);
    }
    aDataset.mComponents.mIsNetworkNamePresent = true;

    /* Set the Active Operational Dataset to this dataset */
    error = otDatasetSetActive(otInstanceGetSingle(), &aDataset);
    if (error != OT_ERROR_NONE)
    {
        otCliOutputFormat("otDatasetSetActive failed with: %d, %s\r\n", error, otThreadErrorToString(error));
    }
}

void initUdp(void)
{
    otError    error;
    otSockAddr bindAddr;

    // Initialize bindAddr
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.mPort = RECV_PORT;

    // Open the socket
    error = otUdpOpen(otInstanceGetSingle(), &sMtdSocket, mtdReceiveCallback, NULL);
    if (error != OT_ERROR_NONE)
    {
        otCliOutputFormat("MTD failed to open udp socket with: %d, %s\r\n", error, otThreadErrorToString(error));
        return;
    }

    // Bind to the socket. Close the socket if bind fails.
    error = otUdpBind(otInstanceGetSingle(), &sMtdSocket, &bindAddr, OT_NETIF_THREAD_INTERNAL);
    if (error != OT_ERROR_NONE)
    {
        otCliOutputFormat("MTD failed to bind udp socket with: %d, %s\r\n", error, otThreadErrorToString(error));
        IgnoreReturnValue(otUdpClose(otInstanceGetSingle(), &sMtdSocket));
    }
}

#ifdef SL_CATALOG_KERNEL_PRESENT
#define applicationTick sl_ot_rtos_application_tick
#endif

void applicationTick(void)
{
    // Check for deferred EM4 request retry from sleeptimer
    if (sRequestRetryPending)
    {
        sRequestRetryPending = false;
        requestEm4(otInstanceGetSingle());
    }
}

void mtdReceiveCallback(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    OT_UNUSED_VARIABLE(aContext);
    OT_UNUSED_VARIABLE(aMessageInfo);

    uint8_t buf[64];
    int     length;

    // Read the received message's payload
    length = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, sizeof(buf) - 1);
    if (length < 0)
    {
        return;
    }
    buf[length] = '\0';
    otCliOutputFormat("Message Received: %s\r\n", buf);
}
