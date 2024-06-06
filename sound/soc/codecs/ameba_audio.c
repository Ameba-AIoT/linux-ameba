// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/io.h>
#include "ameba_audio.h"

/*micros for amic*/
#define DEAL_UNMUTE_ADC_INPUT_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp &= ~(AUD_BIT_ADC_##x##_AD_MIX_MUTE | AUD_MASK_ADC_##x##_AD_SRC_SEL);\
										tmp |= AUD_ADC_##x##_AD_SRC_SEL(sel_adc_sdm_num-1); \
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

/*micros for amic*/
#define DEAL_MUTE_ADC_INPUT_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp |= AUD_BIT_ADC_##x##_AD_MIX_MUTE;\
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

/*micros for amic*/
#define DEAL_ADC_ENABLE_DCHPF_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp &= ~AUD_MASK_ADC_##x##_DCHPF_FC_SEL;\
										tmp |= AUD_BIT_ADC_##x##_DCHPF_EN | AUD_ADC_##x##_DCHPF_FC_SEL(2);\
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

/*micros for amic*/
#define DEAL_ADC_DISABLE_DCHPF_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp &= ~AUD_BIT_ADC_##x##_DCHPF_EN;\
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

void audio_codec_mute_adc_input(bool NewState, u32 ad_channel_num, u32 sel_adc_sdm_num,void __iomem	* audio_base_addr)
{
	u32 tmp;

	if (NewState == false) {
		switch (ad_channel_num)
		{
			case AD_CHANNEL_0:
				DEAL_UNMUTE_ADC_INPUT_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_UNMUTE_ADC_INPUT_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_UNMUTE_ADC_INPUT_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_UNMUTE_ADC_INPUT_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_UNMUTE_ADC_INPUT_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_UNMUTE_ADC_INPUT_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_UNMUTE_ADC_INPUT_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_UNMUTE_ADC_INPUT_FUNC(7);
				break;
			default:
				break;
		}
	}else{
		switch (ad_channel_num){
			case AD_CHANNEL_0:
				DEAL_MUTE_ADC_INPUT_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_MUTE_ADC_INPUT_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_MUTE_ADC_INPUT_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_MUTE_ADC_INPUT_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_MUTE_ADC_INPUT_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_MUTE_ADC_INPUT_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_MUTE_ADC_INPUT_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_MUTE_ADC_INPUT_FUNC(7);
				break;
			default:
				break;
		}

	}

}

void audio_codec_enable_adc_dchpf(bool NewState, u32 ad_channel_num, void __iomem	* audio_base_addr)
{
	u32 tmp;

	if (NewState == false) {
		switch (ad_channel_num)
		{
			case AD_CHANNEL_0:
				DEAL_ADC_DISABLE_DCHPF_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_ADC_DISABLE_DCHPF_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_ADC_DISABLE_DCHPF_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_ADC_DISABLE_DCHPF_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_ADC_DISABLE_DCHPF_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_ADC_DISABLE_DCHPF_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_ADC_DISABLE_DCHPF_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_ADC_DISABLE_DCHPF_FUNC(7);
				break;
			default:
				break;
		}
	}else{
		switch (ad_channel_num){
			case AD_CHANNEL_0:
				DEAL_ADC_ENABLE_DCHPF_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_ADC_ENABLE_DCHPF_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_ADC_ENABLE_DCHPF_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_ADC_ENABLE_DCHPF_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_ADC_ENABLE_DCHPF_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_ADC_ENABLE_DCHPF_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_ADC_ENABLE_DCHPF_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_ADC_ENABLE_DCHPF_FUNC(7);
				break;
			default:
				break;
		}

	}

}

/*micros for dmic*/
#define DEAL_UNMUTE_DMIC_INPUT_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp &= ~(AUD_BIT_ADC_##x##_DMIC_MIX_MUTE | AUD_MASK_ADC_##x##_DMIC_SRC_SEL);\
										tmp |= AUD_ADC_##x##_DMIC_SRC_SEL(sel_dmic_num-1); \
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

/*micros for dmic*/
#define DEAL_MUTE_DMIC_INPUT_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp |= AUD_BIT_ADC_##x##_DMIC_MIX_MUTE;\
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

/*micros for dmic*/
#define DEAL_DMIC_ENABLE_DCHPF_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp &= ~AUD_MASK_ADC_##x##_DCHPF_FC_SEL;\
										tmp |= AUD_BIT_ADC_##x##_DCHPF_EN | AUD_ADC_##x##_DCHPF_FC_SEL(7);\
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

