/**
 * @file
 * Defines a platform abstraction layer for the Z-Wave miscellaneous
 * functions, not covered by other modules.
 *
 * @copyright 2021 Silicon Laboratories Inc.
 */

#ifndef ZPAL_MISC_H_
#define ZPAL_MISC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup zpal
 * @brief
 * Z-Wave Platform Abstraction Layer.
 * @{
 * @addtogroup zpal-misc
 * @brief
 * Defines a platform abstraction layer for the Z-Wave miscellaneous
 * functions, not covered by other modules.
 *
 * @{
 */

/**
 * @brief Defines for identifying the secure element type supported by the chip.
 */
typedef enum {
  ZPAL_CHIP_SE_UNKNOWN,
  ZPAL_CHIP_SE_MID,
  ZPAL_CHIP_SE_HIGH
} zpal_chip_se_type_t;

/**
 * @brief Manufacturer's reset information
 */
typedef uint16_t zpal_soft_reset_info_t;

/**
 * @brief Manufacturer ID used by zpal_reboot_with_info.
 */
typedef uint16_t zpal_soft_reset_mfid_t;

/**
 * @brief type to store a time in zpal format. This format of this data depend on the zpal, so the stack
 * should never try to manipulate this data.
 * TODO: how to make this definition configurable by the zpal?
 */
typedef uint32_t zpal_time_t;

static const zpal_soft_reset_info_t ZPAL_RESET_REQUESTED_BY_SAPI     = 0x0000;
static const zpal_soft_reset_info_t ZPAL_RESET_UNHANDLED_RADIO_EVENT = 0x0001;
static const zpal_soft_reset_info_t ZPAL_RESET_RADIO_ASSERT          = 0x0002;
static const zpal_soft_reset_info_t ZPAL_RESET_ASSERT_PTR            = 0x0003;
static const zpal_soft_reset_info_t ZPAL_RESET_EVENT_FLUSH_MEMORY    = 0x0004;
static const zpal_soft_reset_info_t ZPAL_RESET_RADIO_RECONFIGURE     = 0x0005;
static const zpal_soft_reset_info_t ZPAL_RESET_INFO_DEFAULT          = 0xFFFF;

/**
 * @brief Perform a system reboot and provide information about the context.
 *
 * @param[in] manufacturer_id manufacturer ID identifier.
 * @param[in] reset_info the information to pass to boot.
 */
void zpal_reboot_with_info(const zpal_soft_reset_mfid_t manufacturer_id,
                           const zpal_soft_reset_info_t reset_info);

/**
 * @brief Get serial number length.
 *
 * @return Serial number length.
 */
size_t zpal_get_serial_number_length(void);

/**
 * @brief Get serial number.
 *
 * @param[out] serial_number Serial number.
 */
void zpal_get_serial_number(uint8_t *serial_number);

/**
 * @brief Check if in ISR context.
 *
 * @return True if the CPU is in handler mode (currently executing an interrupt handler).
 *         False if the CPU is in thread mode.
 */
bool zpal_in_isr(void);

/**
 * @brief Get chip type.
 *
 * @return Chip type.
 */
uint8_t zpal_get_chip_type(void);

/**
 * @brief Get chip revision.
 *
 * @return Chip revision.
 */
uint8_t zpal_get_chip_revision(void);

/**
 * @brief Initialize debug output.
 */
void zpal_debug_init(void);

/**
 * @brief Output debug logs.
 *
 * @param[out] data   Pointer to debug data.
 * @param[in]  length Length of debug data.
 */
void zpal_debug_output(const uint8_t *data, uint32_t length);

/**
 * @brief Disable interrupts.
 */
void zpal_disable_interrupts(void);

/**
 * @brief Get secure element type supported in the chip.
 *
 * @return Secure element type supported.
 *
 */
zpal_chip_se_type_t zpal_get_secure_element_type(void);

/**
 * @brief Set vendor specific location for storing
 * keys persistently in wrapped or plain form based
 * on the secure element type supported by the chip.
 *
 * @param[in] attributes of the key
 */
void zpal_psa_set_location_persistent_key(const void *attributes);

/**
 * @brief Set vendor specific location for storing
 * keys in volatile memory, in wrapped or plain form based
 * on the secure element type supported by the chip.
 *
 * @param[in] attributes of the key
 */
void zpal_psa_set_location_volatile_key(const void *attributes);

/**
 * @} //zpal-misc
 * @} //zpal
 */

#ifdef __cplusplus
}
#endif

#endif /* ZPAL_MISC_H_ */
