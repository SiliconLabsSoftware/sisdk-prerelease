#ifndef SL_PSA_TRUSTZONE_CONFIG_H
#define SL_PSA_TRUSTZONE_CONFIG_H

// -----------------------------------------------------------------------------
// User exposed config options

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Key management configuration

// <o SL_PSA_KEY_USER_SLOT_COUNT> PSA User Maximum Open Keys Count <0-128>
// <i> Maximum amount of keys that the user application will have open
// <i> simultaneously. In context of PSA Crypto, an open key means any key
// <i> either stored in RAM (lifetime set to PSA_KEY_LIFETIME_VOLATILE), or
// <i> used as part of a cryptographic operation.
// <i> When using a key for a multi-part (setup/update/finish) operation, a key
// <i> is considered to be open from the moment the operation is successfully
// <i> setup, until it finishes or aborts.
// <i> When an application tries to open more keys than this value accounts for,
// <i> the PSA API may return PSA_ERROR_INSUFFICIENT_MEMORY. Keep in mind that
// <i> other software included in the application (e.g. wireless protocol stacks)
// <i> also can have a need to have open keys in PSA Crypto. This could lead to
// <i> a race condition when the application key slot count is set too low for
// <i> the actual usage of the application, as a software stack may not fail
// <i> gracefully in case an application opens more than its declared amount of
// <i> keys, thereby precluding the stack from functioning.
// <i> Default: 4
#define SL_PSA_KEY_USER_SLOT_COUNT     (4)

// <o SL_PSA_ITS_USER_MAX_FILES> Maximum User Persistent PSA Key Count <0-1024>
// <i> Maximum amount of keys (or other files) that can be stored persistently
// <i> by the user application, when PSA ITS (Internal Trusted Storage) support
// <i> is included in the project.
// <i> NOTE:
// <i> In addition to SL_PSA_ITS_USER_MAX_FILES (number of user keys) the
// <i> application may be configured to include SDK components that require an
// <i> additional number of keys. The sum of user keys and SDK component keys
// <i> is computed and the result, called SL_PSA_ITS_MAX_FILES, is used
// <i> internally in the PSA ITS driver.
// <i>
// <i> WARNING:
// <i> For applications using PSA ITS driver version 1 or 2, when changing the
// <i> SL_PSA_ITS_USER_MAX_FILES in an application that is already depeloyed,
// <i> and thus will get the change through an application upgrade, care should
// <i> be taken to ensure that the total sum of keys SL_PSA_ITS_MAX_FILES is
// <i> only ever equal or increased, and never decreased. Decreasing this
// <i> setting might cause previously stored keys/files to become inaccessible,
// <i> ITS should be cleared and all files need to be stored again.
// <i>
// <i> For applications using PSA ITS driver version 3, it is not possible to
// <i> change this setting because the file-storage indexing is dependent on the
// <i> maximum number of files (SL_PSA_ITS_MAX_FILES) being consistent, and if
// <i> the sum of SDK component keys and user keys (SL_PSA_ITS_USER_MAX_FILES)
// <i> is changed, may cause previously stored keys/files to become inaccessible
// <i> ITS should be cleared and all files need to be stored again.
// <i> Default: 128
#define SL_PSA_ITS_USER_MAX_FILES           (128)

// <o SL_PSA_ITS_SUPPORT_V1_DRIVER> Enable V1 Format Support For ITS Files <0-1>
// <i> Devices that used PSA ITS together with gecko_sdk_3.1.x  or earlier
// <i> might have keys (or other files) stored in V1 format.
// <i> If no v1 files are used, its support can be disabled for space
// <i> optimization.
// <i> Default: 0
#define SL_PSA_ITS_SUPPORT_V1_DRIVER 0

// <o SL_PSA_ITS_SUPPORT_V2_DRIVER> Enable V2 ITS Driver Support <0-1>
// <i> Devices that have used GSDK 4.1.x and earlier, and used ITS have the keys
// <i> (or other files) stored using different address range. Enabling this
// <i> config option adds upgrade code which converts V2 (and V1 if
// <i> supported) format ITS keys/files to the latest V3 format. Update is
// <i> fully automatic, needs to be run once and require extra flash space of
// <i> approximately the size of the largest key.
// <i> V1 ITS driver support can be disabled if the device has never used ITS
// <i> driver before in GSDK 4.1.x and earlier, or the keys has been already
// <i> migrated.
// <i> Default: 0
#define SL_PSA_ITS_SUPPORT_V2_DRIVER 0