/*micros for dmic*/
#define DEAL_DMIC_DISABLE_DCHPF_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_ADC_##x##_CONTROL_0);\
										tmp &= ~AUD_BIT_ADC_##x##_DCHPF_EN;\
										writel(tmp, audio_base_addr + CODEC_ADC_##x##_CONTROL_0); }

void audio_codec_mute_dmic_input(bool NewState, u32 ad_channel_num, u32 sel_dmic_num, void __iomem	* audio_base_addr)
{
	u32 tmp;

	if (NewState == false) {
		switch (ad_channel_num)
		{
			case AD_CHANNEL_0:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_UNMUTE_DMIC_INPUT_FUNC(7);
				break;
			default:
				break;
		}
	}else{
		switch (ad_channel_num){
			case AD_CHANNEL_0:
				DEAL_MUTE_DMIC_INPUT_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_MUTE_DMIC_INPUT_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_MUTE_DMIC_INPUT_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_MUTE_DMIC_INPUT_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_MUTE_DMIC_INPUT_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_MUTE_DMIC_INPUT_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_MUTE_DMIC_INPUT_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_MUTE_DMIC_INPUT_FUNC(7);
				break;
			default:
				break;
		}

	}
}

void audio_codec_enable_dmic_dchpf(bool NewState, u32 ad_channel_num, void __iomem	* audio_base_addr)
{
	u32 tmp;

	if (NewState == false) {
		switch (ad_channel_num)
		{
			case AD_CHANNEL_0:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_DMIC_DISABLE_DCHPF_FUNC(7);
				break;
			default:
				break;
		}
	}else{
		switch (ad_channel_num){
			case AD_CHANNEL_0:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(0);
				break;
			case AD_CHANNEL_1:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(1);
				break;
			case AD_CHANNEL_2:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(2);
				break;
			case AD_CHANNEL_3:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(3);
				break;
			case AD_CHANNEL_4:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(4);
				break;
			case AD_CHANNEL_5:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(5);
				break;
			case AD_CHANNEL_6:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(6);
				break;
			case AD_CHANNEL_7:
				DEAL_DMIC_ENABLE_DCHPF_FUNC(7);
				break;
			default:
				break;
		}

	}

}

/**
  * @brief  Select i2s rx and i2s rx tdm mode
  * @param  i2s: select i2s: i2s0 or i2s1
  *			 This parameter can be one of the following values:
  *			   @arg I2S0
  *			   @arg I2S1
  * @param  tdmmode: select adc input path channel
  *			 This parameter can be one of the following values:
  *			   @arg I2S_NOTDM
  *			   @arg I2S_TDM4
  *			   @arg I2S_TDM6
  *			   @arg I2S_TDM8
  * @return  None
  */
static void audio_codec_set_i2s_rx_tdm(u32 i2s, u32 tdmmode,void __iomem	* audio_base_addr)
{
	u32 tmp1;
	u32 tmp2;
	if (i2s == I2S0) {
		//pr_info("%s,I2S0",__func__);
		tmp1 = readl(audio_base_addr + CODEC_I2S_0_CONTROL);
		tmp1 &= ~(AUD_MASK_I2S_0_TDM_MODE_RX);
		tmp2 = readl(audio_base_addr + CODEC_I2S_0_CONTROL_1);

		if (tdmmode == I2S_NOTDM) {
			tmp1 |= AUD_I2S_0_TDM_MODE_RX(0);
			tmp2 &= ~(AUD_MASK_I2S_0_DATA_CH0_SEL_RX | AUD_MASK_I2S_0_DATA_CH1_SEL_RX);
			tmp2 |= (AUD_I2S_0_DATA_CH0_SEL_RX(0) | AUD_I2S_0_DATA_CH1_SEL_RX(1) | AUD_BIT_I2S_0_DATA_CH2_RX_DISABLE | AUD_BIT_I2S_0_DATA_CH3_RX_DISABLE
					 | AUD_BIT_I2S_0_DATA_CH4_RX_DISABLE | AUD_BIT_I2S_0_DATA_CH5_RX_DISABLE | AUD_BIT_I2S_0_DATA_CH6_RX_DISABLE | AUD_BIT_I2S_0_DATA_CH7_RX_DISABLE);
		} else if (tdmmode == I2S_TDM4) {
			tmp1 |= AUD_I2S_0_TDM_MODE_RX(1);
			tmp2 &= ~(AUD_MASK_I2S_0_DATA_CH0_SEL_RX | AUD_MASK_I2S_0_DATA_CH1_SEL_RX | AUD_MASK_I2S_0_DATA_CH2_SEL_RX | AUD_MASK_I2S_0_DATA_CH3_SEL_RX);
			tmp2 |= (AUD_I2S_0_DATA_CH0_SEL_RX(0) | AUD_I2S_0_DATA_CH1_SEL_RX(1) | AUD_I2S_0_DATA_CH2_SEL_RX(2) | AUD_I2S_0_DATA_CH3_SEL_RX(3)
					 | AUD_BIT_I2S_0_DATA_CH4_RX_DISABLE | AUD_BIT_I2S_0_DATA_CH5_RX_DISABLE | AUD_BIT_I2S_0_DATA_CH6_RX_DISABLE | AUD_BIT_I2S_0_DATA_CH7_RX_DISABLE);
		} else if (tdmmode == I2S_TDM6) {
			tmp1 |= AUD_I2S_0_TDM_MODE_RX(3);
			tmp2 = readl(audio_base_addr + CODEC_I2S_0_CONTROL_1);
			tmp2 &= ~(AUD_MASK_I2S_0_DATA_CH0_SEL_RX | AUD_MASK_I2S_0_DATA_CH1_SEL_RX | AUD_MASK_I2S_0_DATA_CH2_SEL_RX | AUD_MASK_I2S_0_DATA_CH3_SEL_RX
					  | AUD_MASK_I2S_0_DATA_CH4_SEL_RX | AUD_MASK_I2S_0_DATA_CH5_SEL_RX | AUD_MASK_I2S_0_DATA_CH6_SEL_RX | AUD_MASK_I2S_0_DATA_CH7_SEL_RX);
			tmp2 |= (AUD_I2S_0_DATA_CH0_SEL_RX(0) | AUD_I2S_0_DATA_CH1_SEL_RX(1) | AUD_I2S_0_DATA_CH2_SEL_RX(2) | AUD_I2S_0_DATA_CH3_SEL_RX(3)
					 | AUD_I2S_0_DATA_CH4_SEL_RX(4) | AUD_I2S_0_DATA_CH5_SEL_RX(5) | AUD_I2S_0_DATA_CH6_SEL_RX(6) | AUD_I2S_0_DATA_CH7_SEL_RX(7));
		} else {
			tmp1 |= AUD_I2S_0_TDM_MODE_RX(3);
			tmp2 = readl(audio_base_addr + CODEC_I2S_0_CONTROL_1);
			tmp2 &= ~(AUD_MASK_I2S_0_DATA_CH0_SEL_RX | AUD_MASK_I2S_0_DATA_CH1_SEL_RX | AUD_MASK_I2S_0_DATA_CH2_SEL_RX | AUD_MASK_I2S_0_DATA_CH3_SEL_RX
					  | AUD_MASK_I2S_0_DATA_CH4_SEL_RX | AUD_MASK_I2S_0_DATA_CH5_SEL_RX | AUD_MASK_I2S_0_DATA_CH6_SEL_RX | AUD_MASK_I2S_0_DATA_CH7_SEL_RX);
			tmp2 |= (AUD_I2S_0_DATA_CH0_SEL_RX(0) | AUD_I2S_0_DATA_CH1_SEL_RX(1) | AUD_I2S_0_DATA_CH2_SEL_RX(2) | AUD_I2S_0_DATA_CH3_SEL_RX(3)
					 | AUD_I2S_0_DATA_CH4_SEL_RX(4) | AUD_I2S_0_DATA_CH5_SEL_RX(5) | AUD_I2S_0_DATA_CH6_SEL_RX(6) | AUD_I2S_0_DATA_CH7_SEL_RX(7));
		}
		writel(tmp1, audio_base_addr + CODEC_I2S_0_CONTROL);
		writel(tmp2, audio_base_addr + CODEC_I2S_0_CONTROL_1);
	} else {
		//pr_info("%s,I2S1",__func__);
		tmp1 = readl(audio_base_addr + CODEC_I2S_1_CONTROL);
		tmp1 &= ~(AUD_MASK_I2S_1_TDM_MODE_RX);
		tmp2 = readl(audio_base_addr + CODEC_I2S_1_CONTROL_1);

		if (tdmmode == I2S_NOTDM) {
			tmp1 |= AUD_I2S_1_TDM_MODE_RX(0);
			tmp2 &= ~(AUD_MASK_I2S_1_DATA_CH0_SEL_RX | AUD_MASK_I2S_1_DATA_CH1_SEL_RX);
			tmp2 |= (AUD_I2S_1_DATA_CH0_SEL_RX(0) | AUD_I2S_1_DATA_CH1_SEL_RX(1) | AUD_BIT_I2S_1_DATA_CH2_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH3_RX_DISABLE
					 | AUD_BIT_I2S_1_DATA_CH4_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH5_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH6_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH7_RX_DISABLE);
		} else if (tdmmode == I2S_TDM4) {
			tmp1 |= AUD_I2S_1_TDM_MODE_RX(1);
			tmp2 = readl(audio_base_addr + CODEC_I2S_1_CONTROL_1);
			tmp2 &= ~(AUD_MASK_I2S_1_DATA_CH0_SEL_RX | AUD_MASK_I2S_1_DATA_CH1_SEL_RX | AUD_MASK_I2S_1_DATA_CH2_SEL_RX | AUD_MASK_I2S_1_DATA_CH3_SEL_RX);
			tmp2 &= ~(AUD_BIT_I2S_1_DATA_CH2_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH3_RX_DISABLE);
			tmp2 |= (AUD_I2S_1_DATA_CH0_SEL_RX(0) | AUD_I2S_1_DATA_CH1_SEL_RX(1) | AUD_I2S_1_DATA_CH2_SEL_RX(2) | AUD_I2S_1_DATA_CH3_SEL_RX(3)
					 | AUD_BIT_I2S_1_DATA_CH4_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH5_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH6_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH7_RX_DISABLE);
		} else if (tdmmode == I2S_TDM6) {
			tmp1 |= AUD_I2S_1_TDM_MODE_RX(2);
			tmp2 &= ~(AUD_MASK_I2S_1_DATA_CH0_SEL_RX | AUD_MASK_I2S_1_DATA_CH1_SEL_RX | AUD_MASK_I2S_1_DATA_CH2_SEL_RX
					  | AUD_MASK_I2S_1_DATA_CH3_SEL_RX | AUD_MASK_I2S_1_DATA_CH4_SEL_RX | AUD_MASK_I2S_1_DATA_CH5_SEL_RX);
			tmp2 &= ~(AUD_BIT_I2S_1_DATA_CH2_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH3_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH4_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH5_RX_DISABLE);
			tmp2 |= (AUD_I2S_1_DATA_CH0_SEL_RX(0) | AUD_I2S_1_DATA_CH1_SEL_RX(1) | AUD_I2S_1_DATA_CH2_SEL_RX(2) | AUD_I2S_1_DATA_CH3_SEL_RX(3)
					 | AUD_I2S_1_DATA_CH4_SEL_RX(4) | AUD_I2S_1_DATA_CH5_SEL_RX(5) | AUD_BIT_I2S_1_DATA_CH6_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH7_RX_DISABLE);
		} else {
			tmp1 |= AUD_I2S_1_TDM_MODE_RX(3);
			tmp2 &= ~(AUD_MASK_I2S_1_DATA_CH0_SEL_RX | AUD_MASK_I2S_1_DATA_CH1_SEL_RX | AUD_MASK_I2S_1_DATA_CH2_SEL_RX
					  | AUD_MASK_I2S_1_DATA_CH3_SEL_RX | AUD_MASK_I2S_1_DATA_CH4_SEL_RX | AUD_MASK_I2S_1_DATA_CH5_SEL_RX
					  | AUD_MASK_I2S_1_DATA_CH6_SEL_RX | AUD_MASK_I2S_1_DATA_CH7_SEL_RX);
			tmp2 &= ~(AUD_BIT_I2S_1_DATA_CH2_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH3_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH4_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH5_RX_DISABLE
					| AUD_BIT_I2S_1_DATA_CH6_RX_DISABLE | AUD_BIT_I2S_1_DATA_CH7_RX_DISABLE);
			tmp2 |= (AUD_I2S_1_DATA_CH0_SEL_RX(0) | AUD_I2S_1_DATA_CH1_SEL_RX(1) | AUD_I2S_1_DATA_CH2_SEL_RX(2) | AUD_I2S_1_DATA_CH3_SEL_RX(3)
					 | AUD_I2S_1_DATA_CH4_SEL_RX(4) | AUD_I2S_1_DATA_CH5_SEL_RX(5) |  AUD_I2S_1_DATA_CH6_SEL_RX(6) | AUD_I2S_1_DATA_CH7_SEL_RX(7));
		}
		writel(tmp1, audio_base_addr + CODEC_I2S_1_CONTROL);
		writel(tmp2, audio_base_addr + CODEC_I2S_1_CONTROL_1);
	}
}

#define DEAL_SEL_ADC_CHANNEL_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_1);\
										tmp |= AUD_BIT_AD_##x##_EN | AUD_BIT_AD_##x##_FIFO_EN;\
										writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_1);\
										tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_2);\
										tmp |= AUD_BIT_AD_ANA_##x##_EN;\
										writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_2); }

void audio_codec_sel_adc_channel(u32 ad_channel_num,void __iomem	* audio_base_addr)
{
	u32 tmp;
/*
	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_1);
	tmp |= AUD_BIT_AD_X_EN(ad_channel_num) | AUD_BIT_AD_X_FIFO_EN(ad_channel_num);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_1);

	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_2);
	tmp |= AUD_BIT_AD_ANA_X_EN(ad_channel_num);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_2);
*/
	switch (ad_channel_num)
	{
		case AD_CHANNEL_0:
			DEAL_SEL_ADC_CHANNEL_FUNC(0);
			break;
		case AD_CHANNEL_1:
			DEAL_SEL_ADC_CHANNEL_FUNC(1);
			break;
		case AD_CHANNEL_2:
			DEAL_SEL_ADC_CHANNEL_FUNC(2);
			break;
		case AD_CHANNEL_3:
			DEAL_SEL_ADC_CHANNEL_FUNC(3);
			break;
		case AD_CHANNEL_4:
			DEAL_SEL_ADC_CHANNEL_FUNC(4);
			break;
		case AD_CHANNEL_5:
			DEAL_SEL_ADC_CHANNEL_FUNC(5);
			break;
		case AD_CHANNEL_6:
			DEAL_SEL_ADC_CHANNEL_FUNC(6);
			break;
		case AD_CHANNEL_7:
			DEAL_SEL_ADC_CHANNEL_FUNC(7);
			break;
		default:
			break;
	}
}

#define DEAL_SEL_DMIC_CHANNEL_FUNC(x) { \
										tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_1);\
										tmp |= AUD_BIT_AD_##x##_EN | AUD_BIT_AD_##x##_FIFO_EN;\
										writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_1);\
										tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_2);\
										tmp |= AUD_BIT_DMIC_##x##_EN;\
										writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_2); }

void audio_codec_sel_dmic_channel(u32 ad_channel_num,void __iomem	* audio_base_addr)
{
	u32 tmp;

	switch (ad_channel_num)
	{
		case AD_CHANNEL_0:
			DEAL_SEL_DMIC_CHANNEL_FUNC(0);
			break;
		case AD_CHANNEL_1:
			DEAL_SEL_DMIC_CHANNEL_FUNC(1);
			break;
		case AD_CHANNEL_2:
			DEAL_SEL_DMIC_CHANNEL_FUNC(2);
			break;
		case AD_CHANNEL_3:
			DEAL_SEL_DMIC_CHANNEL_FUNC(3);
			break;
		case AD_CHANNEL_4:
			DEAL_SEL_DMIC_CHANNEL_FUNC(4);
			break;
		case AD_CHANNEL_5:
			DEAL_SEL_DMIC_CHANNEL_FUNC(5);
			break;
		case AD_CHANNEL_6:
			DEAL_SEL_DMIC_CHANNEL_FUNC(6);
			break;
		case AD_CHANNEL_7:
			DEAL_SEL_DMIC_CHANNEL_FUNC(7);
			break;
		default:
			break;
	}
}

/**
  * @brief Audio Codec LDO power on or power down.
  * @param  ldo_mode: LDO power on or power down
  *          This parameter can be one of the following values:
  *            @arg LDO_POWER_ON:
  *            @arg LDO_POWER_DOWN:
  * @return  None
  */
