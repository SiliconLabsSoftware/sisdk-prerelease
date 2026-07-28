/***************************************************************************//**
 * @file
 * @brief See @ref diagnostics for detailed documentation.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup legacyhal
 *  @{
 ******************************************************************************/

/** @addtogroup diagnostics Diagnostics
 * @brief Crash and watchdog diagnostic functions.
 *
 * See diagnostic.h for source code.
 *  @{
 */

#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

/// @brief Define the reset reasons that should print out detailed crash data.
#define RESET_CRASH_REASON_MASK ((1 << RESET_UNKNOWN)    \
                                 | (1 << RESET_WATCHDOG) \
                                 | (1 << RESET_CRASH)    \
                                 | (1 << RESET_FLASH)    \
                                 | (1 << RESET_FAULT)    \
                                 | (1 << RESET_FATAL))

/// @brief Define Hal assert information type
typedef struct {
  const char * file;                               ///< file
  uint32_t     line;                               ///< line
} HalAssertInfoType;

/// @brief note that assertInfo and dmaProt are written just before a forced reboot
typedef union {
  HalAssertInfoType assertInfo;                    ///< assertInfo
  struct { uint32_t channel; uint32_t address; } dmaProt;                                            ///< dmaProt
} HalCrashSpecificDataType;

/// @brief Define crash registers as structs so a debugger can display their bit fields
typedef union {
  struct {
    uint32_t EXCPT          : 9;  // B0-8
    uint32_t ICIIT_LOW      : 7;  // B9-15
    uint32_t                : 8;  // B16-23
    uint32_t T              : 1;  // B24
    uint32_t ICIIT_HIGH     : 2;  // B25-26
    uint32_t Q              : 1;  // B27
    uint32_t V              : 1;  // B28
    uint32_t C              : 1;  // B29
    uint32_t Z              : 1;  // B30
    uint32_t N              : 1;  // B31
  } bits;                                          ///< bits

  uint32_t word;                                   ///< word
} HalCrashxPsrType;

/// @brief  Define crash registers as structs so a debugger can display their bit fields
typedef union {
  struct {
    uint32_t VECTACTIVE     : 9;  // B0-8
    uint32_t                : 2;  // B9-10
    uint32_t RETTOBASE      : 1;  // B11
    uint32_t VECTPENDING    : 9;  // B12-20
    uint32_t                : 1;  // B21
    uint32_t ISRPENDING     : 1;  // B22
    uint32_t ISRPREEMPT     : 1;  // B23
    uint32_t                : 1;  // B24
    uint32_t PENDSTCLR      : 1;  // B25
    uint32_t PENDSTSET      : 1;  // B26
    uint32_t PENDSVCLR      : 1;  // B27
    uint32_t PENDSVSET      : 1;  // B28
    uint32_t                : 2;  // B29-30
    uint32_t NMIPENDSET     : 1;  // B31
  } bits;                                          ///< bits

  uint32_t word;                                   ///< word
} HalCrashIcsrType;

typedef union {
  struct {
#if defined (_SILICON_LABS_32B_SERIES_1_CONFIG_1)
    uint32_t EMU_IRQn         : 1;  // B0
    uint32_t FRC_PRI_IRQn     : 1;  // B1
    uint32_t WDOG0_IRQn       : 1;  // B2
    uint32_t FRC_IRQn         : 1;  // B3
    uint32_t MODEM_IRQn       : 1;  // B4
    uint32_t RAC_SEQ_IRQn     : 1;  // B5
    uint32_t RAC_RSM_IRQn     : 1;  // B6
    uint32_t BUFC_IRQn        : 1;  // B7
    uint32_t LDMA_IRQn        : 1;  // B8
    uint32_t GPIO_EVEN_IRQn   : 1;  // B9
    uint32_t TIMER0_IRQn      : 1;  // B10
    uint32_t USART0_RX_IRQn   : 1;  // B11
    uint32_t USART0_TX_IRQn   : 1;  // B12
    uint32_t ACMP0_IRQn       : 1;  // B13
    uint32_t ADC0_IRQn        : 1;  // B14
    uint32_t IDAC0_IRQn       : 1;  // B15
    uint32_t I2C0_IRQn        : 1;  // B16
    uint32_t GPIO_ODD_IRQn    : 1;  // B17
    uint32_t TIMER1_IRQn      : 1;  // B18
    uint32_t USART1_RX_IRQn   : 1;  // B19
    uint32_t USART1_TX_IRQn   : 1;  // B20
    uint32_t LEUART0_IRQn     : 1;  // B21
    uint32_t PCNT0_IRQn       : 1;  // B22
    uint32_t CMU_IRQn         : 1;  // B23
    uint32_t MSC_IRQn         : 1;  // B24
    uint32_t CRYPTO_IRQn      : 1;  // B25
    uint32_t LETIMER0_IRQn    : 1;  // B26
    uint32_t AGC_IRQn         : 1;  // B27
    uint32_t PROTIMER_IRQn    : 1;  // B28
    uint32_t RTCC_IRQn        : 1;  // B29
    uint32_t SYNTH_IRQn       : 1;  // B30
    uint32_t CRYOTIMER_IRQn   : 1;  // B31
    uint32_t RFSENSE_IRQn     : 1;  // B32
    uint32_t FPUEH_IRQn       : 1;  // B33
    uint32_t                : 30; // B34-63
  } bits;
  uint32_t word[2];
#elif defined (_SILICON_LABS_32B_SERIES_1_CONFIG_2)
    uint32_t EMU_IRQn         : 1;  // B0
    uint32_t FRC_PRI_IRQn     : 1;  // B1
    uint32_t WDOG0_IRQn       : 1;  // B2
    uint32_t WDOG1_IRQn       : 1;  // B3
    uint32_t FRC_IRQn         : 1;  // B4
    uint32_t MODEM_IRQn       : 1;  // B5
    uint32_t RAC_SEQ_IRQn     : 1;  // B6
    uint32_t RAC_RSM_IRQn     : 1;  // B7
    uint32_t BUFC_IRQn        : 1;  // B8
    uint32_t LDMA_IRQn        : 1;  // B9
    uint32_t GPIO_EVEN_IRQn   : 1;  // B10
    uint32_t TIMER0_IRQn      : 1;  // B11
    uint32_t USART0_RX_IRQn   : 1;  // B12
    uint32_t USART0_TX_IRQn   : 1;  // B13
    uint32_t ACMP0_IRQn       : 1;  // B14
    uint32_t ADC0_IRQn        : 1;  // B15
    uint32_t IDAC0_IRQn       : 1;  // B16
    uint32_t I2C0_IRQn        : 1;  // B17
    uint32_t GPIO_ODD_IRQn    : 1;  // B18
    uint32_t TIMER1_IRQn      : 1;  // B19
    uint32_t USART1_RX_IRQn   : 1;  // B20
    uint32_t USART1_TX_IRQn   : 1;  // B21
    uint32_t LEUART0_IRQn     : 1;  // B22
    uint32_t PCNT0_IRQn       : 1;  // B23
    uint32_t CMU_IRQn         : 1;  // B24
    uint32_t MSC_IRQn         : 1;  // B25
    uint32_t CRYPTO0_IRQn     : 1;  // B26
    uint32_t LETIMER0_IRQn    : 1;  // B27
    uint32_t AGC_IRQn         : 1;  // B28
    uint32_t PROTIMER_IRQn    : 1;  // B29
    uint32_t RTCC_IRQn        : 1;  // B30
    uint32_t SYNTH_IRQn       : 1;  // B31
    uint32_t CRYOTIMER_IRQn   : 1;  // B32
    uint32_t RFSENSE_IRQn     : 1;  // B33
    uint32_t FPUEH_IRQn       : 1;  // B34
    uint32_t SMU_IRQn         : 1;  // B35
    uint32_t WTIMER0_IRQn     : 1;  // B36
    uint32_t WTIMER1_IRQn     : 1;  // B37
    uint32_t PCNT1_IRQn       : 1;  // B38
    uint32_t PCNT2_IRQn       : 1;  // B39
    uint32_t USART2_RX_IRQn   : 1;  // B40
    uint32_t USART2_TX_IRQn   : 1;  // B41
    uint32_t I2C1_IRQn        : 1;  // B42
    uint32_t USART3_RX_IRQn   : 1;  // B43
    uint32_t USART3_TX_IRQn   : 1;  // B44
    uint32_t VDAC0_IRQn       : 1;  // B45
    uint32_t CSEN_IRQn        : 1;  // B46
    uint32_t LESENSE_IRQn     : 1;  // B47
    uint32_t CRYPTO1_IRQn     : 1;  // B48
    uint32_t TRNG0_IRQn       : 1;  // B49
    uint32_t                : 14; // B50-63
  } bits;
  uint32_t word[2];
#elif defined (_SILICON_LABS_32B_SERIES_1_CONFIG_3)
    uint32_t EMU_IRQn         : 1;  // B0
    uint32_t FRC_PRI_IRQn     : 1;  // B1
    uint32_t WDOG0_IRQn       : 1;  // B2
    uint32_t WDOG1_IRQn       : 1;  // B3
    uint32_t FRC_IRQn         : 1;  // B4
    uint32_t MODEM_IRQn       : 1;  // B5
    uint32_t RAC_SEQ_IRQn     : 1;  // B6
    uint32_t RAC_RSM_IRQn     : 1;  // B7
    uint32_t BUFC_IRQn        : 1;  // B8
    uint32_t LDMA_IRQn        : 1;  // B9
    uint32_t GPIO_EVEN_IRQn   : 1;  // B10
    uint32_t TIMER0_IRQn      : 1;  // B11
    uint32_t USART0_RX_IRQn   : 1;  // B12
    uint32_t USART0_TX_IRQn   : 1;  // B13
    uint32_t ACMP0_IRQn       : 1;  // B14
    uint32_t ADC0_IRQn        : 1;  // B15
    uint32_t IDAC0_IRQn       : 1;  // B16
    uint32_t I2C0_IRQn        : 1;  // B17
    uint32_t GPIO_ODD_IRQn    : 1;  // B18
    uint32_t TIMER1_IRQn      : 1;  // B19
    uint32_t USART1_RX_IRQn   : 1;  // B20
    uint32_t USART1_TX_IRQn   : 1;  // B21
    uint32_t LEUART0_IRQn     : 1;  // B22
    uint32_t PCNT0_IRQn       : 1;  // B23
    uint32_t CMU_IRQn         : 1;  // B24
    uint32_t MSC_IRQn         : 1;  // B25
    uint32_t CRYPTO0_IRQn     : 1;  // B26
    uint32_t LETIMER0_IRQn    : 1;  // B27
    uint32_t AGC_IRQn         : 1;  // B28
    uint32_t PROTIMER_IRQn    : 1;  // B29
    uint32_t PRORTC_IRQn      : 1;  // B30
    uint32_t RTCC_IRQn        : 1;  // B31
    uint32_t SYNTH_IRQn       : 1;  // B32
    uint32_t CRYOTIMER_IRQn   : 1;  // B33
    uint32_t RFSENSE_IRQn     : 1;  // B34
    uint32_t FPUEH_IRQn       : 1;  // B35
    uint32_t SMU_IRQn         : 1;  // B36
    uint32_t WTIMER0_IRQn     : 1;  // B37
    uint32_t USART2_RX_IRQn   : 1;  // B38
    uint32_t USART2_TX_IRQn   : 1;  // B39
    uint32_t I2C1_IRQn        : 1;  // B40
    uint32_t VDAC0_IRQn       : 1;  // B41
    uint32_t CSEN_IRQn        : 1;  // B42
    uint32_t LESENSE_IRQn     : 1;  // B43
    uint32_t CRYPTO1_IRQn     : 1;  // B44
    uint32_t TRNG0_IRQn       : 1;  // B45
    uint32_t                : 18; // B46-63
  } bits;
  uint32_t word[2];
