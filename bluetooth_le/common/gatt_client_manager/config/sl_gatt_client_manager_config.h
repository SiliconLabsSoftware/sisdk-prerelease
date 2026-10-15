/***************************************************************************//**
 * @file
 * @brief GATT Client Manager configuration
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

#ifndef SL_GATT_CLIENT_MANAGER_CONFIG_H
#define SL_GATT_CLIENT_MANAGER_CONFIG_H

/***************************************************************************//**
 * @addtogroup gatt_client_manager
 * @{
 ******************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> GATT Client Manager

// <q SL_GATT_CLIENT_MANAGER_AUTO_START_DISCOVERY>
//   Auto-discover all registered profiles on connection opened
// <i> Default: 1
// <i> Enabled (1): on sl_bt_evt_connection_opened every registered profile is
// <i> discovered automatically, in registration order, and one
// <i> discovery-complete event is emitted per profile. The cache is consulted
// <i> first when caching is enabled.
// <i> Disabled (0): nothing happens on connect. The application drives
// <i> discovery manually with sl_gatt_client_manager_discover_profile() for one
// <i> profile at a time (typically starting the next profile from the previous
// <i> discovery-complete event), or runs them all with
// <i> sl_gatt_client_manager_start_discovery().
#define SL_GATT_CLIENT_MANAGER_AUTO_START_DISCOVERY        (1)

// <q SL_GATT_CLIENT_MANAGER_AUTO_REDISCOVERY>
//   Auto-start rediscovery when the peer GATT database is out of sync
// <i> Default: 0
// <i> When enabled, an ATT Out Of Sync (or equivalent) condition, including a
// <i> Service Changed indication, emits
// <i> SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED and immediately starts
// <i> force discovery. When disabled, only the event is emitted and the
// <i> application must call sl_gatt_client_manager_force_discovery().
// <i> Independent of SL_GATT_CLIENT_MANAGER_AUTO_START_DISCOVERY: this applies
// <i> any time the peer's database is found to have changed, no matter how the
// <i> connection's first discovery was started (automatically or manually).
#define SL_GATT_CLIENT_MANAGER_AUTO_REDISCOVERY            (1)

// <o SL_GATT_CLIENT_MANAGER_MAX_PROFILES> Max registered profiles <1..32>
// <i> Default: 4
// <i> Maximum number of profiles the application may register. Each profile
// <i> consumes one profile handle.
#define SL_GATT_CLIENT_MANAGER_MAX_PROFILES                (4)

// <o SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE> Max svc-char pairs per profile <1..32>
// <i> Default: 4
// <i> Maximum number of service/characteristic pairs that one profile may
// <i> contain. A profile is only complete when all of its pairs are found.
#define SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE       (4)

// <o SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS> Max concurrent connections tracked <1..32>
// <i> Default: 4
// <i> Number of simultaneous connections the component can track.
// <i> Should be <= SL_BT_CONFIG_MAX_CONNECTIONS.
#define SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS             (4)

// <q SL_GATT_CLIENT_MANAGER_CACHE_ENABLE>
//   Enable RAM characteristic cache
// <i> Default: 1
// <i> When enabled, complete discovery results are cached and keyed by the
// <i> profile identity plus the remote GATT Database Hash. The peer address is
// <i> NOT part of the key, so a peer using a rotating address still hits the
// <i> cache while its GATT database is unchanged, and peers exposing an
// <i> identical database share one entry. Peers without a Database Hash
// <i> characteristic are never cached. Connection closed does NOT clear the
// <i> cache.
#define SL_GATT_CLIENT_MANAGER_CACHE_ENABLE                (1)

// <o SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES> Max cache entries <1..16>
// <i> Default: 4
// <i> Maximum number of (profile, database hash) results cached in RAM. The
// <i> least recently used entry is evicted when the table is full.
#define SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES           (4)

// <q SL_GATT_CLIENT_MANAGER_AUTO_CONFIRM_INDICATIONS>
//   Auto-confirm indications
// <i> Default: 1
// <i> When enabled, the component sends characteristic confirmations for
// <i> indications. Disable if the application will confirm manually via
// <i> sl_gatt_client_manager_send_characteristic_confirmation().
#define SL_GATT_CLIENT_MANAGER_AUTO_CONFIRM_INDICATIONS    (1)

// <q SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED>
//   Subscribe to Service Changed indications (0x2A05)
// <i> Default: 1
// <i> When enabled, the component discovers Service Changed once per
// <i> connection during the Generic Attribute phase and enables indications.
// <i> Those indications are consumed internally and treated as a database
// <i> change (REDISCOVERY_NEEDED). Disable (0) to skip the subscribe and ignore
// <i> Service Changed traffic; database-change detection then relies on robust
// <i> caching (Out Of Sync) and/or application-driven rediscovery.
#define SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED      (1)

// <q SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING>
//   Offer GATT robust caching to the peer
// <i> Default: 1
// <i> When enabled, the component sets the Robust Caching bit in the peer's
// <i> Client Supported Features characteristic (0x2B29) once per connection,
// <i> before reading the Database Hash. A GATT server only answers with the
// <i> ATT Database Out Of Sync error, and therefore only triggers
// <i> SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED, for clients that have
// <i> done this. Peers without 0x2B29, and peers that reject the write, are
// <i> tolerated: discovery continues normally. Service Changed indications
// <i> (0x2A05) still report database changes when
// <i> SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED is enabled.
#define SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING       (1)

// <q SL_GATT_CLIENT_MANAGER_SUPPORT_128BIT_UUID>
//   Support 128-bit UUIDs in addition to 16-bit
// <i> Default: 1
// <i> Disable (0) when every registered service/characteristic UUID is 16-bit
// <i> to shrink per-profile RAM (interest and cache pair tables).
#define SL_GATT_CLIENT_MANAGER_SUPPORT_128BIT_UUID         (1)

// <e SL_GATT_CLIENT_MANAGER_LOG>
//   Log
// <i> Default: 1
// <i> Emit discovery and cache debug messages via app_log when the
// <i> application includes the app_log component.
#define SL_GATT_CLIENT_MANAGER_LOG                         (1)

// <s SL_GATT_CLIENT_MANAGER_LOG_PREFIX> Log prefix
// <i> Default: "[GCM] "
#define SL_GATT_CLIENT_MANAGER_LOG_PREFIX                  "[GCM] "

// <q SL_GATT_CLIENT_MANAGER_LOG_EVENTS> Log dispatched events
// <i> Default: 1
// <i> Emit one debug line per event in the dispatcher, before handlers run.
// <i> Disable to keep discovery/cache messages while suppressing event traces.
#define SL_GATT_CLIENT_MANAGER_LOG_EVENTS                  (1)

// </e>

// </h>

// <h> Runtime settings

// <o SL_GATT_CLIENT_MANAGER_RTA_WAIT_FOR_GUARD> Timeout for guard acquisition (in ticks) <0-65535>
// <i> Number of RTOS ticks to wait when acquiring the RTA guard mutex that
// <i> protects the profile table, connection table and cache from races
// <i> between the application thread and the Bluetooth stack event task.
// <i> Default: 10
#define SL_GATT_CLIENT_MANAGER_RTA_WAIT_FOR_GUARD  10

// </h>

// <<< end of configuration section >>>

/** @} (end addtogroup gatt_client_manager) */

#endif // SL_GATT_CLIENT_MANAGER_CONFIG_H