void audio_codec_set_ldo_mode(u32 ldo_mode,void __iomem * aud_analog)
{
	u32 tmp;
	if (ldo_mode == LDO_POWER_ON) {
		//pr_info("%s",__func__);
		tmp = readl(aud_analog + ADDA_CTL);
		tmp |= AUD_BIT_POWADDACK;
		writel(tmp,aud_analog + ADDA_CTL);
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp &= ~(AUD_BIT_LDO_PREC);
		tmp |= AUD_BIT_LDO_POW | AUD_BIT_LDO_POW_0P9V;
		writel(tmp,aud_analog + MICBST_CTL1);
		tmp = readl(aud_analog + MICBIAS_CTL0);
		tmp |= AUD_BIT_MBIAS_POW;
		writel(tmp,aud_analog + MICBIAS_CTL0);

		/*AVCC 电压调整至1.798V，作为默认值使用,bit[17:13] = 01110*/
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp &= ~AUD_MASK_LDO_TUNE;
		tmp |= AUD_LDO_TUNE(14);
		writel(tmp,aud_analog + MICBST_CTL1);

	} else {
		tmp = readl(aud_analog + ADDA_CTL);
		tmp &= ~AUD_BIT_POWADDACK;
		writel(tmp,aud_analog + ADDA_CTL);
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp &= ~(AUD_BIT_LDO_POW | AUD_BIT_LDO_POW_0P9V | AUD_BIT_LDO_PREC);
		writel(tmp,aud_analog + MICBST_CTL1);
		tmp = readl(aud_analog + MICBIAS_CTL0);
		tmp &= ~(AUD_BIT_MBIAS_POW);
		writel(tmp,aud_analog + MICBIAS_CTL0);
	}
}

void audio_codec_enable(void __iomem * audio_base_addr,void __iomem	* aud_analog){
	u32 tmp;

	/*Enable audio IP*/
	tmp = readl(audio_base_addr + CODEC_AUDIO_CONTROL_0);
	tmp |= AUD_BIT_AUDIO_IP_EN;
	writel(tmp, audio_base_addr + CODEC_AUDIO_CONTROL_0);

	/*audio analog clk: AD/DA clock power down control*/
	tmp = readl(aud_analog + ADDA_CTL);
	tmp |= AUD_BIT_POWADDACK;
	writel(tmp,aud_analog + ADDA_CTL);

	/*AVCC：enable AVCC LDO, IP default AVCC=1.8V,VREF=0.9V*/
	tmp = readl(aud_analog + MICBST_CTL1);
	tmp &= ~AUD_BIT_LDO_PREC;
	tmp |= AUD_BIT_LDO_POW | AUD_BIT_LDO_POW_0P9V;
	writel(tmp,aud_analog + MICBST_CTL1);

	/*MBIAS: enable mirror bias electric current*/
	tmp = readl(aud_analog + MICBIAS_CTL0);
	tmp |= AUD_BIT_MBIAS_POW;
	writel(tmp,aud_analog + MICBIAS_CTL0);

}

void audio_codec_disable(void __iomem	* aud_analog){
	u32 tmp;

	/*audio analog clk: AD/DA clock power down control*/
	tmp = readl(aud_analog + ADDA_CTL);
	tmp &= ~AUD_BIT_POWADDACK;
	writel(tmp,aud_analog + ADDA_CTL);

	/*AVCC：diable AVCC, AVCC=1.8V,VREF=0.9V*/
	tmp = readl(aud_analog + MICBST_CTL1);
	tmp &= ~(AUD_BIT_LDO_POW | AUD_BIT_LDO_POW_0P9V | AUD_BIT_LDO_PREC);
	writel(tmp,aud_analog + MICBST_CTL1);

	/*MBIAS: disable*/
	tmp = readl(aud_analog + MICBIAS_CTL0);
	tmp &= ~(AUD_BIT_MBIAS_POW);

	writel(tmp,aud_analog + MICBIAS_CTL0);

}

void audio_codec_i2s_enable(i2s_init_params * i2s_params,void __iomem * audio_base_addr,void __iomem	* aud_analog, u32 channel){
	u32 tmp;
	//pr_info("%s,channel:%d",__func__,channel);

	/*Enable I2S*/
	switch(channel){
		case I2S0:
			tmp = readl(audio_base_addr + CODEC_I2S_0_CONTROL);
			tmp &= ~(AUD_BIT_I2S_0_RST_N_REG);
			tmp |= AUD_BIT_I2S_0_RST_N_REG;
			writel(tmp, audio_base_addr + CODEC_I2S_0_CONTROL);
			break;
		case I2S1:
			tmp = readl(audio_base_addr + CODEC_I2S_1_CONTROL);
			tmp &= ~(AUD_BIT_I2S_1_RST_N_REG);
			tmp |= AUD_BIT_I2S_1_RST_N_REG;
			writel(tmp, audio_base_addr + CODEC_I2S_1_CONTROL);
			break;
	}
}

void audio_codec_i2s_disable(i2s_init_params * i2s_params,void __iomem * audio_base_addr,void __iomem	* aud_analog, u32 channel){
	u32 tmp;
	//pr_info("%s,channel:%d",__func__,channel);
	/*Disable I2S*/
	switch(channel){
		case I2S0:
			tmp = readl(audio_base_addr + CODEC_I2S_0_CONTROL);
			tmp &= ~(AUD_BIT_I2S_0_RST_N_REG);
			writel(tmp, audio_base_addr + CODEC_I2S_0_CONTROL);
			break;
		case I2S1:
			tmp = readl(audio_base_addr + CODEC_I2S_1_CONTROL);
			tmp &= ~(AUD_BIT_I2S_1_RST_N_REG);
			writel(tmp, audio_base_addr + CODEC_I2S_1_CONTROL);
			break;
	}
}

void audio_codec_i2s_init(i2s_init_params * i2s_params,void __iomem * audio_base_addr,void __iomem	* aud_analog, u32 channel){
	u32 tmp;
	//pr_info("%s,channel:%d",__func__,channel);

	/*I2S0/I2S1 channel length,data length,data format,channel sel */
	switch(channel){
		case I2S0:
			tmp = readl(audio_base_addr + CODEC_I2S_0_CONTROL);
			tmp &= ~(AUD_MASK_I2S_0_CH_LEN_SEL_TX | AUD_MASK_I2S_0_CH_LEN_SEL_RX | AUD_MASK_I2S_0_DATA_LEN_SEL_TX | AUD_MASK_I2S_0_DATA_LEN_SEL_RX
					 | AUD_MASK_I2S_0_DATA_FORMAT_SEL_TX | AUD_MASK_I2S_0_DATA_FORMAT_SEL_RX | AUD_MASK_I2S_0_DATA_CH_SEL_TX);
			tmp |= (AUD_I2S_0_CH_LEN_SEL_TX(i2s_params->codec_sel_i2s_tx_ch_len) | AUD_I2S_0_CH_LEN_SEL_RX(i2s_params->codec_sel_i2s_rx_ch_len)
					| AUD_I2S_0_DATA_LEN_SEL_TX(i2s_params->codec_sel_i2s_tx_word_len) | AUD_I2S_0_DATA_LEN_SEL_RX(i2s_params->codec_sel_i2s_rx_word_len)
					| AUD_I2S_0_DATA_FORMAT_SEL_TX(i2s_params->codec_sel_i2s_tx_data_format) | AUD_I2S_0_DATA_FORMAT_SEL_RX(
						i2s_params->codec_sel_i2s_rx_data_format)
					| AUD_I2S_0_DATA_CH_SEL_TX(i2s_params->codec_sel_i2s_tx_ch));
			writel(tmp, audio_base_addr + CODEC_I2S_0_CONTROL);

			audio_codec_set_i2s_rx_tdm(I2S0, i2s_params->codec_sel_rx_i2s_tdm,audio_base_addr);

			break;
		case I2S1:
			tmp = readl(audio_base_addr + CODEC_I2S_1_CONTROL);
			tmp &= ~(AUD_MASK_I2S_1_CH_LEN_SEL_RX | AUD_MASK_I2S_1_CH_LEN_SEL_TX | AUD_MASK_I2S_1_DATA_LEN_SEL_TX | AUD_MASK_I2S_1_DATA_LEN_SEL_RX
					 | AUD_MASK_I2S_1_DATA_FORMAT_SEL_TX | AUD_MASK_I2S_1_DATA_FORMAT_SEL_RX | AUD_MASK_I2S_1_DATA_CH_SEL_TX);
			tmp |= (AUD_I2S_1_CH_LEN_SEL_TX(i2s_params->codec_sel_i2s_tx_ch_len) | AUD_I2S_1_CH_LEN_SEL_RX(i2s_params->codec_sel_i2s_rx_ch_len)
					| AUD_I2S_1_DATA_LEN_SEL_TX(i2s_params->codec_sel_i2s_tx_word_len) | AUD_I2S_1_DATA_LEN_SEL_RX(i2s_params->codec_sel_i2s_rx_word_len)
					| AUD_I2S_1_DATA_FORMAT_SEL_TX(i2s_params->codec_sel_i2s_tx_data_format) | AUD_I2S_1_DATA_FORMAT_SEL_RX(
						i2s_params->codec_sel_i2s_rx_data_format)
					|  AUD_I2S_1_DATA_CH_SEL_TX(i2s_params->codec_sel_i2s_tx_ch));
			writel(tmp, audio_base_addr + CODEC_I2S_1_CONTROL);

			audio_codec_set_i2s_rx_tdm(I2S1, i2s_params->codec_sel_rx_i2s_tdm,audio_base_addr);
			break;
	}

}

void audio_codec_i2s_deinit(void __iomem * audio_base_addr, u32 channel){
	u32 tmp;
	if(channel == I2S0){
		/*Disable I2S0*/
		tmp = readl(audio_base_addr + CODEC_I2S_0_CONTROL);
		tmp &= ~(AUD_BIT_I2S_0_RST_N_REG);
		writel(tmp, audio_base_addr + CODEC_I2S_0_CONTROL);
	}
	else{
		/*Disable I2S1*/
		tmp = readl(audio_base_addr + CODEC_I2S_1_CONTROL);
		tmp &= ~(AUD_BIT_I2S_1_RST_N_REG);
		writel(tmp, audio_base_addr + CODEC_I2S_1_CONTROL);
	}
}

void audio_codec_params_init(codec_init_params *codec_init,void __iomem * audio_base_addr,void __iomem * aud_analog)
{
	u32 tmp;
	//pr_info("%s",__func__);

	/*DAC source0,sample rate*/
	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_5);
	tmp &= ~ AUD_MASK_DAC_FS_SRC_SEL;
	tmp |= AUD_DAC_FS_SRC_SEL(0);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_5);

	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_4);
	tmp &= ~ AUD_MASK_SAMPLE_RATE_0;
	tmp |= AUD_SAMPLE_RATE_0(codec_init->codec_dac_sr);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_4);

	if ((codec_init->codec_application & APP_LINE_OUT) == APP_LINE_OUT) {

		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_0);
		tmp &= ~AUD_MASK_DAC_L_DA_SRC_SEL;
		tmp |= AUD_DAC_L_DA_SRC_SEL(codec_init->codec_sel_dac_lin);
		writel(tmp,audio_base_addr + CODEC_DAC_L_CONTROL_0);

		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_0);
		tmp &= ~AUD_MASK_DAC_R_DA_SRC_SEL;
		tmp |= AUD_DAC_R_DA_SRC_SEL(codec_init->codec_sel_dac_rin);
		writel(tmp,audio_base_addr + CODEC_DAC_R_CONTROL_0);

		/*I2S Select*/
		tmp = readl(audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
		if (codec_init->codec_sel_dac_i2s == I2S0) {
			tmp &= ~(AUD_BIT_DAC_I2S_SRC_SEL);
		} else {
			tmp |= AUD_BIT_DAC_I2S_SRC_SEL;
		}
		writel(tmp,audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
	}

}

void audio_codec_init(void __iomem * audio_base_addr,void __iomem * aud_analog)
{
	u32 tmp;
	//pr_info("%s,new codec addr:%p",__func__,audio_base_addr);
	/*default: HPO enable single out*/
	/*DAC enable*/
	tmp = readl(aud_analog + ADDA_CTL);
	tmp &= ~(AUD_BIT_DAC_CKXSEL);
	writel(tmp, aud_analog + ADDA_CTL);
	tmp = readl(aud_analog + ADDA_CTL);
	tmp |= AUD_BIT_DAC_CKXEN | AUD_BIT_DAC_L_POW | AUD_BIT_DAC_R_POW;
	writel(tmp, aud_analog + ADDA_CTL);
	/*HPO Enable Single flow*/
	tmp = readl(audio_base_addr + CODEC_AUDIO_CONTROL_1); /*sel_bb_ck_depop 32kHz*/
	tmp &= ~AUD_MASK_SEL_BB_CK_DEPOP;
	tmp |= AUD_SEL_BB_CK_DEPOP(1) | AUD_BIT_BB_CK_DEPOP_EN; //尝试改为01，pop音问题
	writel(tmp,audio_base_addr + CODEC_AUDIO_CONTROL_1);

	tmp = readl(aud_analog + ADDA_CTL);
	tmp &= ~(AUD_MASK_DPRAMP_CSEL);
	writel(tmp, aud_analog + ADDA_CTL);

	tmp = readl(aud_analog + ADDA_CTL);
	tmp |= AUD_DPRAMP_CSEL(3);
	writel(tmp, aud_analog + ADDA_CTL);

	tmp = readl(aud_analog + HPO_CTL);
	tmp &= ~(AUD_MASK_HPO_ML | AUD_MASK_HPO_MR);
	writel(tmp, aud_analog + HPO_CTL);

	tmp = readl(aud_analog + HPO_CTL);
	tmp |= AUD_BIT_HPO_ENDPL | AUD_BIT_HPO_ENDPR | AUD_HPO_ML(2) | AUD_HPO_MR(2) | AUD_BIT_HPO_L_POW | AUD_BIT_HPO_R_POW;
	writel(tmp, aud_analog + HPO_CTL);

	usleep_range(10,20);

	tmp = readl(aud_analog + HPO_CTL);
	tmp |= AUD_BIT_HPO_MDPL | AUD_BIT_HPO_MDPR	| AUD_BIT_HPO_OPPDPL | AUD_BIT_HPO_OPPDPR;
	writel(tmp, aud_analog + HPO_CTL);

	/*DAC part*/
	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_1);
	tmp |= AUD_BIT_DA_L_EN | AUD_BIT_DA_R_EN | AUD_BIT_MOD_L_EN | AUD_BIT_MOD_R_EN | AUD_BIT_DA_ANA_CLK_EN | AUD_BIT_DA_FIFO_EN;
	writel(tmp,audio_base_addr + CODEC_CLOCK_CONTROL_1);

}
void audio_codec_amic_power(void __iomem	* audio_base_addr,void __iomem	* aud_analog){
		u32 tmp;
		//pr_info("amic power");
		/*Micbias enable*/
		tmp = readl(aud_analog + MICBIAS_CTL0);
		tmp |= AUD_BIT_MICBIAS1_POW;
		writel(tmp, aud_analog + MICBIAS_CTL0);
		/*Micbias power cut enable: 5channel*/
		tmp = readl(aud_analog + MICBIAS_CTL0);
		tmp &= ~(AUD_BIT_MICBIAS1_PCUT1_EN | AUD_BIT_MICBIAS1_PCUT2_EN | AUD_BIT_MICBIAS1_PCUT5_EN);  //add PCUT5 provide power for amic5
		writel(tmp, aud_analog + MICBIAS_CTL0);

		/*Micbias power cut enable: 5channel*/
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp |= AUD_BIT_MICBST_POWL | AUD_BIT_MICBST_POWR;
		writel(tmp, aud_analog + MICBST_CTL0);

		/*added by anna*/
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp |= AUD_BIT_MICBST3_POW;    //micbst3 provide power for ADC5
		writel(tmp, aud_analog + MICBST_CTL1);

		/*DTSDM Enable*/
		tmp = readl(aud_analog + ADDA_CTL);
		tmp |= AUD_BIT_DTSDM_CKXEN | AUD_BIT_DTSDM_POW_L | AUD_BIT_DTSDM_POW_R;
		writel(tmp, aud_analog + ADDA_CTL);

		/*default:5 amic differential mode*/
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp |= AUD_BIT_MICBST_ENDFL | AUD_BIT_MICBST_ENDFR | AUD_BIT_MICBST2_ENDFL	| AUD_BIT_MICBST2_ENDFR;
		writel(tmp, aud_analog + MICBST_CTL0);
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp |= AUD_BIT_MICBST3_ENDF;
		writel(tmp, aud_analog + MICBST_CTL1);

		/*micbst gain select: dufault 30dB*/
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_MASK_MICBST_GSELL | AUD_MASK_MICBST_GSELR);
		writel(tmp, aud_analog + MICBST_CTL0);

		tmp = readl(aud_analog + MICBST_CTL0);
		tmp |= (AUD_MICBST_GSELL(MICBST_GAIN_30DB) | AUD_MICBST_GSELR(MICBST_GAIN_30DB));
		writel(tmp, aud_analog + MICBST_CTL0);
		/*added by anna set micbst3 gain 30dB for ADC5*/
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp &= ~AUD_MASK_MICBST3_GSEL;
		writel(tmp, aud_analog + MICBST_CTL1);

		tmp = readl(aud_analog + MICBST_CTL1);
		tmp |= AUD_MICBST3_GSEL(MICBST_GAIN_30DB);
		writel(tmp, aud_analog + MICBST_CTL1);

		/*unmute amic enable*/
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_MASK_MICBST_MUTE_L | AUD_MASK_MICBST_MUTE_R);
		writel(tmp, aud_analog + MICBST_CTL0);

		tmp = readl(aud_analog + MICBST_CTL0);
		tmp |= ((AUD_MICBST_MUTE_L(2)) | (AUD_MICBST_MUTE_R(2)));
		writel(tmp, aud_analog + MICBST_CTL0);

		/*added by anna unmute amic5 enable*/
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp &= ~AUD_MASK_MICBST3_MUTE;
		writel(tmp, aud_analog + MICBST_CTL1);

		tmp = readl(aud_analog + MICBST_CTL1);
		tmp |= AUD_MICBST3_MUTE(0);
		writel(tmp, aud_analog + MICBST_CTL1);

		/*MICBIAS EANBLE*/
		tmp = readl(audio_base_addr + CODEC_AUDIO_CONTROL_1);
		tmp |= AUD_BIT_CKX_MICBIAS_EN;
		writel(tmp,audio_base_addr + CODEC_AUDIO_CONTROL_1);

		/*micbias_vest 1.8V*/
		tmp = readl(aud_analog + MICBIAS_CTL0);
		tmp &= ~(AUD_MASK_MICBIAS1_VSET);
		writel(tmp, aud_analog + MICBIAS_CTL0);

		tmp = readl(aud_analog + MICBIAS_CTL0);
		tmp |= AUD_MICBIAS1_VSET(7);
		writel(tmp, aud_analog + MICBIAS_CTL0);
}


void audio_codec_amic_init(codec_init_params *codec_init,void __iomem	* audio_base_addr,void __iomem	* aud_analog)
{
	u32 tmp;
	//pr_info("%s,addr:%p",__func__,audio_base_addr);

	/*ADC source1, sample rate*/
	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_5);
	tmp &= ~(AUD_MASK_ADC_0_FS_SRC_SEL | AUD_MASK_ADC_1_FS_SRC_SEL);
	tmp |= AUD_ADC_0_FS_SRC_SEL(1) | AUD_ADC_1_FS_SRC_SEL(1) | AUD_ADC_2_FS_SRC_SEL(1) | AUD_ADC_3_FS_SRC_SEL(1)
		   | AUD_ADC_4_FS_SRC_SEL(1) | AUD_ADC_5_FS_SRC_SEL(1) | AUD_ADC_6_FS_SRC_SEL(1) | AUD_ADC_7_FS_SRC_SEL(1);
	writel(tmp,audio_base_addr + CODEC_CLOCK_CONTROL_5);

	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_4);
	tmp &= ~ AUD_MASK_SAMPLE_RATE_1;
	tmp |= AUD_SAMPLE_RATE_1(codec_init->codec_adc_sr);
	//pr_info("codec_init->codec_adc_sr:%d,samplerate tmp:%x",codec_init->codec_adc_sr,tmp);
	writel(tmp,audio_base_addr + CODEC_CLOCK_CONTROL_4);

	/* ================= CODEC initialize ======================== */
	if ((codec_init->codec_application & APP_AMIC_IN) == APP_AMIC_IN) {/*Normal mode: default 2amic*/

		/*ADC part*/
		tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_1);
		tmp |= AUD_BIT_AD_ANA_CLK_EN | AUD_BIT_DA_ANA_CLK_EN;
		writel(tmp,audio_base_addr + CODEC_CLOCK_CONTROL_1);

		if (codec_init->codec_sel_adc_i2s == I2S0) {
			tmp = readl(audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
			tmp &= ~(AUD_BIT_ADC_0_I2S_SRC_SEL | AUD_BIT_ADC_1_I2S_SRC_SEL | AUD_BIT_ADC_2_I2S_SRC_SEL | AUD_BIT_ADC_3_I2S_SRC_SEL
					 | AUD_BIT_ADC_4_I2S_SRC_SEL | AUD_BIT_ADC_5_I2S_SRC_SEL | AUD_BIT_ADC_6_I2S_SRC_SEL | AUD_BIT_ADC_7_I2S_SRC_SEL);
			writel(tmp,audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
		} else {
			tmp = readl(audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
			tmp |= (AUD_BIT_ADC_0_I2S_SRC_SEL | AUD_BIT_ADC_1_I2S_SRC_SEL | AUD_BIT_ADC_2_I2S_SRC_SEL | AUD_BIT_ADC_3_I2S_SRC_SEL
					| AUD_BIT_ADC_4_I2S_SRC_SEL | AUD_BIT_ADC_5_I2S_SRC_SEL | AUD_BIT_ADC_6_I2S_SRC_SEL | AUD_BIT_ADC_7_I2S_SRC_SEL);
			writel(tmp,audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
		}
	}
}

void audio_codec_dmic_init(codec_init_params *codec_init,void __iomem	* audio_base_addr,void __iomem	* aud_analog)
{
	u32 tmp;

	pr_info("%s \n", __func__);
	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_3);
	tmp &= ~ AUD_MASK_DMIC_CLK_SEL;
	tmp |= AUD_BIT_DMIC_CLK_EN | AUD_DMIC_CLK_SEL(DMIC_2P5M);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_3);
	/* Dmic delay: steady time*/
	msleep(100);

	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_4);
	tmp &= ~ AUD_MASK_SAMPLE_RATE_1;
	tmp |= AUD_SAMPLE_RATE_1(codec_init->codec_adc_sr);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_4);

	if (codec_init->codec_sel_adc_i2s == I2S0) {
		tmp = readl(audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
		tmp &= ~(AUD_BIT_ADC_0_I2S_SRC_SEL | AUD_BIT_ADC_1_I2S_SRC_SEL | AUD_BIT_ADC_2_I2S_SRC_SEL | AUD_BIT_ADC_3_I2S_SRC_SEL
					 | AUD_BIT_ADC_4_I2S_SRC_SEL | AUD_BIT_ADC_5_I2S_SRC_SEL | AUD_BIT_ADC_6_I2S_SRC_SEL | AUD_BIT_ADC_7_I2S_SRC_SEL);
		writel(tmp, audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);

	} else {
		tmp = readl(audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);
		tmp |= (AUD_BIT_ADC_0_I2S_SRC_SEL | AUD_BIT_ADC_1_I2S_SRC_SEL | AUD_BIT_ADC_2_I2S_SRC_SEL | AUD_BIT_ADC_3_I2S_SRC_SEL
					| AUD_BIT_ADC_4_I2S_SRC_SEL | AUD_BIT_ADC_5_I2S_SRC_SEL | AUD_BIT_ADC_6_I2S_SRC_SEL | AUD_BIT_ADC_7_I2S_SRC_SEL);
		writel(tmp, audio_base_addr + CODEC_I2S_AD_SEL_CONTROL);

	}

}

void audio_codec_sel_dmic_clk(u32 clk, void __iomem	* audio_base_addr)
{
	u32 tmp;
	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_3);
	tmp &= ~ AUD_MASK_DMIC_CLK_SEL;
	tmp |= AUD_BIT_DMIC_CLK_EN | AUD_DMIC_CLK_SEL(clk);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_3);
}

/**
  * @brief  Set amic in to single end mode.
  * @param  None
  * @return  None
  */
void audio_codec_set_amic_in_se(u32 Amic_sel,void __iomem	*  aud_analog)
{
	u32 tmp;
	if (Amic_sel == AMIC1) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_BIT_MICBST_ENDFL);
		writel(tmp, aud_analog + MICBST_CTL0);
	} else if (Amic_sel == AMIC2) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_BIT_MICBST_ENDFR);
		writel(tmp, aud_analog + MICBST_CTL0);
	} else if (Amic_sel == AMIC3) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_BIT_MICBST2_ENDFL);
		writel(tmp, aud_analog + MICBST_CTL0);
	} else if (Amic_sel == AMIC4) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_BIT_MICBST2_ENDFR);
		writel(tmp, aud_analog + MICBST_CTL0);
	} else {
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp &= ~(AUD_BIT_MICBST3_ENDF);
		writel(tmp, aud_analog + MICBST_CTL1);
	}
}

/**
  * @brief  Set line in to single end mode. Only amic1 and amic2 support linein.
  * @param  None
  * @return  None
  */
void audio_codec_set_line_in_se(u32 Linein_sel,void __iomem	*  aud_analog)
{
	u32 tmp;
	if (Linein_sel == LINEIN1) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_BIT_MICBST_ENDFL);
		writel(tmp, aud_analog + MICBST_CTL0);
	} else if (Linein_sel == LINEIN2) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_BIT_MICBST_ENDFR);
		writel(tmp, aud_analog + MICBST_CTL0);
	}
}

/**
  * @brief  mute or unmute per ad channel.
  * @param  Amic_sel: select amic channel
  *			 This parameter can be one of the following values:
  *			   @arg Amic1
  *			   @arg Amic2
  *			   @arg Amic3
  *			   @arg Amic4
  *			   @arg Amic5
  * @param  NewState: mute or unmute option for ad channel
  *			 This parameter can be one of the following values:
  *			   @arg false: unmute
  *			   @arg true: mute
  * @return  None
  */
void audio_codec_mute_amic_in(bool NewState, u32 Amic_sel,void __iomem * aud_analog)
{
	u32 tmp;
	if (NewState == false) {
		if (Amic_sel == AMIC1) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp &= ~AUD_MASK_MICBST_MUTE_L;
			tmp |= AUD_MICBST_MUTE_L(2);
			writel(tmp,aud_analog + MICBST_CTL0);
		} else if (Amic_sel == AMIC2) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp &= ~AUD_MASK_MICBST_MUTE_R;
			tmp |= AUD_MICBST_MUTE_R(2);
			writel(tmp,aud_analog + MICBST_CTL0);
		} else if (Amic_sel == AMIC3) {
			tmp = readl(aud_analog + MICBST_CTL1);
			tmp &= ~AUD_MASK_MICBST2_MUTE_L;
			writel(tmp,aud_analog + MICBST_CTL1);
		} else if (Amic_sel == AMIC4) {
			tmp = readl(aud_analog + MICBST_CTL1);
			tmp &= ~AUD_MASK_MICBST2_MUTE_R;
			writel(tmp,aud_analog + MICBST_CTL1);
		} else {
			tmp = readl(aud_analog + MICBST_CTL1);
			tmp &= ~ AUD_MASK_MICBST3_MUTE;
			tmp |= AUD_MICBST3_MUTE(0);
			writel(tmp,aud_analog + MICBST_CTL1);
		}
	} else {
		if (Amic_sel == AMIC1) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp |= AUD_MASK_MICBST_MUTE_L;
			writel(tmp,aud_analog + MICBST_CTL0);
		} else if (Amic_sel == AMIC2) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp |= AUD_MASK_MICBST_MUTE_R;
			writel(tmp,aud_analog + MICBST_CTL0);
		} else if (Amic_sel == AMIC3) {
			tmp = readl(aud_analog + MICBST_CTL1);
			tmp |= AUD_MASK_MICBST2_MUTE_L;
			writel(tmp,aud_analog + MICBST_CTL1);
		} else if (Amic_sel == AMIC4) {
			tmp = readl(aud_analog + MICBST_CTL1);
			tmp |= AUD_MASK_MICBST2_MUTE_R;
			writel(tmp,aud_analog + MICBST_CTL1);
		} else {
			tmp = readl(aud_analog + MICBST_CTL1);
			tmp |= AUD_MASK_MICBST3_MUTE;
			writel(tmp,aud_analog + MICBST_CTL1);
		}
	}
}

/**
  * @brief  mute or unmute per ad channel.
  * @param  Linein_sel: select amic channel
  *			 This parameter can be one of the following values:
  *			   @arg LINEIN1
  *			   @arg LINEIN2
  * @param  NewState: mute or unmute option for ad channel
  *			 This parameter can be one of the following values:
  *			   @arg false: unmute
  *			   @arg true: mute
  * @return  None
  */
void audio_codec_mute_line_in(bool NewState, u32 Linein_sel,void __iomem * aud_analog)
{
	u32 tmp;
	if (NewState == false) {
		if (Linein_sel == LINEIN1) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp &= ~ AUD_MASK_MICBST_MUTE_L;
			tmp |= AUD_MICBST_MUTE_L(1);
			writel(tmp,aud_analog + MICBST_CTL0);
		} else if (Linein_sel == LINEIN2) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp &= ~ AUD_MASK_MICBST_MUTE_R;
			tmp |= AUD_MICBST_MUTE_R(1);
			writel(tmp,aud_analog + MICBST_CTL0);
		}
	} else {
		if (Linein_sel == LINEIN1) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp |= AUD_MASK_MICBST_MUTE_L;
			writel(tmp,aud_analog + MICBST_CTL0);
		} else if (Linein_sel == LINEIN2) {
			tmp = readl(aud_analog + MICBST_CTL0);
			tmp |= AUD_MASK_MICBST_MUTE_R;
			writel(tmp,aud_analog + MICBST_CTL0);
		}
	}
}

/**
  * @brief  Set codec ADC gain.
  * @param  Amic_sel: ADC channel select
  *          This parameter can be one of the following values:
  *            @arg Amic1
  *            @arg Amic2
  *            @arg Amic3
  *            @arg Amic4
  *            @arg Amic5
  * @param  gain: ADC channel micbst gain select
  *          This parameter can be one of the following values:
  *            @arg MICBST_GAIN_0DB: 0dB
  *            @arg MICBST_Gain_5dB: 5dB
  *            ...
  *            @arg MICBST_Gain_40dB: 40dB
  * @note ADC digital volume is 0dB~+40dB in 5dB step.
  * @return  None
  */
void audio_codec_set_adc_gain(u32 Amic_sel, u32 gain,void __iomem * aud_analog)
{
	u32 tmp;
	//pr_info("%s",__func__);
	if (Amic_sel == AMIC1) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_MASK_MICBST_GSELL);
		tmp |= AUD_MICBST_GSELL(gain);
		writel(tmp,aud_analog + MICBST_CTL0);
	} else if (Amic_sel == AMIC2) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_MASK_MICBST_GSELR);
		tmp |= AUD_MICBST_GSELR(gain);
		writel(tmp,aud_analog + MICBST_CTL0);
	} else if (Amic_sel == AMIC3) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_MASK_MICBST2_GSELL);
		tmp |= AUD_MICBST2_GSELL(gain);
		writel(tmp,aud_analog + MICBST_CTL0);
	} else if (Amic_sel == AMIC4) {
		tmp = readl(aud_analog + MICBST_CTL0);
		tmp &= ~(AUD_MASK_MICBST2_GSELR);
		tmp |= AUD_MICBST2_GSELR(gain);
		writel(tmp,aud_analog + MICBST_CTL0);
	} else {
		tmp = readl(aud_analog + MICBST_CTL1);
		tmp &= ~(AUD_MASK_MICBST3_GSEL);
		tmp |= AUD_MICBST3_GSEL(gain);
		writel(tmp,aud_analog + MICBST_CTL1);
	}
}


/**
  * @brief  Set codec DMIC clk.
  * @param  clk: dmic clk select
  *          This parameter can be one of the following values:
  *            @arg DMIC_5M
  *            @arg DMIC_2P5M
  *            @arg DMIC_1P25M
  *            @arg DMIC_625K
  *            @arg DMIC_312P5K
  *            @arg DMIC_769P2K
  * @return  None
  */
void audio_codec_set_dmic_clk(u32 clk, void __iomem	* audio_base_addr)
{
	u32 tmp;
	tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_3);
	tmp &= ~ AUD_MASK_DMIC_CLK_SEL;
	tmp |= AUD_BIT_DMIC_CLK_EN | AUD_DMIC_CLK_SEL(clk);
	writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_3);
}

