#ifndef _AMEBA_AUD_H_
#define _AMEBA_AUD_H_

/* AUTO_GEN_START */

/**************************************************************************//**
 * @defgroup AUD_ADDA_CTL
 * @brief
 * @{
 *****************************************************************************/
#define AUD_BIT_POWADDACK                  ((u32)0x00000001 << 0)          /*!<R/W 1'b0  AD/DA clock power down control (0: power down, 1: power on) */
#define AUD_BIT_DAC_CKXEN                  ((u32)0x00000001 << 1)          /*!<R/W 1'b1  DAC chopper clock enable control (0: disable, 1: enable) */
#define AUD_BIT_DAC_CKXSEL                 ((u32)0x00000001 << 2)          /*!<R/W 1'b0  DAC chopper clock selection (0: ckx2/4, 1: ckx2/8) */
#define AUD_BIT_DAC_L_POW                  ((u32)0x00000001 << 3)          /*!<R/W 1'b0  DAC left channel power down control (0: power down, 1: power on) */
#define AUD_BIT_DAC_R_POW                  ((u32)0x00000001 << 4)          /*!<R/W 1'b0  DAC right channel power down control (0: power down, 1: power on) */
#define AUD_MASK_DPRAMP_CSEL               ((u32)0x00000003 << 5)          /*!<R/W 2'b11  Depop C size selection (00: 1x, 01: 2x, 10: 3x, 11: 4x) */
#define AUD_DPRAMP_CSEL(x)                 ((u32)(((x) & 0x00000003) << 5))
#define AUD_GET_DPRAMP_CSEL(x)             ((u32)(((x >> 5) & 0x00000003)))
#define AUD_BIT_DPRAMP_ENRAMP              ((u32)0x00000001 << 7)          /*!<R/W 1'b0  DPRAMP enable ramp control (0: disable, 1: enable) */
#define AUD_BIT_DPRAMP_POW                 ((u32)0x00000001 << 8)          /*!<R/W 1'b0  DPRAMP power down control (0: power down, 1: power on) */
#define AUD_BIT_DTSDM_CKXEN                ((u32)0x00000001 << 9)          /*!<R/W 1'b1  ADC integrater 1 OP chopper enable: 0 : disable 1 : enable (default) */
#define AUD_BIT_DTSDM_POW_L                ((u32)0x00000001 << 10)          /*!<R/W 1'b0  Left channel ADC power on control: 0: power down 1: power on */
#define AUD_BIT_DTSDM_POW_R                ((u32)0x00000001 << 11)          /*!<R/W 1'b0  Right channel ADC power on control: 0: power down 1: power on */
/** @} */

/**************************************************************************//**
 * @defgroup AUD_HPO_CTL
 * @brief
 * @{
 *****************************************************************************/
