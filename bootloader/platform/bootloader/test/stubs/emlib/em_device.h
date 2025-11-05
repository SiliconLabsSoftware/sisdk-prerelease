#ifndef EM_DEVICE_H
#define EM_DEVICE_H

#if !defined(BOOTLOADER_ON_HOST)
#define SRAM_BASE                                         (0x20000000UL) /**< SRAM Base Address */
#define SRAM_SIZE                                         (0x00010000UL) /**< Available SRAM Memory */

#define FLASH_PAGE_SIZE 0x200
#define FLASH_BASE 0
#define FLASH_SIZE 0x1000000
#endif

  #ifndef   __ASM
    #define __ASM                  (void)
  #endif
  #ifndef   __INLINE
    #define __INLINE               inline
  #endif
  #ifndef   __STATIC_INLINE
    #define __STATIC_INLINE        static inline
  #endif

  #ifndef SL_WEAK
    #define SL_WEAK __attribute__ ((weak))
  #endif

#endif