/**
  * @brief  Set codec PDM clk.
  * @param  clk: PDM clk select
  *          This parameter can be one of the following values:
  *            @arg PDM_5M
  *            @arg PDM_2P5M
  *            @arg PDM_6P67M
  * @param  NewState: pdm enable or disable
  *			 This parameter can be one of the following values:
  *			   @arg false
  *			   @arg true

  * @return  None
  */
void audio_codec_set_pdm(bool NewState, u32 clk, void __iomem	* audio_base_addr)
{
	u32 tmp;
	if (NewState == true) {
		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_1);
		tmp |= AUD_BIT_DAC_L_PDM_EN;
		writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_1);

		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_1);
		tmp |= AUD_BIT_DAC_R_PDM_EN;
		writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_1);

		tmp = readl(audio_base_addr + CODEC_AUDIO_CONTROL_1);
		tmp &= ~ AUD_MASK_PDM_CLK_SEL;
		tmp |= AUD_PDM_CLK_SEL(clk);
		writel(tmp, audio_base_addr + CODEC_AUDIO_CONTROL_1);
	} else {
		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_1);
		tmp &= ~AUD_BIT_DAC_L_PDM_EN;
		writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_1);

		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_1);
		tmp &= ~AUD_BIT_DAC_R_PDM_EN;
		writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_1);
	}
}

void audio_codec_set_adc_volume_by_channel(u32 channel, u32 Gain, void __iomem	* audio_base_addr)
{

	u32 tmp;
	tmp = readl(audio_base_addr + CODEC_ADC_0_CONTROL_1);
	tmp &= ~(0xff << 0);
	tmp |= (Gain << 0);

	switch (channel){
		case AD_CHANNEL_0:
			writel(tmp, audio_base_addr + CODEC_ADC_0_CONTROL_1);
			break;
		case AD_CHANNEL_1:
			writel(tmp, audio_base_addr + CODEC_ADC_1_CONTROL_1);
			break;
		case AD_CHANNEL_2:
			writel(tmp, audio_base_addr + CODEC_ADC_2_CONTROL_1);
			break;
		case AD_CHANNEL_3:
			writel(tmp, audio_base_addr + CODEC_ADC_3_CONTROL_1);
			break;
		case AD_CHANNEL_4:
			writel(tmp, audio_base_addr + CODEC_ADC_4_CONTROL_1);
			break;
		case AD_CHANNEL_5:
			writel(tmp, audio_base_addr + CODEC_ADC_5_CONTROL_1);
			break;
		case AD_CHANNEL_6:
			writel(tmp, audio_base_addr + CODEC_ADC_6_CONTROL_1);
			break;
		case AD_CHANNEL_7:
			writel(tmp, audio_base_addr + CODEC_ADC_7_CONTROL_1);
			break;
		default:
			break;
	}

}


/**
  * @brief  Set the gain of ADC digital volume.
  * @param  tdmmode: the value of tdm mode.
  *          This parameter can be one of the following values:
  *            @arg I2S_NOTDM
  *            @arg I2S_TDM4
  *            @arg I2S_TDM6
  *            @arg I2S_TDM8
  * @param  Gain: the value of gain.
  *          This parameter can be -17.625dB~48dB in 0.375dB step
  *            @arg 0x00 -17.625dB
  *            @arg 0xaf 48dB
  * @retval None
  */
void audio_codec_set_adc_volume(u32 tdmmode, u32 Gain, void __iomem	* audio_base_addr)
{

	u32 tmp;
	tmp = readl(audio_base_addr + CODEC_ADC_0_CONTROL_1);
	tmp &= ~(0xff << 0);
	tmp |= (Gain << 0);

	if (tdmmode == I2S_NOTDM) {
		writel(tmp, audio_base_addr + CODEC_ADC_0_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_1_CONTROL_1);
	} else if (tdmmode == I2S_TDM4) {
		writel(tmp, audio_base_addr + CODEC_ADC_0_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_1_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_2_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_3_CONTROL_1);
	} else if (tdmmode == I2S_TDM6) {
		writel(tmp, audio_base_addr + CODEC_ADC_0_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_1_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_2_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_3_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_4_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_5_CONTROL_1);
	} else {
		writel(tmp, audio_base_addr + CODEC_ADC_0_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_1_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_2_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_3_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_4_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_5_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_6_CONTROL_1);
		writel(tmp, audio_base_addr + CODEC_ADC_7_CONTROL_1);
	}
}

void audio_codec_set_adc_mute(u32 channel,bool state, void __iomem	* audio_base_addr){
	u32 tmp;
	//pr_info("%s",__func__);

	if(state == false){
		switch(channel){
			case 0:
				tmp = readl(audio_base_addr + CODEC_ADC_0_CONTROL_0);
				tmp &= ~AUD_BIT_ADC_0_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_0_CONTROL_0);
				break;
			case 1:
				tmp = readl(audio_base_addr + CODEC_ADC_1_CONTROL_0);
				tmp &= ~AUD_BIT_ADC_1_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_1_CONTROL_0);
				break;
			case 2:
				tmp = readl(audio_base_addr + CODEC_ADC_2_CONTROL_0);
				tmp &= ~AUD_BIT_ADC_2_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_2_CONTROL_0);
				break;
			case 3:
				tmp = readl(audio_base_addr + CODEC_ADC_3_CONTROL_0);
				tmp &= ~AUD_BIT_ADC_3_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_3_CONTROL_0);
				break;
			case 4:
				tmp = readl(audio_base_addr + CODEC_ADC_4_CONTROL_0);
				tmp &= ~AUD_BIT_ADC_4_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_4_CONTROL_0);
				break;
			case 5:
				tmp = readl(audio_base_addr + CODEC_ADC_5_CONTROL_0);
				tmp &= ~AUD_BIT_ADC_5_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_5_CONTROL_0);
				break;
			case 6:
				tmp = readl(audio_base_addr + CODEC_ADC_6_CONTROL_0);
				tmp &= ~AUD_BIT_ADC_6_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_6_CONTROL_0);
				break;
			default:
				break;
		}
	}
	else{
		switch(channel){
			case 0:
				tmp = readl(audio_base_addr + CODEC_ADC_0_CONTROL_0);
				tmp |= AUD_BIT_ADC_0_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_0_CONTROL_0);
				break;
			case 1:
				tmp = readl(audio_base_addr + CODEC_ADC_1_CONTROL_0);
				tmp |= AUD_BIT_ADC_1_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_1_CONTROL_0);
				break;
			case 2:
				tmp = readl(audio_base_addr + CODEC_ADC_2_CONTROL_0);
				tmp |= AUD_BIT_ADC_2_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_2_CONTROL_0);
				break;
			case 3:
				tmp = readl(audio_base_addr + CODEC_ADC_3_CONTROL_0);
				tmp |= AUD_BIT_ADC_3_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_3_CONTROL_0);
				break;
			case 4:
				tmp = readl(audio_base_addr + CODEC_ADC_4_CONTROL_0);
				tmp |= AUD_BIT_ADC_4_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_4_CONTROL_0);
				break;
			case 5:
				tmp = readl(audio_base_addr + CODEC_ADC_5_CONTROL_0);
				tmp |= AUD_BIT_ADC_5_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_5_CONTROL_0);
				break;
			case 6:
				tmp = readl(audio_base_addr + CODEC_ADC_6_CONTROL_0);
				tmp |= AUD_BIT_ADC_6_AD_MUTE;
				writel(tmp, audio_base_addr + CODEC_ADC_6_CONTROL_0);
				break;
			default:
				break;
		}
	}

}
/**
  * @brief  Set ADC ASRC MODE.
  * @param  tdmmode: the value of tdm mode.
  *          This parameter can be one of the following values:
  *            @arg I2S_NOTDM
  *            @arg I2S_TDM4
  *            @arg I2S_TDM6
  *            @arg I2S_TDM8
  *			 This parameter can be one of the following values:
  *			   @arg false
  *			   @arg true
  * @retval None
  */
void audio_codec_set_adc_asrc(bool NewState, u32 tdmmode, void __iomem	* audio_base_addr)
{

	u32 tmp;
	if (NewState == true) {
		if (tdmmode == I2S_NOTDM) {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp |= AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN;
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);

		} else if (tdmmode == I2S_TDM4) {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp |= AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN | AUD_BIT_ADC_2_ASRC_EN | AUD_BIT_ADC_3_ASRC_EN;
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);

		} else if (tdmmode == I2S_TDM6) {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp |= AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN | AUD_BIT_ADC_2_ASRC_EN | AUD_BIT_ADC_3_ASRC_EN
				   | AUD_BIT_ADC_4_ASRC_EN | AUD_BIT_ADC_5_ASRC_EN;
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);

		} else {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp |= AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN | AUD_BIT_ADC_2_ASRC_EN | AUD_BIT_ADC_3_ASRC_EN
				   | AUD_BIT_ADC_4_ASRC_EN | AUD_BIT_ADC_5_ASRC_EN | AUD_BIT_ADC_6_ASRC_EN	| AUD_BIT_ADC_7_ASRC_EN;
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);
		}
	} else {
		if (tdmmode == I2S_NOTDM) {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp &= ~(AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN);
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);

		} else if (tdmmode == I2S_TDM4) {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp &= ~(AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN | AUD_BIT_ADC_2_ASRC_EN | AUD_BIT_ADC_3_ASRC_EN);
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);

		} else if (tdmmode == I2S_TDM6) {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp &= ~(AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN | AUD_BIT_ADC_2_ASRC_EN | AUD_BIT_ADC_3_ASRC_EN
					 | AUD_BIT_ADC_4_ASRC_EN | AUD_BIT_ADC_5_ASRC_EN);
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);

		} else {
			tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
			tmp &= ~(AUD_BIT_ADC_0_ASRC_EN | AUD_BIT_ADC_1_ASRC_EN | AUD_BIT_ADC_2_ASRC_EN | AUD_BIT_ADC_3_ASRC_EN
					 | AUD_BIT_ADC_4_ASRC_EN | AUD_BIT_ADC_5_ASRC_EN | AUD_BIT_ADC_6_ASRC_EN	| AUD_BIT_ADC_7_ASRC_EN);
			writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);
		}
	}
}


/**
  * @brief  Set the gain of DAC digital volume.
  * @param  channel: the value of dac path.
  *          This parameter can be one of the following values:
  *            @arg 0: DAC_L
  *            @arg 1: DAC_R
  * @param  Gain: the value of gain.
  *          This parameter can be -62.5dB~0dB in 0.375dB step
  *            @arg 0x00 -65.625dB
  *            @arg 0xaf 0dB
  * @retval None
  */