#define AUD_BIT_HPO_CALL                   ((u32)0x00000001 << 0)          /*!<R/W 1'b0  Headphone left channel capless mode control (0: non-capless, 1: capless) */
#define AUD_BIT_HPO_CLNDPL                 ((u32)0x00000001 << 1)          /*!<R/W 1'b0  Headphone left channel capless negative depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_CLNDPR                 ((u32)0x00000001 << 2)          /*!<R/W 1'b0  Headphone right channel capless negative depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_CLPDPL                 ((u32)0x00000001 << 3)          /*!<R/W 1'b0  Headphone left channel capless positive depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_CLPDPR                 ((u32)0x00000001 << 4)          /*!<R/W 1'b0  Headphone right channel capless positive depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_CALR                   ((u32)0x00000001 << 5)          /*!<R/W 1'b0  Headphone right channel capless mode control (0: non-capless, 1: capless) */
#define AUD_MASK_HPO_DPRSELL               ((u32)0x00000003 << 6)          /*!<R/W 2'b11  Headphone left channel depop R size selection (00: 1x, 01: 2x, 10: 3x, 11: 4x) */
#define AUD_HPO_DPRSELL(x)                 ((u32)(((x) & 0x00000003) << 6))
#define AUD_GET_HPO_DPRSELL(x)             ((u32)(((x >> 6) & 0x00000003)))
#define AUD_MASK_HPO_DPRSELR               ((u32)0x00000003 << 8)          /*!<R/W 2'b11  Headphone right channel depop R size selection (00: 1x, 01: 2x, 10: 3x, 11: 4x) */
#define AUD_HPO_DPRSELR(x)                 ((u32)(((x) & 0x00000003) << 8))
#define AUD_GET_HPO_DPRSELR(x)             ((u32)(((x >> 8) & 0x00000003)))
#define AUD_BIT_HPO_ENAL                   ((u32)0x00000001 << 10)          /*!<R/W 1'b1  Headphone left channel enable amplifier control (0: disable, 1: enable) */
#define AUD_BIT_HPO_ENAR                   ((u32)0x00000001 << 11)          /*!<R/W 1'b1  Headphone right channel enable amplifier control (0: disable, 1: enable) */
#define AUD_BIT_HPO_ENDPL                  ((u32)0x00000001 << 12)          /*!<R/W 1'b1  Headphone left channel enable depop control (0: disable, 1: enable) */
#define AUD_BIT_HPO_ENDPR                  ((u32)0x00000001 << 13)          /*!<R/W 1'b1  Headphone right channel enable depop control (0: disable, 1: enable) */
#define AUD_BIT_HPO_L_POW                  ((u32)0x00000001 << 14)          /*!<R/W 1'b0  Headphone left channel power down control (0: power down, 1: power on) */
#define AUD_BIT_HPO_MDPL                   ((u32)0x00000001 << 15)          /*!<R/W 1'b0  Headphone left channel mute depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_MDPR                   ((u32)0x00000001 << 16)          /*!<R/W 1'b0  Headphone right channel mute depop mode control (0: no depop, 1: depop) */
#define AUD_MASK_HPO_ML                    ((u32)0x00000003 << 17)          /*!<R/W 2'b11  Headphone left channel mute control (0: un-mute, 1: mute), <0>: DAC, <1>: Analog in */
#define AUD_HPO_ML(x)                      ((u32)(((x) & 0x00000003) << 17))
#define AUD_GET_HPO_ML(x)                  ((u32)(((x >> 17) & 0x00000003)))
#define AUD_MASK_HPO_MR                    ((u32)0x00000003 << 19)          /*!<R/W 2'b11  Headphone right channel mute control (0: un-mute, 1: mute), <0>: DAC, <1>: Analog in */
#define AUD_HPO_MR(x)                      ((u32)(((x) & 0x00000003) << 19))
#define AUD_GET_HPO_MR(x)                  ((u32)(((x >> 19) & 0x00000003)))
#define AUD_BIT_HPO_OPNDPL                 ((u32)0x00000001 << 21)          /*!<R/W 1'b0  Headphone left channel op negative depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_OPNDPR                 ((u32)0x00000001 << 22)          /*!<R/W 1'b0  Headphone right channel op negative depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_OPPDPL                 ((u32)0x00000001 << 23)          /*!<R/W 1'b0  Headphone left channel op positive depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_OPPDPR                 ((u32)0x00000001 << 24)          /*!<R/W 1'b0  Headphone right channel op positive depop mode control (0: no depop, 1: depop) */
#define AUD_BIT_HPO_R_POW                  ((u32)0x00000001 << 25)          /*!<R/W 1'b0  Headphone right channel power down control (0: power down, 1: power on) */
#define AUD_BIT_HPO_SEL                    ((u32)0x00000001 << 26)          /*!<R/W 1'b1  Headphone left channel single-end mode control (0: differential, 1: single-end) */
#define AUD_BIT_HPO_SER                    ((u32)0x00000001 << 27)          /*!<R/W 1'b1  Headphone right channel single-end mode control (0: differential, 1: single-end) */
#define AUD_MASK_HPO_GSELL                 ((u32)0x00000003 << 28)          /*!<R/W 2'b00  HPO gain conrtol */
#define AUD_HPO_GSELL(x)                   ((u32)(((x) & 0x00000003) << 28))
#define AUD_GET_HPO_GSELL(x)               ((u32)(((x >> 28) & 0x00000003)))
#define AUD_MASK_HPO_GSELR                 ((u32)0x00000003 << 30)          /*!<R/W 2'b00  HPO gain conrtol */
#define AUD_HPO_GSELR(x)                   ((u32)(((x) & 0x00000003) << 30))
#define AUD_GET_HPO_GSELR(x)               ((u32)(((x >> 30) & 0x00000003)))
/** @} */

/**************************************************************************//**
 * @defgroup AUD_MICBIAS_CTL0
 * @brief
 * @{
 *****************************************************************************/