#elif defined (_SILICON_LABS_32B_SERIES_1_CONFIG_4)
    uint32_t EMU_IRQn         : 1;  // B0
    uint32_t FRC_PRI_IRQn     : 1;  // B1
    uint32_t WDOG0_IRQn       : 1;  // B2
    uint32_t WDOG1_IRQn       : 1;  // B3
    uint32_t FRC_IRQn         : 1;  // B4
    uint32_t MODEM_IRQn       : 1;  // B5
    uint32_t RAC_SEQ_IRQn     : 1;  // B6
    uint32_t RAC_RSM_IRQn     : 1;  // B7
    uint32_t BUFC_IRQn        : 1;  // B8
    uint32_t LDMA_IRQn        : 1;  // B9
    uint32_t GPIO_EVEN_IRQn   : 1;  // B10
    uint32_t TIMER0_IRQn      : 1;  // B11
    uint32_t USART0_RX_IRQn   : 1;  // B12
    uint32_t USART0_TX_IRQn   : 1;  // B13
    uint32_t ACMP0_IRQn       : 1;  // B14
    uint32_t ADC0_IRQn        : 1;  // B15
    uint32_t IDAC0_IRQn       : 1;  // B16
    uint32_t I2C0_IRQn        : 1;  // B17
    uint32_t GPIO_ODD_IRQn    : 1;  // B18
    uint32_t TIMER1_IRQn      : 1;  // B19
    uint32_t USART1_RX_IRQn   : 1;  // B20
    uint32_t USART1_TX_IRQn   : 1;  // B21
    uint32_t LEUART0_IRQn     : 1;  // B22
    uint32_t PCNT0_IRQn       : 1;  // B23
    uint32_t CMU_IRQn         : 1;  // B24
    uint32_t MSC_IRQn         : 1;  // B25
    uint32_t CRYPTO0_IRQn     : 1;  // B26
    uint32_t LETIMER0_IRQn    : 1;  // B27
    uint32_t AGC_IRQn         : 1;  // B28
    uint32_t PROTIMER_IRQn    : 1;  // B29
    uint32_t PRORTC_IRQn      : 1;  // B30
    uint32_t RTCC_IRQn        : 1;  // B31
    uint32_t SYNTH_IRQn       : 1;  // B32
    uint32_t CRYOTIMER_IRQn   : 1;  // B33
    uint32_t RFSENSE_IRQn     : 1;  // B34
    uint32_t FPUEH_IRQn       : 1;  // B35
    uint32_t SMU_IRQn         : 1;  // B36
    uint32_t WTIMER0_IRQn     : 1;  // B37
    uint32_t VDAC0_IRQn       : 1;  // B38
    uint32_t LESENSE_IRQn     : 1;  // B39
    uint32_t TRNG0_IRQn       : 1;  // B40
    uint32_t SYSCFG_IRQn      : 1;  // B41
    uint32_t                : 22; // B42-63
  } bits;
  uint32_t word[2];
#elif defined (_SILICON_LABS_32B_SERIES_2_CONFIG_1)
    uint32_t SETAMPERHOST_IRQn     : 1;  // B0
    uint32_t SEMBRX_IRQn           : 1;  // B1
    uint32_t SEMBTX_IRQn           : 1;  // B2
    uint32_t SMU_SECURE_IRQn       : 1;  // B3
    uint32_t SMU_PRIVILEGED_IRQn   : 1;  // B4
    uint32_t EMU_IRQn              : 1;  // B5
    uint32_t TIMER0_IRQn           : 1;  // B6
    uint32_t TIMER1_IRQn           : 1;  // B7
    uint32_t TIMER2_IRQn           : 1;  // B8
    uint32_t TIMER3_IRQn           : 1;  // B9
    uint32_t RTCC_IRQn             : 1;  // B10
    uint32_t USART0_RX_IRQn        : 1;  // B11
    uint32_t USART0_TX_IRQn        : 1;  // B12
    uint32_t USART1_RX_IRQn        : 1;  // B13
    uint32_t USART1_TX_IRQn        : 1;  // B14
    uint32_t USART2_RX_IRQn        : 1;  // B15
    uint32_t USART2_TX_IRQn        : 1;  // B16
    uint32_t ICACHE0_IRQn          : 1;  // B17
    uint32_t BURTC_IRQn            : 1;  // B18
    uint32_t LETIMER0_IRQn         : 1;  // B19
    uint32_t SYSCFG_IRQn           : 1;  // B20
    uint32_t LDMA_IRQn             : 1;  // B21
    uint32_t LFXO_IRQn             : 1;  // B22
    uint32_t LFRCO_IRQn            : 1;  // B23
    uint32_t ULFRCO_IRQn           : 1;  // B24
    uint32_t GPIO_ODD_IRQn         : 1;  // B25
    uint32_t GPIO_EVEN_IRQn        : 1;  // B26
    uint32_t I2C0_IRQn             : 1;  // B27
    uint32_t I2C1_IRQn             : 1;  // B28
    uint32_t EMUDG_IRQn            : 1;  // B29
    uint32_t EMUSE_IRQn            : 1;  // B30
    uint32_t AGC_IRQn              : 1;  // B31
    uint32_t BUFC_IRQn             : 1;  // B32
    uint32_t FRC_PRI_IRQn          : 1;  // B33
    uint32_t FRC_IRQn              : 1;  // B34
    uint32_t MODEM_IRQn            : 1;  // B35
    uint32_t PROTIMER_IRQn         : 1;  // B36
    uint32_t RAC_RSM_IRQn          : 1;  // B37
    uint32_t RAC_SEQ_IRQn          : 1;  // B38
    uint32_t PRORTC_IRQn           : 1;  // B39
    uint32_t SYNTH_IRQn            : 1;  // B40
    uint32_t ACMP0_IRQn            : 1;  // B41
    uint32_t ACMP1_IRQn            : 1;  // B42
    uint32_t WDOG0_IRQn            : 1;  // B43
    uint32_t WDOG1_IRQn            : 1;  // B44
    uint32_t HFXO00_IRQn           : 1;  // B45
    uint32_t HFRCO0_IRQn           : 1;  // B46
    uint32_t HFRCOEM23_IRQn        : 1;  // B47
    uint32_t CMU_IRQn              : 1;  // B48
    uint32_t AES_IRQn              : 1;  // B49
    uint32_t IADC_IRQn             : 1;  // B50
    uint32_t MSC_IRQn              : 1;  // B51
    uint32_t DPLL0_IRQn            : 1;  // B52
    uint32_t SW0_IRQn              : 1;  // B53
    uint32_t SW1_IRQn              : 1;  // B54
    uint32_t SW2_IRQn              : 1;  // B55
    uint32_t SW3_IRQn              : 1;  // B56
    uint32_t KERNEL0_IRQn          : 1;  // B57
    uint32_t KERNEL1_IRQn          : 1;  // B58
    uint32_t M33CTI0_IRQn          : 1;  // B59
    uint32_t M33CTI1_IRQn          : 1;  // B60
    uint32_t                       : 3;  // B61-63
  } bits;
  uint32_t word[2];
#elif defined (_SILICON_LABS_32B_SERIES_2_CONFIG_2)
    uint32_t CRYPTOACC_IRQn         : 1;  // B0
    uint32_t TRNG_IRQn              : 1;  // B1
    uint32_t PKE_IRQn               : 1;  // B2
    uint32_t SMU_SECURE_IRQn        : 1;  // B3
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1;  // B4
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1;  // B5
    uint32_t EMU_IRQn               : 1;  // B6
    uint32_t TIMER0_IRQn            : 1;  // B7
    uint32_t TIMER1_IRQn            : 1;  // B8
    uint32_t TIMER2_IRQn            : 1;  // B9
    uint32_t TIMER3_IRQn            : 1;  // B10
    uint32_t TIMER4_IRQn            : 1;  // B11
    uint32_t RTCC_IRQn              : 1;  // B12
    uint32_t USART0_RX_IRQn         : 1;  // B13
    uint32_t USART0_TX_IRQn         : 1;  // B14
    uint32_t USART1_RX_IRQn         : 1;  // B15
    uint32_t USART1_TX_IRQn         : 1;  // B16
    uint32_t ICACHE0_IRQn           : 1;  // B17
    uint32_t BURTC_IRQn             : 1;  // B18
    uint32_t LETIMER0_IRQn          : 1;  // B19
    uint32_t SYSCFG_IRQn            : 1;  // B20
    uint32_t LDMA_IRQn              : 1;  // B21
    uint32_t LFXO_IRQn              : 1;  // B22
    uint32_t LFRCO_IRQn             : 1;  // B23
    uint32_t ULFRCO_IRQn            : 1;  // B24
    uint32_t GPIO_ODD_IRQn          : 1;  // B25
    uint32_t GPIO_EVEN_IRQn         : 1;  // B26
    uint32_t I2C0_IRQn              : 1;  // B27
    uint32_t I2C1_IRQn              : 1;  // B28
    uint32_t EMUDG_IRQn             : 1;  // B29
    uint32_t EMUSE_IRQn             : 1;  // B30
    uint32_t AGC_IRQn               : 1;  // B31
    uint32_t BUFC_IRQn              : 1;  // B32
    uint32_t FRC_PRI_IRQn           : 1;  // B33
    uint32_t FRC_IRQn               : 1;  // B34
    uint32_t MODEM_IRQn             : 1;  // B35
    uint32_t PROTIMER_IRQn          : 1;  // B36
    uint32_t RAC_RSM_IRQn           : 1;  // B37
    uint32_t RAC_SEQ_IRQn           : 1;  // B38
    uint32_t RDMAILBOX_IRQn         : 1;  // B39
    uint32_t RFSENSE_IRQn           : 1;  // B40
    uint32_t PRORTC_IRQn            : 1;  // B41
    uint32_t SYNTH_IRQn             : 1;  // B42
    uint32_t WDOG0_IRQn             : 1;  // B43
    uint32_t HFXO0_IRQn             : 1;  // B44
    uint32_t HFRCO0_IRQn            : 1;  // B45
    uint32_t CMU_IRQn               : 1;  // B46
    uint32_t AES_IRQn               : 1;  // B47
    uint32_t IADC_IRQn              : 1;  // B48
    uint32_t MSC_IRQn               : 1;  // B49
    uint32_t DPLL0_IRQn             : 1;  // B50
    uint32_t PDM_IRQn               : 1;  // B51
    uint32_t SW0_IRQn               : 1;  // B52
    uint32_t SW1_IRQn               : 1;  // B53
    uint32_t SW2_IRQn               : 1;  // B54
    uint32_t SW3_IRQn               : 1;  // B55
    uint32_t KERNEL0_IRQn           : 1;  // B56
    uint32_t KERNEL1_IRQn           : 1;  // B57
    uint32_t M33CTI0_IRQn           : 1;  // B58
    uint32_t M33CTI1_IRQn           : 1;  // B59
    uint32_t EMUEFP_IRQn            : 1;  // B60
    uint32_t DCDC_IRQn              : 1;  // B61
    uint32_t EUART0_RX_IRQn         : 1;  // B62
    uint32_t EUART0_TX_IRQn         : 1;  // B63
  } bits;
  uint32_t word[2];
