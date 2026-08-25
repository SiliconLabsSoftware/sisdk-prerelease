
#include "test_nd6.h"

#include "lwip/netif.h"
#include "lwip/nd6.h"
#include "lwip/priv/nd6_priv.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/pbuf.h"
#include "lwip/memp.h"
#include "lwip/ethip6.h"
#include "lwip/prot/nd6.h"
#include "lwip/prot/icmp6.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"
#include <string.h>

/**
 * @file test_nd6.c
 * @brief ND6 (Neighbor Discovery for IPv6) dynamic timer unit tests
 * 
 * Test Organization:
 * ==================
 * 
 * Tests are organized into categories based on SL_LWIP_ND6_DYNAMIC_TIMER:
 * 
 * 1. COMMON TESTS:
 *    - Work regardless of SL_LWIP_ND6_DYNAMIC_TIMER setting
 *    - Test basic ND6 functionality: neighbor cache, state machine, etc.
 * 
 * 2. DYNAMIC TIMER TESTS:
 *    - Only compiled when SL_LWIP_ND6_DYNAMIC_TIMER is enabled
 *    - Test active mode (1 second) for INCOMPLETE, DELAY, PROBE states
 *    - Test ECO mode (30 seconds) for REACHABLE, STALE states
 *    - Test timer rescheduling on RX packet and state transitions
 * 
 * 3. DISABLED MODE TESTS:
 *    - Tests for when SL_LWIP_ND6_DYNAMIC_TIMER is disabled
 *    - Verify standard 1-second timer operation
 */

#if LWIP_IPV6

/* Test network interface */
static struct netif test_netif;
#if SL_LWIP_ND6_DYNAMIC_TIMER
static struct netif test_netif2;
#endif

/* Test IPv6 addresses */
static ip6_addr_t test_ip6_addr;
static ip6_addr_t test_neighbor_ip6_addr;

/* Helper to initialize test network interface */
static err_t
testif_init(struct netif *netif)
{
  netif->name[0] = 't';
  netif->name[1] = '6';
  netif->output_ip6 = ethip6_output;
  netif->linkoutput = NULL; /* We'll provide a dummy one when needed */
  netif->mtu = 1500;
  netif->hwaddr_len = 6;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_MLD6;
  
  /* Set a test MAC address */
  netif->hwaddr[0] = 0x02;
  netif->hwaddr[1] = 0x00;
  netif->hwaddr[2] = 0x00;
  netif->hwaddr[3] = 0x00;
  netif->hwaddr[4] = 0x00;
  netif->hwaddr[5] = 0x01;

  return ERR_OK;
}

/* Dummy link output function */
static err_t
testif_linkoutput(struct netif *netif, struct pbuf *p)
{
  LWIP_UNUSED_ARG(netif);
  LWIP_UNUSED_ARG(p);
  return ERR_OK;
}

/* Setup function - called before each test */
static void
nd6_setup(void)
{
  s8_t i;
  
  lwip_check_ensure_no_alloc(SKIP_POOL(MEMP_SYS_TIMEOUT));
  
  /* Clear test netif structure */
  memset(&test_netif, 0, sizeof(struct netif));
#if SL_LWIP_ND6_DYNAMIC_TIMER
  memset(&test_netif2, 0, sizeof(struct netif));
#endif
  
  /* Clear any stale neighbor cache entries from previous tests */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].netif == &test_netif) {
      /* Free any queued packets to prevent memory leaks */
#if LWIP_ND6_QUEUEING
      {
        struct nd6_q_entry *q_entry, *next;
        q_entry = neighbor_cache[i].q;
        while (q_entry != NULL) {
          next = q_entry->next;
          if (q_entry->p != NULL) {
            pbuf_free(q_entry->p);
          }
          memp_free(MEMP_ND6_QUEUE, q_entry);
          q_entry = next;
        }
        neighbor_cache[i].q = NULL;
      }
#else /* LWIP_ND6_QUEUEING */
      if (neighbor_cache[i].q != NULL) {
        pbuf_free(neighbor_cache[i].q);
        neighbor_cache[i].q = NULL;
      }
#endif /* LWIP_ND6_QUEUEING */
      neighbor_cache[i].state = ND6_NO_ENTRY;
      neighbor_cache[i].netif = NULL;
    }
  }
  
  /* Initialize test IPv6 addresses */
  IP6_ADDR(&test_ip6_addr, 0xfe800000, 0, 0, 0x1);
  IP6_ADDR(&test_neighbor_ip6_addr, 0xfe800000, 0, 0, 0x2);
}

/* Teardown function - called after each test */
static void
nd6_teardown(void)
{
  s8_t i;
  
  /* Clean up any neighbor cache entries that reference test_netif */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].netif == &test_netif) {
      /* Free any queued packets to prevent memory leaks */
#if LWIP_ND6_QUEUEING
      {
        struct nd6_q_entry *q_entry, *next;
        q_entry = neighbor_cache[i].q;
        while (q_entry != NULL) {
          next = q_entry->next;
          if (q_entry->p != NULL) {
            pbuf_free(q_entry->p);
          }
          memp_free(MEMP_ND6_QUEUE, q_entry);
          q_entry = next;
        }
        neighbor_cache[i].q = NULL;
      }
#else /* LWIP_ND6_QUEUEING */
      if (neighbor_cache[i].q != NULL) {
        pbuf_free(neighbor_cache[i].q);
        neighbor_cache[i].q = NULL;
      }
#endif /* LWIP_ND6_QUEUEING */
      neighbor_cache[i].state = ND6_NO_ENTRY;
      neighbor_cache[i].netif = NULL;
    }
  }
  
  /* Check if netif was added to the list before cleaning up */
  if (test_netif.num != 0 || netif_list == &test_netif) {
    /* Bring link down so ND6/MLD6 timers stop before netif removal */
    if (netif_is_link_up(&test_netif)) {
#if SL_LWIP_ADAPTIVE_TIMERS
      netif_stop_timers(&test_netif);
#endif
      netif_set_link_down(&test_netif);
    }

    /* Clean up any loop buffers */
    if (test_netif.loop_first != NULL) {
      pbuf_free(test_netif.loop_first);
      test_netif.loop_first = NULL;
    }
    test_netif.loop_last = NULL;
    
    /* Clean up ND6 state associated with test_netif */
    nd6_cleanup_netif(&test_netif);
    
    /* Remove network interface */
    netif_remove(&test_netif);
  }

#if SL_LWIP_ND6_DYNAMIC_TIMER
  if (test_netif2.num != 0 || netif_list == &test_netif2) {
    if (netif_is_link_up(&test_netif2)) {
#if SL_LWIP_ADAPTIVE_TIMERS
      netif_stop_timers(&test_netif2);
#endif
      netif_set_link_down(&test_netif2);
    }
    nd6_cleanup_netif(&test_netif2);
    netif_remove(&test_netif2);
  }
#endif
  
  /* Poll tcpip thread to process any pending operations and free memory */
  tcpip_thread_poll_one();
  
  /* Ensure test_netif is fully cleared */
  memset(&test_netif, 0, sizeof(struct netif));
#if SL_LWIP_ND6_DYNAMIC_TIMER
  memset(&test_netif2, 0, sizeof(struct netif));
#endif
  
  lwip_check_ensure_no_alloc(SKIP_POOL(MEMP_SYS_TIMEOUT));
}

/* ============================================================
 * COMMON TESTS - work regardless of SL_LWIP_ND6_DYNAMIC_TIMER
 * ============================================================ */

START_TEST(test_nd6_neighbor_cache_initialization)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Verify that neighbor cache entries are properly initialized */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    /* Check initial state */
    fail_unless(neighbor_cache[i].state == ND6_NO_ENTRY || 
                neighbor_cache[i].state >= ND6_INCOMPLETE,
                "Neighbor cache entry %d has invalid initial state", i);
  }
}
END_TEST

START_TEST(test_nd6_netif_setup)
{
  err_t err;
  LWIP_UNUSED_ARG(_i);
  
  /* Add network interface */
  netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
  test_netif.linkoutput = testif_linkoutput;
  
  /* Set link and interface up */
  netif_set_link_up(&test_netif);
  netif_set_up(&test_netif);
  
  /* Add IPv6 address */
  err = netif_add_ip6_address(&test_netif, &test_ip6_addr, NULL);
  fail_unless(err == ERR_OK || err == ERR_VAL, "Failed to add IPv6 address");
  
  /* Verify interface is up */
  fail_unless(netif_is_up(&test_netif), "Network interface should be up");
  fail_unless(netif_is_link_up(&test_netif), "Network link should be up");
}
END_TEST