#define AUD_BIT_MBIAS_POW                  ((u32)0x00000001 << 0)          /*!<R/W 1'b0  MBIAS power control 0: power down 1: power on */
#define AUD_BIT_MICBIAS1_ENCHX             ((u32)0x00000001 << 1)          /*!<R/W 1'b1  MICBIAS enable chopper clock 0:disable 1:enable */
#define AUD_BIT_MICBIAS1_POW               ((u32)0x00000001 << 2)          /*!<R/W 1'b0  MICBIAS power control 0:power down 1:power on */
#define AUD_MASK_MICBIAS1_VSET             ((u32)0x0000000F << 3)          /*!<R/W 4'b0011  MICBIAS select output voltage level 0.1V per step 0000:1.15V 0001:1.25V 0010:1.35V 0011:1.45V 0100:1.55V 0101:1.65V 0110:1.75V 0111:1.8V */
#define AUD_MICBIAS1_VSET(x)               ((u32)(((x) & 0x0000000F) << 3))
#define AUD_GET_MICBIAS1_VSET(x)           ((u32)(((x >> 3) & 0x0000000F)))
#define AUD_MASK_MICBIAS1_OCSEL            ((u32)0x00000003 << 7)          /*!<R/W 2'b01  OCP current selection 00: 2.5mA 01: 5mA 10: 7.5mA 11: 10mA */
#define AUD_MICBIAS1_OCSEL(x)              ((u32)(((x) & 0x00000003) << 7))
#define AUD_GET_MICBIAS1_OCSEL(x)          ((u32)(((x >> 7) & 0x00000003)))
#define AUD_MASK_MICBIAS1_COUNT            ((u32)0x00000003 << 9)          /*!<R/W 2'b01  when OCP happen disable time x312.5kHz 00: 819.2us 01: 1638.4us 10: 3276.8us 11: 6553.6us */
#define AUD_MICBIAS1_COUNT(x)              ((u32)(((x) & 0x00000003) << 9))
#define AUD_GET_MICBIAS1_COUNT(x)          ((u32)(((x >> 9) & 0x00000003)))
#define AUD_BIT_MICBIAS1_POWSHDT           ((u32)0x00000001 << 11)          /*!<R/W 1'b0  MICBIAS OCP power control 0: disable OCP 1: enable OCP */
#define AUD_MASK_MICBIAS1_COMP             ((u32)0x0000000F << 12)          /*!<R/W 4'b0000  reserve */
#define AUD_MICBIAS1_COMP(x)               ((u32)(((x) & 0x0000000F) << 12))
#define AUD_GET_MICBIAS1_COMP(x)           ((u32)(((x >> 12) & 0x0000000F)))
#define AUD_BIT_MICBIAS1_PCUT1_EN          ((u32)0x00000001 << 16)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS1_PCUT2_EN          ((u32)0x00000001 << 17)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS1_PCUT3_EN          ((u32)0x00000001 << 18)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS1_PCUT4_EN          ((u32)0x00000001 << 19)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS1_PCUT5_EN          ((u32)0x00000001 << 20)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS2_ENCHX             ((u32)0x00000001 << 21)          /*!<R/W 1'b1  MICBIAS enable chopper clock 0:disable 1:enable */
#define AUD_BIT_MICBIAS2_POW               ((u32)0x00000001 << 22)          /*!<R/W 1'b0  MICBIAS power control 0:power down 1:power on */
#define AUD_MASK_MICBIAS2_VSET             ((u32)0x0000000F << 23)          /*!<R/W 4'b0011  MICBIAS select output voltage level 0.1V per step 0000:1.15V 0001:1.25V 0010:1.35V 0011:1.45V 0100:1.55V 0101:1.65V 0110:1.75V 0111:1.8V */
#define AUD_MICBIAS2_VSET(x)               ((u32)(((x) & 0x0000000F) << 23))
#define AUD_GET_MICBIAS2_VSET(x)           ((u32)(((x >> 23) & 0x0000000F)))
#define AUD_MASK_MICBIAS2_OCSEL            ((u32)0x00000003 << 27)          /*!<R/W 2'b01  OCP current selection 00: 2.5mA 01: 5mA 10: 7.5mA 11: 10mA */
#define AUD_MICBIAS2_OCSEL(x)              ((u32)(((x) & 0x00000003) << 27))
#define AUD_GET_MICBIAS2_OCSEL(x)          ((u32)(((x >> 27) & 0x00000003)))
#define AUD_MASK_MICBIAS2_COUNT            ((u32)0x00000003 << 29)          /*!<R/W 2'b01  when OCP happen disable time x312.5kHz 00: 819.2us 01: 1638.4us 10: 3276.8us 11: 6553.6us */
#define AUD_MICBIAS2_COUNT(x)              ((u32)(((x) & 0x00000003) << 29))
#define AUD_GET_MICBIAS2_COUNT(x)          ((u32)(((x >> 29) & 0x00000003)))
#define AUD_BIT_MICBIAS2_POWSHDT           ((u32)0x00000001 << 31)          /*!<R/W 1'b0  MICMIAS OCP power control 0: disable OCP 1: enable OCP */
/** @} */

/**************************************************************************//**
 * @defgroup AUD_MICBIAS_CTL1
 * @brief
 * @{
 *****************************************************************************/
#define AUD_MASK_MICBIAS2_COMP             ((u32)0x0000000F << 0)          /*!<R/W 4'b0000  reserve */
#define AUD_MICBIAS2_COMP(x)               ((u32)(((x) & 0x0000000F) << 0))
#define AUD_GET_MICBIAS2_COMP(x)           ((u32)(((x >> 0) & 0x0000000F)))
#define AUD_BIT_MICBIAS2_PCUT1_EN          ((u32)0x00000001 << 4)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS2_PCUT2_EN          ((u32)0x00000001 << 5)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS2_PCUT3_EN          ((u32)0x00000001 << 6)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS2_PCUT4_EN          ((u32)0x00000001 << 7)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
#define AUD_BIT_MICBIAS2_PCUT5_EN          ((u32)0x00000001 << 8)          /*!<R/W 1'b1  MICBIAS power cut 0: on 1. off */
/** @} */

/**************************************************************************//**
 * @defgroup AUD_MICBST_CTL0
 * @brief
 * @{
 *****************************************************************************/
#define AUD_BIT_MICBST_POWL                ((u32)0x00000001 << 0)          /*!<R/W 1'b0  MICBST power control left channel 0:power down 1:power on */
#define AUD_BIT_MICBST_POWR                ((u32)0x00000001 << 1)          /*!<R/W 1'b0  MICBST power control right channel 0:power down 1:power on */
#define AUD_BIT_MICBST_ENDFL               ((u32)0x00000001 << 2)          /*!<R/W 1'b0  MICBST left channel enable differential 0:single to differential 1:differential to differential */
#define AUD_BIT_MICBST_ENDFR               ((u32)0x00000001 << 3)          /*!<R/W 1'b0  MICBST right channel enable differential 0:single to differential 1:differential to differential */
#define AUD_BIT_MICBST_ENCALL              ((u32)0x00000001 << 4)          /*!<R/W 1'b0  MICBST left channel enable calibration path 0:disable 1:enable */
#define AUD_BIT_MICBST_ENCALR              ((u32)0x00000001 << 5)          /*!<R/W 1'b0  MICBST right channel enable calibration path 0:disable 1:enable */
#define AUD_BIT_MICBST_ENCAL_SWAPL         ((u32)0x00000001 << 6)          /*!<R/W 1'b0  MICBST left channel swao_calibratio path 0:disable 1:enable */
#define AUD_BIT_MICBST_ENCAL_SWAPR         ((u32)0x00000001 << 7)          /*!<R/W 1'b0  MICBST right channel swao_calibratio path 0:disable 1:enable */
#define AUD_MASK_MICBST_GSELL              ((u32)0x0000000F << 8)          /*!<R/W 4'b0000  MICBST left channel gain select 0000: 0dB 0001:5dB 0010:10dB 0011:15dB 0100:20dB 0101:25dB 0110:30dB 0111:35dB 1XXX:40dB */
#define AUD_MICBST_GSELL(x)                ((u32)(((x) & 0x0000000F) << 8))
#define AUD_GET_MICBST_GSELL(x)            ((u32)(((x >> 8) & 0x0000000F)))
#define AUD_MASK_MICBST_GSELR              ((u32)0x0000000F << 12)          /*!<R/W 4'b0000  MICBST right channel gain select 0000: 0dB 0001:5dB 0010:10dB 0011:15dB 0100:20dB 0101:25dB 0110:30dB 0111:35dB 1XXX:40dB */
#define AUD_MICBST_GSELR(x)                ((u32)(((x) & 0x0000000F) << 12))
#define AUD_GET_MICBST_GSELR(x)            ((u32)(((x >> 12) & 0x0000000F)))
#define AUD_MASK_MICBST_MUTE_L             ((u32)0x00000003 << 16)          /*!<R/W 2'b11  MICBST mute control mute<0>: mic in mute<1>: line in 0:unmute 1:mute */
#define AUD_MICBST_MUTE_L(x)               ((u32)(((x) & 0x00000003) << 16))
#define AUD_GET_MICBST_MUTE_L(x)           ((u32)(((x >> 16) & 0x00000003)))
#define AUD_MASK_MICBST_MUTE_R             ((u32)0x00000003 << 18)          /*!<R/W 2'b11  MICBST mute control mute<0>: mic in mute<1>: line in 0:unmute 1:mute */
#define AUD_MICBST_MUTE_R(x)               ((u32)(((x) & 0x00000003) << 18))
#define AUD_GET_MICBST_MUTE_R(x)           ((u32)(((x >> 18) & 0x00000003)))
#define AUD_BIT_MICBST2_POWL               ((u32)0x00000001 << 20)          /*!<R/W 1'b0  MICBST power control left channel 0:power down 1:power on */
#define AUD_BIT_MICBST2_POWR               ((u32)0x00000001 << 21)          /*!<R/W 1'b0  MICBST power control right channel 0:power down 1:power on */
#define AUD_BIT_MICBST2_ENDFL              ((u32)0x00000001 << 22)          /*!<R/W 1'b0  MICBST left channel enable differential 0:single to differential 1:differential to differential */
#define AUD_BIT_MICBST2_ENDFR              ((u32)0x00000001 << 23)          /*!<R/W 1'b0  MICBST right channel enable differential 0:single to differential 1:differential to differential */
#define AUD_MASK_MICBST2_GSELL             ((u32)0x0000000F << 24)          /*!<R/W 4'b0000  MICBST left channel gain select 0000: 0dB 0001:5dB 0010:10dB 0011:15dB 0100:20dB 0101:25dB 0110:30dB 0111:35dB 1XXX:40dB */
#define AUD_MICBST2_GSELL(x)               ((u32)(((x) & 0x0000000F) << 24))
#define AUD_GET_MICBST2_GSELL(x)           ((u32)(((x >> 24) & 0x0000000F)))
#define AUD_MASK_MICBST2_GSELR             ((u32)0x0000000F << 28)          /*!<R/W 4'b0000  MICBST right channel gain select 0000: 0dB 0001:5dB 0010:10dB 0011:15dB 0100:20dB 0101:25dB 0110:30dB 0111:35dB 1XXX:40dB */
#define AUD_MICBST2_GSELR(x)               ((u32)(((x) & 0x0000000F) << 28))
#define AUD_GET_MICBST2_GSELR(x)           ((u32)(((x >> 28) & 0x0000000F)))
/** @} */

/**************************************************************************//**
 * @defgroup AUD_MICBST_CTL1
 * @brief
 * @{
 *****************************************************************************/
#define AUD_MASK_MICBST2_MUTE_L            ((u32)0x00000003 << 0)          /*!<R/W 2'b1  MICBST mute control mute: mic in 0:unmute 1:mute */
#define AUD_MICBST2_MUTE_L(x)              ((u32)(((x) & 0x00000003) << 0))
#define AUD_GET_MICBST2_MUTE_L(x)          ((u32)(((x >> 0) & 0x00000003)))
#define AUD_MASK_MICBST2_MUTE_R            ((u32)0x00000003 << 2)          /*!<R/W 2'b1  MICBST mute control mute: mic in 0:unmute 1:mute */
#define AUD_MICBST2_MUTE_R(x)              ((u32)(((x) & 0x00000003) << 2))
#define AUD_GET_MICBST2_MUTE_R(x)          ((u32)(((x >> 2) & 0x00000003)))
#define AUD_BIT_MICBST3_POW                ((u32)0x00000001 << 4)          /*!<R/W 1'b0  MICBST power control left channel 0:power down 1:power on */
#define AUD_BIT_MICBST3_ENDF               ((u32)0x00000001 << 5)          /*!<R/W 1'b0  MICBST left channel enable differential 0:single to differential 1:differential to differential */
#define AUD_MASK_MICBST3_GSEL              ((u32)0x0000000F << 6)          /*!<R/W 4'b0000  MICBST left channel gain select 0000: 0dB 0001:5dB 0010:10dB 0011:15dB 0100:20dB 0101:25dB 0110:30dB 0111:35dB 1XXX:40dB */
#define AUD_MICBST3_GSEL(x)                ((u32)(((x) & 0x0000000F) << 6))
#define AUD_GET_MICBST3_GSEL(x)            ((u32)(((x >> 6) & 0x0000000F)))
#define AUD_MASK_MICBST3_MUTE              ((u32)0x00000003 << 10)          /*!<R/W 2'b1  MICBST mute control mute: mic in 0:unmute 1:mute */
#define AUD_MICBST3_MUTE(x)                ((u32)(((x) & 0x00000003) << 10))
#define AUD_GET_MICBST3_MUTE(x)            ((u32)(((x >> 10) & 0x00000003)))
#define AUD_BIT_LDO_POW                    ((u32)0x00000001 << 12)          /*!<R/W 1'b1  LDO power control 0: disable LDO 1: enable LDO */
#define AUD_MASK_LDO_TUNE                  ((u32)0x0000001F << 13)          /*!<R/W 5'b10000  LDO voltage control 10000 Vref 0.9V LDO 1.8V */
#define AUD_LDO_TUNE(x)                    ((u32)(((x) & 0x0000001F) << 13))
#define AUD_GET_LDO_TUNE(x)                ((u32)(((x >> 13) & 0x0000001F)))
#define AUD_BIT_LDO_COMP_INT               ((u32)0x00000001 << 18)          /*!<R/W 1'b0  LDO Miller Compensation 0:close 1:short */
#define AUD_BIT_LDO_POW_0P9V               ((u32)0x00000001 << 19)          /*!<R/W 1'b1  LDO Precharge 0: LDO 1: Unity Gain buffer */
#define AUD_BIT_LDO_PREC                   ((u32)0x00000001 << 20)          /*!<R/W 1'b1   */
#define AUD_MASK_CODEC_RESERVE_1_0         ((u32)0x00000003 << 0)          /*!<R/W 0   */
#define AUD_CODEC_RESERVE_1_0(x)           ((u32)(((x) & 0x00000003) << 0))
#define AUD_GET_CODEC_RESERVE_1_0(x)       ((u32)(((x >> 0) & 0x00000003)))
#define AUD_MASK_CODEC_RESERVE_3_2         ((u32)0x00000003 << 2)          /*!<R/W 0   */
#define AUD_CODEC_RESERVE_3_2(x)           ((u32)(((x) & 0x00000003) << 2))
#define AUD_GET_CODEC_RESERVE_3_2(x)       ((u32)(((x >> 2) & 0x00000003)))
#define AUD_MASK_CODEC_RESERVE_5_4         ((u32)0x00000003 << 4)          /*!<R/W 0   */
#define AUD_CODEC_RESERVE_5_4(x)           ((u32)(((x) & 0x00000003) << 4))
#define AUD_GET_CODEC_RESERVE_5_4(x)       ((u32)(((x >> 4) & 0x00000003)))
#define AUD_MASK_CODEC_RESERVE_7_6         ((u32)0x00000003 << 6)          /*!<R/W 0   */
#define AUD_CODEC_RESERVE_7_6(x)           ((u32)(((x) & 0x00000003) << 6))
#define AUD_GET_CODEC_RESERVE_7_6(x)       ((u32)(((x >> 6) & 0x00000003)))
#define AUD_BIT_CODEC_RESERVE_8            ((u32)0x00000001 << 8)          /*!<R/W 0   */
#define AUD_MASK_CODEC_RESERVE_15_9        ((u32)0x0000007F << 9)          /*!<R/W 0   */
#define AUD_CODEC_RESERVE_15_9(x)          ((u32)(((x) & 0x0000007F) << 9))
#define AUD_GET_CODEC_RESERVE_15_9(x)      ((u32)(((x >> 9) & 0x0000007F)))
#define AUD_MASK_CODEC_RESERVE1            ((u32)0x0000FFFF << 16)          /*!<R/W 0   */
#define AUD_CODEC_RESERVE1(x)              ((u32)(((x) & 0x0000FFFF) << 16))
#define AUD_GET_CODEC_RESERVE1(x)          ((u32)(((x >> 16) & 0x0000FFFF)))
/** @} */

/**************************************************************************//**
 * @defgroup AUD_DTS_CTL
 * @brief
 * @{
 *****************************************************************************/
#define AUD_BIT_DTSDM2_CKXEN               ((u32)0x00000001 << 0)          /*!<R/W 1'b1  ADC integrater 1 OP chopper enable: 0 : disable 1 : enable (default) */
#define AUD_BIT_DTSDM2_POW_L               ((u32)0x00000001 << 1)          /*!<R/W 1'b0  Left channel ADC power on control: 0: power down 1: power on */
#define AUD_BIT_DTSDM2_POW_R               ((u32)0x00000001 << 2)          /*!<R/W 1'b0  Right channel ADC power on control: 0: power down 1: power on */
#define AUD_BIT_DTSDM3_CKXEN               ((u32)0x00000001 << 3)          /*!<R/W 1'b1  ADC integrater 1 OP chopper enable: 0 : disable 1 : enable (default) */
#define AUD_BIT_DTSDM3_POW                 ((u32)0x00000001 << 4)          /*!<R/W 1'b0  Left channel ADC power on control: 0: power down 1: power on */
#define AUD_BIT_LPMODE_EN                  ((u32)0x00000001 << 5)          /*!<R/W 1'b0  low power mode en 0: disable , current use 12.5uA 1: enable , current use 1uA */
/** @} */

/**************************************************************************//**
 * @defgroup AUD_MBIAS_CTL0
 * @brief
 * @{
 *****************************************************************************/
#define AUD_MASK_MBIAS_ISEL_DAC            ((u32)0x00000007 << 0)          /*!<R/W 3'b110  DAC current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_DAC(x)              ((u32)(((x) & 0x00000007) << 0))
#define AUD_GET_MBIAS_ISEL_DAC(x)          ((u32)(((x >> 0) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_HPO            ((u32)0x00000007 << 4)          /*!<R/W 3'b110  HPO current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_HPO(x)              ((u32)(((x) & 0x00000007) << 4))
#define AUD_GET_MBIAS_ISEL_HPO(x)          ((u32)(((x >> 4) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM3         ((u32)0x00000007 << 8)          /*!<R/W 3'b110  DTSDM3 current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_DTSDM3(x)           ((u32)(((x) & 0x00000007) << 8))
#define AUD_GET_MBIAS_ISEL_DTSDM3(x)       ((u32)(((x >> 8) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM3_INT1    ((u32)0x00000007 << 12)          /*!<R/W 3'b110  DTSDM3 current control: default 8uA, 1uA per step */
#define AUD_MBIAS_ISEL_DTSDM3_INT1(x)      ((u32)(((x) & 0x00000007) << 12))
#define AUD_GET_MBIAS_ISEL_DTSDM3_INT1(x)  ((u32)(((x >> 12) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM2_L       ((u32)0x00000007 << 16)          /*!<R/W 3'b110  DTSDM2_L current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_DTSDM2_L(x)         ((u32)(((x) & 0x00000007) << 16))
#define AUD_GET_MBIAS_ISEL_DTSDM2_L(x)     ((u32)(((x >> 16) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM2_INT1_L  ((u32)0x00000007 << 20)          /*!<R/W 3'b110  DTSDM2_L current control: default 8uA, 1uA per step */
#define AUD_MBIAS_ISEL_DTSDM2_INT1_L(x)    ((u32)(((x) & 0x00000007) << 20))
#define AUD_GET_MBIAS_ISEL_DTSDM2_INT1_L(x) ((u32)(((x >> 20) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM2_R       ((u32)0x00000007 << 24)          /*!<R/W 3'b110  DTSDM2_R current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_DTSDM2_R(x)         ((u32)(((x) & 0x00000007) << 24))
#define AUD_GET_MBIAS_ISEL_DTSDM2_R(x)     ((u32)(((x >> 24) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM2_INT1_R  ((u32)0x00000007 << 28)          /*!<R/W 3'b110  DTSDM2_R current control: default 8uA, 1uA per step */
#define AUD_MBIAS_ISEL_DTSDM2_INT1_R(x)    ((u32)(((x) & 0x00000007) << 28))
#define AUD_GET_MBIAS_ISEL_DTSDM2_INT1_R(x) ((u32)(((x >> 28) & 0x00000007)))
/** @} */

/**************************************************************************//**
 * @defgroup AUD_MBIAS_CTL1
 * @brief
 * @{
 *****************************************************************************/
#define AUD_MASK_MBIAS_ISEL_DTSDM_L        ((u32)0x00000007 << 0)          /*!<R/W 3'b110  DTSDM1_L current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_DTSDM_L(x)          ((u32)(((x) & 0x00000007) << 0))
#define AUD_GET_MBIAS_ISEL_DTSDM_L(x)      ((u32)(((x >> 0) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM_INT1_L   ((u32)0x00000007 << 4)          /*!<R/W 3'b110  DTSDM1_L current control: default 8uA, 1uA per step */
#define AUD_MBIAS_ISEL_DTSDM_INT1_L(x)     ((u32)(((x) & 0x00000007) << 4))
#define AUD_GET_MBIAS_ISEL_DTSDM_INT1_L(x) ((u32)(((x >> 4) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM_R        ((u32)0x00000007 << 8)          /*!<R/W 3'b110  DTSDM1_R current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_DTSDM_R(x)          ((u32)(((x) & 0x00000007) << 8))
#define AUD_GET_MBIAS_ISEL_DTSDM_R(x)      ((u32)(((x >> 8) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_DTSDM_INT1_R   ((u32)0x00000007 << 12)          /*!<R/W 3'b110  DTSDM1_R current control: default 8uA, 1uA per step */
#define AUD_MBIAS_ISEL_DTSDM_INT1_R(x)     ((u32)(((x) & 0x00000007) << 12))
#define AUD_GET_MBIAS_ISEL_DTSDM_INT1_R(x) ((u32)(((x >> 12) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_MICBIAS1       ((u32)0x00000007 << 16)          /*!<R/W 3'b110  MICBIAS1 current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_MICBIAS1(x)         ((u32)(((x) & 0x00000007) << 16))
#define AUD_GET_MBIAS_ISEL_MICBIAS1(x)     ((u32)(((x >> 16) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_MICBIAS2       ((u32)0x00000007 << 20)          /*!<R/W 3'b110  MICBIAS2 current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_MICBIAS2(x)         ((u32)(((x) & 0x00000007) << 20))
#define AUD_GET_MBIAS_ISEL_MICBIAS2(x)     ((u32)(((x >> 20) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_MICBST_L       ((u32)0x00000007 << 24)          /*!<R/W 3'b110  MICBST_L current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_MICBST_L(x)         ((u32)(((x) & 0x00000007) << 24))
#define AUD_GET_MBIAS_ISEL_MICBST_L(x)     ((u32)(((x >> 24) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_MICBST_R       ((u32)0x00000007 << 28)          /*!<R/W 3'b110  MICBST_R current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_MICBST_R(x)         ((u32)(((x) & 0x00000007) << 28))
#define AUD_GET_MBIAS_ISEL_MICBST_R(x)     ((u32)(((x >> 28) & 0x00000007)))
/** @} */

/**************************************************************************//**
 * @defgroup AUD_MBIAS_CTL2
 * @brief
 * @{
 *****************************************************************************/
#define AUD_MASK_MBIAS_ISEL_MICBST2_L      ((u32)0x00000007 << 0)          /*!<R/W 3'b110  MICBST2_L current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_MICBST2_L(x)        ((u32)(((x) & 0x00000007) << 0))
#define AUD_GET_MBIAS_ISEL_MICBST2_L(x)    ((u32)(((x >> 0) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_MICBST2_R      ((u32)0x00000007 << 4)          /*!<R/W 3'b110  MICBST2_R current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_MICBST2_R(x)        ((u32)(((x) & 0x00000007) << 4))
#define AUD_GET_MBIAS_ISEL_MICBST2_R(x)    ((u32)(((x >> 4) & 0x00000007)))
#define AUD_MASK_MBIAS_ISEL_MICBST3        ((u32)0x00000007 << 8)          /*!<R/W 3'b110  MICBST3 current control: default 4uA, 0.5uA per step */
#define AUD_MBIAS_ISEL_MICBST3(x)          ((u32)(((x) & 0x00000007) << 8))
#define AUD_GET_MBIAS_ISEL_MICBST3(x)      ((u32)(((x >> 8) & 0x00000007)))
#define AUD_BIT_MICBIAS1_OC                ((u32)0x00000001 << 16)          /*!<R    */
#define AUD_BIT_MICBIAS2_OC                ((u32)0x00000001 << 17)          /*!<R    */
/** @} */

