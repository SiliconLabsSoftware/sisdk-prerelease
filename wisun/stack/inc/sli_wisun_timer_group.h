/*
 * Copyright (c) 2026 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1]. This software is distributed to you in
 * Source Code format and is governed by the sections of the MSLA applicable to
 * Source Code and Modified Open Source Code. By using this software, you agree
 * to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */

#ifndef SLI_WISUN_TIMER_GROUP_H
#define SLI_WISUN_TIMER_GROUP_H

#include <stdbool.h>
#include <stdint.h>

#include "sli_wisun_timer_service.h"

typedef struct sli_wisun_timer_group sli_wisun_timer_group_t;

struct sli_wisun_timer_group {
    void (*on_expiration)(sli_wisun_timer_group_t *group, uint64_t now_us);
    uint64_t (*get_next_expiration)(sli_wisun_timer_group_t *group);
    sli_wisun_timer_t timer;
    bool dispatching;
};

/**
 * Initialize a timer group.
 *
 * Caller must set get_next_expiration and on_expiration before calling
 * this function.
 *
 * @param group Group owned by the calling module.
 * @param context Timer context in which expiration callbacks run.
 * @param name Timer name used in debug traces.
 */
void sli_wisun_timer_group_init(sli_wisun_timer_group_t *group,
                                sli_wisun_timer_context_t *context,
                                const char *name);

/**
 * Update the timer after changing one entry's expiration timestamp.
 *
 * The entry's expiration timestamp must be set to new_expire_us prior to
 * calling this function. Zero and UINT64_MAX represent entries without a
 * finite expiration.
 *
 * @param group Group containing the changed entry.
 * @param old_expire_us Previous absolute expiration timestamp.
 * @param new_expire_us New absolute expiration timestamp.
 */
void sli_wisun_timer_group_update_one(sli_wisun_timer_group_t *group,
                                      uint64_t old_expire_us,
                                      uint64_t new_expire_us);

// Stop the shared timer without modifying entry timestamps.
void sli_wisun_timer_group_stop(sli_wisun_timer_group_t *group);

#endif