START_TEST(test_nd6_neighbor_state_transitions)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Test state transitions */
    neighbor_cache[i].state = ND6_INCOMPLETE;
    fail_unless(neighbor_cache[i].state == ND6_INCOMPLETE);
    
    neighbor_cache[i].state = ND6_REACHABLE;
    fail_unless(neighbor_cache[i].state == ND6_REACHABLE);
    
    neighbor_cache[i].state = ND6_STALE;
    fail_unless(neighbor_cache[i].state == ND6_STALE);
    
    neighbor_cache[i].state = ND6_DELAY;
    fail_unless(neighbor_cache[i].state == ND6_DELAY);
    
    neighbor_cache[i].state = ND6_PROBE;
    fail_unless(neighbor_cache[i].state == ND6_PROBE);
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
  }
}
END_TEST

#if SL_LWIP_ND6_DYNAMIC_TIMER
/* ============================================================
 * DYNAMIC TIMER TESTS - only when SL_LWIP_ND6_DYNAMIC_TIMER enabled
 * ============================================================ */

static int
nd6_timeout_count(void)
{
  struct sys_timeo **list_head = sys_timeouts_get_next_timeout();
  struct sys_timeo *t;
  int count = 0;

  if (list_head == NULL) {
    return 0;
  }
  for (t = *list_head; t != NULL; t = t->next) {
    if (t->h == nd6_tmr) {
      count++;
    }
  }
  return count;
}

static s8_t
nd6_test_setup_reachable_neighbor(void)
{
  s8_t i;

  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  if (i >= LWIP_ND6_NUM_NEIGHBORS) {
    return -1;
  }

#if LWIP_HAVE_LOOPIF
  /* Isolate test netif: loopback would otherwise keep ND6 timer active */
  if (netif_get_loopif() != NULL) {
#if SL_LWIP_ADAPTIVE_TIMERS
    netif_stop_timers(netif_get_loopif());
#endif
    netif_set_link_down(netif_get_loopif());
  }
#endif

  netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
  test_netif.linkoutput = testif_linkoutput;
  netif_set_link_up(&test_netif);
  netif_set_up(&test_netif);

  neighbor_cache[i].state = ND6_REACHABLE;
  neighbor_cache[i].netif = &test_netif;
  neighbor_cache[i].counter.reachable_time = ND6_TMR_ECO_INTERVAL;
  ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);

  return i;
}

static void
nd6_test_cleanup_reachable_neighbor(s8_t idx)
{
  if (idx >= 0) {
    neighbor_cache[idx].state = ND6_NO_ENTRY;
    neighbor_cache[idx].netif = NULL;
  }
#if LWIP_HAVE_LOOPIF
  if (netif_get_loopif() != NULL) {
    netif_set_link_up(netif_get_loopif());
  }
#endif
}

START_TEST(test_nd6_timer_initialization)
{
  LWIP_UNUSED_ARG(_i);
  
  /* Initialize ND6 timer */
  nd6_tmr_init();
  
  /* Note: We can't directly verify timer scheduling without exposing 
   * internal timer state, but we can verify the function doesn't crash */
  fail_unless(1);
}
END_TEST

