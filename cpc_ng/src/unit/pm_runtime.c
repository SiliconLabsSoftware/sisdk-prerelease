/***************************************************************************/ /**
 * @file pm_runtime.c
 * @brief Unit tests for sli_cpc_bus_pm_runtime_get / put reference counting.
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

#include <string.h>

#include <unity_fixture.h>

#include "sl_component_catalog.h"
#include "sl_cpc_bus.h"
#include "sl_cpc_wake.h"
#include "sl_status.h"

#include "../sli_cpc_bus.h"
#include "../sli_cpc_drv.h"

#if defined(SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT)

TEST_GROUP(cpc_pm_runtime);

static sl_cpc_bus_t s_bus;
static unsigned int s_resume_count;
static unsigned int s_suspend_count;

static sl_status_t stub_pm_runtime_resume(sl_cpc_bus_t *bus)
{
  TEST_ASSERT_EQUAL_PTR(&s_bus, bus);
  s_resume_count++;
  return SL_STATUS_OK;
}

static void stub_pm_runtime_suspend(sl_cpc_bus_t *bus)
{
  TEST_ASSERT_EQUAL_PTR(&s_bus, bus);
  s_suspend_count++;
}

static const sli_cpc_drv_ops_t s_ops = {
  .pm_runtime_resume = &stub_pm_runtime_resume,
  .pm_runtime_suspend = &stub_pm_runtime_suspend,
};

TEST_SETUP(cpc_pm_runtime)
{
  memset(&s_bus, 0, sizeof(s_bus));
  s_bus.drv_ops = &s_ops;
  s_bus.pm_runtime.mode = SL_CPC_POWER_MODE_WAKE_FROM_GPIO;
  s_resume_count = 0;
  s_suspend_count = 0;
}

TEST_TEAR_DOWN(cpc_pm_runtime)
{ /* empty */
}

/**
 * @brief get() calls pm_runtime_resume only on the 0→1 transition.
 */
TEST(cpc_pm_runtime, get_calls_resume_only_on_0_to_1)
{
  sli_cpc_bus_pm_runtime_get(&s_bus);
  TEST_ASSERT_EQUAL_UINT(1, s_resume_count);
  TEST_ASSERT_EQUAL_UINT(0, s_suspend_count);
  TEST_ASSERT_EQUAL_UINT(1, s_bus.pm_runtime.ref_cnt);

  sli_cpc_bus_pm_runtime_get(&s_bus);
  TEST_ASSERT_EQUAL_UINT(1, s_resume_count);
  TEST_ASSERT_EQUAL_UINT(2, s_bus.pm_runtime.ref_cnt);

  sli_cpc_bus_pm_runtime_get(&s_bus);
  TEST_ASSERT_EQUAL_UINT(1, s_resume_count);
  TEST_ASSERT_EQUAL_UINT(3, s_bus.pm_runtime.ref_cnt);
}

/**
 * @brief put() calls pm_runtime_suspend only on the 1→0 transition.
 */
TEST(cpc_pm_runtime, put_calls_suspend_only_on_1_to_0)
{
  sli_cpc_bus_pm_runtime_get(&s_bus);
  sli_cpc_bus_pm_runtime_get(&s_bus);
  sli_cpc_bus_pm_runtime_get(&s_bus);
  TEST_ASSERT_EQUAL_UINT(1, s_resume_count);
  TEST_ASSERT_EQUAL_UINT(0, s_suspend_count);
  TEST_ASSERT_EQUAL_UINT(3, s_bus.pm_runtime.ref_cnt);

  sli_cpc_bus_pm_runtime_put(&s_bus);
  TEST_ASSERT_EQUAL_UINT(0, s_suspend_count);
  TEST_ASSERT_EQUAL_UINT(2, s_bus.pm_runtime.ref_cnt);

  sli_cpc_bus_pm_runtime_put(&s_bus);
  TEST_ASSERT_EQUAL_UINT(0, s_suspend_count);
  TEST_ASSERT_EQUAL_UINT(1, s_bus.pm_runtime.ref_cnt);

  sli_cpc_bus_pm_runtime_put(&s_bus);
  TEST_ASSERT_EQUAL_UINT(1, s_suspend_count);
  TEST_ASSERT_EQUAL_UINT(0, s_bus.pm_runtime.ref_cnt);
}

/**
 * @brief get/put are no-ops when the bus is not in WAKE_FROM_GPIO mode.
 */
TEST(cpc_pm_runtime, ignored_when_not_wake_from_gpio)
{
  s_bus.pm_runtime.mode = SL_CPC_POWER_MODE_ALWAYS_ON;

  sli_cpc_bus_pm_runtime_get(&s_bus);
  sli_cpc_bus_pm_runtime_get(&s_bus);
  TEST_ASSERT_EQUAL_UINT(0, s_resume_count);
  TEST_ASSERT_EQUAL_UINT(0, s_bus.pm_runtime.ref_cnt);

  sli_cpc_bus_pm_runtime_put(&s_bus);
  TEST_ASSERT_EQUAL_UINT(0, s_suspend_count);
  TEST_ASSERT_EQUAL_UINT(0, s_bus.pm_runtime.ref_cnt);
}

TEST_GROUP_RUNNER(cpc_pm_runtime)
{
  RUN_TEST_CASE(cpc_pm_runtime, get_calls_resume_only_on_0_to_1);
  RUN_TEST_CASE(cpc_pm_runtime, put_calls_suspend_only_on_1_to_0);
  RUN_TEST_CASE(cpc_pm_runtime, ignored_when_not_wake_from_gpio);
}

#endif // SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT
