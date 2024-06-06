// SPDX-License-Identifier: GPL-2.0-only
/*
* Realtek ALSA support
*
* Copyright (C) 2021, Realtek Corporation. All rights reserved.
*/

#include <linux/string.h>
#include <linux/types.h>
#include "ameba_sport.h"
#include "ameba_gdma.h"

#define _memset memset
#define assert_param(expr) ((expr) ? (void)0 : printk("io_assert issue:%s,%d",__func__,__LINE__))

void audio_sp_set_i2s_mode(void __iomem * sportx, u32 mode)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_CTRL0);
	if (mode == I2S_MODE_SLAVE) {
		tmp |= SP_BIT_SLAVE_DATA_SEL;
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp |= SP_BIT_SLAVE_CLK_SEL;
		writel(tmp, sportx + REG_SP_CTRL0);
	} else {
		tmp &= ~SP_BIT_SLAVE_DATA_SEL;
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp &= ~SP_BIT_SLAVE_CLK_SEL;
		writel(tmp, sportx + REG_SP_CTRL0);
	}
}

void audio_sp_set_mclk_clk_div(void __iomem * sportx, u32 divider)
{
	u32 tmp;
	u32 div_set = MCLK_DIV_1;
	switch(divider) {
		case 1:
			div_set = MCLK_DIV_1;
			break;
		case 2:
			div_set = MCLK_DIV_2;
			break;
		case 4:
			div_set = MCLK_DIV_4;
			break;
		default:
			pr_info("unsupported mclk div:%d", divider);
			break;
	}
	tmp = readl(sportx + REG_SP_CTRL0);
	tmp &= ~SP_MASK_MCLK_SEL;
	tmp |= SP_MCLK_SEL(div_set);
	writel(tmp, sportx + REG_SP_CTRL0);
}

void audio_sp_set_tx_clk_div(void __iomem * sportx, unsigned long clock, u32 sr, u32 tdm, u32 chn_len, u32 multi_io)
{
	u32 tmp;
	u32 MI;
	u32 NI;
	u32 Bclk_div;
	int ni = 1;

	u32 channel_count = 2;
	if (multi_io == SP_TX_MULTIIO_DIS)
		channel_count = (tdm + 1) * 2;

	switch (clock) {
	case 40000000:
		if (multi_io == SP_TX_MULTIIO_EN)
			tdm = SP_TX_NOTDM;
		if ((!(sr % 4000)) && (sr % 11025)) {  //48k系列
			MI = 1250;
			NI = (chn_len / 4) * (tdm + 1) * (sr / 4000);
		} else {
			if (chn_len == 16 || chn_len == 32) {
				MI = 50000;
				NI = (441 * (chn_len / 4) * (tdm + 1) * (sr / 11025)) / 4;
			} else if (chn_len == 20) {
				MI = 40000;
				NI = (441 * (chn_len / 4) * (tdm + 1) * (sr / 11025)) / 5;
			} else {
				MI = 60000;
				NI = (441 * 3 * (chn_len / 4) * (tdm + 1) * (sr / 11025)) / 10;
			}
		}
		break;
	default:  //for other plls:
		NI = 1;
		if (clock * NI % (channel_count * chn_len * sr) == 0) {
			MI = clock * NI / (channel_count * chn_len * sr);
		} else {
			for (; ni < 65536 ; ni++) {
				if (clock * ni % (channel_count * chn_len * sr) == 0) {
					NI = ni;
					MI = clock * NI / (channel_count * chn_len * sr);
					break;
				}
			}
		}
	}

	switch (chn_len) {
	case SP_CL_16:
		Bclk_div = 32;
		break;
	case SP_CL_20:
		Bclk_div = 40;
		break;
	case SP_CL_24:
		Bclk_div = 48;
		break;
	default:
		Bclk_div = 64;
		break;

	}

	//pr_info("%s: ni:%d, mi:%d \n", __func__, NI, MI);
	assert_param(NI <= 0xFFFF);
	assert_param(MI <= 0xFFFF);

	tmp = readl(sportx + REG_SP_SR_TX_BCLK);
	tmp &= ~(SP_MASK_TX_MI | SP_MASK_TX_NI | SP_BIT_TX_MI_NI_UPDATE);
	tmp |= (SP_TX_MI(MI) | SP_TX_NI(NI) | SP_BIT_TX_MI_NI_UPDATE);
	writel(tmp, sportx + REG_SP_SR_TX_BCLK);

	if (tdm == SP_TX_NOTDM) {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_TX_BCLK_DIV_RATIO);
		tmp |= SP_TX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	} else if (tdm == SP_TX_TDM4) {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_TX_BCLK_DIV_RATIO);
		if (multi_io == SP_RX_MULTIIO_DIS)
			tmp |= SP_TX_BCLK_DIV_RATIO(Bclk_div * 2 - 1);
		else
			tmp |= SP_TX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	} else if (tdm == SP_TX_TDM6) {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_TX_BCLK_DIV_RATIO);
		if (multi_io == SP_RX_MULTIIO_DIS)
			tmp |= SP_TX_BCLK_DIV_RATIO(Bclk_div * 3 - 1);
		else
			tmp |= SP_TX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	} else {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_TX_BCLK_DIV_RATIO);
		if (multi_io == SP_RX_MULTIIO_DIS)
			tmp |= SP_TX_BCLK_DIV_RATIO(Bclk_div * 4 - 1);
		else
			tmp |= SP_TX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	}
}