#elif defined(_SILICON_LABS_32B_SERIES_2_CONFIG_3)
    uint32_t SMU_SECURE_IRQn        : 1; // B0
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1; // B1
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1; // B2
    uint32_t EMU_IRQn               : 1; // B3
    uint32_t TIMER0_IRQn            : 1; // B4
    uint32_t TIMER1_IRQn            : 1; // B5
    uint32_t TIMER2_IRQn            : 1; // B6
    uint32_t TIMER3_IRQn            : 1; // B7
    uint32_t TIMER4_IRQn            : 1; // B8
    uint32_t USART0_RX_IRQn         : 1; // B9
    uint32_t USART0_TX_IRQn         : 1; // B10
    uint32_t EUSART0_RX_IRQn        : 1; // B11
    uint32_t EUSART0_TX_IRQn        : 1; // B12
    uint32_t EUSART1_RX_IRQn        : 1; // B13
    uint32_t EUSART1_TX_IRQn        : 1; // B14
    uint32_t EUSART2_RX_IRQn        : 1; // B15
    uint32_t EUSART2_TX_IRQn        : 1; // B16
    uint32_t ICACHE0_IRQn           : 1; // B17
    uint32_t BURTC_IRQn             : 1; // B18
    uint32_t LETIMER0_IRQn          : 1; // B19
    uint32_t SYSCFG_IRQn            : 1; // B20
    uint32_t MPAHBRAM_IRQn          : 1; // B21
    uint32_t LDMA_IRQn              : 1; // B22
    uint32_t LFXO_IRQn              : 1; // B23
    uint32_t LFRCO_IRQn             : 1; // B24
    uint32_t ULFRCO_IRQn            : 1; // B25
    uint32_t GPIO_ODD_IRQn          : 1; // B26
    uint32_t GPIO_EVEN_IRQn         : 1; // B27
    uint32_t I2C0_IRQn              : 1; // B28
    uint32_t I2C1_IRQn              : 1; // B29
    uint32_t EMUDG_IRQn             : 1; // B30
    uint32_t AGC_IRQn               : 1; // B31
    uint32_t BUFC_IRQn              : 1; // B32
    uint32_t FRC_PRI_IRQn           : 1; // B33
    uint32_t FRC_IRQn               : 1; // B34
    uint32_t MODEM_IRQn             : 1; // B35
    uint32_t PROTIMER_IRQn          : 1; // B36
    uint32_t RAC_RSM_IRQn           : 1; // B37
    uint32_t RAC_SEQ_IRQn           : 1; // B38
    uint32_t HOSTMAILBOX_IRQn       : 1; // B39
    uint32_t SYNTH_IRQn             : 1; // B40
    uint32_t ACMP0_IRQn             : 1; // B41
    uint32_t ACMP1_IRQn             : 1; // B42
    uint32_t WDOG0_IRQn             : 1; // B43
    uint32_t WDOG1_IRQn             : 1; // B44
    uint32_t HFXO0_IRQn             : 1; // B45
    uint32_t HFRCO0_IRQn            : 1; // B46
    uint32_t HFRCOEM23_IRQn         : 1; // B47
    uint32_t CMU_IRQn               : 1; // B48
    uint32_t AES_IRQn               : 1; // B49
    uint32_t IADC_IRQn              : 1; // B50
    uint32_t MSC_IRQn               : 1; // B51
    uint32_t DPLL0_IRQn             : 1; // B52
    uint32_t EMUEFP_IRQn            : 1; // B53
    uint32_t DCDC_IRQn              : 1; // B54
    uint32_t VDAC_IRQn              : 1; // B55
    uint32_t PCNT0_IRQn             : 1; // B56
    uint32_t SW0_IRQn               : 1; // B57
    uint32_t SW1_IRQn               : 1; // B58
    uint32_t SW2_IRQn               : 1; // B59
    uint32_t SW3_IRQn               : 1; // B60
    uint32_t KERNEL0_IRQn           : 1; // B61
    uint32_t KERNEL1_IRQn           : 1; // B62
    uint32_t M33CTI0_IRQn           : 1; // B63
    uint32_t M33CTI1_IRQn           : 1; // B64
    uint32_t FPUEXH_IRQn            : 1; // B65
    uint32_t SEMBRX_IRQn            : 1; // B67
    uint32_t SEMBTX_IRQn            : 1; // B68
    uint32_t LESENSE_IRQn           : 1; // B69
    uint32_t SYSRTC_APP_IRQn        : 1; // B70
    uint32_t SYSRTC_SEQ_IRQn        : 1; // B71
    uint32_t LCD_IRQn               : 1; // B72
    uint32_t KEYSCAN_IRQn           : 1; // B73
    uint32_t RFECA0_IRQn            : 1; // B74
    uint32_t RFECA1_IRQn            : 1; // B75
    uint32_t                : 20; // B76-95
  } bits;
  uint32_t word[3];
#elif defined (_SILICON_LABS_32B_SERIES_2_CONFIG_4)
    uint32_t SMU_SECURE_IRQn        : 1;  // B0
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1;  // B1
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1;  // B2
    uint32_t EMU_IRQn               : 1;  // B3
    uint32_t TIMER0_IRQn            : 1;  // B4
    uint32_t TIMER1_IRQn            : 1;  // B5
    uint32_t TIMER2_IRQn            : 1;  // B6
    uint32_t TIMER3_IRQn            : 1;  // B7
    uint32_t TIMER4_IRQn            : 1;  // B8
    uint32_t USART0_RX_IRQn         : 1;  // B9
    uint32_t USART0_TX_IRQn         : 1;  // B10
    uint32_t EUSART0_RX_IRQn        : 1;  // B11
    uint32_t EUSART0_TX_IRQn        : 1;  // B12
    uint32_t EUSART1_RX_IRQn        : 1;  // B13
    uint32_t EUSART1_TX_IRQn        : 1;  // B14
    uint32_t MVP_IRQn               : 1;  // B15
    uint32_t ICACHE0_IRQn           : 1;  // B16
    uint32_t BURTC_IRQn             : 1;  // B17
    uint32_t LETIMER0_IRQn          : 1;  // B18
    uint32_t SYSCFG_IRQn            : 1;  // B19
    uint32_t MPAHBRAM_IRQn          : 1;  // B20
    uint32_t LDMA_IRQn              : 1;  // B21
    uint32_t LFXO_IRQn              : 1;  // B22
    uint32_t LFRCO_IRQn             : 1;  // B23
    uint32_t ULFRCO_IRQn            : 1;  // B24
    uint32_t GPIO_ODD_IRQn          : 1;  // B25
    uint32_t GPIO_EVEN_IRQn         : 1;  // B26
    uint32_t I2C0_IRQn              : 1;  // B27
    uint32_t I2C1_IRQn              : 1;  // B28
    uint32_t EMUDG_IRQn             : 1;  // B29
    uint32_t AGC_IRQn               : 1;  // B30
    uint32_t BUFC_IRQn              : 1;  // B31
    uint32_t FRC_PRI_IRQn           : 1;  // B32
    uint32_t FRC_IRQn               : 1;  // B33
    uint32_t MODEM_IRQn             : 1;  // B34
    uint32_t PROTIMER_IRQn          : 1;  // B35
    uint32_t RAC_RSM_IRQn           : 1;  // B36
    uint32_t RAC_SEQ_IRQn           : 1;  // B37
    uint32_t HOSTMAILBOX_IRQn       : 1;  // B38
    uint32_t SYNTH_IRQn             : 1;  // B39
    uint32_t AHBSRW_BUS_ERR_IRQn    : 1;  // B40
    uint32_t ACMP0_IRQn             : 1;  // B41
    uint32_t ACMP1_IRQn             : 1;  // B42
    uint32_t WDOG0_IRQn             : 1;  // B43
    uint32_t WDOG1_IRQn             : 1;  // B44
    uint32_t SYXO0_IRQn             : 1;  // B45
    uint32_t HFRCO0_IRQn            : 1;  // B46
    uint32_t HFRCOEM23_IRQn         : 1;  // B47
    uint32_t CMU_IRQn               : 1;  // B48
    uint32_t AES_IRQn               : 1;  // B49
    uint32_t IADC_IRQn              : 1;  // B50
    uint32_t MSC_IRQn               : 1;  // B51
    uint32_t DPLL0_IRQn             : 1;  // B52
    uint32_t EMUEFP_IRQn            : 1;  // B53
    uint32_t DCDC_IRQn              : 1;  // B54
    uint32_t PCNT0_IRQn             : 1;  // B55
    uint32_t SW0_IRQn               : 1;  // B56
    uint32_t SW1_IRQn               : 1;  // B57
    uint32_t SW2_IRQn               : 1;  // B58
    uint32_t SW3_IRQn               : 1;  // B59
    uint32_t KERNEL0_IRQn           : 1;  // B60
    uint32_t KERNEL1_IRQn           : 1;  // B61
    uint32_t M33CTI0_IRQn           : 1;  // B62
    uint32_t M33CTI1_IRQn           : 1;  // B63
    uint32_t FPUEXH_IRQn            : 1;  // B64
    uint32_t SETAMPERHOST_IRQn      : 1;  // B65
    uint32_t SEMBRX_IRQn            : 1;  // B66
    uint32_t SEMBTX_IRQn            : 1;  // B67
    uint32_t SYSRTC_APP_IRQn        : 1;  // B68
    uint32_t SYSRTC_SEQ_IRQn        : 1;  // B69
    uint32_t KEYSCAN_IRQn           : 1;  // B70
    uint32_t RFECA0_IRQn            : 1;  // B71
    uint32_t RFECA1_IRQn            : 1;  // B72
    uint32_t VDAC0_IRQn             : 1;  // B73
    uint32_t VDAC1_IRQn             : 1;  // B74
    uint32_t                : 21; // B75-95
  } bits;
  uint32_t word[3];