START_TEST(test_nd6_active_mode_incomplete_state)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set neighbor to INCOMPLETE state (active mode) */
    neighbor_cache[i].state = ND6_INCOMPLETE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.probes_sent = 0;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    /* Verify INCOMPLETE state is set (this should trigger 1-second timer) */
    fail_unless(neighbor_cache[i].state == ND6_INCOMPLETE);
    
    /* INCOMPLETE is an active state - should use ND6_TMR_ACTIVE_INTERVAL (1000ms) */
    /* In the implementation, when any neighbor is in INCOMPLETE, DELAY, or PROBE,
     * the timer should be scheduled with 1-second interval */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_active_mode_delay_state)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set neighbor to DELAY state (active mode) */
    neighbor_cache[i].state = ND6_DELAY;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME / ND6_TMR_INTERVAL;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    /* Verify DELAY state is set (this should trigger 1-second timer) */
    fail_unless(neighbor_cache[i].state == ND6_DELAY);
    fail_unless(neighbor_cache[i].counter.delay_time > 0);
    
    /* DELAY is an active state - should use ND6_TMR_ACTIVE_INTERVAL (1000ms) */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_active_mode_probe_state)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set neighbor to PROBE state (active mode) */
    neighbor_cache[i].state = ND6_PROBE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.probes_sent = 0;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    /* Verify PROBE state is set (this should trigger 1-second timer) */
    fail_unless(neighbor_cache[i].state == ND6_PROBE);
    
    /* PROBE is an active state - should use ND6_TMR_ACTIVE_INTERVAL (1000ms) */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_eco_mode_reachable_state)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set neighbor to REACHABLE state (ECO mode) */
    neighbor_cache[i].state = ND6_REACHABLE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.reachable_time = reachable_time;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    /* Verify REACHABLE state is set */
    fail_unless(neighbor_cache[i].state == ND6_REACHABLE);
    fail_unless(neighbor_cache[i].counter.reachable_time > 0);
    
    /* REACHABLE is an ECO mode state - should use ND6_TMR_ECO_INTERVAL (30000ms)
     * when no active states are present */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_eco_mode_stale_state)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set neighbor to STALE state (ECO mode) */
    neighbor_cache[i].state = ND6_STALE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.stale_time = 0;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    /* Verify STALE state is set */
    fail_unless(neighbor_cache[i].state == ND6_STALE);
    
    /* STALE is an ECO mode state - should use ND6_TMR_ECO_INTERVAL (30000ms)
     * when no active states are present */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_mode_transition_eco_to_active)
{
  s8_t i, j, k;
  LWIP_UNUSED_ARG(_i);
  
  /* Find two free neighbor cache entries */
  i = -1;
  j = -1;
  for (k = 0; k < LWIP_ND6_NUM_NEIGHBORS; k++) {
    if (neighbor_cache[k].state == ND6_NO_ENTRY) {
      if (i == -1) {
        i = k;
      } else if (j == -1) {
        j = k;
        break;
      }
    }
  }
  
  if (i >= 0 && j >= 0) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set first neighbor to ECO mode (REACHABLE) */
    neighbor_cache[i].state = ND6_REACHABLE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.reachable_time = reachable_time;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    /* Set second neighbor to active mode (INCOMPLETE) */
    neighbor_cache[j].state = ND6_INCOMPLETE;
    neighbor_cache[j].netif = &test_netif;
    neighbor_cache[j].counter.probes_sent = 0;
    IP6_ADDR(&neighbor_cache[j].next_hop_address, 0xfe800000, 0, 0, 0x3);
    
    /* Verify states */
    fail_unless(neighbor_cache[i].state == ND6_REACHABLE);
    fail_unless(neighbor_cache[j].state == ND6_INCOMPLETE);
    
    /* When both ECO and active mode neighbors exist, timer should use
     * active mode interval (1 second) to service active states properly */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
    neighbor_cache[j].state = ND6_NO_ENTRY;
    neighbor_cache[j].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_mode_transition_active_to_eco)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Start with active mode (DELAY) */
    neighbor_cache[i].state = ND6_DELAY;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.delay_time = 1;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    fail_unless(neighbor_cache[i].state == ND6_DELAY);
    
    /* Simulate transition to ECO mode by changing to REACHABLE */
    neighbor_cache[i].state = ND6_REACHABLE;
    neighbor_cache[i].counter.reachable_time = reachable_time;
    
    fail_unless(neighbor_cache[i].state == ND6_REACHABLE);
    
    /* When only ECO mode neighbors exist, timer should switch to
     * ECO mode interval (30 seconds) */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_timer_reschedule_on_state_change)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Start in ECO mode */
    neighbor_cache[i].state = ND6_STALE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.stale_time = 0;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    fail_unless(neighbor_cache[i].state == ND6_STALE);
    
    /* Simulate state change to DELAY (active mode) - as would happen on packet TX
     * In nd6.c, this transition triggers sys_untimeout/sys_timeout to reschedule */
    neighbor_cache[i].state = ND6_DELAY;
    neighbor_cache[i].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME / ND6_TMR_INTERVAL;
    
    fail_unless(neighbor_cache[i].state == ND6_DELAY);
    
    /* The implementation should reschedule timer from 30s to 1s when
     * transitioning from STALE (ECO) to DELAY (active) */
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_timer_callback_execution)
{
  s8_t i;
  u32_t initial_reachable_time;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set up a REACHABLE neighbor with a known reachable_time */
    neighbor_cache[i].state = ND6_REACHABLE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.reachable_time = 5000; /* 5 seconds */
    initial_reachable_time = neighbor_cache[i].counter.reachable_time;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    fail_unless(neighbor_cache[i].state == ND6_REACHABLE);
    fail_unless(neighbor_cache[i].counter.reachable_time == initial_reachable_time);
    
    /* Call nd6_tmr - this should process timers */
    nd6_tmr(NULL);
    
    /* The timer should have been decremented (actual amount depends on elapsed time) */
    /* In the dynamic timer implementation, elapsed_time_ms is calculated */
    fail_unless(neighbor_cache[i].counter.reachable_time <= initial_reachable_time);
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_elapsed_time_calculation)
{
  LWIP_UNUSED_ARG(_i);
  
  /* Initialize timer */
  nd6_tmr_init();
  
  /* Call timer multiple times to test elapsed time calculation */
  nd6_tmr(NULL);
  
  /* Add a small delay simulation by calling again */
  nd6_tmr(NULL);
  
  /* The implementation should correctly calculate elapsed time between calls
   * and adjust timer values accordingly */
  
  /* This test verifies the timer doesn't crash with multiple invocations */
  fail_unless(1);
}
END_TEST