void audio_sp_set_rx_clk_div(void __iomem * sportx, u32 clock, u32 sr, u32 tdm, u32 chn_len, u32 multi_io)
{
	u32 tmp;
	u32 NI;
	u32 MI;
	u32 Bclk_div;
	int ni = 1;

	u32 channel_count = 2;
	if (multi_io == SP_RX_MULTIIO_DIS)
		channel_count = (tdm + 1) * 2;

	switch (clock) {
	case 40000000:
		if (multi_io == SP_RX_MULTIIO_EN)
			tdm = SP_RX_NOTDM;
		if ((!(sr % 4000)) && (sr % 11025)) {  //48k系列
			MI = 1250;
			NI = (chn_len / 4) * (tdm + 1) * (sr / 4000);
		} else {
			if (chn_len == 16 || chn_len == 32) {
				MI = 50000;
				NI = (441 * (chn_len / 4) * (tdm + 1) * (sr / 11025)) / 4;
			} else if (chn_len == 20) {
				MI = 40000;
				NI = (441 * (chn_len / 4) * (tdm + 1) * (sr / 11025)) / 5;
			} else {
				MI = 60000;
				NI = (441 * 3 * (chn_len / 4) * (tdm + 1) * (sr / 11025)) / 10;
			}
		}
		break;
	default:  //for other plls:
		NI = 1;
		if (clock * NI % (channel_count * chn_len * sr) == 0) {
			MI = clock * NI / (channel_count * chn_len * sr);
		} else {
			for (; ni < 65536 ; ni++) {
				if (clock * ni % (channel_count * chn_len * sr) == 0) {
					NI = ni;
					MI = clock * NI / (channel_count * chn_len * sr);
					break;
				}
			}
		}
	}

	assert_param(MI <= 0xFFFF);
	assert_param(NI <= 0xFFFF);

	tmp = readl(sportx + REG_SP_RX_BCLK);
	tmp &= ~(SP_MASK_RX_MI | SP_MASK_RX_NI | SP_BIT_RX_MI_NI_UPDATE);
	tmp |= (SP_RX_MI(MI) | SP_RX_NI(NI) | SP_BIT_RX_MI_NI_UPDATE);
	writel(tmp, sportx + REG_SP_RX_BCLK);

	switch (chn_len) {
	case SP_CL_16:
		Bclk_div = 32;
		break;
	case SP_CL_20:
		Bclk_div = 40;
		break;
	case SP_CL_24:
		Bclk_div = 48;
		break;
	default:
		Bclk_div = 64;
		break;
	}

	if (tdm == SP_RX_NOTDM) {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_RX_BCLK_DIV_RATIO);
		tmp |= SP_RX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	} else if (tdm == SP_RX_TDM4) {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_RX_BCLK_DIV_RATIO);
		if (multi_io == SP_RX_MULTIIO_DIS)
			tmp |= SP_RX_BCLK_DIV_RATIO(Bclk_div * 2 - 1);
		else
			tmp |= SP_RX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	} else if (tdm == SP_TX_TDM6) {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_RX_BCLK_DIV_RATIO);
		if (multi_io == SP_RX_MULTIIO_DIS)
			tmp |= SP_RX_BCLK_DIV_RATIO(Bclk_div * 3 - 1);
		else
			tmp |= SP_RX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	} else {
		tmp = readl(sportx + REG_SP_TX_LRCLK);
		tmp &= ~(SP_MASK_RX_BCLK_DIV_RATIO);
		if (multi_io == SP_RX_MULTIIO_DIS)
			tmp |= SP_RX_BCLK_DIV_RATIO(Bclk_div * 4 - 1);
		else
			tmp |= SP_RX_BCLK_DIV_RATIO(Bclk_div - 1);
		writel(tmp, sportx + REG_SP_TX_LRCLK);
	}
}

void audio_sp_enable(void __iomem * sportx){
	u32 tmp;

	tmp = readl(sportx + REG_SP_CTRL0);
	tmp |= SP_BIT_RESET;
	writel(tmp, sportx + REG_SP_CTRL0);
	tmp &= ~ SP_BIT_RESET;
	writel(tmp, sportx + REG_SP_CTRL0);

	tmp = readl(sportx + REG_SP_CTRL1);
	tmp |= SP_BIT_ENABLE_MCLK;
	writel(tmp, sportx + REG_SP_CTRL1);

}

void audio_sp_disable(void __iomem * sportx){

}