#elif defined (_SILICON_LABS_32B_SERIES_2_CONFIG_6)
    uint32_t SMU_SECURE_IRQn        : 1;  // B0
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1;  // B1
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1;  // B2
    uint32_t EMU_IRQn               : 1;  // B3
    uint32_t TIMER0_IRQn            : 1;  // B4
    uint32_t TIMER1_IRQn            : 1;  // B5
    uint32_t TIMER2_IRQn            : 1;  // B6
    uint32_t TIMER3_IRQn            : 1;  // B7
    uint32_t TIMER4_IRQn            : 1;  // B8
    uint32_t TIMER5_IRQn            : 1;  // B9
    uint32_t TIMER6_IRQn            : 1;  // B10
    uint32_t TIMER7_IRQn            : 1;  // B11
    uint32_t TIMER8_IRQn            : 1;  // B12
    uint32_t TIMER9_IRQn            : 1;  // B13
    uint32_t USART0_RX_IRQn         : 1;  // B14
    uint32_t USART0_TX_IRQn         : 1;  // B15
    uint32_t USART1_RX_IRQn         : 1;  // B16
    uint32_t USART1_TX_IRQn         : 1;  // B17
    uint32_t USART2_RX_IRQn         : 1;  // B18
    uint32_t USART2_TX_IRQn         : 1;  // B19
    uint32_t EUSART0_RX_IRQn        : 1;  // B20
    uint32_t EUSART0_TX_IRQn        : 1;  // B21
    uint32_t EUSART1_RX_IRQn        : 1;  // B22
    uint32_t EUSART1_TX_IRQn        : 1;  // B23
    uint32_t EUSART2_RX_IRQn        : 1;  // B24
    uint32_t EUSART2_TX_IRQn        : 1;  // B25
    uint32_t EUSART3_RX_IRQn        : 1;  // B26
    uint32_t EUSART3_TX_IRQn        : 1;  // B27
    uint32_t MVP_IRQn               : 1;  // B28
    uint32_t ICACHE0_IRQn           : 1;  // B29
    uint32_t BURTC_IRQn             : 1;  // B30
    uint32_t LETIMER0_IRQn          : 1;  // B31
    uint32_t SYSCFG_IRQn            : 1;  // B32
    uint32_t MPAHBRAM0_IRQn         : 1;  // B33
    uint32_t MPAHBRAM1_IRQn         : 1;  // B34
    uint32_t LDMA_IRQn              : 1;  // B35
    uint32_t LFXO_IRQn              : 1;  // B36
    uint32_t LFRCO_IRQn             : 1;  // B37
    uint32_t ULFRCO_IRQn            : 1;  // B38
    uint32_t GPIO_ODD_IRQn          : 1;  // B39
    uint32_t GPIO_EVEN_IRQn         : 1;  // B40
    uint32_t I2C0_IRQn              : 1;  // B41
    uint32_t I2C1_IRQn              : 1;  // B42
    uint32_t I2C2_IRQn              : 1;  // B43
    uint32_t I2C3_IRQn              : 1;  // B44
    uint32_t EMUDG_IRQn             : 1;  // B45
    uint32_t AGC_IRQn               : 1;  // B46
    uint32_t BUFC_IRQn              : 1;  // B47
    uint32_t FRC_PRI_IRQn           : 1;  // B48
    uint32_t FRC_IRQn               : 1;  // B49
    uint32_t MODEM_IRQn             : 1;  // B50
    uint32_t PROTIMER_IRQn          : 1;  // B51
    uint32_t RAC_RSM_IRQn           : 1;  // B52
    uint32_t RAC_SEQ_IRQn           : 1;  // B53
    uint32_t HOSTMAILBOX_IRQn       : 1;  // B54
    uint32_t SYNTH_IRQn             : 1;  // B55
    uint32_t ACMP0_IRQn             : 1;  // B56
    uint32_t ACMP1_IRQn             : 1;  // B57
    uint32_t WDOG0_IRQn             : 1;  // B58
    uint32_t WDOG1_IRQn             : 1;  // B59
    uint32_t HFXO0_IRQn             : 1;  // B60
    uint32_t HFRCO0_IRQn            : 1;  // B61
    uint32_t HFRCOEM23_IRQn         : 1;  // B62
    uint32_t CMU_IRQn               : 1;  // B63
    uint32_t AES_IRQn               : 1;  // B64
    uint32_t IADC_IRQn              : 1;  // B65
    uint32_t MSC_IRQn               : 1;  // B66
    uint32_t DPLL0_IRQn             : 1;  // B67
    uint32_t EMUEFP_IRQn            : 1;  // B68
    uint32_t DCDC_IRQn              : 1;  // B69
    uint32_t PCNT0_IRQn             : 1;  // B70
    uint32_t SW0_IRQn               : 1;  // B71
    uint32_t SW1_IRQn               : 1;  // B72
    uint32_t SW2_IRQn               : 1;  // B73
    uint32_t SW3_IRQn               : 1;  // B74
    uint32_t KERNEL0_IRQn           : 1;  // B75
    uint32_t KERNEL1_IRQn           : 1;  // B76
    uint32_t M33CTI0_IRQn           : 1;  // B77
    uint32_t M33CTI1_IRQn           : 1;  // B78
    uint32_t FPUEXH_IRQn            : 1;  // B79
    uint32_t SETAMPERHOST_IRQn      : 1;  // B80
    uint32_t SEMBRX_IRQn            : 1;  // B81
    uint32_t SEMBTX_IRQn            : 1;  // B82
    uint32_t SYSRTC_APP_IRQn        : 1;  // B83
    uint32_t SYSRTC_SEQ_IRQn        : 1;  // B84
    uint32_t KEYSCAN_IRQn           : 1;  // B85
    uint32_t RFECA0_IRQn            : 1;  // B86
    uint32_t RFECA1_IRQn            : 1;  // B87
    uint32_t VDAC0_IRQn             : 1;  // B88
    uint32_t VDAC1_IRQn             : 1;  // B89
    uint32_t AHB2AHB0_IRQn          : 1;  // B90
    uint32_t AHB2AHB1_IRQn          : 1;  // B91
    uint32_t LCD_IRQn               : 1;  // B92
    uint32_t                : 3; // B93-95
  } bits;
  uint32_t word[3];
#elif defined (_SILICON_LABS_32B_SERIES_2_CONFIG_7)
    uint32_t CRYPTOACC_IRQn         : 1;  // B0
    uint32_t TRNG_IRQn              : 1;  // B1
    uint32_t PKE_IRQn               : 1;  // B2
    uint32_t SMU_SECURE_IRQn        : 1;  // B3
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1;  // B4
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1;  // B5
    uint32_t EMU_IRQn               : 1;  // B6
    uint32_t EMUEFP_IRQn            : 1;  // B7
    uint32_t DCDC_IRQn              : 1;  // B8
    uint32_t ETAMPDET_IRQn          : 1;  // B9
    uint32_t TIMER0_IRQn            : 1;  // B10
    uint32_t TIMER1_IRQn            : 1;  // B11
    uint32_t TIMER2_IRQn            : 1;  // B12
    uint32_t TIMER3_IRQn            : 1;  // B13
    uint32_t TIMER4_IRQn            : 1;  // B14
    uint32_t RTCC_IRQn              : 1;  // B15
    uint32_t USART0_RX_IRQn         : 1;  // B16
    uint32_t USART0_TX_IRQn         : 1;  // B17
    uint32_t USART1_RX_IRQn         : 1;  // B18
    uint32_t USART1_TX_IRQn         : 1;  // B19
    uint32_t EUSART0_RX_IRQn        : 1;  // B20
    uint32_t EUSART0_TX_IRQn        : 1;  // B21
    uint32_t ICACHE0_IRQn           : 1;  // B22
    uint32_t BURTC_IRQn             : 1;  // B23
    uint32_t LETIMER0_IRQn          : 1;  // B24
    uint32_t SYSCFG_IRQn            : 1;  // B25
    uint32_t LDMA_IRQn              : 1;  // B26
    uint32_t LFXO_IRQn              : 1;  // B27
    uint32_t LFRCO_IRQn             : 1;  // B28
    uint32_t ULFRCO_IRQn            : 1;  // B29
    uint32_t GPIO_ODD_IRQn          : 1;  // B30
    uint32_t GPIO_EVEN_IRQn         : 1;  // B31
    uint32_t I2C0_IRQn              : 1;  // B32
    uint32_t I2C1_IRQn              : 1;  // B33
    uint32_t EMUDG_IRQn             : 1;  // B34
    uint32_t EMUSE_IRQn             : 1;  // B35
    uint32_t AGC_IRQn               : 1;  // B36
    uint32_t BUFC_IRQn              : 1;  // B37
    uint32_t FRC_PRI_IRQn           : 1;  // B38
    uint32_t FRC_IRQn               : 1;  // B39
    uint32_t MODEM_IRQn             : 1;  // B40
    uint32_t PROTIMER_IRQn          : 1;  // B41
    uint32_t RAC_RSM_IRQn           : 1;  // B42
    uint32_t RAC_SEQ_IRQn           : 1;  // B43
    uint32_t RDMAILBOX_IRQn         : 1;  // B44
    uint32_t RFSENSE_IRQn           : 1;  // B45
    uint32_t SYNTH_IRQn             : 1;  // B46
    uint32_t PRORTC_IRQn            : 1;  // B47
    uint32_t ACMP0_IRQn             : 1;  // B48
    uint32_t WDOG0_IRQn             : 1;  // B49
    uint32_t HFXO0_IRQn             : 1;  // B50
    uint32_t HFRCO0_IRQn            : 1;  // B51
    uint32_t CMU_IRQn               : 1;  // B52
    uint32_t AES_IRQn               : 1;  // B53
    uint32_t IADC_IRQn              : 1;  // B54
    uint32_t MSC_IRQn               : 1;  // B55
    uint32_t DPLL0_IRQn             : 1;  // B56
    uint32_t PDM_IRQn               : 1;  // B57
    uint32_t SW0_IRQn               : 1;  // B58
    uint32_t SW1_IRQn               : 1;  // B59
    uint32_t SW2_IRQn               : 1;  // B60
    uint32_t SW3_IRQn               : 1;  // B61
    uint32_t KERNEL0_IRQn           : 1;  // B62
    uint32_t KERNEL1_IRQn           : 1;  // B63
    uint32_t M33CTI0_IRQn           : 1;  // B64
    uint32_t M33CTI1_IRQn           : 1;  // B65
    uint32_t FPUEXH_IRQn            : 1;  // B66
    uint32_t                : 29; // B67-95
  } bits;
  uint32_t word[3];
#elif defined (_SILICON_LABS_32B_SERIES_2_CONFIG_8)
    uint32_t SMU_SECURE_IRQn        : 1;  // B0
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1;  // B1
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1;  // B2
    uint32_t EMU_IRQn               : 1;  // B3
    uint32_t TIMER0_IRQn            : 1;  // B4
    uint32_t TIMER1_IRQn            : 1;  // B5
    uint32_t TIMER2_IRQn            : 1;  // B6
    uint32_t TIMER3_IRQn            : 1;  // B7
    uint32_t TIMER4_IRQn            : 1;  // B8
    uint32_t USART0_RX_IRQn         : 1;  // B9
    uint32_t USART0_TX_IRQn         : 1;  // B10
    uint32_t EUSART0_RX_IRQn        : 1;  // B11
    uint32_t EUSART0_TX_IRQn        : 1;  // B12
    uint32_t EUSART1_RX_IRQn        : 1;  // B13
    uint32_t EUSART1_TX_IRQn        : 1;  // B14
    uint32_t EUSART2_RX_IRQn        : 1;  // B15
    uint32_t EUSART2_TX_IRQn        : 1;  // B16
    uint32_t ICACHE0_IRQn           : 1;  // B17
    uint32_t BURTC_IRQn             : 1;  // B18
    uint32_t LETIMER0_IRQn          : 1;  // B19
    uint32_t SYSCFG_IRQn            : 1;  // B20
    uint32_t MPAHBRAM_IRQn          : 1;  // B21
    uint32_t LDMA_IRQn              : 1;  // B22
    uint32_t LFXO_IRQn              : 1;  // B23
    uint32_t LFRCO_IRQn             : 1;  // B24
    uint32_t ULFRCO_IRQn            : 1;  // B25
    uint32_t GPIO_ODD_IRQn          : 1;  // B26
    uint32_t GPIO_EVEN_IRQn         : 1;  // B27
    uint32_t I2C0_IRQn              : 1;  // B28
    uint32_t I2C1_IRQn              : 1;  // B29
    uint32_t EMUDG_IRQn             : 1;  // B30
    uint32_t AGC_IRQn               : 1;  // B31
    uint32_t BUFC_IRQn              : 1;  // B32
    uint32_t FRC_PRI_IRQn           : 1;  // B33
    uint32_t FRC_IRQn               : 1;  // B34
    uint32_t MODEM_IRQn             : 1;  // B35
    uint32_t PROTIMER_IRQn          : 1;  // B36
    uint32_t RAC_RSM_IRQn           : 1;  // B37
    uint32_t RAC_SEQ_IRQn           : 1;  // B38
    uint32_t HOSTMAILBOX_IRQn       : 1;  // B39
    uint32_t SYNTH_IRQn             : 1;  // B40
    uint32_t ACMP0_IRQn             : 1;  // B41
    uint32_t ACMP1_IRQn             : 1;  // B42
    uint32_t WDOG0_IRQn             : 1;  // B43
    uint32_t WDOG1_IRQn             : 1;  // B44
    uint32_t HFXO0_IRQn             : 1;  // B45
    uint32_t HFRCO0_IRQn            : 1;  // B46
    uint32_t HFRCOEM23_IRQn         : 1;  // B47
    uint32_t CMU_IRQn               : 1;  // B48
    uint32_t AES_IRQn               : 1;  // B49
    uint32_t IADC_IRQn              : 1;  // B50
    uint32_t MSC_IRQn               : 1;  // B51
    uint32_t DPLL0_IRQn             : 1;  // B52
    uint32_t EMUEFP_IRQn            : 1;  // B53
    uint32_t DCDC_IRQn              : 1;  // B54
    uint32_t VDAC0_IRQn             : 1;  // B55
    uint32_t PCNT0_IRQn             : 1;  // B56
    uint32_t SW0_IRQn               : 1;  // B57
    uint32_t SW1_IRQn               : 1;  // B58
    uint32_t SW2_IRQn               : 1;  // B59
    uint32_t SW3_IRQn               : 1;  // B60
    uint32_t KERNEL0_IRQn           : 1;  // B61
    uint32_t KERNEL1_IRQn           : 1;  // B62
    uint32_t M33CTI0_IRQn           : 1;  // B63
    uint32_t M33CTI1_IRQn           : 1;  // B64
    uint32_t FPUEXH_IRQn            : 1;  // B65
    uint32_t SETAMPERHOST_IRQn      : 1;  // B66
    uint32_t SEMBRX_IRQn            : 1;  // B67
    uint32_t SEMBTX_IRQn            : 1;  // B68
    uint32_t LESENSE_IRQn           : 1;  // B69
    uint32_t SYSRTC_APP_IRQn        : 1;  // B70
    uint32_t SYSRTC_SEQ_IRQn        : 1;  // B71
    uint32_t LCD_IRQn               : 1;  // B72
    uint32_t KEYSCAN_IRQn           : 1;  // B73
    uint32_t RFECA0_IRQn            : 1;  // B74
    uint32_t RFECA1_IRQn            : 1;  // B75
    uint32_t AHB2AHB0_IRQn          : 1;  // B76
    uint32_t AHB2AHB1_IRQn          : 1;  // B77
    uint32_t MVP_IRQn               : 1;  // B78
    uint32_t                : 17; // B79-95
  } bits;
  uint32_t word[3];