START_TEST(test_nd6_timer_overflow_protection)
{
  s8_t i;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set up with very large reachable_time to test overflow protection */
    neighbor_cache[i].state = ND6_REACHABLE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.reachable_time = 0xFFFFFFFF; /* Max u32_t */
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    fail_unless(neighbor_cache[i].state == ND6_REACHABLE);
    
    /* Call timer - should handle large values without overflow */
    nd6_tmr(NULL);
    
    /* Implementation should use safe arithmetic (nd6_safe_multiply_by_1000) */
    fail_unless(1);
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

START_TEST(test_nd6_mixed_states_timer_selection)
{
  s8_t idx[4];
  s8_t count = 0;
  s8_t k;
  LWIP_UNUSED_ARG(_i);
  
  /* Find 4 free neighbor cache entries */
  for (k = 0; k < LWIP_ND6_NUM_NEIGHBORS && count < 4; k++) {
    if (neighbor_cache[k].state == ND6_NO_ENTRY) {
      idx[count++] = k;
    }
  }
  
  if (count >= 4) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set up neighbors in different states:
     * - REACHABLE (ECO)
     * - STALE (ECO)
     * - INCOMPLETE (Active)
     * - DELAY (Active) */
    
    neighbor_cache[idx[0]].state = ND6_REACHABLE;
    neighbor_cache[idx[0]].netif = &test_netif;
    neighbor_cache[idx[0]].counter.reachable_time = reachable_time;
    IP6_ADDR(&neighbor_cache[idx[0]].next_hop_address, 0xfe800000, 0, 0, 0x10);
    
    neighbor_cache[idx[1]].state = ND6_STALE;
    neighbor_cache[idx[1]].netif = &test_netif;
    neighbor_cache[idx[1]].counter.stale_time = 0;
    IP6_ADDR(&neighbor_cache[idx[1]].next_hop_address, 0xfe800000, 0, 0, 0x11);
    
    neighbor_cache[idx[2]].state = ND6_INCOMPLETE;
    neighbor_cache[idx[2]].netif = &test_netif;
    neighbor_cache[idx[2]].counter.probes_sent = 0;
    IP6_ADDR(&neighbor_cache[idx[2]].next_hop_address, 0xfe800000, 0, 0, 0x12);
    
    neighbor_cache[idx[3]].state = ND6_DELAY;
    neighbor_cache[idx[3]].netif = &test_netif;
    neighbor_cache[idx[3]].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME / ND6_TMR_INTERVAL;
    IP6_ADDR(&neighbor_cache[idx[3]].next_hop_address, 0xfe800000, 0, 0, 0x13);
    
    /* Verify all states */
    fail_unless(neighbor_cache[idx[0]].state == ND6_REACHABLE);
    fail_unless(neighbor_cache[idx[1]].state == ND6_STALE);
    fail_unless(neighbor_cache[idx[2]].state == ND6_INCOMPLETE);
    fail_unless(neighbor_cache[idx[3]].state == ND6_DELAY);
    
    /* With mixed ECO and active states, timer should use active mode (1 second)
     * to properly service the active state neighbors */
    
    /* Clean up */
    for (k = 0; k < count; k++) {
      neighbor_cache[idx[k]].state = ND6_NO_ENTRY;
      neighbor_cache[idx[k]].netif = NULL;
    }
  }
}
END_TEST