void audio_sp_rx_init(void __iomem * sportx, sport_init_params *SP_RXInitStruct)
{
	u32 tmp;
	u32 chn_len;

	/* Check the parameters*/
	assert_param(IS_SP_DATA_FMT(SP_RXInitStruct->sp_sel_data_format));
	assert_param(IS_SP_RX_WL(SP_RXInitStruct->sp_sel_word_len));
	assert_param(IS_SP_RXCH_LEN(SP_RXInitStruct->sp_sel_ch_len));
	assert_param(IS_SP_SEL_RX_CH(SP_RXInitStruct->sp_sel_ch));
	assert_param(IS_SP_SEL_RX_TDM(SP_RXInitStruct->sp_sel_tdm));
	assert_param(IS_SP_SEL_RX_FIFO(SP_RXInitStruct->sp_sel_fifo));
	assert_param(IS_SP_CHN_NUM(SP_RXInitStruct->sp_sel_i2s0_mono_stereo));
	assert_param(IS_SP_CHN_NUM(SP_RXInitStruct->sp_sel_i2s1_mono_stereo));

	switch (SP_RXInitStruct->sp_sel_ch_len) {
	case 0:
		chn_len = 16;
		break;
	case 1:
		chn_len = 20;
		break;
	case 2:
		chn_len = 24;
		break;
	default:
		chn_len = 32;
		break;
	}

	audio_sp_enable(sportx);

	/* Configure parameters: disable RX, disable TX, AUDIO SPORT mode */
	audio_sp_tx_start(sportx, false);
	audio_sp_rx_start(sportx, false);

	/* Configure FIFO0 parameters: data format, word length, channel number, etc */
	tmp = readl(sportx + REG_SP_CTRL0);
	tmp &= ~(SP_MASK_SEL_I2S_RX_CH | SP_BIT_TRX_SAME_CH | SP_MASK_TDM_MODE_SEL_RX);
	writel(tmp, sportx + REG_SP_CTRL0);
	tmp |= SP_SEL_I2S_RX_CH(SP_RXInitStruct->sp_sel_ch);
	writel(tmp, sportx + REG_SP_CTRL0);

	/* Configure parameters:Channel Length, data format*/
	tmp = readl(sportx + REG_SP_FORMAT);
	tmp &= ~(SP_MASK_CH_LEN_SEL_RX | SP_MASK_DATA_FORMAT_SEL_RX | SP_BIT_EN_I2S_MONO_RX_0 | SP_MASK_DATA_LEN_SEL_RX_0 |
						   SP_BIT_TRX_SAME_CH_LEN | SP_BIT_TRX_SAME_CH | SP_BIT_TRX_SAME_LENGTH);
	writel(tmp, sportx + REG_SP_FORMAT);
	tmp |= ((SP_CH_LEN_SEL_RX(SP_RXInitStruct->sp_sel_ch_len)) | (SP_DATA_FORMAT_SEL_RX(
							  SP_RXInitStruct->sp_sel_data_format))
						  | (SP_DATA_LEN_SEL_RX_0(SP_RXInitStruct->sp_sel_word_len)));
	writel(tmp, sportx + REG_SP_FORMAT);

	//to be checked for 24bit...
	tmp = readl(sportx + REG_SP_CTRL0);
	tmp &= ~(SP_MASK_DATA_LEN_SEL_TX_0 | SP_MASK_DATA_FORMAT_SEL_TX);
	tmp |= ((SP_DATA_FORMAT_SEL_TX(SP_RXInitStruct->sp_sel_data_format))
						 | (SP_DATA_LEN_SEL_TX_0(SP_RXInitStruct->sp_sel_word_len)));
	writel(tmp, sportx + REG_SP_CTRL0);
	//to be checked for 24bit...

	if (SP_RXInitStruct->sp_sel_i2s0_mono_stereo == SP_CH_STEREO) {
		tmp = readl(sportx + REG_SP_FORMAT);
		tmp &= ~ SP_BIT_EN_I2S_MONO_RX_0;
		writel(tmp, sportx + REG_SP_FORMAT);
	} else {
		tmp = readl(sportx + REG_SP_FORMAT);
		tmp |= (SP_BIT_EN_I2S_MONO_RX_0);
		writel(tmp, sportx + REG_SP_FORMAT);
	}

	/* Configure FIFO1 parameters*/
	tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
	tmp &= ~SP_MASK_DATA_LEN_SEL_RX_1;
	writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
	tmp |= SP_DATA_LEN_SEL_RX_1(SP_RXInitStruct->sp_sel_word_len);
	writel(tmp, sportx + REG_SP_DIRECT_CTRL1);

	if (SP_RXInitStruct->sp_sel_i2s1_mono_stereo == SP_CH_STEREO) {
		tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
		tmp &= ~ SP_BIT_EN_I2S_MONO_RX_1;
		writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
	} else {
		tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
		tmp |= (SP_BIT_EN_I2S_MONO_RX_1);
		writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
	}

	/* Configure parameters: RX TDM mode */
	if (SP_RXInitStruct->sp_sel_tdm == SP_RX_NOTDM) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_RX(NOTDM);
		writel(tmp, sportx + REG_SP_CTRL0);
	} else if (SP_RXInitStruct->sp_sel_tdm == SP_RX_TDM4) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_RX(TDM4);
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_RX;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_RXInitStruct->sp_sel_tdm == SP_RX_TDM6) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_RX(TDM6);
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_RX;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_RX(TDM8);
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_RX;
		writel(tmp, sportx + REG_SP_CTRL1);
	}

	audio_sp_set_tx_clk_div(sportx, SP_RXInitStruct->sp_in_clock, SP_RXInitStruct->sp_sr, SP_RXInitStruct->sp_sel_tdm, chn_len, SP_RXInitStruct->sp_set_multi_io);
	audio_sp_set_rx_clk_div(sportx, SP_RXInitStruct->sp_in_clock, SP_RXInitStruct->sp_sr, SP_RXInitStruct->sp_sel_tdm, chn_len, SP_RXInitStruct->sp_set_multi_io);

	/* Configure RX FIFO Channel */
	if (SP_RXInitStruct->sp_sel_fifo == SP_RX_FIFO2) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~(SP_BIT_RX_FIFO_0_REG_1_EN | SP_BIT_RX_FIFO_1_REG_0_EN | SP_BIT_RX_FIFO_1_REG_1_EN);
		tmp |= SP_BIT_RX_FIFO_0_REG_0_EN;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_RXInitStruct->sp_sel_fifo == SP_RX_FIFO4) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~(SP_BIT_RX_FIFO_1_REG_0_EN | SP_BIT_RX_FIFO_1_REG_1_EN);
		tmp |= SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_RXInitStruct->sp_sel_fifo == SP_RX_FIFO6) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_RX_FIFO_1_REG_1_EN;
		tmp |= (SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN | SP_BIT_RX_FIFO_1_REG_0_EN);
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_RXInitStruct->sp_sel_fifo == SP_RX_FIFO8) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp |= (SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN | SP_BIT_RX_FIFO_1_REG_0_EN | SP_BIT_RX_FIFO_1_REG_1_EN);
		writel(tmp, sportx + REG_SP_CTRL1);
	}

	/* Configure SPORT parameters:RX MULTI_IO Mode*/
	if (SP_RXInitStruct->sp_set_multi_io == SP_RX_MULTIIO_EN) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp |= SP_BIT_MULTI_IO_EN_RX;
		writel(tmp, sportx + REG_SP_CTRL1);
		audio_sp_set_tx_clk_div(sportx, SP_RXInitStruct->sp_in_clock, SP_RXInitStruct->sp_sr, SP_RX_NOTDM, chn_len, SP_RXInitStruct->sp_set_multi_io);
		audio_sp_set_rx_clk_div(sportx, SP_RXInitStruct->sp_in_clock, SP_RXInitStruct->sp_sr, SP_RX_NOTDM, chn_len, SP_RXInitStruct->sp_set_multi_io);
	} else {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_RX;
		writel(tmp, sportx + REG_SP_CTRL1);
	}

	/*set DMA burstsize: 8*/
	tmp = readl(sportx + REG_SP_TX_LRCLK);
	tmp &= ~(SP_MASK_RXDMA_BUSRTSIZE);
	tmp |= SP_RXDMA_BUSRTSIZE(8);
	writel(tmp, sportx + REG_SP_TX_LRCLK);

}