#elif defined (_SILICON_LABS_32B_SERIES_2_CONFIG_9)
    uint32_t SETAMPERHOST_IRQn      : 1;  /*!<  0 EFR32 SETAMPERHOST Interrupt */
    uint32_t SEMBRX_IRQn            : 1;  /*!<  1 EFR32 SEMBRX Interrupt */
    uint32_t SEMBTX_IRQn            : 1;  /*!<  2 EFR32 SEMBTX Interrupt */
    uint32_t SMU_SECURE_IRQn        : 1;  /*!<  3 EFR32 SMU_SECURE Interrupt */
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1;  /*!<  4 EFR32 SMU_S_PRIVILEGED Interrupt */
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1;  /*!<  5 EFR32 SMU_NS_PRIVILEGED Interrupt */
    uint32_t EMU_IRQn               : 1;  /*!<  6 EFR32 EMU Interrupt */
    uint32_t EMUEFP_IRQn            : 1;  /*!<  7 EFR32 EMUEFP Interrupt */
    uint32_t DCDC_IRQn              : 1;  /*!<  8 EFR32 DCDC Interrupt */
    uint32_t ETAMPDET_IRQn          : 1;  /*!<  9 EFR32 ETAMPDET Interrupt */
    uint32_t TIMER0_IRQn            : 1;  /*!< 10 EFR32 TIMER0 Interrupt */
    uint32_t TIMER1_IRQn            : 1;  /*!< 11 EFR32 TIMER1 Interrupt */
    uint32_t TIMER2_IRQn            : 1;  /*!< 12 EFR32 TIMER2 Interrupt */
    uint32_t TIMER3_IRQn            : 1;  /*!< 13 EFR32 TIMER3 Interrupt */
    uint32_t TIMER4_IRQn            : 1;  /*!< 14 EFR32 TIMER4 Interrupt */
    uint32_t RTCC_IRQn              : 1;  /*!< 15 EFR32 RTCC Interrupt */
    uint32_t USART0_RX_IRQn         : 1;  /*!< 16 EFR32 USART0_RX Interrupt */
    uint32_t USART0_TX_IRQn         : 1;  /*!< 17 EFR32 USART0_TX Interrupt */
    uint32_t USART1_RX_IRQn         : 1;  /*!< 18 EFR32 USART1_RX Interrupt */
    uint32_t USART1_TX_IRQn         : 1;  /*!< 19 EFR32 USART1_TX Interrupt */
    uint32_t EUSART0_RX_IRQn        : 1;  /*!< 20 EFR32 EUSART0_RX Interrupt */
    uint32_t EUSART0_TX_IRQn        : 1;  /*!< 21 EFR32 EUSART0_TX Interrupt */
    uint32_t ICACHE0_IRQn           : 1;  /*!< 22 EFR32 ICACHE0 Interrupt */
    uint32_t BURTC_IRQn             : 1;  /*!< 23 EFR32 BURTC Interrupt */
    uint32_t LETIMER0_IRQn          : 1;  /*!< 24 EFR32 LETIMER0 Interrupt */
    uint32_t SYSCFG_IRQn            : 1;  /*!< 25 EFR32 SYSCFG Interrupt */
    uint32_t LDMA_IRQn              : 1;  /*!< 26 EFR32 LDMA Interrupt */
    uint32_t LFXO_IRQn              : 1;  /*!< 27 EFR32 LFXO Interrupt */
    uint32_t LFRCO_IRQn             : 1;  /*!< 28 EFR32 LFRCO Interrupt */
    uint32_t ULFRCO_IRQn            : 1;  /*!< 29 EFR32 ULFRCO Interrupt */
    uint32_t GPIO_ODD_IRQn          : 1;  /*!< 30 EFR32 GPIO_ODD Interrupt */
    uint32_t GPIO_EVEN_IRQn         : 1;  /*!< 31 EFR32 GPIO_EVEN Interrupt */
    uint32_t I2C0_IRQn              : 1;  /*!< 32 EFR32 I2C0 Interrupt */
    uint32_t I2C1_IRQn              : 1;  /*!< 33 EFR32 I2C1 Interrupt */
    uint32_t EMUDG_IRQn             : 1;  /*!< 34 EFR32 EMUDG Interrupt */
    uint32_t EMUSE_IRQn             : 1;  /*!< 35 EFR32 EMUSE Interrupt */
    uint32_t AGC_IRQn               : 1;  /*!< 36 EFR32 AGC Interrupt */
    uint32_t BUFC_IRQn              : 1;  /*!< 37 EFR32 BUFC Interrupt */
    uint32_t FRC_PRI_IRQn           : 1;  /*!< 38 EFR32 FRC_PRI Interrupt */
    uint32_t FRC_IRQn               : 1;  /*!< 39 EFR32 FRC Interrupt */
    uint32_t MODEM_IRQn             : 1;  /*!< 40 EFR32 MODEM Interrupt */
    uint32_t PROTIMER_IRQn          : 1;  /*!< 41 EFR32 PROTIMER Interrupt */
    uint32_t RAC_RSM_IRQn           : 1;  /*!< 42 EFR32 RAC_RSM Interrupt */
    uint32_t RAC_SEQ_IRQn           : 1;  /*!< 43 EFR32 RAC_SEQ Interrupt */
    uint32_t RDMAILBOX_IRQn         : 1;  /*!< 44 EFR32 RDMAILBOX Interrupt */
    uint32_t RFSENSE_IRQn           : 1;  /*!< 45 EFR32 RFSENSE Interrupt */
    uint32_t SYNTH_IRQn             : 1;  /*!< 46 EFR32 SYNTH Interrupt */
    uint32_t PRORTC_IRQn            : 1;  /*!< 47 EFR32 PRORTC Interrupt */
    uint32_t ACMP0_IRQn             : 1;  /*!< 48 EFR32 ACMP0 Interrupt */
    uint32_t WDOG0_IRQn             : 1;  /*!< 49 EFR32 WDOG0 Interrupt */
    uint32_t HFXO0_IRQn             : 1;  /*!< 50 EFR32 HFXO0 Interrupt */
    uint32_t HFRCO0_IRQn            : 1;  /*!< 51 EFR32 HFRCO0 Interrupt */
    uint32_t CMU_IRQn               : 1;  /*!< 52 EFR32 CMU Interrupt */
    uint32_t AES_IRQn               : 1;  /*!< 53 EFR32 AES Interrupt */
    uint32_t IADC_IRQn              : 1;  /*!< 54 EFR32 IADC Interrupt */
    uint32_t MSC_IRQn               : 1;  /*!< 55 EFR32 MSC Interrupt */
    uint32_t DPLL0_IRQn             : 1;  /*!< 56 EFR32 DPLL0 Interrupt */
    uint32_t PDM_IRQn               : 1;  /*!< 57 EFR32 PDM Interrupt */
    uint32_t SW0_IRQn               : 1;  /*!< 58 EFR32 SW0 Interrupt */
    uint32_t SW1_IRQn               : 1;  /*!< 59 EFR32 SW1 Interrupt */
    uint32_t SW2_IRQn               : 1;  /*!< 60 EFR32 SW2 Interrupt */
    uint32_t SW3_IRQn               : 1;  /*!< 61 EFR32 SW3 Interrupt */
    uint32_t KERNEL0_IRQn           : 1;  /*!< 62 EFR32 KERNEL0 Interrupt */
    uint32_t KERNEL1_IRQn           : 1;  /*!< 63 EFR32 KERNEL1 Interrupt */
    uint32_t M33CTI0_IRQn           : 1;  /*!< 64 EFR32 M33CTI0 Interrupt */
    uint32_t M33CTI1_IRQn           : 1;  /*!< 65 EFR32 M33CTI1 Interrupt */
    uint32_t FPUEXH_IRQn            : 1;  /*!< 66 EFR32 FPUEXH Interrupt */
    uint32_t MPAHBRAM_IRQn          : 1;  /*!< 67 EFR32 MPAHBRAM Interrupt */
    uint32_t EUSART1_RX_IRQn        : 1;  /*!< 68 EFR32 EUSART1_RX Interrupt */
    uint32_t EUSART1_TX_IRQn        : 1;  /*!< 69 EFR32 EUSART1_TX Interrupt */
    uint32_t                : 26; // B70-95
  } bits;
  uint32_t word[3];