/**************************************************************************//**
 * @defgroup AUD_HPO_BIAS_CTL
 * @brief
 * @{
 *****************************************************************************/
#define AUD_MASK_HPO_BIASL                 ((u32)0x00000003 << 0)          /*!<R/W 2'b0   */
#define AUD_HPO_BIASL(x)                   ((u32)(((x) & 0x00000003) << 0))
#define AUD_GET_HPO_BIASL(x)               ((u32)(((x >> 0) & 0x00000003)))
#define AUD_MASK_HPO_BIASR                 ((u32)0x00000003 << 2)          /*!<R/W 2'b0   */
#define AUD_HPO_BIASR(x)                   ((u32)(((x) & 0x00000003) << 2))
#define AUD_GET_HPO_BIASR(x)               ((u32)(((x >> 2) & 0x00000003)))
#define AUD_MASK_HPO_CCSELL                ((u32)0x00000003 << 4)          /*!<R/W 2'b0   */
#define AUD_HPO_CCSELL(x)                  ((u32)(((x) & 0x00000003) << 4))
#define AUD_GET_HPO_CCSELL(x)              ((u32)(((x >> 4) & 0x00000003)))
#define AUD_MASK_HPO_CCSELR                ((u32)0x00000003 << 6)          /*!<R/W 2'b0   */
#define AUD_HPO_CCSELR(x)                  ((u32)(((x) & 0x00000003) << 6))
#define AUD_GET_HPO_CCSELR(x)              ((u32)(((x >> 6) & 0x00000003)))
#define AUD_MASK_HPO_RESERVED              ((u32)0x000000FF << 16)          /*!<R/W 0   */
#define AUD_HPO_RESERVED(x)                ((u32)(((x) & 0x000000FF) << 16))
#define AUD_GET_HPO_RESERVED(x)            ((u32)(((x >> 16) & 0x000000FF)))
/** @} */


#define ADDA_CTL		0x0000
#define HPO_CTL			0x0004
#define MICBIAS_CTL0	0x0008
#define MICBIAS_CTL1	0x000C
#define MICBST_CTL0		0x0010
#define MICBST_CTL1		0x0014
#define ANALOG_RSVD0    0x0018
#define DTS_CTL			0x001C
#define MBIAS_CTL0		0x0020
#define MBIAS_CTL1		0x0024
#define MBIAS_CTL2		0x0028
#define HPO_BIAS_CTL	0x002C


/**************************************************************************//**
 * @defgroup AMEBA_AUD
 * @{
 * @brief AMEBA_AUD Register Declaration
 *****************************************************************************/
typedef struct {
	volatile uint32_t AUD_ADDA_CTL;                           /*!<  Register,  Address offset: 0x00 */
	volatile uint32_t AUD_HPO_CTL;                            /*!<  Register,  Address offset: 0x04 */
	volatile uint32_t AUD_MICBIAS_CTL0;                       /*!<  Register,  Address offset: 0x08 */
	volatile uint32_t AUD_MICBIAS_CTL1;                       /*!<  Register,  Address offset: 0x0C */
	volatile uint32_t AUD_MICBST_CTL0;                        /*!<  Register,  Address offset: 0x010 */
	volatile uint32_t AUD_MICBST_CTL1;                        /*!<  Register,  Address offset: 0x014 */
	volatile uint32_t RSVD0;                                  /*!<  Reserved,  Address offset:0x18 */
	volatile uint32_t AUD_DTS_CTL;                            /*!<  Register,  Address offset: 0x01C */
	volatile uint32_t AUD_MBIAS_CTL0;                         /*!<  Register,  Address offset: 0x020 */
	volatile uint32_t AUD_MBIAS_CTL1;                         /*!<  Register,  Address offset: 0x024 */
	volatile uint32_t AUD_MBIAS_CTL2;                         /*!<  Register,  Address offset: 0x028 */
	volatile uint32_t AUD_HPO_BIAS_CTL;                       /*!<  Register,  Address offset: 0x02C */
} AUD_TypeDef;

#define AUD_SYS_BASE ((AUD_TypeDef *) (0x4100C100))

/** @} */
/* AUTO_GEN_END */

/* MANUAL_GEN_START */

//Please add your defination here

/* MANUAL_GEN_END */

#endif