void audio_sp_tx_init(void __iomem * sportx, sport_init_params *SP_TXInitStruct)
{
	u32 tmp;
	u32 chn_len;

	/* Check the parameters*/
	assert_param(IS_SP_DATA_FMT(SP_TXInitStruct->sp_sel_data_format));
	assert_param(IS_SP_TX_WL(SP_TXInitStruct->sp_sel_word_len));
	assert_param(IS_SP_TXCH_LEN(SP_TXInitStruct->sp_sel_ch_len));
	assert_param(IS_SP_SEL_TX_CH(SP_TXInitStruct->sp_sel_ch));
	assert_param(IS_SP_SEL_TX_TDM(SP_TXInitStruct->sp_sel_tdm));
	assert_param(IS_SP_SEL_TX_FIFO(SP_TXInitStruct->sp_sel_fifo));
	assert_param(IS_SP_CHN_NUM(SP_TXInitStruct->sp_sel_i2s0_mono_stereo));
	assert_param(IS_SP_CHN_NUM(SP_TXInitStruct->sp_sel_i2s1_mono_stereo));
	assert_param(IS_SP_SET_TX_MULTIIO(SP_TXInitStruct->sp_set_multi_io));
	switch (SP_TXInitStruct->sp_sel_ch_len) {
	case 0:
		chn_len = 16;
		break;
	case 1:
		chn_len = 20;
		break;
	case 2:
		chn_len = 24;
		break;
	default:
		chn_len = 32;
		break;
	}
	audio_sp_set_tx_clk_div(sportx, SP_TXInitStruct->sp_in_clock, SP_TXInitStruct->sp_sr, SP_TXInitStruct->sp_sel_tdm, chn_len, SP_TXInitStruct->sp_set_multi_io);

	audio_sp_enable(sportx);

	/* Configure parameters: disable RX, disable TX, AUDIO SPORT mode */
	audio_sp_tx_start(sportx, false);
	audio_sp_rx_start(sportx, false);

	/* Configure FIFO0 parameters: data format, word length, channel number, etc */
	tmp = readl(sportx + REG_SP_FORMAT);
	tmp &= ~(SP_BIT_TRX_SAME_LENGTH);
	writel(tmp, sportx + REG_SP_FORMAT);

	tmp = readl(sportx + REG_SP_CTRL0);
	tmp &= ~(SP_MASK_DATA_LEN_SEL_TX_0 | SP_MASK_DATA_FORMAT_SEL_TX | SP_BIT_EN_I2S_MONO_TX_0
						  | SP_MASK_SEL_I2S_TX_CH | SP_BIT_TRX_SAME_CH | SP_MASK_TDM_MODE_SEL_TX);
	writel(tmp, sportx + REG_SP_CTRL0);
	tmp |= ((SP_DATA_LEN_SEL_TX_0(SP_TXInitStruct->sp_sel_word_len)) | (SP_DATA_FORMAT_SEL_TX(SP_TXInitStruct->sp_sel_data_format))
						 | (SP_SEL_I2S_TX_CH(SP_TXInitStruct->sp_sel_ch)));
	writel(tmp, sportx + REG_SP_CTRL0);

	if (SP_TXInitStruct->sp_sel_i2s0_mono_stereo == SP_CH_STEREO) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp &= ~ SP_BIT_EN_I2S_MONO_TX_0;
		writel(tmp, sportx + REG_SP_CTRL0);
	} else {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= (SP_BIT_EN_I2S_MONO_TX_0);
		writel(tmp, sportx + REG_SP_CTRL0);
	}

	/* Configure parameters:Channel Length, data format*/
	tmp = readl(sportx + REG_SP_FORMAT);
	tmp &= ~(SP_MASK_CH_LEN_SEL_TX | SP_BIT_TRX_SAME_CH_LEN | SP_BIT_TRX_SAME_CH);
	writel(tmp, sportx + REG_SP_FORMAT);
	tmp |= SP_CH_LEN_SEL_TX(SP_TXInitStruct->sp_sel_ch_len);
	writel(tmp, sportx + REG_SP_FORMAT);

	/* Configure FIFO1 parameters*/
	tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
	tmp &= ~SP_MASK_DATA_LEN_SEL_TX_1;
	writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
	tmp |= SP_DATA_LEN_SEL_TX_1(SP_TXInitStruct->sp_sel_word_len);
	writel(tmp, sportx + REG_SP_DIRECT_CTRL1);

	if (SP_TXInitStruct->sp_sel_i2s1_mono_stereo == SP_CH_STEREO) {
		tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
		tmp &= ~ SP_BIT_EN_I2S_MONO_TX_1;
		writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
	} else {
		tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
		tmp |= (SP_BIT_EN_I2S_MONO_TX_1);
		writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
	}

	/* Configure parameters: TX TDM mode */
	if (SP_TXInitStruct->sp_sel_tdm == SP_TX_NOTDM) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_TX(NOTDM);
		writel(tmp, sportx + REG_SP_CTRL0);
	} else if (SP_TXInitStruct->sp_sel_tdm == SP_TX_TDM4) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_TX(TDM4);
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_TX;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_TXInitStruct->sp_sel_tdm == SP_TX_TDM6) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_TX(TDM6);
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_TX;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_TDM_MODE_SEL_TX(TDM8);
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_TX;
		writel(tmp, sportx + REG_SP_CTRL1);
	}

	/* Configure TX FIFO Channel */
	if (SP_TXInitStruct->sp_sel_fifo == SP_TX_FIFO2) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~(SP_BIT_TX_FIFO_0_REG_1_EN | SP_BIT_TX_FIFO_1_REG_0_EN | SP_BIT_TX_FIFO_1_REG_1_EN);
		tmp |= SP_BIT_TX_FIFO_0_REG_0_EN;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_TXInitStruct->sp_sel_fifo == SP_TX_FIFO4) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~(SP_BIT_TX_FIFO_1_REG_0_EN | SP_BIT_TX_FIFO_1_REG_1_EN);
		tmp |= SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_TXInitStruct->sp_sel_fifo == SP_TX_FIFO6) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_TX_FIFO_1_REG_1_EN;
		tmp |= (SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN | SP_BIT_TX_FIFO_1_REG_0_EN);
		writel(tmp, sportx + REG_SP_CTRL1);
	} else if (SP_TXInitStruct->sp_sel_fifo == SP_TX_FIFO8) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp |= (SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN | SP_BIT_TX_FIFO_1_REG_0_EN | SP_BIT_TX_FIFO_1_REG_1_EN);
		writel(tmp, sportx + REG_SP_CTRL1);
	}

	/* Configure SPORT parameters: TX MULTI_IO Mode*/
	if (SP_TXInitStruct->sp_set_multi_io == SP_TX_MULTIIO_EN) {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp |= SP_BIT_MULTI_IO_EN_TX;
		writel(tmp, sportx + REG_SP_CTRL1);
	} else {
		tmp = readl(sportx + REG_SP_CTRL1);
		tmp &= ~SP_BIT_MULTI_IO_EN_TX;
		writel(tmp, sportx + REG_SP_CTRL1);
	}

	/*set DMA burstsize: 8*/
	tmp = readl(sportx + REG_SP_TX_LRCLK);
	tmp &= ~(SP_MASK_TXDMA_BURSTSIZE);
	tmp |= SP_TXDMA_BURSTSIZE(8);
	writel(tmp, sportx + REG_SP_TX_LRCLK);

}