void audio_codec_set_dac_volume(u32 channel, u32 Gain, void __iomem	* audio_base_addr)
{
	u32 tmp;
	//pr_info("%s",__func__);
	if (channel == 0) {
		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_0);
		tmp &= ~ AUD_MASK_DAC_L_DA_GAIN;
		tmp |= AUD_DAC_L_DA_GAIN(Gain);
		writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_0);
	} else {
		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_0);
		tmp &= ~ AUD_MASK_DAC_R_DA_GAIN;
		tmp |= AUD_DAC_R_DA_GAIN(Gain);
		writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_0);
	}
}

u32 audio_codec_get_dac_volume(u32 channel, void __iomem	* audio_base_addr){
	u32 tmp;
	//pr_info("%s channel:%d",__func__,channel);
	if (channel == 0) {
		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_0);
		tmp &=  AUD_MASK_DAC_L_DA_GAIN;
	} else {
		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_0);
		tmp &=  AUD_MASK_DAC_R_DA_GAIN;
	}
	return tmp;
}

void audio_codec_zdet_init(int timeout_level, void __iomem	* audio_base_addr){
	u32 tmp;

	tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_1);
	tmp &= ~AUD_MASK_DAC_L_DA_ZDET_FUNC;
	tmp |= AUD_DAC_L_DA_ZDET_FUNC(DA_ZDET_FUNC_ZDET_AND_STEP);
	writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_1);

	tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_1);
	tmp &= ~AUD_MASK_DAC_R_DA_ZDET_FUNC;
	tmp |= AUD_DAC_R_DA_ZDET_FUNC(DA_ZDET_FUNC_ZDET_AND_STEP);
	writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_1);

	tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_1);
	tmp &= ~AUD_MASK_DAC_L_DA_ZDET_TOUT;
	tmp |= AUD_DAC_L_DA_ZDET_TOUT(timeout_level);
	writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_1);

	tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_1);
	tmp &= ~AUD_MASK_DAC_R_DA_ZDET_TOUT;
	tmp |= AUD_DAC_R_DA_ZDET_TOUT(timeout_level);
	writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_1);
}

void audio_codec_set_dac_mute(u32 channel, bool mute, void __iomem	* audio_base_addr){
	u32 tmp;
	if (mute == true) {
		if(channel == 0){
			tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_1);
			tmp |= AUD_BIT_DAC_L_MUSIC_MUTE_EN;
			writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_1);
		}
		else{
			tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_1);
			tmp |= AUD_BIT_DAC_R_MUSIC_MUTE_EN;
			writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_1);
		}
	} else {
		if(channel == 0){
			tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_1);
			tmp &= ~AUD_BIT_DAC_L_MUSIC_MUTE_EN;
			writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_1);
		}
		else{
			tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_1);
			tmp &= ~AUD_BIT_DAC_R_MUSIC_MUTE_EN;
			writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_1);
		}
	}
}

bool  audio_codec_get_dac_mute(u32 channel, void __iomem	* audio_base_addr){
	u32 tmp;
	if (channel == 0) {
		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_1);
		tmp &= AUD_BIT_DAC_L_MUSIC_MUTE_EN;
	} else {
		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_1);
		tmp &= AUD_BIT_DAC_R_MUSIC_MUTE_EN;
	}
	return (tmp > 0)? true : false;
}

/**
  * @brief  Enable or disable DAC ASRC mode.
  *			 This parameter can be one of the following values:
  *			   @arg false
  *			   @arg true
  * @retval None
  */
void audio_codec_set_dac_asrc(bool NewState, void __iomem	* audio_base_addr)
{
	u32 tmp;
	//pr_info("%s",__func__);
	if (NewState == true) {
		tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
		tmp |= AUD_BIT_DAC_ASRC_EN;
		writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);

	} else {
		tmp = readl(audio_base_addr + CODEC_CLOCK_CONTROL_6);
		tmp &= ~AUD_BIT_DAC_ASRC_EN;
		writel(tmp, audio_base_addr + CODEC_CLOCK_CONTROL_6);
	}
}

/**
  * @brief  Set adc power on or down and select power mode.
  * @param  adc_sel: select amic channel
  *			 This parameter can be one of the following values:
  *			   @arg ADC1
  *			   @arg ADC2
  *			   @arg ADC3
  *			   @arg ADC4
  *			   @arg ADC5
   * @param  powermode: normal or low power mode
  *			 This parameter can be one of the following values:
  *			   @arg NormalPower
  *			   @arg LowPower
  *			   @arg Shutdown
  * @return  None
  */
void audio_codec_set_adc_mode(u32 adc_sel, u32 powermode,void __iomem	*  aud_analog)
{
	u32 tmp_MICBIAS_CTL0 = readl(aud_analog + MICBIAS_CTL0);
	u32 tmp_MICBST_CTL0 = readl(aud_analog + MICBST_CTL0);
	u32 tmp_MICBST_CTL1 = readl(aud_analog + MICBST_CTL1);
	u32 tmp_ADDA_CTL = readl(aud_analog + ADDA_CTL);
	u32 tmp_MBIAS_CTL1 = readl(aud_analog + MBIAS_CTL1);
	u32 tmp_MBIAS_CTL0 = readl(aud_analog + MBIAS_CTL0);
	u32 tmp_MBIAS_CTL2 = readl(aud_analog + MBIAS_CTL2);
	u32 tmp_DTS_CTL = readl(aud_analog + DTS_CTL);

	if (powermode == NORMALPOWER) {
		if (adc_sel == ADC1) {
			tmp_MICBIAS_CTL0 &= ~AUD_BIT_MICBIAS1_PCUT1_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST_POWL;
			tmp_ADDA_CTL |= AUD_BIT_DTSDM_CKXEN | AUD_BIT_DTSDM_POW_L;
			tmp_MBIAS_CTL1 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM_L | AUD_MASK_MBIAS_ISEL_DTSDM_INT1_L | AUD_MASK_MBIAS_ISEL_MICBST_L);
			tmp_MBIAS_CTL1 |= AUD_MBIAS_ISEL_DTSDM_L(6) | AUD_MBIAS_ISEL_DTSDM_INT1_L(6) | AUD_MBIAS_ISEL_MICBST_L(6);
		} else if (adc_sel == ADC2) {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT2_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST_POWR;
			tmp_ADDA_CTL |= AUD_BIT_DTSDM_CKXEN | AUD_BIT_DTSDM_POW_R;
			tmp_MBIAS_CTL1 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM_R | AUD_MASK_MBIAS_ISEL_DTSDM_INT1_R | AUD_MASK_MBIAS_ISEL_MICBST_R);
			tmp_MBIAS_CTL1 |= AUD_MBIAS_ISEL_DTSDM_R(6) | AUD_MBIAS_ISEL_DTSDM_INT1_R(6) | AUD_MBIAS_ISEL_MICBST_R(6);
		} else if (adc_sel == ADC3) {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT3_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST2_POWL;
			tmp_DTS_CTL |= AUD_BIT_DTSDM2_CKXEN | AUD_BIT_DTSDM2_POW_L;
			tmp_MBIAS_CTL0 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM2_L | AUD_MASK_MBIAS_ISEL_DTSDM2_INT1_L);
			tmp_MBIAS_CTL0 |= AUD_MBIAS_ISEL_DTSDM2_L(6) | AUD_MBIAS_ISEL_DTSDM2_INT1_L(6);
			tmp_MBIAS_CTL2 &= ~AUD_MASK_MBIAS_ISEL_MICBST2_L;
			tmp_MBIAS_CTL2 |= AUD_MBIAS_ISEL_MICBST2_L(6);
		} else if (adc_sel == ADC4) {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT4_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST2_POWR;
			tmp_DTS_CTL |= AUD_BIT_DTSDM2_CKXEN | AUD_BIT_DTSDM2_POW_R;
			tmp_MBIAS_CTL0 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM2_R | AUD_MASK_MBIAS_ISEL_DTSDM2_INT1_R);
			tmp_MBIAS_CTL0 |= AUD_MBIAS_ISEL_DTSDM2_R(6) | AUD_MBIAS_ISEL_DTSDM2_INT1_R(6);
			tmp_MBIAS_CTL2 &= ~AUD_MASK_MBIAS_ISEL_MICBST2_R;
			tmp_MBIAS_CTL2 |= AUD_MBIAS_ISEL_MICBST2_R(6);
		} else {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT5_EN;
			tmp_MICBST_CTL1 |= AUD_BIT_MICBST3_POW;
			tmp_DTS_CTL |= AUD_BIT_DTSDM3_CKXEN | AUD_BIT_DTSDM3_POW;
			tmp_MBIAS_CTL0 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM3 | AUD_MASK_MBIAS_ISEL_DTSDM3_INT1);
			tmp_MBIAS_CTL0 |= AUD_MBIAS_ISEL_DTSDM3(6) | AUD_MBIAS_ISEL_DTSDM3_INT1(6);
			tmp_MBIAS_CTL2 &= ~AUD_MASK_MBIAS_ISEL_MICBST3;
			tmp_MBIAS_CTL2 |= AUD_MBIAS_ISEL_MICBST3(6);
		}
	} else if (powermode == LOWPOWER) {
		tmp_DTS_CTL |= AUD_BIT_LPMODE_EN;

		if (adc_sel == ADC1) {
			tmp_MICBIAS_CTL0 &= ~AUD_BIT_MICBIAS1_PCUT1_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST_POWL;
			tmp_ADDA_CTL |= AUD_BIT_DTSDM_CKXEN | AUD_BIT_DTSDM_POW_L;
			tmp_MBIAS_CTL1 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM_L | AUD_MASK_MBIAS_ISEL_DTSDM_INT1_L | AUD_MASK_MBIAS_ISEL_MICBST_L);
			tmp_MBIAS_CTL1 |= AUD_MBIAS_ISEL_DTSDM_L(0) | AUD_MBIAS_ISEL_DTSDM_INT1_L(0) | AUD_MBIAS_ISEL_MICBST_L(0);
		} else if (adc_sel == ADC2) {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT2_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST_POWR;
			tmp_ADDA_CTL |= AUD_BIT_DTSDM_CKXEN | AUD_BIT_DTSDM_POW_R;
			tmp_MBIAS_CTL1 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM_R | AUD_MASK_MBIAS_ISEL_DTSDM_INT1_R | AUD_MASK_MBIAS_ISEL_MICBST_R);
			tmp_MBIAS_CTL1 |= AUD_MBIAS_ISEL_DTSDM_R(0) | AUD_MBIAS_ISEL_DTSDM_INT1_R(0) | AUD_MBIAS_ISEL_MICBST_R(0);
		} else if (adc_sel == ADC3) {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT3_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST2_POWL;
			tmp_DTS_CTL |= AUD_BIT_DTSDM2_CKXEN | AUD_BIT_DTSDM2_POW_L;
			tmp_MBIAS_CTL0 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM2_L | AUD_MASK_MBIAS_ISEL_DTSDM2_INT1_L);
			tmp_MBIAS_CTL0 |= AUD_MBIAS_ISEL_DTSDM2_L(1) | AUD_MBIAS_ISEL_DTSDM2_INT1_L(1);
			tmp_MBIAS_CTL2 &= ~AUD_MASK_MBIAS_ISEL_MICBST2_L;
			tmp_MBIAS_CTL2 |= AUD_MBIAS_ISEL_MICBST2_L(1);
		}  else if (adc_sel == ADC4) {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT4_EN;
			tmp_MICBST_CTL0 |= AUD_BIT_MICBST2_POWR;
			tmp_DTS_CTL |= AUD_BIT_DTSDM2_CKXEN | AUD_BIT_DTSDM2_POW_R;
			tmp_MBIAS_CTL0 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM2_R | AUD_MASK_MBIAS_ISEL_DTSDM2_INT1_R);
			tmp_MBIAS_CTL0 |= AUD_MBIAS_ISEL_DTSDM2_R(1) | AUD_MBIAS_ISEL_DTSDM2_INT1_R(1);
			tmp_MBIAS_CTL2 &= ~AUD_MASK_MBIAS_ISEL_MICBST2_R;
			tmp_MBIAS_CTL2 |= AUD_MBIAS_ISEL_MICBST2_R(1);
		} else {
			tmp_MICBIAS_CTL0 &= ~ AUD_BIT_MICBIAS1_PCUT5_EN;
			tmp_MICBST_CTL1 |= AUD_BIT_MICBST3_POW;
			tmp_DTS_CTL |= AUD_BIT_DTSDM3_CKXEN | AUD_BIT_DTSDM3_POW;
			tmp_MBIAS_CTL0 &= ~(AUD_MASK_MBIAS_ISEL_DTSDM3 | AUD_MASK_MBIAS_ISEL_DTSDM3_INT1);
			tmp_MBIAS_CTL0 |= AUD_MBIAS_ISEL_DTSDM3(1) | AUD_MBIAS_ISEL_DTSDM3_INT1(1);
			tmp_MBIAS_CTL2 &= ~AUD_MASK_MBIAS_ISEL_MICBST3;
			tmp_MBIAS_CTL2 |= AUD_MBIAS_ISEL_MICBST3(1);
		}
	} else {
		if (adc_sel == ADC1) {
			/*Micbias power cut/Micbst power control/DTSDM enable*/
			/*NOTE: AUD_BIT_DTSDM_CKXEN暂时采用Enable状态，以单独控制ADC1 OR ADC2的disable状态*/
			tmp_MICBIAS_CTL0 |= AUD_BIT_MICBIAS1_PCUT1_EN;
			tmp_MICBST_CTL0 &= ~ AUD_BIT_MICBST_POWL;
			tmp_ADDA_CTL &= ~(AUD_BIT_DTSDM_POW_L);
		} else if (adc_sel == ADC2) {
			tmp_MICBIAS_CTL0 |= AUD_BIT_MICBIAS1_PCUT2_EN;
			tmp_MICBST_CTL0 &= ~ AUD_BIT_MICBST_POWR;
			tmp_ADDA_CTL &= ~(AUD_BIT_DTSDM_POW_R);
		} else if (adc_sel == ADC3) {
			tmp_MICBIAS_CTL0 |= AUD_BIT_MICBIAS1_PCUT3_EN;
			tmp_MICBST_CTL0 &= ~ AUD_BIT_MICBST2_POWL;
			tmp_DTS_CTL &= ~(AUD_BIT_DTSDM2_POW_L);
		} else if (adc_sel == ADC4) {
			tmp_MICBIAS_CTL0 |= AUD_BIT_MICBIAS1_PCUT4_EN;
			tmp_MICBST_CTL0 &= ~ AUD_BIT_MICBST2_POWR;
			tmp_DTS_CTL &= ~(AUD_BIT_DTSDM2_POW_R);
		} else {
			tmp_MICBIAS_CTL0 |= AUD_BIT_MICBIAS1_PCUT5_EN;
			tmp_MICBST_CTL0 &= ~ AUD_BIT_MICBST3_POW;
			tmp_DTS_CTL &= ~(AUD_BIT_DTSDM3_POW);
		}
	}

	writel(tmp_MICBIAS_CTL0, aud_analog + MICBIAS_CTL0);
	writel(tmp_MICBST_CTL0, aud_analog + MICBST_CTL0);
	writel(tmp_ADDA_CTL,aud_analog + ADDA_CTL);
	writel(tmp_MBIAS_CTL1,aud_analog + MBIAS_CTL1);
	writel(tmp_MBIAS_CTL0,aud_analog + MBIAS_CTL0);
	writel(tmp_MBIAS_CTL2,aud_analog + MBIAS_CTL2);
	writel(tmp_DTS_CTL,aud_analog + DTS_CTL);
	writel(tmp_MICBST_CTL1,aud_analog + MICBST_CTL1);
}

