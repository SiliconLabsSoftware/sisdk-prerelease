/*******************************************************************************
 * @file
 * @brief SubGHz CLI
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#if SL_OPENTHREAD_SUBGHZ_CLI_ENABLE

#include <inttypes.h>
#include <string.h>

#include "sl_ot_custom_cli.h"
#include <openthread/cli.h>
#include "common/code_utils.hpp"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

static otError helpCommand(void *context, uint8_t argc, char *argv[]);

#if defined(SL_CATALOG_OT_DUTY_CYCLE_MANAGER_PRESENT)
#include "radio_duty_cycle_manager.h"

//-----------------------------------------------------------------------------
// List of accepted duty-cycle labels
static const char *dutyCycleLabels[] = {
    "get-stats",
    "get-buckets",
};

//-----------------------------------------------------------------------------
// Compares an unvalidated char array argument to string literal
static bool literalMatch(const char *literal, char *arg)
{
    return (strncmp(literal, arg, strlen(literal)) == 0);
}

static otError slStatusToOtError(sl_status_t status)
{
    otError error = OT_ERROR_FAILED;

    switch (status)
    {
    case SL_STATUS_OK:
        error = OT_ERROR_NONE;
        break;
    case SL_STATUS_INVALID_PARAMETER:
        error = OT_ERROR_INVALID_ARGS;
        break;
    case SL_STATUS_NOT_INITIALIZED:
        error = OT_ERROR_INVALID_STATE;
        break;
    default:
        break;
    }

    return error;
}

//-----------------------------------------------------------------------------
// Prints accepted duty-cycle labels
// Console Command: "subghz duty-cycle"
// Console Response" "[labels]"
static void dutyCycleHelpCommand(const char *labels[], size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        otCliOutputFormat("%s\r\n", labels[i]);
    }
}

//-----------------------------------------------------------------------------
// Console Command: "subghz duty-cycle get-stats"
static otError dutyCycleGetStatsCommand(void)
{
    sl_ot_duty_cycle_status_t status;
    sl_status_t               sl_status = sl_ot_duty_cycle_get_status(&status);
    uint32_t                  duty_scaled;

    VerifyOrExit(sl_status == SL_STATUS_OK);

    duty_scaled =
        (uint32_t)(((uint64_t)status.total_tx_us * 1000000ull) / SL_OT_RADIO_TX_TIME_WINDOW_DURATION_US_DEFAULT);

    otCliOutputFormat("Max budget (us):%*s%12" PRIu32 "\r\n", 14, "", status.max_budget_us);
    otCliOutputFormat("Current period TX (us):%*s%12" PRIu32 "\r\n", 7, "", status.current_period_tx_us);
    otCliOutputFormat("Total TX (us):%*s%12" PRIu32 "\r\n", 16, "", status.total_tx_us);
    otCliOutputFormat("Remaining budget (us):%*s%12" PRIu32 "\r\n", 8, "", status.remaining_budget_us);
    otCliOutputFormat("Duty (permille):%*s%12u\r\n", 14, "", status.duty_permille);
    otCliOutputFormat("Max duty (permille):%*s%12u\r\n", 10, "", status.max_duty_permille);
    otCliOutputFormat("Duty (%% of hour):%*s%7lu.%04lu%%\r\n",
                      12,
                      "",
                      (unsigned long)(duty_scaled / 10000u),
                      (unsigned long)(duty_scaled % 10000u));
    otCliOutputFormat("Level:%*s%12s\r\n", 24, "", sl_ot_duty_cycle_alert_level_to_string(status.level));

exit:
    return slStatusToOtError(sl_status);
}

//-----------------------------------------------------------------------------
// Console Command: "subghz duty-cycle get-buckets"
static otError dutyCycleGetBucketsCommand(void)
{
    sl_ot_duty_cycle_buckets_t buckets;
    sl_status_t                sl_status = sl_ot_duty_cycle_get_buckets(&buckets);
    uint32_t                   padded[SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT + 5] = {0};

    VerifyOrExit(sl_status == SL_STATUS_OK);

    memcpy(padded, buckets.bucket_tx_us, sizeof(uint32_t) * buckets.bucket_count);

    otCliOutputFormat("current:%u count:%u\r\n", buckets.current_bucket, buckets.bucket_count);

    // Each row prints five buckets. The index step matches that width so a
    // bucket is not repeated as the last column of one row and the first of the next.
    for (uint8_t i = 0; i < buckets.bucket_count; i += 5)
    {
        otCliOutputFormat("b%2u-%2u: %8" PRIu32 " %8" PRIu32 " %8" PRIu32 " %8" PRIu32 " %8" PRIu32 "\r\n",
                          i,
                          (uint8_t)(i + 4),
                          padded[i],
                          padded[i + 1u],
                          padded[i + 2u],
                          padded[i + 3u],
                          padded[i + 4u]);
    }

exit:
    return slStatusToOtError(sl_status);
}

//-----------------------------------------------------------------------------
// Console Command: "subghz duty-cycle [command]"
static otError dutyCycleCommand(void *context, uint8_t argc, char *argv[])
{
    OT_UNUSED_VARIABLE(context);

    otError error = OT_ERROR_NONE;

    if (argc == 1)
    {
        if (literalMatch(dutyCycleLabels[0], argv[0]))
        {
            error = dutyCycleGetStatsCommand();
        }
        else if (literalMatch(dutyCycleLabels[1], argv[0]))
        {
            error = dutyCycleGetBucketsCommand();
        }
        else
        {
            error = OT_ERROR_INVALID_ARGS;
        }
    }
    else
    {
        dutyCycleHelpCommand(dutyCycleLabels, OT_ARRAY_LENGTH(dutyCycleLabels));
        if (argc != 0)
        {
            error = OT_ERROR_INVALID_ARGS;
        }
    }

    return error;
}

#endif // SL_CATALOG_OT_DUTY_CYCLE_MANAGER_PRESENT

//-----------------------------------------------------------------------------

static otCliCommand subghzCommands[] = {
    {"help", &helpCommand},
#if defined(SL_CATALOG_OT_DUTY_CYCLE_MANAGER_PRESENT)
    {"duty-cycle", &dutyCycleCommand},
#endif
};

otError subghzCommandHandler(void *context, uint8_t argc, char *argv[])
{
    otError error = processCommand(context, argc, argv, OT_ARRAY_LENGTH(subghzCommands), subghzCommands);

    if (error == OT_ERROR_INVALID_COMMAND)
    {
        (void)helpCommand(NULL, 0, NULL);
    }

    return error;
}

static otError helpCommand(void *context, uint8_t argc, char *argv[])
{
    OT_UNUSED_VARIABLE(context);
    OT_UNUSED_VARIABLE(argc);
    OT_UNUSED_VARIABLE(argv);

    printCommands(subghzCommands, OT_ARRAY_LENGTH(subghzCommands));

    return OT_ERROR_NONE;
}

#endif // SL_OPENTHREAD_SUBGHZ_CLI_ENABLE
