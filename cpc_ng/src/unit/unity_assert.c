#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unity_fixture.h>

#include "sl_assert.h"
#include "sl_compiler.h"

#include "../sli_cpc_panic.h"
#include "cpc_atomic.h"
#include "unity_assert.h"

typedef struct {
  uint32_t rx_free_block_count;
  uint32_t tx_free_block_count;
} cpc_frame_pool_snapshot_t;

static cpc_frame_pool_snapshot_t s_frame_pool_snapshot;

void assertEFM(const char *file, int line)
{
  printf("CRASH [%s:%d] Assertion failed\n", file, line);
  sli_cpc_panic();
}

void cpc_frame_pools_start_test(unsigned int lineno, const sl_cpc_bus_t *bus)
{
  UNITY_TEST_ASSERT_NOT_NULL(bus, lineno, "cpc_frame_pools_start_test: bus is NULL");

  s_frame_pool_snapshot.rx_free_block_count = sl_memory_pool_get_free_block_count(&bus->rx_frame_pool);
  s_frame_pool_snapshot.tx_free_block_count = sl_memory_pool_get_free_block_count(&bus->tx_frame_pool);
}

void cpc_frame_pools_end_test(unsigned int lineno, const sl_cpc_bus_t *bus)
{
  UNITY_TEST_ASSERT_NOT_NULL(bus, lineno, "cpc_frame_pools_end_test: bus is NULL");

  UNITY_TEST_ASSERT_EQUAL_UINT32(s_frame_pool_snapshot.rx_free_block_count,
                                 sl_memory_pool_get_free_block_count(&bus->rx_frame_pool), lineno,
                                 "RX frame pool leak detected");
  UNITY_TEST_ASSERT_EQUAL_UINT32(s_frame_pool_snapshot.tx_free_block_count,
                                 sl_memory_pool_get_free_block_count(&bus->tx_frame_pool), lineno,
                                 "TX frame pool leak detected");
}
