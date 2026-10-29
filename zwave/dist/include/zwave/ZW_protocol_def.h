/**
 * @file ZW_protocol_def.h
 * @copyright 2026 Silicon Laboratories Inc.
 */
#ifndef ZWAVE_ZW_PROTOCOL_DEF_H_
#define ZWAVE_ZW_PROTOCOL_DEF_H_

/* Number of bytes in a homeID */
#define HOMEID_LENGTH      4

/* Max hops in route */
#define MAX_REPEATERS      4

/* predefined node ID's */
#define NODE_UNINIT         0x00    /* uninitialized slave node */
#define NODE_SLAVE_MIN      0x01    /* first slave node */
#define NODE_CONTROLLER     0x01    /* network controller node */
#define NODE_CONTROLLER_OLD 0xEF    /* pre 3.40 devkit NODE_CONTROLLER */
#define NODE_BROADCAST      0xFF    /* broadcast */
#define BEAM_BROADCAST      0xFF    /* broadcast address for wakeup beams */

/** Define Long Range Node ID ranges
 *  According to Z-Wave Long Range MAC Layer Specification (Release 5.5.0 - chapter 6.3.1.2) */
#define LOWEST_LONG_RANGE_NODE_ID           (0x0100)
#define HIGHEST_LONG_RANGE_NODE_ID          (0xFA0)
#define LOWEST_LONG_RANGE_RESERVED_NODE_ID  (HIGHEST_LONG_RANGE_NODE_ID + 1)
#define HIGHEST_LONG_RANGE_RESERVED_NODE_ID (0xFFE)
//node ID reserved for LR virtual node is as the beginning of reserved node ID range
#define LOWEST_LONG_RANGE_VIRTUAL_NODE_ID   (0xFA2)
#define HIGHEST_LONG_RANGE_VIRTUAL_NODE_ID  (0xFA5)
#define NODE_BROADCAST_LR                   0xFFF    /* broadcast LR*/
#define BEAM_BROADCAST_LR                   0xFFF    /* broadcast address for LR wakeup beams */

#endif /* ZWAVE_ZW_PROTOCOL_DEF_H_ */