#elif defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
    // These come from sixg301m114lih.h
    uint32_t SETAMPERHOST_IRQn      : 1;  /*!<  0 Si SETAMPERHOST Interrupt */
    uint32_t SEMBRX_IRQn            : 1;  /*!<  1 Si SEMBRX Interrupt */
    uint32_t SEMBTX_IRQn            : 1;  /*!<  2 Si SEMBTX Interrupt */
    uint32_t SMU_SECURE_IRQn        : 1;  /*!<  3 Si SMU_SECURE Interrupt */
    uint32_t SMU_S_PRIVILEGED_IRQn  : 1;  /*!<  4 Si SMU_S_PRIVILEGED Interrupt */
    uint32_t SMU_NS_PRIVILEGED_IRQn : 1;  /*!<  5 Si SMU_NS_PRIVILEGED Interrupt */
    uint32_t EMU_IRQn               : 1;  /*!<  6 Si EMU Interrupt */
    uint32_t EMUDG_IRQn             : 1;  /*!<  7 Si EMUDG Interrupt */
    uint32_t SYSMBLPW0CPU_IRQn      : 1;  /*!<  8 Si SYSMBLPW0CPU Interrupt */
    uint32_t ETAMPDET_IRQn          : 1;  /*!<  9 Si ETAMPDET Interrupt */
    uint32_t TIMER0_IRQn            : 1; /*!< 10 Si TIMER0 Interrupt */
    uint32_t TIMER1_IRQn            : 1; /*!< 11 Si TIMER1 Interrupt */
    uint32_t TIMER2_IRQn            : 1; /*!< 12 Si TIMER2 Interrupt */
    uint32_t TIMER3_IRQn            : 1; /*!< 13 Si TIMER3 Interrupt */
    uint32_t SYSRTC_SEQ_IRQn        : 1; /*!< 14 Si SYSRTC_SEQ Interrupt */
    uint32_t SYSRTC_APP_IRQn        : 1; /*!< 15 Si SYSRTC_APP Interrupt */
    uint32_t SYSRTC_MS_IRQn         : 1; /*!< 16 Si SYSRTC_MS Interrupt */
    uint32_t EUSART0_RX_IRQn        : 1; /*!< 17 Si EUSART0_RX Interrupt */
    uint32_t EUSART0_TX_IRQn        : 1; /*!< 18 Si EUSART0_TX Interrupt */
    uint32_t EUSART1_RX_IRQn        : 1; /*!< 19 Si EUSART1_RX Interrupt */
    uint32_t EUSART1_TX_IRQn        : 1; /*!< 20 Si EUSART1_TX Interrupt */
    uint32_t EUSART2_RX_IRQn        : 1; /*!< 21 Si EUSART2_RX Interrupt */
    uint32_t EUSART2_TX_IRQn        : 1; /*!< 22 Si EUSART2_TX Interrupt */
    uint32_t L1ICACHE0_IRQn         : 1; /*!< 23 Si L1ICACHE0 Interrupt */
    uint32_t L2ICACHE0_IRQn         : 1; /*!< 24 Si L2ICACHE0 Interrupt */
    uint32_t BURTC_IRQn             : 1; /*!< 25 Si BURTC Interrupt */
    uint32_t LETIMER0_IRQn          : 1; /*!< 26 Si LETIMER0 Interrupt */
    uint32_t PIXELRZ0_IRQn          : 1; /*!< 27 Si PIXELRZ0 Interrupt */
    uint32_t PIXELRZ1_IRQn          : 1; /*!< 28 Si PIXELRZ1 Interrupt */
    uint32_t SYSCFG_IRQn            : 1; /*!< 29 Si SYSCFG Interrupt */
    uint32_t DMEM_IRQn              : 1; /*!< 30 Si DMEM Interrupt */
    uint32_t LDMA0_CHNL0_IRQn       : 1; /*!< 31 Si LDMA0_CHNL0 Interrupt */
    uint32_t LDMA0_CHNL1_IRQn       : 1; /*!< 32 Si LDMA0_CHNL1 Interrupt */
    uint32_t LDMA0_CHNL2_IRQn       : 1; /*!< 33 Si LDMA0_CHNL2 Interrupt */
    uint32_t LDMA0_CHNL3_IRQn       : 1; /*!< 34 Si LDMA0_CHNL3 Interrupt */
    uint32_t LDMA0_CHNL4_IRQn       : 1; /*!< 35 Si LDMA0_CHNL4 Interrupt */
    uint32_t LDMA0_CHNL5_IRQn       : 1; /*!< 36 Si LDMA0_CHNL5 Interrupt */
    uint32_t LDMA0_CHNL6_IRQn       : 1; /*!< 37 Si LDMA0_CHNL6 Interrupt */
    uint32_t LDMA0_CHNL7_IRQn       : 1; /*!< 38 Si LDMA0_CHNL7 Interrupt */
    uint32_t LFXO_IRQn              : 1; /*!< 39 Si LFXO Interrupt */
    uint32_t LFRCO_IRQn             : 1; /*!< 40 Si LFRCO Interrupt */
    uint32_t ULFRCO_IRQn            : 1; /*!< 41 Si ULFRCO Interrupt */
    uint32_t GPIO_ODD_IRQn          : 1; /*!< 42 Si GPIO_ODD Interrupt */
    uint32_t GPIO_EVEN_IRQn         : 1; /*!< 43 Si GPIO_EVEN Interrupt */
    uint32_t I2C0_IRQn              : 1; /*!< 44 Si I2C0 Interrupt */
    uint32_t I2C1_IRQn              : 1; /*!< 45 Si I2C1 Interrupt */
    uint32_t I2C2_IRQn              : 1; /*!< 46 Si I2C2 Interrupt */
    uint32_t BUFC_IRQn              : 1; /*!< 47 Si BUFC Interrupt */
    uint32_t FRC_PRI_IRQn           : 1; /*!< 48 Si FRC_PRI Interrupt */
    uint32_t FRC_IRQn               : 1; /*!< 49 Si FRC Interrupt */
    uint32_t PROTIMER_IRQn          : 1; /*!< 50 Si PROTIMER Interrupt */
    uint32_t RAC_RSM_IRQn           : 1; /*!< 51 Si RAC_RSM Interrupt */
    uint32_t RAC_SEQ_IRQn           : 1; /*!< 52 Si RAC_SEQ Interrupt */
    uint32_t SYNTH_IRQn             : 1; /*!< 53 Si SYNTH Interrupt */
    uint32_t RFECA0_IRQn            : 1; /*!< 54 Si RFECA0 Interrupt */
    uint32_t RFECA1_IRQn            : 1; /*!< 55 Si RFECA1 Interrupt */
    uint32_t MODEM_IRQn             : 1; /*!< 56 Si MODEM Interrupt */
    uint32_t AGC_IRQn               : 1; /*!< 57 Si AGC Interrupt */
    uint32_t RFTIMER_IRQn           : 1; /*!< 58 Si RFTIMER Interrupt */
    uint32_t SEQACC_IRQn            : 1; /*!< 59 Si SEQACC Interrupt */
    uint32_t HFRCOLPW_IRQn          : 1; /*!< 60 Si HFRCOLPW Interrupt */
    uint32_t HFRCODPLLLPW_IRQn      : 1; /*!< 61 Si HFRCODPLLLPW Interrupt */
    uint32_t ACMP0_IRQn             : 1; /*!< 62 Si ACMP0 Interrupt */
    uint32_t ACMP1_IRQn             : 1; /*!< 63 Si ACMP1 Interrupt */
    uint32_t WDOG0_IRQn             : 1; /*!< 64 Si WDOG0 Interrupt */
    uint32_t WDOG1_IRQn             : 1; /*!< 65 Si WDOG1 Interrupt */
    uint32_t HFXO0_IRQn             : 1; /*!< 66 Si HFXO0 Interrupt */
    uint32_t HFRCO0_IRQn            : 1; /*!< 67 Si HFRCO0 Interrupt */
    uint32_t HFRCOEM23_IRQn         : 1; /*!< 68 Si HFRCOEM23 Interrupt */
    uint32_t CMU_IRQn               : 1; /*!< 69 Si CMU Interrupt */
    uint32_t RPA_IRQn               : 1; /*!< 70 Si RPA Interrupt */
    uint32_t KSURPA_IRQn            : 1; /*!< 71 Si KSURPA Interrupt */
    uint32_t KSULPWAES_IRQn         : 1; /*!< 72 Si KSULPWAES Interrupt */
    uint32_t KSUHOSTSYMCRYPTO_IRQn  : 1; /*!< 73 Si KSUHOSTSYMCRYPTO Interrupt */
    uint32_t SYMCRYPTO_IRQn         : 1; /*!< 74 Si SYMCRYPTO Interrupt */
    uint32_t AES_IRQn               : 1; /*!< 75 Si AES Interrupt */
    uint32_t ADC0_IRQn              : 1; /*!< 76 Si ADC0 Interrupt */
    uint32_t LEDDRV0_IRQn           : 1; /*!< 77 Si LEDDRV0 Interrupt */
    uint32_t DPLL0_IRQn             : 1; /*!< 78 Si DPLL0 Interrupt */
    uint32_t SOCPLL0_IRQn           : 1; /*!< 79 Si SOCPLL0 Interrupt */
    uint32_t PCNT0_IRQn             : 1; /*!< 80 Si PCNT0 Interrupt */
    uint32_t SW0_IRQn               : 1; /*!< 81 Si SW0 Interrupt */
    uint32_t SW1_IRQn               : 1; /*!< 82 Si SW1 Interrupt */
    uint32_t SW2_IRQn               : 1; /*!< 83 Si SW2 Interrupt */
    uint32_t SW3_IRQn               : 1; /*!< 84 Si SW3 Interrupt */
    uint32_t KERNEL0_IRQn           : 1; /*!< 85 Si KERNEL0 Interrupt */
    uint32_t KERNEL1_IRQn           : 1; /*!< 86 Si KERNEL1 Interrupt */
    uint32_t M33CTI0_IRQn           : 1; /*!< 87 Si M33CTI0 Interrupt */
    uint32_t M33CTI1_IRQn           : 1; /*!< 88 Si M33CTI1 Interrupt */
    uint32_t FPUEXH_IRQn            : 1; /*!< 89 Si FPUEXH Interrupt */
    uint32_t : 6; // B90-95
  } bits;
  uint32_t word[3];
  #elif defined(_SILICON_LABS_32B_SERIES_3_CONFIG_302)
  // There are 100 used bits according to simg302m114lnl.h
    uint32_t : 32; // B0-31
    uint32_t : 32; // B32-63
    uint32_t : 32; // B64-95
    uint32_t : 32; // B96-127
  } bits;
  uint32_t word[4];