/**
  * @brief  Set HPO Poweron differential mode.
  * @return  None
  */
void audio_codec_set_hpo_diff(void __iomem * aud_analog)
{
	u32 tmp;
	tmp = readl(aud_analog + HPO_CTL);
	tmp |= AUD_BIT_HPO_ENAL | AUD_BIT_HPO_ENAR | AUD_BIT_HPO_L_POW | AUD_BIT_HPO_R_POW;
	tmp &= ~(AUD_BIT_HPO_ENDPL | AUD_BIT_HPO_ENDPR);
	tmp |= AUD_BIT_HPO_L_POW | AUD_BIT_HPO_R_POW;
	tmp &= ~(AUD_MASK_HPO_ML | AUD_MASK_HPO_MR);
	tmp |= AUD_HPO_ML(2) | AUD_HPO_MR(2);
	tmp &= ~(AUD_BIT_HPO_SEL | AUD_BIT_HPO_SER);
	writel(tmp, aud_analog + HPO_CTL);
}

void audio_codec_set_hpo_single_end(void __iomem * aud_analog){
	u32 tmp;
	tmp = readl(aud_analog + HPO_CTL);
	tmp |= AUD_BIT_HPO_SEL | AUD_BIT_HPO_SER;
	writel(tmp, aud_analog + HPO_CTL);
}

/**
  * @brief  DAC Testtone mode.
  * @param  fc: the value of test frequency.
  *          This parameter can be one of the following values:
  *          @arg fc
  *          frequency= (fs/192)*(fc+1)Hz
  * @param  NewState: the status of testtone mode.
  *          This parameter can be the following values:
  *            @arg true
  *            @arg false
  * @retval None
  */

void audio_codec_set_test_tone(bool NewState, u32 fc, void __iomem	* audio_base_addr)
{
	u32 tmp;
	tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_0);
	tmp &= ~AUD_MASK_DAC_L_DA_SRC_SEL;
	tmp |= AUD_DAC_L_DA_SRC_SEL(TESTTONE);
	writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_0);

	tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_0);
	tmp &= ~AUD_MASK_DAC_R_DA_SRC_SEL;
	tmp |= AUD_DAC_R_DA_SRC_SEL(TESTTONE);
	writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_0);

	if (NewState == true) {

		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_0);
		tmp &= ~ AUD_MASK_DAC_L_TEST_FC_SEL;
		tmp |= AUD_BIT_DAC_L_TEST_TONE_EN | (AUD_DAC_L_TEST_FC_SEL(fc));
		writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_0);

		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_0);
		tmp &= ~ AUD_MASK_DAC_R_TEST_FC_SEL;
		tmp |= AUD_BIT_DAC_R_TEST_TONE_EN | (AUD_DAC_R_TEST_FC_SEL(fc));
		writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_0);

	} else {

		tmp = readl(audio_base_addr + CODEC_DAC_L_CONTROL_0);
		tmp &= ~ AUD_BIT_DAC_L_TEST_TONE_EN;
		writel(tmp, audio_base_addr + CODEC_DAC_L_CONTROL_0);

		tmp = readl(audio_base_addr + CODEC_DAC_R_CONTROL_0);
		tmp &= ~ AUD_BIT_DAC_R_TEST_TONE_EN;
		writel(tmp, audio_base_addr + CODEC_DAC_R_CONTROL_0);	//0x9c DAC_test tone
	}
}

void audio_codec_deinit(void __iomem	* audio_base_addr,void __iomem	* aud_analog)
{
	u32 tmp;

	/* ================= CODEC initialize ======================== */
	/*default: Disable HPO single out*/
	tmp = readl(aud_analog + HPO_CTL);
	tmp &= ~(AUD_BIT_HPO_L_POW | AUD_BIT_HPO_R_POW) ;
	writel(tmp, aud_analog + HPO_CTL);

	tmp = readl(aud_analog + ADDA_CTL);
	tmp &= ~ AUD_BIT_DPRAMP_POW;
	writel(tmp, aud_analog + ADDA_CTL);
	/*Disable DAC*/
	tmp = readl(aud_analog + ADDA_CTL);
	tmp &= ~(AUD_BIT_DAC_L_POW | AUD_BIT_DAC_R_POW);
	writel(tmp, aud_analog + ADDA_CTL);

}

void audio_codec_dmic_deinit(void __iomem	* audio_base_addr,void __iomem	* aud_analog)
{

}

void audio_codec_amic_deinit(void __iomem	* audio_base_addr,void __iomem	* aud_analog)
{
	u32 tmp;

	/*Disable Micbias*/
	tmp = readl(aud_analog + MICBIAS_CTL0);
	tmp &= ~AUD_BIT_MICBIAS1_POW;
	writel(tmp, aud_analog + MICBIAS_CTL0);
	/*Disable Micbias power cut: 5channel*/
	tmp = readl(aud_analog + MICBIAS_CTL0);
	tmp &= ~(AUD_BIT_MICBIAS1_PCUT1_EN | AUD_BIT_MICBIAS1_PCUT2_EN);
	writel(tmp, aud_analog + MICBIAS_CTL0);
	/*Disable Micbias power cut: 5channel*/
	tmp = readl(aud_analog + MICBST_CTL0);
	tmp &= ~(AUD_BIT_MICBST_POWL | AUD_BIT_MICBST_POWR);
	writel(tmp, aud_analog + MICBST_CTL0);
	/*Disable DTSDM*/
	tmp = readl(aud_analog + ADDA_CTL);
	tmp &= ~(AUD_BIT_DTSDM_CKXEN | AUD_BIT_DTSDM_POW_L | AUD_BIT_DTSDM_POW_R);
	writel(tmp, aud_analog + ADDA_CTL);

}


void audio_codec_set_dac_asrc_rate(int rate, void __iomem *audio_base_addr)
{
	u32 tmp;
	u32 sel;

	if (rate < 60000) {
		sel = 0;
	} else if (rate < 120000) {
		sel = 1;
	} else {
		sel = 2;
	}

	tmp = readl(audio_base_addr + CODEC_ASRC_CONTROL_0);
	tmp &= ~AUD_MASK_ASRC_RATE_SEL_TX;
	tmp |= AUD_ASRC_RATE_SEL_TX(sel);
	writel(tmp, audio_base_addr + CODEC_ASRC_CONTROL_0);
}