void audio_sp_tx_start(void __iomem * sportx, bool NewState)
{
	u32 tmp;

	if (NewState == true) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp &= ~ SP_BIT_TX_DISABLE;
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp |= SP_BIT_START_TX;
		writel(tmp, sportx + REG_SP_CTRL0);
	} else {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_BIT_TX_DISABLE;
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp &= ~ SP_BIT_START_TX;
		writel(tmp, sportx + REG_SP_CTRL0);
	}
}

void audio_sp_rx_start(void __iomem * sportx, bool NewState)
{
	u32 tmp;

	if (NewState == true) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp &= ~ SP_BIT_RX_DISABLE;
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp |= SP_BIT_START_RX;;
		writel(tmp, sportx + REG_SP_CTRL0);

	} else {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_BIT_RX_DISABLE;
		writel(tmp, sportx + REG_SP_CTRL0);
		tmp &= ~ SP_BIT_START_RX;
		writel(tmp, sportx + REG_SP_CTRL0);
	}
}

void audio_sp_dma_cmd(void __iomem * sportx, bool NewState)
{
	u32 tmp;

	if (NewState == true) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp &= ~ SP_BIT_DSP_CTL_MODE;
		writel(tmp, sportx + REG_SP_CTRL0);
	} else {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp |= SP_BIT_DSP_CTL_MODE;
		writel(tmp, sportx + REG_SP_CTRL0);
	}
}

