#ifndef CPC_UNITY_ASSERT_H
#define CPC_UNITY_ASSERT_H

#include "../sli_cpc_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void cpc_frame_pools_start_test(unsigned int lineno, const sl_cpc_bus_t *bus);
void cpc_frame_pools_end_test(unsigned int lineno, const sl_cpc_bus_t *bus);

#define CPC_FRAME_POOLS_START_TEST(_inst) cpc_frame_pools_start_test(__LINE__, (_inst))

#define CPC_FRAME_POOLS_END_TEST(_inst) cpc_frame_pools_end_test(__LINE__, (_inst))

#ifdef __cplusplus
}
#endif

#endif