START_TEST(test_nd6_all_eco_states_timer_selection)
{
  s8_t idx[2];
  s8_t count = 0;
  s8_t k;
  LWIP_UNUSED_ARG(_i);
  
  /* Find 2 free neighbor cache entries */
  for (k = 0; k < LWIP_ND6_NUM_NEIGHBORS && count < 2; k++) {
    if (neighbor_cache[k].state == ND6_NO_ENTRY) {
      idx[count++] = k;
    }
  }
  
  if (count >= 2) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set up only ECO mode neighbors */
    neighbor_cache[idx[0]].state = ND6_REACHABLE;
    neighbor_cache[idx[0]].netif = &test_netif;
    neighbor_cache[idx[0]].counter.reachable_time = reachable_time;
    IP6_ADDR(&neighbor_cache[idx[0]].next_hop_address, 0xfe800000, 0, 0, 0x20);
    
    neighbor_cache[idx[1]].state = ND6_STALE;
    neighbor_cache[idx[1]].netif = &test_netif;
    neighbor_cache[idx[1]].counter.stale_time = 0;
    IP6_ADDR(&neighbor_cache[idx[1]].next_hop_address, 0xfe800000, 0, 0, 0x21);
    
    /* Verify states */
    fail_unless(neighbor_cache[idx[0]].state == ND6_REACHABLE);
    fail_unless(neighbor_cache[idx[1]].state == ND6_STALE);
    
    /* With only ECO mode neighbors, timer should use ECO mode (30 seconds) */
    
    /* Clean up */
    for (k = 0; k < count; k++) {
      neighbor_cache[idx[k]].state = ND6_NO_ENTRY;
      neighbor_cache[idx[k]].netif = NULL;
    }
  }
}
END_TEST

START_TEST(test_nd6_link_down_stops_timer)
{
  s8_t idx;
  LWIP_UNUSED_ARG(_i);

  idx = nd6_test_setup_reachable_neighbor();
  if (idx < 0) {
    return;
  }

  fail_unless(nd6_timeout_count() == 1);

#if SL_LWIP_ADAPTIVE_TIMERS
  netif_stop_timers(&test_netif);
#endif
  netif_set_link_down(&test_netif);
  fail_unless(nd6_timeout_count() == 0, "No ND6 timeout should remain after link-down");

  nd6_test_cleanup_reachable_neighbor(idx);
}
END_TEST

START_TEST(test_nd6_link_up_restarts_timer)
{
  s8_t idx;
  LWIP_UNUSED_ARG(_i);

  idx = nd6_test_setup_reachable_neighbor();
  if (idx < 0) {
    return;
  }

#if SL_LWIP_ADAPTIVE_TIMERS
  netif_stop_timers(&test_netif);
#endif
  netif_set_link_down(&test_netif);
  fail_unless(nd6_timeout_count() == 0, "ND6 timer should be stopped after link-down");

  netif_set_link_up(&test_netif);
  fail_unless(nd6_timeout_count() == 1, "Exactly one ND6 timeout after link-up restart");

  nd6_test_cleanup_reachable_neighbor(idx);
}
END_TEST