void audio_sp_set_tx_word_len(void __iomem * sportx, u32 SP_TX_WordLen, u32 SP_RX_WordLen)
{
	u32 tmp;
	assert_param(IS_SP_TX_WL(SP_TX_WordLen));

	tmp = readl(sportx + REG_SP_CTRL0);
	tmp &= ~(SP_MASK_DATA_LEN_SEL_TX_0);
	tmp |= SP_DATA_LEN_SEL_TX_0(SP_TX_WordLen);
	writel(tmp, sportx + REG_SP_CTRL0);

	tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
	tmp &= ~SP_MASK_DATA_LEN_SEL_TX_1;
	tmp |=  SP_DATA_LEN_SEL_TX_1(SP_TX_WordLen);
	writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
}

void audio_sp_set_rx_word_len(void __iomem * sportx, u32 SP_TX_WordLen, u32 SP_RX_WordLen)
{
	u32 tmp;
	assert_param(IS_SP_RX_WL(SP_RX_WordLen));

	tmp = readl(sportx + REG_SP_FORMAT);
	tmp &= ~(SP_MASK_DATA_LEN_SEL_RX_0);
	tmp |= SP_DATA_LEN_SEL_RX_0(SP_RX_WordLen);
	writel(tmp, sportx + REG_SP_FORMAT);

	tmp = readl(sportx + REG_SP_DIRECT_CTRL1);
	tmp &= ~SP_MASK_DATA_LEN_SEL_RX_1;
	tmp |=  SP_DATA_LEN_SEL_RX_1(SP_RX_WordLen);
	writel(tmp, sportx + REG_SP_DIRECT_CTRL1);
}

u32 audio_sp_get_tx_word_len(void __iomem * sportx)
{
	u32 len_TX = ((readl(sportx + REG_SP_CTRL0)) & SP_MASK_DATA_LEN_SEL_TX_0) >> 12;
	return len_TX;
}

u32 audio_sp_get_rx_word_len(void __iomem * sportx)
{
	u32 len_RX = ((readl(sportx + REG_SP_FORMAT)) & SP_MASK_DATA_LEN_SEL_RX_0) >> 12;
	return len_RX;
}

void audio_sp_set_mono_stereo(void __iomem * sportx, u32 SP_MonoStereo)
{
	u32 tmp;
	assert_param(IS_SP_CHN_NUM(SP_MonoStereo));

	if (SP_MonoStereo == SP_CH_STEREO) {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp &= ~(SP_BIT_EN_I2S_MONO_TX_0);
		writel(tmp, sportx + REG_SP_CTRL0);
	} else {
		tmp = readl(sportx + REG_SP_CTRL0);
		tmp &= ~SP_BIT_TRX_SAME_CH;
		tmp |= SP_BIT_EN_I2S_MONO_TX_0;
		writel(tmp, sportx + REG_SP_CTRL0);
	}
}

/**
  * @brief  Set SPORT phase latch.
  * @param  index: select SPORT.
  */