#elif defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
    // These come from siwx353xfull.h (IRQn_Type)
    uint32_t SETAMPERHOST_IRQn           : 1;  /*!<  0 Si SETAMPERHOST Interrupt */
    uint32_t SEMBRX_IRQn                 : 1;  /*!<  1 Si SEMBRX Interrupt */
    uint32_t SEMBTX_IRQn                 : 1;  /*!<  2 Si SEMBTX Interrupt */
    uint32_t SMU_SECURE_IRQn             : 1;  /*!<  3 Si SMU_SECURE Interrupt */
    uint32_t SMU_S_PRIVILEGED_IRQn       : 1;  /*!<  4 Si SMU_S_PRIVILEGED Interrupt */
    uint32_t SMU_NS_PRIVILEGED_IRQn      : 1;  /*!<  5 Si SMU_NS_PRIVILEGED Interrupt */
    uint32_t EMU_IRQn                    : 1;  /*!<  6 Si EMU Interrupt */
    uint32_t TEMPSENSE_IRQn              : 1;  /*!<  7 Si TEMPSENSE Interrupt */
    uint32_t DCDC_IRQn                   : 1;  /*!<  8 Si DCDC Interrupt */
    uint32_t HOSTPBOXLPW0GEN0_IRQn       : 1;  /*!<  9 Si HOSTPBOXLPW0GEN0 Interrupt */
    uint32_t HOSTPBOXLPW0RX0_IRQn        : 1;  /*!< 10 Si HOSTPBOXLPW0RX0 Interrupt */
    uint32_t HOSTPBOXLPW0TX0_IRQn        : 1;  /*!< 11 Si HOSTPBOXLPW0TX0 Interrupt */
    uint32_t HOSTPBOXWIFI0GEN0_IRQn      : 1;  /*!< 12 Si HOSTPBOXWIFI0GEN0 Interrupt */
    uint32_t HOSTPBOXWIFI0RX0_IRQn       : 1;  /*!< 13 Si HOSTPBOXWIFI0RX0 Interrupt */
    uint32_t HOSTPBOXWIFI0TX0_IRQn       : 1;  /*!< 14 Si HOSTPBOXWIFI0TX0 Interrupt */
    uint32_t ETAMPDET_IRQn               : 1;  /*!< 15 Si ETAMPDET Interrupt */
    uint32_t TIMER0_IRQn                 : 1;  /*!< 16 Si TIMER0 Interrupt */
    uint32_t TIMER1_IRQn                 : 1;  /*!< 17 Si TIMER1 Interrupt */
    uint32_t TIMER2_IRQn                 : 1;  /*!< 18 Si TIMER2 Interrupt */
    uint32_t TIMER3_IRQn                 : 1;  /*!< 19 Si TIMER3 Interrupt */
    uint32_t TIMER4_IRQn                 : 1;  /*!< 20 Si TIMER4 Interrupt */
    uint32_t SYSRTC_APP_IRQn             : 1;  /*!< 21 Si SYSRTC_APP Interrupt */
    uint32_t SYSRTC_SEQ_IRQn             : 1;  /*!< 22 Si SYSRTC_SEQ Interrupt */
    uint32_t SYSRTC_WIFI_IRQn            : 1;  /*!< 23 Si SYSRTC_WIFI Interrupt */
    uint32_t SYSRTC_MS_IRQn              : 1;  /*!< 24 Si SYSRTC_MS Interrupt */
    uint32_t CAN0_INT0_IRQn              : 1;  /*!< 25 Si CAN0_INT0 Interrupt */
    uint32_t CAN0_INT1_IRQn              : 1;  /*!< 26 Si CAN0_INT1 Interrupt */
    uint32_t CAN0_DMU_IRQn               : 1;  /*!< 27 Si CAN0_DMU Interrupt */
    uint32_t CAN1_INT0_IRQn              : 1;  /*!< 28 Si CAN1_INT0 Interrupt */
    uint32_t CAN1_INT1_IRQn              : 1;  /*!< 29 Si CAN1_INT1 Interrupt */
    uint32_t CAN1_DMU_IRQn               : 1;  /*!< 30 Si CAN1_DMU Interrupt */
    uint32_t CSEN0_IRQn                  : 1;  /*!< 31 Si CSEN0 Interrupt */
    uint32_t EMAC0_IRQn                  : 1;  /*!< 32 Si EMAC0 Interrupt */
    uint32_t EUSART0_RX_IRQn             : 1;  /*!< 33 Si EUSART0_RX Interrupt */
    uint32_t EUSART0_TX_IRQn             : 1;  /*!< 34 Si EUSART0_TX Interrupt */
    uint32_t EUSART1_RX_IRQn             : 1;  /*!< 35 Si EUSART1_RX Interrupt */
    uint32_t EUSART1_TX_IRQn             : 1;  /*!< 36 Si EUSART1_TX Interrupt */
    uint32_t EUSART2_RX_IRQn             : 1;  /*!< 37 Si EUSART2_RX Interrupt */
    uint32_t EUSART2_TX_IRQn             : 1;  /*!< 38 Si EUSART2_TX Interrupt */
    uint32_t EUSART3_RX_IRQn             : 1;  /*!< 39 Si EUSART3_RX Interrupt */
    uint32_t EUSART3_TX_IRQn             : 1;  /*!< 40 Si EUSART3_TX Interrupt */
    uint32_t EUSART4_RX_IRQn             : 1;  /*!< 41 Si EUSART4_RX Interrupt */
    uint32_t EUSART4_TX_IRQn             : 1;  /*!< 42 Si EUSART4_TX Interrupt */
    uint32_t HSPI0_RX_IRQn               : 1;  /*!< 43 Si HSPI0_RX Interrupt */
    uint32_t HSPI0_TX_IRQn               : 1;  /*!< 44 Si HSPI0_TX Interrupt */
    uint32_t L2DCACHE0_IRQn              : 1;  /*!< 45 Si L2DCACHE0 Interrupt */
    uint32_t L2ICACHE0_IRQn              : 1;  /*!< 46 Si L2ICACHE0 Interrupt */
    uint32_t BURTC_IRQn                  : 1;  /*!< 47 Si BURTC Interrupt */
    uint32_t LETIMER0_IRQn               : 1;  /*!< 48 Si LETIMER0 Interrupt */
    uint32_t LETIMER1_IRQn               : 1;  /*!< 49 Si LETIMER1 Interrupt */
    uint32_t LESENSE0_IRQn               : 1;  /*!< 50 Si LESENSE0 Interrupt */
    uint32_t USB0_IRQn                   : 1;  /*!< 51 Si USB0 Interrupt */
    uint32_t UTIMER0MAIN_IRQn            : 1;  /*!< 52 Si UTIMER0MAIN Interrupt */
    uint32_t UTIMER1MAIN_IRQn            : 1;  /*!< 53 Si UTIMER1MAIN Interrupt */
    uint32_t UTIMER2MAIN_IRQn            : 1;  /*!< 54 Si UTIMER2MAIN Interrupt */
    uint32_t UTIMER3MAIN_IRQn            : 1;  /*!< 55 Si UTIMER3MAIN Interrupt */
    uint32_t SDHC0_IRQn                  : 1;  /*!< 56 Si SDHC0 Interrupt */
    uint32_t SDHC0RXHSPHY_IRQn           : 1;  /*!< 57 Si SDHC0RXHSPHY Interrupt */
    uint32_t SDIO0_IRQn                  : 1;  /*!< 58 Si SDIO0 Interrupt */
    uint32_t SYSCFG_IRQn                 : 1;  /*!< 59 Si SYSCFG Interrupt */
    uint32_t DMEM_IRQn                   : 1;  /*!< 60 Si DMEM Interrupt */
    uint32_t LDMA0_CHNL0_IRQn            : 1;  /*!< 61 Si LDMA0_CHNL0 Interrupt */
    uint32_t LDMA0_CHNL1_IRQn            : 1;  /*!< 62 Si LDMA0_CHNL1 Interrupt */
    uint32_t LDMA0_CHNL2_IRQn            : 1;  /*!< 63 Si LDMA0_CHNL2 Interrupt */
    uint32_t LDMA0_CHNL3_IRQn            : 1;  /*!< 64 Si LDMA0_CHNL3 Interrupt */
    uint32_t LDMA0_CHNL4_IRQn            : 1;  /*!< 65 Si LDMA0_CHNL4 Interrupt */
    uint32_t LDMA0_CHNL5_IRQn            : 1;  /*!< 66 Si LDMA0_CHNL5 Interrupt */
    uint32_t LDMA0_CHNL6_IRQn            : 1;  /*!< 67 Si LDMA0_CHNL6 Interrupt */
    uint32_t LDMA0_CHNL7_IRQn            : 1;  /*!< 68 Si LDMA0_CHNL7 Interrupt */
    uint32_t LDMA0_CHNL8_IRQn            : 1;  /*!< 69 Si LDMA0_CHNL8 Interrupt */
    uint32_t LDMA0_CHNL9_IRQn            : 1;  /*!< 70 Si LDMA0_CHNL9 Interrupt */
    uint32_t LDMA0_CHNL10_IRQn           : 1;  /*!< 71 Si LDMA0_CHNL10 Interrupt */
    uint32_t LDMA0_CHNL11_IRQn           : 1;  /*!< 72 Si LDMA0_CHNL11 Interrupt */
    uint32_t LDMA0_CHNL12_IRQn           : 1;  /*!< 73 Si LDMA0_CHNL12 Interrupt */
    uint32_t LDMA0_CHNL13_IRQn           : 1;  /*!< 74 Si LDMA0_CHNL13 Interrupt */
    uint32_t LDMA0_CHNL14_IRQn           : 1;  /*!< 75 Si LDMA0_CHNL14 Interrupt */
    uint32_t LDMA0_CHNL15_IRQn           : 1;  /*!< 76 Si LDMA0_CHNL15 Interrupt */
    uint32_t LFXO_IRQn                   : 1;  /*!< 77 Si LFXO Interrupt */
    uint32_t PLFRCO_IRQn                 : 1;  /*!< 78 Si PLFRCO Interrupt */
    uint32_t ULFRCO_IRQn                 : 1;  /*!< 79 Si ULFRCO Interrupt */
    uint32_t GPIO_ODD_IRQn               : 1;  /*!< 80 Si GPIO_ODD Interrupt */
    uint32_t GPIO_EVEN_IRQn              : 1;  /*!< 81 Si GPIO_EVEN Interrupt */
    uint32_t I2C0_IRQn                   : 1;  /*!< 82 Si I2C0 Interrupt */
    uint32_t I2C1_IRQn                   : 1;  /*!< 83 Si I2C1 Interrupt */
    uint32_t I3C0_IRQn                   : 1;  /*!< 84 Si I3C0 Interrupt */
    uint32_t I2S0_IRQn                   : 1;  /*!< 85 Si I2S0 Interrupt */
    uint32_t I2ST0_IRQn                  : 1;  /*!< 86 Si I2ST0 Interrupt */
    uint32_t PDM0_IRQn                   : 1;  /*!< 87 Si PDM0 Interrupt */
    uint32_t LPWXDMA0_IRQn               : 1;  /*!< 88 Si LPWXDMA0 Interrupt */
    uint32_t LPWXDMA1_IRQn               : 1;  /*!< 89 Si LPWXDMA1 Interrupt */
    uint32_t BUFC_IRQn                   : 1;  /*!< 90 Si BUFC Interrupt */
    uint32_t FRC_PRI_IRQn                : 1;  /*!< 91 Si FRC_PRI Interrupt */
    uint32_t FRC_IRQn                    : 1;  /*!< 92 Si FRC Interrupt */
    uint32_t PROTIMER_IRQn               : 1;  /*!< 93 Si PROTIMER Interrupt */
    uint32_t RAC_RSM_IRQn                : 1;  /*!< 94 Si RAC_RSM Interrupt */
    uint32_t RAC_SEQ_IRQn                : 1;  /*!< 95 Si RAC_SEQ Interrupt */
    uint32_t SYNTH_IRQn                  : 1;  /*!< 96 Si SYNTH Interrupt */
    uint32_t RFECA0_IRQn                 : 1;  /*!< 97 Si RFECA0 Interrupt */
    uint32_t RFECA1_IRQn                 : 1;  /*!< 98 Si RFECA1 Interrupt */
    uint32_t MODEM_IRQn                  : 1;  /*!< 99 Si MODEM Interrupt */
    uint32_t AGC_IRQn                    : 1;  /*!< 100 Si AGC Interrupt */
    uint32_t RFTIMER_IRQn                : 1;  /*!< 101 Si RFTIMER Interrupt */
    uint32_t SEQACC_IRQn                 : 1;  /*!< 102 Si SEQACC Interrupt */
    uint32_t HFRCODPLLLPW_IRQn           : 1;  /*!< 103 Si HFRCODPLLLPW Interrupt */
    uint32_t HFRCOLPW_IRQn               : 1;  /*!< 104 Si HFRCOLPW Interrupt */
    uint32_t BTCPHY_IRQn                 : 1;  /*!< 105 Si BTCPHY Interrupt */
    uint32_t BTCLMAC_IRQn                : 1;  /*!< 106 Si BTCLMAC Interrupt */
    uint32_t BTCFH_IRQn                  : 1;  /*!< 107 Si BTCFH Interrupt */
    uint32_t HADM_IRQn                   : 1;  /*!< 108 Si HADM Interrupt */
    uint32_t BLEHDT_IRQn                 : 1;  /*!< 109 Si BLEHDT Interrupt */
    uint32_t MOD_IRQn                    : 1;  /*!< 110 Si MOD Interrupt */
    uint32_t ACMP0_IRQn                  : 1;  /*!< 111 Si ACMP0 Interrupt */
    uint32_t ACMP1_IRQn                  : 1;  /*!< 112 Si ACMP1 Interrupt */
    uint32_t WDOG0_IRQn                  : 1;  /*!< 113 Si WDOG0 Interrupt */
    uint32_t WDOG1_IRQn                  : 1;  /*!< 114 Si WDOG1 Interrupt */
    uint32_t HFXO0_IRQn                  : 1;  /*!< 115 Si HFXO0 Interrupt */
    uint32_t HFRCO0_IRQn                 : 1;  /*!< 116 Si HFRCO0 Interrupt */
    uint32_t HFRCOEM23_IRQn              : 1;  /*!< 117 Si HFRCOEM23 Interrupt */
    uint32_t CMU_IRQn                    : 1;  /*!< 118 Si CMU Interrupt */
    uint32_t RPA_IRQn                    : 1;  /*!< 119 Si RPA Interrupt */
    uint32_t KSURPA_IRQn                 : 1;  /*!< 120 Si KSURPA Interrupt */
    uint32_t KSULPWAES_IRQn              : 1;  /*!< 121 Si KSULPWAES Interrupt */
    uint32_t KSUHOSTSYMCRYPTO_IRQn       : 1;  /*!< 122 Si KSUHOSTSYMCRYPTO Interrupt */
    uint32_t SYMCRYPTO_IRQn              : 1;  /*!< 123 Si SYMCRYPTO Interrupt */
    uint32_t LPWAES_IRQn                 : 1;  /*!< 124 Si LPWAES Interrupt */
    uint32_t HOSTRDSU_IRQn               : 1;  /*!< 125 Si HOSTRDSU Interrupt */
    uint32_t ADC0_IRQn                   : 1;  /*!< 126 Si ADC0 Interrupt */
    uint32_t DPLL0_IRQn                  : 1;  /*!< 127 Si DPLL0 Interrupt */
    uint32_t SOCPLL0_IRQn                : 1;  /*!< 128 Si SOCPLL0 Interrupt */
    uint32_t SOCPLL1_IRQn                : 1;  /*!< 129 Si SOCPLL1 Interrupt */
    uint32_t SOCPLL2_IRQn                : 1;  /*!< 130 Si SOCPLL2 Interrupt */
    uint32_t KEYSCAN0_IRQn               : 1;  /*!< 131 Si KEYSCAN0 Interrupt */
    uint32_t NPU_IRQn                    : 1;  /*!< 132 Si NPU Interrupt */
    uint32_t NPUDMACH0_IRQn              : 1;  /*!< 133 Si NPUDMACH0 Interrupt */
    uint32_t NPUDMACH1_IRQn              : 1;  /*!< 134 Si NPUDMACH1 Interrupt */
    uint32_t NPUDMACH2_IRQn              : 1;  /*!< 135 Si NPUDMACH2 Interrupt */
    uint32_t NPUDMACH3_IRQn              : 1;  /*!< 136 Si NPUDMACH3 Interrupt */
    uint32_t OPA0_IRQn                   : 1;  /*!< 137 Si OPA0 Interrupt */
    uint32_t OPA1_IRQn                   : 1;  /*!< 138 Si OPA1 Interrupt */
    uint32_t PCNT0_IRQn                  : 1;  /*!< 139 Si PCNT0 Interrupt */
    uint32_t PERPLL0_IRQn                : 1;  /*!< 140 Si PERPLL0 Interrupt */
    uint32_t PERPLL1_IRQn                : 1;  /*!< 141 Si PERPLL1 Interrupt */
    uint32_t VDAC0_IRQn                  : 1;  /*!< 142 Si VDAC0 Interrupt */
    uint32_t WIFI0SEQ_IRQn               : 1;  /*!< 143 Si WIFI0SEQ Interrupt */
    uint32_t WIFI0SEQACC_IRQn            : 1;  /*!< 144 Si WIFI0SEQACC Interrupt */
    uint32_t WIFIBUFC0E_IRQn             : 1;  /*!< 145 Si WIFIBUFC0E Interrupt */
    uint32_t WIFIBUFC0F_IRQn             : 1;  /*!< 146 Si WIFIBUFC0F Interrupt */
    uint32_t FREQPLAN_IRQn               : 1;  /*!< 147 Si FREQPLAN Interrupt */
    uint32_t PORTAL_IRQn                 : 1;  /*!< 148 Si PORTAL Interrupt */
    uint32_t SW0_IRQn                    : 1;  /*!< 149 Si SW0 Interrupt */
    uint32_t SW1_IRQn                    : 1;  /*!< 150 Si SW1 Interrupt */
    uint32_t SW2_IRQn                    : 1;  /*!< 151 Si SW2 Interrupt */
    uint32_t SW3_IRQn                    : 1;  /*!< 152 Si SW3 Interrupt */
    uint32_t KERNEL0_IRQn                : 1;  /*!< 153 Si KERNEL0 Interrupt */
    uint32_t KERNEL1_IRQn                : 1;  /*!< 154 Si KERNEL1 Interrupt */
    uint32_t M55CTI0_IRQn                : 1;  /*!< 155 Si M55CTI0 Interrupt */
    uint32_t M55CTI1_IRQn                : 1;  /*!< 156 Si M55CTI1 Interrupt */
    uint32_t                : 3;  // B157-159
  } bits;
  uint32_t word[5];