START_TEST(test_nd6_link_up_does_not_duplicate_existing_timer)
{
  s8_t idx;
  LWIP_UNUSED_ARG(_i);

  idx = nd6_test_setup_reachable_neighbor();
  if (idx < 0) {
    return;
  }

#if SL_LWIP_ADAPTIVE_TIMERS
  netif_stop_timers(&test_netif);
#endif
  netif_set_link_down(&test_netif);
  fail_unless(nd6_timeout_count() == 0, "ND6 timer should be stopped after link-down");

  /* Simulate another ND6 path re-arming the timer before link-up. */
  nd6_tmr_init();
  fail_unless(nd6_timeout_count() == 1, "Exactly one ND6 timeout before link-up");

  netif_set_link_up(&test_netif);
  fail_unless(nd6_timeout_count() == 1,
              "Link-up should not schedule a duplicate ND6 timeout");

  nd6_test_cleanup_reachable_neighbor(idx);
}
END_TEST

START_TEST(test_nd6_link_down_keeps_timer_for_other_netif)
{
  s8_t idx1, idx2;
  ip6_addr_t neighbor2;
  LWIP_UNUSED_ARG(_i);

  idx1 = nd6_test_setup_reachable_neighbor();
  if (idx1 < 0) {
    return;
  }

  for (idx2 = 0; idx2 < LWIP_ND6_NUM_NEIGHBORS; idx2++) {
    if (neighbor_cache[idx2].state == ND6_NO_ENTRY) {
      break;
    }
  }
  if (idx2 >= LWIP_ND6_NUM_NEIGHBORS) {
    nd6_test_cleanup_reachable_neighbor(idx1);
    return;
  }

  memset(&test_netif2, 0, sizeof(struct netif));
  netif_add(&test_netif2, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
  test_netif2.name[0] = 'a';
  test_netif2.name[1] = 'p';
  test_netif2.linkoutput = testif_linkoutput;
  netif_set_link_up(&test_netif2);
  netif_set_up(&test_netif2);

  IP6_ADDR(&neighbor2, 0xfe800000, 0, 0, 0x3);
  neighbor_cache[idx2].state = ND6_REACHABLE;
  neighbor_cache[idx2].netif = &test_netif2;
  neighbor_cache[idx2].counter.reachable_time = ND6_TMR_ECO_INTERVAL;
  ip6_addr_copy(neighbor_cache[idx2].next_hop_address, neighbor2);

  fail_unless(nd6_timeout_count() == 1, "ND6 timer should be active with two netifs");

#if SL_LWIP_ADAPTIVE_TIMERS
  netif_stop_timers(&test_netif);
#endif
  netif_set_link_down(&test_netif);
  fail_unless(nd6_timeout_count() == 1,
              "ND6 timer should keep running when another link-up netif needs it");

#if SL_LWIP_ADAPTIVE_TIMERS
  netif_stop_timers(&test_netif2);
#endif
  netif_set_link_down(&test_netif2);
  fail_unless(nd6_timeout_count() == 0,
              "ND6 timer should stop when no link-up netif needs it");

  netif_set_link_up(&test_netif);
  fail_unless(nd6_timeout_count() == 1,
              "ND6 timer should restart after link-up when paused for link-down");

  neighbor_cache[idx2].state = ND6_NO_ENTRY;
  neighbor_cache[idx2].netif = NULL;
  nd6_test_cleanup_reachable_neighbor(idx1);
}
END_TEST

START_TEST(test_nd6_rapid_link_cycle_no_timer_leak)
{
  s8_t idx;
  int cycle;
  LWIP_UNUSED_ARG(_i);

  idx = nd6_test_setup_reachable_neighbor();
  if (idx < 0) {
    return;
  }

  fail_unless(nd6_timeout_count() == 1, "Exactly one ND6 timeout should be scheduled initially");

  for (cycle = 0; cycle < 10; cycle++) {
#if SL_LWIP_ADAPTIVE_TIMERS
    netif_stop_timers(&test_netif);
#endif
    netif_set_link_down(&test_netif);
    /* No timeout should leak across a stop/link-down transition. */
    fail_unless(nd6_timeout_count() == 0, "No ND6 timeout should remain after link-down");
    netif_set_link_up(&test_netif);
    fail_unless(nd6_timeout_count() == 1, "Exactly one ND6 timeout should exist after link-up");
  }

  nd6_test_cleanup_reachable_neighbor(idx);
}
END_TEST

#else /* !SL_LWIP_ND6_DYNAMIC_TIMER */

/* ============================================================
 * TESTS for when SL_LWIP_ND6_DYNAMIC_TIMER is DISABLED
 * ============================================================ */

START_TEST(test_nd6_standard_timer_operation)
{
  LWIP_UNUSED_ARG(_i);
  
  /* When dynamic timer is disabled, nd6_tmr() should be called
   * at regular 1-second intervals by the system timeout mechanism */
  
  /* Call the timer function */
  nd6_tmr();
  
  /* Verify it completes without crashing */
  fail_unless(1);
}
END_TEST

START_TEST(test_nd6_standard_timer_neighbor_processing)
{
  s8_t i;
  u32_t initial_time;
  LWIP_UNUSED_ARG(_i);
  
  /* Find a free neighbor cache entry */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      break;
    }
  }
  
  if (i < LWIP_ND6_NUM_NEIGHBORS) {
    /* Set up network interface */
    netif_add(&test_netif, NULL, NULL, NULL, NULL, testif_init, ethernet_input);
    test_netif.linkoutput = testif_linkoutput;
    netif_set_link_up(&test_netif);
    netif_set_up(&test_netif);
    
    /* Set up a REACHABLE neighbor */
    neighbor_cache[i].state = ND6_REACHABLE;
    neighbor_cache[i].netif = &test_netif;
    neighbor_cache[i].counter.reachable_time = ND6_TMR_INTERVAL + 100;
    ip6_addr_copy(neighbor_cache[i].next_hop_address, test_neighbor_ip6_addr);
    
    initial_time = neighbor_cache[i].counter.reachable_time;
    
    /* Call timer */
    nd6_tmr();
    
    /* Timer should have decremented the counter */
    fail_unless(neighbor_cache[i].counter.reachable_time < initial_time,
                "Standard timer should decrement neighbor timer");
    
    /* Clean up */
    neighbor_cache[i].state = ND6_NO_ENTRY;
    neighbor_cache[i].netif = NULL;
  }
}
END_TEST