void audio_sp_set_phase_latch(void __iomem * sportx)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_LRCLK);
	tmp |= SP_BIT_EN_FS_PHASE_LATCH;
	writel(tmp, sportx + REG_SP_RX_LRCLK);
}


void audio_sp_set_tx_count(void __iomem * sportx, u32 comp_val)
{
	u32 tmp;

	tmp = readl(sportx + REG_SP_RX_LRCLK);
	tmp |= SP_BIT_EN_TX_SPORT_INTERRUPT;
	tmp &= ~SP_MASK_TX_SPORT_COMPARE_VAL;
	tmp |= SP_TX_SPORT_COMPARE_VAL(comp_val);
	writel(tmp, sportx + REG_SP_RX_LRCLK);

}

void audio_sp_disable_tx_count(void __iomem * sportx)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_LRCLK);
	tmp &= ~SP_BIT_EN_TX_SPORT_INTERRUPT;
	writel(tmp, sportx + REG_SP_RX_LRCLK);
}

u32 audio_sp_get_tx_count(void __iomem * sportx)
{
	u32 tmp;
	u32 dsp_counter;
	u32 tx_counter;

	dsp_counter = readl(sportx + REG_SP_DSP_COUNTER);
	tx_counter = (dsp_counter & SP_MASK_TX_SPORT_COUNTER) >> 5;

	return tx_counter;
}

/**
  * @brief  Get SPORT tx phase value.
  * @param  index: select SPORT.
  */
u32 audio_sp_get_tx_phase_val(void __iomem * sportx)
{
	u32 tmp;
	u32 tx_phase;

	tmp = readl(sportx + REG_SP_DSP_COUNTER);
	tx_phase = tmp & SP_MASK_TX_FS_PHASE_RPT;

	return tx_phase;
}

bool audio_sp_is_tx_sport_irq(void __iomem * sportx)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_LRCLK);
	tmp &= SP_BIT_CLR_TX_SPORT_RDY;
	return tmp ? true : false;
}

void audio_sp_clear_tx_sport_irq(void __iomem * sportx)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_LRCLK);
	tmp |= SP_BIT_CLR_TX_SPORT_RDY;
	writel(tmp, sportx + REG_SP_RX_LRCLK);
}

void audio_sp_set_rx_count(void __iomem * sportx, u32 comp_val)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_COUNTER1);
	tmp |= SP_BIT_EN_RX_SPORT_INTERRUPT;
	tmp &= ~SP_MASK_RX_SPORT_COMPARE_VAL;
	tmp |= SP_RX_SPORT_COMPARE_VAL(comp_val);
	writel(tmp, sportx + REG_SP_RX_COUNTER1);

}

void audio_sp_disable_rx_count(void __iomem * sportx)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_COUNTER1);
	tmp &= ~SP_BIT_EN_RX_SPORT_INTERRUPT;
	writel(tmp, sportx + REG_SP_RX_COUNTER1);
}

u32 audio_sp_get_rx_count(void __iomem * sportx)
{
	u32 tmp;
	u32 rx_counter2;
	u32 rx_counter;

	rx_counter2 = readl(sportx + REG_SP_RX_COUNTER2);
	rx_counter = (rx_counter2 & SP_MASK_RX_SPORT_COUNTER) >> 5;

	return rx_counter;
}

/**
  * @brief  Get SPORT RX phase.
  * @param  index: select SPORT.
  */
u32 audio_sp_get_rx_phase_val(void __iomem * sportx)
{
	u32 tmp;
	u32 rx_phase;

	tmp = readl(sportx + REG_SP_RX_COUNTER2);
	rx_phase = tmp & SP_MASK_RX_FS_PHASE_RPT;

	return rx_phase;
}

bool audio_sp_is_rx_sport_irq(void __iomem * sportx)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_COUNTER1);
	tmp &= SP_BIT_CLR_RX_SPORT_RDY;
	return tmp ? true : false;
}

void audio_sp_clear_rx_sport_irq(void __iomem * sportx)
{
	u32 tmp;
	tmp = readl(sportx + REG_SP_RX_COUNTER1);
	tmp |= SP_BIT_CLR_RX_SPORT_RDY;
	writel(tmp, sportx + REG_SP_RX_COUNTER1);

}

void audio_sp_disable_rx_tx_sport_irq(void __iomem * sportx)
{
	u32 tmp;
	/* disable tx sport interrupt */
	tmp = readl(sportx + REG_SP_RX_LRCLK);
	tmp &= ~SP_BIT_EN_TX_SPORT_INTERRUPT;
	writel(tmp, sportx + REG_SP_RX_LRCLK);

	/* disable rx sport interrupt */
	tmp = readl(sportx + REG_SP_RX_COUNTER1);
	tmp &= ~SP_BIT_EN_RX_SPORT_INTERRUPT;
	writel(tmp, sportx + REG_SP_RX_COUNTER1);

}

/**
  * @brief  Set AUDIO SPORT TX FIFO enable or disable.
  * @param  index: sport index.
  * @param  fifo_num: tx fifo number.
  * @param  new_state: enable or disable.
  * @retval None
  */