#elif CORTEXM3_EMBER_MICRO
    uint32_t TIM1_IRQn      : 1;  // B0
    uint32_t TIM2_IRQn      : 1;  // B1
    uint32_t MGMT_IRQn      : 1;  // B2
    uint32_t BB_IRQn        : 1;  // B3
    uint32_t SLEEPTMR_IRQn  : 1;  // B4
    uint32_t SC1_IRQn       : 1;  // B5
    uint32_t SC2_IRQn       : 1;  // B6
    uint32_t AESCCM_IRQn    : 1;  // B7
    uint32_t MACTMR_IRQn    : 1;  // B8
    uint32_t MACTX_IRQn     : 1;  // B9
    uint32_t MACRX_IRQn     : 1;  // B10
    uint32_t ADC_IRQn       : 1;  // B11
    uint32_t IRQA_IRQn      : 1;  // B12
    uint32_t IRQB_IRQn      : 1;  // B13
    uint32_t IRQC_IRQn      : 1;  // B14
    uint32_t IRQD_IRQn      : 1;  // B15
    uint32_t DEBUG_IRQn     : 1;  // B16
    uint32_t                : 15; // B17-31
  } bits;
  uint32_t word;
#else
  #error micro not recognized
#endif
} HalCrashIntActiveType;

typedef union {
  struct {
    uint32_t MEMFAULTACT    : 1;  // B0
    uint32_t BUSFAULTACT    : 1;  // B1
    uint32_t                : 1;  // B2
    uint32_t USGFAULTACT    : 1;  // B3
    uint32_t                : 3;  // B4-6
    uint32_t SVCALLACT      : 1;  // B7
    uint32_t MONITORACT     : 1;  // B8
    uint32_t                : 1;  // B9
    uint32_t PENDSVACT      : 1;  // B10
    uint32_t SYSTICKACT     : 1;  // B11
    uint32_t USGFAULTPENDED : 1;  // B12
    uint32_t MEMFAULTPENDED : 1;  // B13
    uint32_t BUSFAULTPENDED : 1;  // B14
    uint32_t SVCALLPENDED   : 1;  // B15
    uint32_t MEMFAULTENA    : 1;  // B16
    uint32_t BUSFAULTENA    : 1;  // B17
    uint32_t USGFAULTENA    : 1;  // B18
    uint32_t                : 13; // B19-31
  } bits;

  uint32_t word;
} HalCrashShcsrType;

typedef union {
  struct {
    uint32_t IACCVIOL       : 1;  // B0
    uint32_t DACCVIOL       : 1;  // B1
    uint32_t                : 1;  // B2
    uint32_t MUNSTKERR      : 1;  // B3
    uint32_t MSTKERR        : 1;  // B4
    uint32_t                : 2;  // B5-6
    uint32_t MMARVALID      : 1;  // B7
    uint32_t IBUSERR        : 1;  // B8
    uint32_t PRECISERR      : 1;  // B9
    uint32_t IMPRECISERR    : 1;  // B10
    uint32_t UNSTKERR       : 1;  // B11
    uint32_t STKERR         : 1;  // B12
    uint32_t                : 2;  // B13-14
    uint32_t BFARVALID      : 1;  // B15
    uint32_t UNDEFINSTR     : 1;  // B16
    uint32_t INVSTATE       : 1;  // B17
    uint32_t INVPC          : 1;  // B18
    uint32_t NOCP           : 1;  // B19
    uint32_t                : 4;  // B20-23
    uint32_t UNALIGNED      : 1;  // B24
    uint32_t DIVBYZERO      : 1;  // B25
    uint32_t                : 6;  // B26-31
  } bits;

  uint32_t word;
} HalCrashCfsrType;

typedef union {
  struct {
    uint32_t                : 1;  // B0
    uint32_t VECTTBL        : 1;  // B1
    uint32_t                : 28; // B2-29
    uint32_t FORCED         : 1;  // B30
    uint32_t DEBUGEVT       : 1;  // B31
  } bits;

  uint32_t word;
} HalCrashHfsrType;

typedef union {
  struct {
    uint32_t HALTED         : 1;  // B0
    uint32_t BKPT           : 1;  // B1
    uint32_t DWTTRAP        : 1;  // B2
    uint32_t VCATCH         : 1;  // B3
    uint32_t EXTERNAL       : 1;  // B4
    uint32_t                : 27; // B5-31
  } bits;

  uint32_t word;
} HalCrashDfsrType;

typedef union {
  struct {
    uint32_t MISSED         : 1;  // B0
    uint32_t RESERVED       : 1;  // B1
    uint32_t PROTECTED      : 1;  // B2
    uint32_t WRONGSIZE      : 1;  // B3
    uint32_t                : 28; // B4-31
  } bits;

  uint32_t word;
} HalCrashAfsrType;

#define NUM_RETURNS     6U

// Define the crash data structure
typedef struct {
  // ***************************************************************************
  // The components within this first block are written by the assembly
  // language common fault handler, and position and order is critical.
  // cstartup-iar-boot-entry.s79 also relies on the position/order here.
  // Do not edit without also modifying that code.
  // ***************************************************************************
  uint32_t R0;            // processor registers
  uint32_t R1;
  uint32_t R2;
  uint32_t R3;
  uint32_t R4;
  uint32_t R5;
  uint32_t R6;
  uint32_t R7;
  uint32_t R8;
  uint32_t R9;
  uint32_t R10;
  uint32_t R11;
  uint32_t R12;
  uint32_t LR;
  uint32_t mainSP;        // main and process stack pointers
  uint32_t processSP;
  // ***************************************************************************
  // End of the block written by the common fault handler.
  // ***************************************************************************

  uint32_t PC;              // stacked return value (if it could be read)
  HalCrashxPsrType xPSR;  // stacked processor status reg (if it could be read)
  uint32_t mainSPUsed;      // bytes used in main stack
  uint32_t processSPUsed;   // bytes used in process stack
  uint32_t mainStackBottom; // address of the bottom of the stack
  HalCrashIcsrType icsr;  // interrupt control state register
  HalCrashShcsrType shcsr;// system handlers control and state register
  HalCrashIntActiveType intActive;  // irq active bit register
  HalCrashCfsrType cfsr;  // configurable fault status register
  HalCrashHfsrType hfsr;  // hard fault status register
  HalCrashDfsrType dfsr;  // debug fault status register
  uint32_t faultAddress;    // fault address register (MMAR or BFAR)
  HalCrashAfsrType afsr;  // auxiliary fault status register
  uint32_t returns[NUM_RETURNS];  // probable return addresses found on the stack
  HalCrashSpecificDataType data;  // additional data specific to the crash type
} HalCrashInfoType;

typedef struct {
  uint16_t reason;
  uint16_t signature;
} HalResetCauseType;

#define RESETINFO_WORDS  ((sizeof(HalResetInfoType) + 3) / 4)

// Macro evaluating to true if the last reset was a crash, false otherwise.
#define halResetWasCrash() \
  (((1 << halGetResetInfo()) & RESET_CRASH_REASON_MASK) != 0U)

// Print a summary of crash details.
void halPrintCrashSummary(uint8_t port);

// Print the complete, decoded crash details.
void halPrintCrashDetails(uint8_t port);

// Print the complete crash data.
void halPrintCrashData(uint8_t port);

// If last reset was from an assert, return saved assert information.
const HalAssertInfoType *halGetAssertInfo(void);

void halInternalAssertFailed(const char *filename, int linenumber);

void halInternalClassifyReset(void);

/** @} (end addtogroup diagnostics) */
/** @} (end addtogroup legacyhal) */

#endif // DIAGNOSTIC_H