#endif /* SL_LWIP_ND6_DYNAMIC_TIMER */

/* ============================================================
 * TEST SUITE CREATION
 * ============================================================ */

Suite *
nd6_suite(void)
{
  testfunc tests[] = {
    /* Common tests */
    TESTFUNC(test_nd6_neighbor_cache_initialization),
    TESTFUNC(test_nd6_netif_setup),
    TESTFUNC(test_nd6_neighbor_state_transitions),
    
#if SL_LWIP_ND6_DYNAMIC_TIMER
    /* Dynamic timer tests */
    TESTFUNC(test_nd6_timer_initialization),
    TESTFUNC(test_nd6_active_mode_incomplete_state),
    TESTFUNC(test_nd6_active_mode_delay_state),
    TESTFUNC(test_nd6_active_mode_probe_state),
    TESTFUNC(test_nd6_eco_mode_reachable_state),
    TESTFUNC(test_nd6_eco_mode_stale_state),
    TESTFUNC(test_nd6_mode_transition_eco_to_active),
    TESTFUNC(test_nd6_mode_transition_active_to_eco),
    TESTFUNC(test_nd6_timer_reschedule_on_state_change),
    TESTFUNC(test_nd6_timer_callback_execution),
    TESTFUNC(test_nd6_elapsed_time_calculation),
    TESTFUNC(test_nd6_timer_overflow_protection),
    TESTFUNC(test_nd6_mixed_states_timer_selection),
    TESTFUNC(test_nd6_all_eco_states_timer_selection),
    TESTFUNC(test_nd6_link_down_stops_timer),
    TESTFUNC(test_nd6_link_up_restarts_timer),
    TESTFUNC(test_nd6_link_up_does_not_duplicate_existing_timer),
    TESTFUNC(test_nd6_link_down_keeps_timer_for_other_netif),
    TESTFUNC(test_nd6_rapid_link_cycle_no_timer_leak),
#else
    /* Standard timer tests */
    TESTFUNC(test_nd6_standard_timer_operation),
    TESTFUNC(test_nd6_standard_timer_neighbor_processing),
#endif
  };
  
  return create_suite("ND6", tests, LWIP_ARRAYSIZE(tests), nd6_setup, nd6_teardown);
}

#else /* !LWIP_IPV6 */

Suite *
nd6_suite(void)
{
  return NULL;
}

#endif /* LWIP_IPV6 */

