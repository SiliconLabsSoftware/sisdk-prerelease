/***************************************************************************//**
 * @file
 * @brief Most analogue indirect register and subfield names changed from the 250 to
 * the 350, while most of the functionality remained the same.  The portions of
 * the phy that support both the 250 and the 350 reference the 350 names.
 * This header file translates 350 names to 250 names for 250 builds.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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
// Rename.  (There was never actually more than one AUXADC reg on the 250).
#define  AUXADC_REG                         AUXADC_H_REG
#define  AUXADC_AUXADC_BIASI_BIT            AUXADC_H_BIAS_AUXADCI_BIT
#define  AUXADC_AUXADC_BIASI_MASK           AUXADC_H_BIAS_AUXADCI_MASK
#define  AUXADC_AUXADC_BIASV_BIT            AUXADC_H_BIAS_AUXADCV_BIT
#define  AUXADC_AUXADC_BIASV_MASK           AUXADC_H_BIAS_AUXADCV_MASK

#define  BIAS_MASTER_BIAS_MASTER_BIASI_BIT  BIAS_MASTER_BIAS_MASTBIASI_BIT
#define  BIAS_MASTER_BIAS_MASTER_BIASI_BITS BIAS_MASTER_BIAS_MASTBIASI_BITS
#define  BIAS_MASTER_BIAS_MASTER_BIASI_MASK BIAS_MASTER_BIAS_MASTBIASI_MASK
#define  BIAS_MASTER_BIAS_MASTER_BIASV_BIT  BIAS_MASTER_BIAS_MASTBIASV_BIT
#define  BIAS_MASTER_BIAS_MASTER_BIASV_BITS BIAS_MASTER_BIAS_MASTBIASV_BITS
#define  BIAS_MASTER_BIAS_MASTER_BIASV_MASK BIAS_MASTER_BIAS_MASTBIASV_MASK

#define  IFAMP_IFAMP_BIASI_BIT              IFAMP_BIAS_IFAMPI_BIT
#define  IFAMP_IFAMP_BIASI_MASK             IFAMP_BIAS_IFAMPI_MASK
#define  IFAMP_IFAMP_BIASV_BIT              IFAMP_BIAS_IFAMPV_BIT
#define  IFAMP_IFAMP_BIASV_MASK             IFAMP_BIAS_IFAMPV_MASK

#define  IFFILTER_H_IFFILTER_CH_TRIM_BIT    IFFILTER_H_TUNE_CH_FILTER_BIT
#define  IFFILTER_H_IFFILTER_CH_TRIM_BITS   IFFILTER_H_TUNE_CH_FILTER_BITS
#define  IFFILTER_H_IFFILTER_CH_TRIM_MASK   IFFILTER_H_TUNE_CH_FILTER_MASK
#define  IFFILTER_H_IFFILTER_PRE_TRIM_BIT   IFFILTER_H_TUNE_PRE_FILTER_BIT
#define  IFFILTER_H_IFFILTER_PRE_TRIM_MASK  IFFILTER_H_TUNE_PRE_FILTER_MASK
#define  IFFILTER_L_IFFILTER_COMPLX_ENB_BIT  IFFILTER_L_CH_FILTER_COMPLEX_EN_B_BIT
#define  IFFILTER_L_IFFILTER_COMPLX_ENB_BITS IFFILTER_L_CH_FILTER_COMPLEX_EN_B_BITS
#define  IFFILTER_L_IFFILTER_COMPLX_ENB_MASK IFFILTER_L_CH_FILTER_COMPLEX_EN_B_MASK
#define  IFFILTER_L_IFFILTER_BIASI_BIT      IFFILTER_L_BIAS_FILTERI_BIT
#define  IFFILTER_L_IFFILTER_BIASI_MASK     IFFILTER_L_BIAS_FILTERI_MASK
#define  IFFILTER_L_IFFILTER_BIASV_BIT      IFFILTER_L_BIAS_FILTERV_BIT
#define  IFFILTER_L_IFFILTER_BIASV_MASK     IFFILTER_L_BIAS_FILTERV_MASK

// Rename.
#define  IQMIXER_REG                        IQ_BUFFER_REG
#define  IQMIXER_IQMIXER_BIASI_BIT          IQ_BUFFER_BIAS_IQBUFFI_BIT
#define  IQMIXER_IQMIXER_BIASI_MASK         IQ_BUFFER_BIAS_IQBUFFI_MASK

// Rename.  (There was never actually more than one LNA reg on the 250).
#define  LNA_REG                            LNA_L_REG
#define  LNA_LNA_BIASV_BIT                  LNA_L_BIAS_LNAV_BIT
#define  LNA_LNA_BIASV_MASK                 LNA_L_BIAS_LNAV_MASK
#define  LNA_LNA_GAIN_TRIM_BIT              LNA_L_LNA_GAIN_TRIM_BIT
#define  LNA_LNA_GAIN_TRIM_MASK             LNA_L_LNA_GAIN_TRIM_MASK
#define  LNA_LNA_TUNE_BIT                   LNA_L_TUNE_LNA_BIT
#define  LNA_LNA_TUNE_BITS                  LNA_L_TUNE_LNA_BITS
#define  LNA_LNA_TUNE_MASK                  LNA_L_TUNE_LNA_MASK

// Rename.
#define  LOOPFILTER_REG                     LOOP_FILTER_REG
#define  LOOPFILTER_LOOPFILTER_BIASI_BIT    LOOP_FILTER_BIAS_LOOPI_BIT
#define  LOOPFILTER_LOOPFILTER_BIASI_MASK   LOOP_FILTER_BIAS_LOOPI_MASK
#define  LOOPFILTER_LOOPFILTER_BIASV_BIT    LOOP_FILTER_BIAS_LOOPV_BIT
#define  LOOPFILTER_LOOPFILTER_BIASV_MASK   LOOP_FILTER_BIAS_LOOPV_MASK

// We assume references to MODDAC_REG are trying to access cell bias data so we
// map to MODULATION_DAC_H_REG on the 250.
#define  MODDAC_REG                         MODULATION_DAC_H_REG
#define  MODDAC_MODDAC_BIASI_BIT            MODULATION_DAC_H_BIAS_MODACI_BIT
#define  MODDAC_MODDAC_BIASI_MASK           MODULATION_DAC_H_BIAS_MODACI_MASK

#define  PA_PA_BIASI_BIT                    PA_BIAS_PAI_BIT
#define  PA_PA_BIASI_MASK                   PA_BIAS_PAI_MASK

// Rename.
#define  PHDET_REG                          PHASE_DETECTOR_REG
#define  PHDET_PHDET_BIASI_BIT              PHASE_DETECTOR_BIAS_PFDI_BIT
#define  PHDET_PHDET_BIASI_MASK             PHASE_DETECTOR_BIAS_PFDI_MASK

#define  PRESCALER_PRESCALER_BIASI_BIT      PRESCALER_BIAS_PRESCALERI_BIT
#define  PRESCALER_PRESCALER_BIASI_MASK     PRESCALER_BIAS_PRESCALERI_MASK

#define  RXADC_H_RXADC_BIASI_BIT            RXADC_H_BIAS_RXADCI_BIT
#define  RXADC_H_RXADC_BIASI_MASK           RXADC_H_BIAS_RXADCI_MASK
#define  RXADC_H_RXADC_BIASV_BIT            RXADC_H_BIAS_RXADCV_BIT
#define  RXADC_H_RXADC_BIASV_MASK           RXADC_H_BIAS_RXADCV_MASK
#define  RXADC_H_RXADC_BIAS_TRIM_MASK       RXADC_H_BIAS_TRIM_MASK

// We assume references to VCO_REG are trying to access cell bias data so we
// map to VCO_L_REG on the 250.
#define  VCO_REG                            VCO_L_REG
#define  VCO_VCO_BIASI_BIT                  VCO_L_BIAS_VCOI_BIT
#define  VCO_VCO_BIASI_MASK                 VCO_L_BIAS_VCOI_MASK