// <o SL_PSA_ITS_SUPPORT_V3_DRIVER> Enable support for V3 ITS Driver <0-1>
// <i> Devices that have used GSDK 4.1.x and earlier, and used ITS have the keys
// <i> (or other files) stored using different address range. In rare case
// <i> that those devices have full nvm3 and not enough space for the
// <i> upgrade, (that requires an extra space to store largest key in memory
// <i> twice), this config option can disable v3 driver and use v2 one.
// <i> To upgrade the device, make space for the upgrade, and enable v3 driver again.
// <i>
// <i> WARNING: When using V3 driver, it is not possible to increase or decrease
// <i> the value of SL_PSA_ITS_USER_MAX_FILES. If the change of
// <i> SL_PSA_ITS_USER_MAX_FILES is required, ITS should be cleared and
// <i> all files need to be stored again.
// <i> Default: 1
#define SL_PSA_ITS_SUPPORT_V3_DRIVER 1

// <o SL_SE_BUILTIN_KEY_AES128_ALG_CONFIG> Built-in AES Key Mode of Operation
// <PSA_ALG_CTR=> CTR Mode
// <PSA_ALG_CFB=> CFB Mode
// <PSA_ALG_OFB=> OFB Mode
// <PSA_ALG_ECB_NO_PADDING=> ECB Mode
// <PSA_ALG_CBC_NO_PADDING=> CBC Mode (no padding)
// <PSA_ALG_CBC_PKCS7=> CBC Mode (PKCS#7 padding)
// <i> PSA Crypto only allows one specific usage algorithm per built-in key ID.
// <i> Default: PSA_ALG_CTR
#define SL_SE_BUILTIN_KEY_AES128_ALG_CONFIG (PSA_ALG_CTR)

#ifndef SL_CRYPTOACC_BUILTIN_KEY_PUF_ALG
// <o SL_CRYPTOACC_BUILTIN_KEY_PUF_ALG> Built-in PUF Key Algorithm
// <PSA_ALG_SP800_108R1_CMAC=> SP 800-108r1 KDF (AES-CMAC, recommended)
// <PSA_ALG_CMAC=> CMAC
// <i> PSA Crypto only allows one specific usage algorithm per built-in key ID.
// <i> On VSE+PUF devices key derivation uses NIST SP 800-108r1.
// <i> Default: PSA_ALG_SP800_108R1_CMAC
#define SL_CRYPTOACC_BUILTIN_KEY_PUF_ALG  (PSA_ALG_SP800_108R1_CMAC)
#endif // SL_CRYPTOACC_BUILTIN_KEY_PUF_ALG

// </h>

// <h> Power optimization configuration

// <e SL_VSE_BUFFER_TRNG_DATA_DURING_SLEEP> Store already-generated random bytes before putting the device to sleep
// <i> Using the hardware TRNG (for example through psa_generate_random()) will
// <i> consume a non-negligible amount of power. A start-up routine must pass
// <i> and a relatively large minimum amount of random bytes will be generated.
// <i> Use cases where the device is frequently entering EM2/EM3 and thereafter
// <i> consumes a small amount of data from the TRNG may benefit from buffering
// <i> the existing random bytes before putting the device to sleep. These
// <i> buffered bytes are then consumed until exhaustion before the TRNG needs
// <i> to be initialized and used again.
// <i>
// <i> NOTE: this configuration option is only applicable for devices with a
// <i> Virtual Secure Engine (VSE), and requires the 'Power Manager' component
// <i> to be included in the project.
// <i>
// <i> Default: 0
#define SL_VSE_BUFFER_TRNG_DATA_DURING_SLEEP  (0)

// <o SL_VSE_MAX_TRNG_WORDS_BUFFERED_DURING_SLEEP> Number of random words to buffer before putting the device to sleep <1-63>
// <i> This option can be used to decrease the amount of random words that
// <i> (if enabled) are buffered before the device enters EM2/EM3. Lowering this
// <i> number will result in less static RAM usage, but also means that the TRNG
// <i> potentially has to be initialized more times--leading to increased power
// <i> consumption. By default this option in configured to buffer as much TRNG
// <i> data as possible (limited by the depth of the TRNG FIFO).
// <i>
// <i> NOTE: this configuration option is only applicable when
// <i> SL_VSE_BUFFER_TRNG_DATA_DURING_SLEEP is enabled.
// <i>
// <i> Default: 63
#define SL_VSE_MAX_TRNG_WORDS_BUFFERED_DURING_SLEEP (63)
// </e>