void audio_sp_tx_set_fifo(void __iomem * sportx, u32 fifo_num, bool new_state)
{
	assert_param(IS_SP_SEL_TX_FIFO(fifo_num));
	u32 tmp;
	tmp = readl(sportx + REG_SP_CTRL1);

	if (new_state) {
		if (fifo_num == SP_TX_FIFO2) {
			tmp |= SP_BIT_TX_FIFO_0_REG_0_EN;
		} else if (fifo_num == SP_TX_FIFO4) {
			tmp |= SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN;
		} else if (fifo_num == SP_TX_FIFO6) {
			tmp |= (SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN | SP_BIT_TX_FIFO_1_REG_0_EN);
		} else {
			tmp |= (SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN | SP_BIT_TX_FIFO_1_REG_0_EN | SP_BIT_TX_FIFO_1_REG_1_EN);
		}
	} else {
		if (fifo_num == SP_TX_FIFO2) {
			tmp &= ~(SP_BIT_TX_FIFO_0_REG_0_EN);
		} else if (fifo_num == SP_TX_FIFO4) {
			tmp &= ~(SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN);
		} else if (fifo_num == SP_TX_FIFO6) {
			tmp &= ~(SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN | SP_BIT_TX_FIFO_1_REG_0_EN);
		} else {
			tmp &= ~(SP_BIT_TX_FIFO_0_REG_0_EN | SP_BIT_TX_FIFO_0_REG_1_EN | SP_BIT_TX_FIFO_1_REG_0_EN | SP_BIT_TX_FIFO_1_REG_1_EN);
		}
	}

	writel(tmp, sportx + REG_SP_CTRL1);
}

/**
  * @brief  Set AUDIO SPORT RX FIFO enable or disable.
  * @param  index: sport index.
  * @param  fifo_num: rx fifo number.
  * @param  new_state: enable or disable.
  * @retval None
  */
void audio_sp_rx_set_fifo(void __iomem * sportx, u32 fifo_num, bool new_state)
{
	assert_param(IS_SP_SEL_RX_FIFO(fifo_num));
	u32 tmp;
	tmp = readl(sportx + REG_SP_CTRL1);

	if (new_state) {
		if (fifo_num == SP_RX_FIFO2) {
			tmp |= SP_BIT_RX_FIFO_0_REG_0_EN;
		} else if (fifo_num == SP_RX_FIFO4) {
			tmp |= SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN;
		} else if (fifo_num == SP_RX_FIFO6) {
			tmp |= (SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN | SP_BIT_RX_FIFO_1_REG_0_EN);
		} else {
			tmp |= (SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN | SP_BIT_RX_FIFO_1_REG_0_EN | SP_BIT_RX_FIFO_1_REG_1_EN);
		}

	} else {
		if (fifo_num == SP_RX_FIFO2) {
			tmp &= ~(SP_BIT_RX_FIFO_0_REG_0_EN);
		} else if (fifo_num == SP_RX_FIFO4) {
			tmp &= ~(SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN);
		} else if (fifo_num == SP_RX_FIFO6) {
			tmp &= ~(SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN | SP_BIT_RX_FIFO_1_REG_0_EN);
		} else {
			tmp &= ~(SP_BIT_RX_FIFO_0_REG_0_EN | SP_BIT_RX_FIFO_0_REG_1_EN | SP_BIT_RX_FIFO_1_REG_0_EN | SP_BIT_RX_FIFO_1_REG_1_EN);
		}
	}

	writel(tmp, sportx + REG_SP_CTRL1);
}

#if 0
struct sport_counter_info {
	void __iomem * sport_base_addr;
	u32 irq_total_tx_counter;
	u32 irq_total_rx_counter;
};

struct sport_counter_info counter_info[4] = {{.sport_base_addr = NULL, .irq_total_tx_counter = 0, .irq_total_rx_counter = 0},
											{.sport_base_addr = NULL, .irq_total_tx_counter = 0, .irq_total_rx_counter = 0},
											{.sport_base_addr = NULL, .irq_total_tx_counter = 0, .irq_total_rx_counter = 0}}

void audio_sp_set_sport_addr(u32 sport_index, void __iomem * sportx)
{
	counter_info[sport_index].sport_base_addr = sportx;
}

__iomem * audio_sp_get_sport_addr(u32 sport_index)
{
	return counter_info[sport_index].sport_base_addr;
}

void audio_sp_set_sport_irq_total_tx_counter(u32 sport_index, u32 irq_total_tx_counter)
{
	counter_info[sport_index].irq_total_tx_counter = irq_total_tx_counter;
}

u32 audio_sp_get_sport_irq_total_tx_counter(u32 sport_index)
{
	return counter_info[sport_index].irq_total_tx_counter;
}

void audio_sp_set_sport_irq_total_rx_counter(u32 sport_index, u32 irq_total_rx_counter);
{
	counter_info[sport_index].irq_total_rx_counter = irq_total_rx_counter;
}

u32 audio_sp_get_sport_irq_total_rx_counter(u32 sport_index)
{
	return counter_info[sport_index].irq_total_rx_counter;
}
#endif