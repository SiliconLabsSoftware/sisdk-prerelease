/***************************************************************************/ /**
 * @file unity_assert.c
 * @brief CPC unit test assertion implementation.
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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unity_fixture.h>

#include "sl_assert.h"
#include "sl_compiler.h"

#include "../sli_cpc_panic.h"
#include "cpc_atomic.h"
#include "unity_assert.h"

typedef struct cpc_frame_pool_snapshot {
  struct cpc_frame_pool_snapshot *next;
  const sl_cpc_bus_t *bus;
  uint32_t rx_free_block_count;
  uint32_t tx_free_block_count;
} cpc_frame_pool_snapshot_t;

static cpc_frame_pool_snapshot_t *s_frame_pool_snapshots;

void assertEFM(const char *file, int line)
{
  printf("CRASH [%s:%d] Assertion failed\n", file, line);
  sli_cpc_panic();
}

static cpc_frame_pool_snapshot_t *find_snapshot(const sl_cpc_bus_t *bus)
{
  for (cpc_frame_pool_snapshot_t *snapshot = s_frame_pool_snapshots; snapshot != NULL; snapshot = snapshot->next) {
    if (snapshot->bus == bus) {
      return snapshot;
    }
  }

  return NULL;
}

static void remove_snapshot(cpc_frame_pool_snapshot_t *snapshot)
{
  cpc_frame_pool_snapshot_t **link = &s_frame_pool_snapshots;

  while (*link != NULL) {
    if (*link == snapshot) {
      *link = snapshot->next;
      free(snapshot);
      return;
    }
    link = &(*link)->next;
  }
}

void cpc_frame_pools_start_test(unsigned int lineno, const sl_cpc_bus_t *bus)
{
  cpc_frame_pool_snapshot_t *snapshot;

  UNITY_TEST_ASSERT_NOT_NULL(bus, lineno, "cpc_frame_pools_start_test: bus is NULL");

  snapshot = find_snapshot(bus);
  if (snapshot == NULL) {
    snapshot = calloc(1, sizeof(*snapshot));
    UNITY_TEST_ASSERT_NOT_NULL(snapshot, lineno, "cpc_frame_pools_start_test: out of memory");
    snapshot->bus = bus;
    snapshot->next = s_frame_pool_snapshots;
    s_frame_pool_snapshots = snapshot;
  }

  snapshot->rx_free_block_count = sl_memory_pool_get_free_block_count(&bus->rx_frame_pool);
  snapshot->tx_free_block_count = sl_memory_pool_get_free_block_count(&bus->tx_frame_pool);
}

void cpc_frame_pools_end_test(unsigned int lineno, const sl_cpc_bus_t *bus)
{
  cpc_frame_pool_snapshot_t *snapshot;

  UNITY_TEST_ASSERT_NOT_NULL(bus, lineno, "cpc_frame_pools_end_test: bus is NULL");

  snapshot = find_snapshot(bus);
  UNITY_TEST_ASSERT_NOT_NULL(snapshot, lineno, "cpc_frame_pools_end_test: no matching start_test");

  UNITY_TEST_ASSERT_EQUAL_UINT32(snapshot->rx_free_block_count,
                                 sl_memory_pool_get_free_block_count(&bus->rx_frame_pool), lineno,
                                 "RX frame pool leak detected");
  UNITY_TEST_ASSERT_EQUAL_UINT32(snapshot->tx_free_block_count,
                                 sl_memory_pool_get_free_block_count(&bus->tx_frame_pool), lineno,
                                 "TX frame pool leak detected");

  remove_snapshot(snapshot);
}