// </h>

// <h> Miscellaneous configuration

// <q SL_MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS> Assume all buffers passed to PSA functions are owned exclusively by the PSA function.
// <i> Default: 0
// <i> When this option is disabled, PSA functions will internally copy or stage
// <i> buffer arguments into memory regions under PSA’s exclusive control. This
// <i> ensures that untrusted code cannot access or modify buffers while a PSA
// <i> operation is in progress, providing stronger isolation and security across
// <i> trust boundaries (e.g. when using TrustZone).
// <i> The trade-off is additional memory use and allocation overhead, and potential
// <i> performance impact due to extra buffer copies.
// <i> This option SHOULD be disabled whenever buffer arguments might be located in
// <i> memory shared with untrusted code, or whenever PSA calls cross a trust
// <i> boundary and isolation is required.
#define SL_MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS 0

// <q SL_PSA_DRIVERS_ENABLED> Enable Silicon Labs' PSA Crypto drivers.
// <i> Default: 1
// <i> Enable PSA drivers for hardware acceleration and secure key handling.
// <i> This setting applies to the Secure application. Non-Secure always
// <i> reaches the drivers through the Secure Key Library.
#define SL_PSA_DRIVERS_ENABLED 1

// </h>

// <<< end of configuration section >>>

// -----------------------------------------------------------------------------
// TrustZone Non-Secure override

// Drivers and their headers belong to the Secure application. A Non-Secure
// application must not enable them, regardless of the setting above.
#if defined(SL_TRUSTZONE_NONSECURE)
  #undef SL_PSA_DRIVERS_ENABLED
  #define SL_PSA_DRIVERS_ENABLED 0
#endif

// -----------------------------------------------------------------------------
// Sub-files

#include "sli_mbedtls_omnipresent.h"

#if SL_PSA_DRIVERS_ENABLED
  #include "sli_psa_acceleration.h"
#endif

#include "sl_psa_driver_config.h"

#if defined(SLI_PSA_CONFIG_AUTOGEN_OVERRIDE_FILE)
  #include SLI_PSA_CONFIG_AUTOGEN_OVERRIDE_FILE
#else
  #include "sli_psa_config_autogen.h"
#endif

#if defined(TFM_CONFIG_SL_SECURE_LIBRARY)
  #include "sli_psa_tfm_translation.h"
#endif

#include "sli_psa_builtin_config_autogen.h"

// set MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS if SL_MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS is enabled
#if SL_MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS
  #define MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS
#endif

// -----------------------------------------------------------------------------
// Non-volatile seed function headers

#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT)

// Provide the NV seed function signatures since we have no specific header
// for them.

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int sli_nv_seed_read(unsigned char *buf, size_t buf_len);
int sli_nv_seed_write(unsigned char *buf, size_t buf_len);
#ifdef __cplusplus
}
#endif
#endif // MBEDTLS_PLATFORM_NV_SEED_ALT

// -----------------------------------------------------------------------------
// Platform macros

#if defined(MBEDTLS_PLATFORM_CALLOC_MACRO) && defined(MBEDTLS_PLATFORM_FREE_MACRO)

// By default MBEDTLS_PLATFORM_CALLOC_MACRO and MBEDTLS_PLATFORM_FREE_MACRO are
// defined in mbedtls_platform_dynamic_memory_allocation_config_default.slcc.
// Alternative implementations can configure MBEDTLS_PLATFORM_CALLOC_MACRO and
// MBEDTLS_PLATFORM_FREE_MACRO to use other platform specific implementations.
// Alternatively some use cases may select runtime initialisation in the
// application by explicitly calling mbedtls_platform_set_calloc_free() by
// selecting mbedtls_platform_dynamic_memory_allocation_config_init_runtime.

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
extern void *MBEDTLS_PLATFORM_CALLOC_MACRO(size_t n, size_t size);
extern void MBEDTLS_PLATFORM_FREE_MACRO(void *ptr);
#ifdef __cplusplus
}
#endif
#endif // MBEDTLS_PLATFORM_CALLOC_MACRO && MBEDTLS_PLATFORM_FREE_MACRO

#endif // PSA_CRYPTO_CONFIG_H
